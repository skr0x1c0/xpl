//
//  xnu_socket.c
//  xe
//
//  Created by admin on 11/20/21.
//

#include "xnu_socket.h"
#include "kmem.h"
#include "platform_types.h"


uintptr_t xe_xnu_socket_get_so_pcb(uintptr_t socket) {
    return xe_kmem_read_uint64(socket + TYPE_SOCKET_MEM_SO_PCB_OFFSET);
}

void xe_xnu_socket_set_so_pcb(uintptr_t socket, uintptr_t pcb) {
    xe_kmem_write_uint64(socket + TYPE_SOCKET_MEM_SO_PCB_OFFSET, pcb);
}

uintptr_t xe_xnu_socket_get_so_proto(uintptr_t socket) {
    return xe_kmem_read_uint64(socket + TYPE_SOCKET_MEM_SO_PROTO_OFFSET);
}

void xe_xnu_socket_set_so_proto(uintptr_t socket, uintptr_t protosw) {
    xe_kmem_write_uint64(socket + TYPE_SOCKET_MEM_SO_PROTO_OFFSET, protosw);
}

uintptr_t xe_xnu_socket_in_get_inpcb_mtx(uintptr_t inpcb) {
    return inpcb + TYPE_INPCB_MEM_INPCB_MTX_OFFSET;
}

uintptr_t xe_xnu_socket_protosw_get_pr_domain(uintptr_t so_proto) {
    return xe_kmem_read_uint64(so_proto + TYPE_PROTOSW_MEM_PR_DOMAIN_OFFSET);
}

void xe_xnu_socket_protosw_set_pr_domain(uintptr_t so_proto, uintptr_t pr_domain) {
    xe_kmem_write_uint64(so_proto + TYPE_PROTOSW_MEM_PR_DOMAIN_OFFSET, pr_domain);
}

uintptr_t xe_xnu_socket_protosw_get_pr_lock(uintptr_t so_proto) {
    return xe_kmem_read_uint64(so_proto + TYPE_PROTOSW_MEM_PR_LOCK_OFFSET);
}

uintptr_t xe_xnu_socket_protosw_get_pr_unlock(uintptr_t so_proto) {
    return xe_kmem_read_uint64(so_proto + TYPE_PROTOSW_MEM_PR_UNLOCK_OFFSET);
}

void xe_xnu_socket_protosw_set_pr_unlock(uintptr_t so_proto, uintptr_t value) {
    xe_kmem_write_uint64(so_proto + TYPE_PROTOSW_MEM_PR_UNLOCK_OFFSET, value);
}

uintptr_t xe_xnu_socket_protosw_get_pr_getlock(uintptr_t so_proto) {
    return xe_kmem_read_uint64(so_proto + TYPE_PROTOSW_MEM_PR_GETLOCK_OFFSET);
}

void xe_xnu_socket_protosw_set_pr_getlock(uintptr_t so_proto, uintptr_t val) {
    xe_kmem_write_uint64(so_proto + TYPE_PROTOSW_MEM_PR_GETLOCK_OFFSET, val);
}

void xe_xnu_socket_protosw_set_pr_lock(uintptr_t so_proto, uintptr_t lock) {
    xe_kmem_write_uint64(so_proto + TYPE_PROTOSW_MEM_PR_LOCK_OFFSET, lock);
}

uintptr_t xe_xnu_socket_domain_get_dom_mtx(uintptr_t pr_domain) {
    return xe_kmem_read_uint64(pr_domain + TYPE_DOMAIN_MEM_DOM_MTX_OFFSET);
}

void xe_xnu_socket_domain_set_dom_mtx(uintptr_t pr_domain, uintptr_t dom_mtx) {
    xe_kmem_write_uint64(pr_domain + TYPE_DOMAIN_MEM_DOM_MTX_OFFSET, dom_mtx);
}

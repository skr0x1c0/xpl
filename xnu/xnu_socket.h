//
//  xnu_socket.h
//  xe
//
//  Created by admin on 11/20/21.
//

#ifndef xnu_socket_h
#define xnu_socket_h

#include <stdio.h>

uintptr_t xe_xnu_socket_get_so_pcb(uintptr_t socket);
void xe_xnu_socket_set_so_pcb(uintptr_t socket, uintptr_t pcb);
uintptr_t xe_xnu_socket_get_so_proto(uintptr_t socket);
void xe_xnu_socket_set_so_proto(uintptr_t socket, uintptr_t protosw);
uintptr_t xe_xnu_socket_in_get_inpcb_mtx(uintptr_t inpcb);
uintptr_t xe_xnu_socket_protosw_get_pr_domain(uintptr_t so_proto);
void xe_xnu_socket_protosw_set_pr_domain(uintptr_t so_proto, uintptr_t pr_domain);
uintptr_t xe_xnu_socket_protosw_get_pr_getlock(uintptr_t so_proto);
void xe_xnu_socket_protosw_set_pr_getlock(uintptr_t so_proto, uintptr_t val);
uintptr_t xe_xnu_socket_protosw_get_pr_lock(uintptr_t so_proto);
void xe_xnu_socket_protosw_set_pr_lock(uintptr_t so_proto, uintptr_t lock);
uintptr_t xe_xnu_socket_protosw_get_pr_unlock(uintptr_t so_proto);
void xe_xnu_socket_protosw_set_pr_unlock(uintptr_t so_proto, uintptr_t value);

uintptr_t xe_xnu_socket_domain_get_dom_mtx(uintptr_t pr_domain);
void xe_xnu_socket_domain_set_dom_mtx(uintptr_t pr_domain, uintptr_t dom_mtx);

#endif /* xnu_socket_h */

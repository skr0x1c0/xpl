
#ifndef xpl_ipc_h
#define xpl_ipc_h

#include <stdio.h>

void xpl_ipc_iter_mach_ports(uintptr_t proc, _Bool(^callback)(uint32_t name, uintptr_t port));
int xpl_ipc_find_mach_port(uintptr_t proc, uint32_t mach_port_name, uintptr_t* out);
uintptr_t xpl_ipc_port_get_receiver_task(uintptr_t port);

#endif /* xpl_ipc_h */

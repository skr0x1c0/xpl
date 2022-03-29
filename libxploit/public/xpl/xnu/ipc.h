//
//  ipc.h
//  libxe
//
//  Created by sreejith on 3/8/22.
//

#ifndef ipc_h
#define ipc_h

#include <stdio.h>

void xpl_xnu_ipc_iter_mach_ports(uintptr_t proc, _Bool(^callback)(uint32_t name, uintptr_t port));
int xpl_xnu_ipc_find_mach_port(uintptr_t proc, uint32_t mach_port_name, uintptr_t* out);
uintptr_t xpl_xnu_ipc_port_get_receiver_task(uintptr_t port);

#endif /* ipc_h */

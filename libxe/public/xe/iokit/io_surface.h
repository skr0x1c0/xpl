//
//  io_surface.h
//  xe
//
//  Created by admin on 12/6/21.
//

#ifndef xe_io_surface_h
#define xe_io_surface_h

#include <stdio.h>

uintptr_t xe_io_surface_root(void);
int xe_io_surface_scan_user_client_for_prop(uintptr_t root_user_client, char* key, uintptr_t* out);
int xe_io_surface_scan_all_clients_for_prop(char* key, uintptr_t* out);

#endif /* xe_io_surface_h */

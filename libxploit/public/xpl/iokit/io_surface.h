//
//  io_surface.h
//  xe
//
//  Created by admin on 12/6/21.
//

#ifndef xpl_io_surface_h
#define xpl_io_surface_h

#include <stdio.h>

#include <IOKit/IOKitLib.h>
#include <IOSurface/IOSurface.h>

uintptr_t xpl_io_surface_root(void);
uintptr_t xpl_io_surface_get_root_user_client(uintptr_t task);
uintptr_t xpl_io_surface_get_user_client(uintptr_t root_user_client, uint32_t index);
IOSurfaceRef xpl_io_surface_create(uintptr_t* addr_out);

#endif /* xpl_io_surface_h */

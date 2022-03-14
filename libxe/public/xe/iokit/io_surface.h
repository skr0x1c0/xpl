//
//  io_surface.h
//  xe
//
//  Created by admin on 12/6/21.
//

#ifndef xe_io_surface_h
#define xe_io_surface_h

#include <stdio.h>

#include <IOKit/IOKitLib.h>
#include <IOSurface/IOSurface.h>

uintptr_t xe_io_surface_root(void);
uintptr_t xe_io_surface_get_root_user_client(uintptr_t task);
uintptr_t xe_io_surface_get_user_client(uintptr_t root_user_client, uint32_t index);
IOSurfaceRef xe_io_surface_create(uintptr_t* addr_out);

#endif /* xe_io_surface_h */

//
//  zone_reserve_va.h
//  kmem_xepg
//
//  Created by admin on 12/26/21.
//

#ifndef zone_kext_reserve_va_h
#define zone_kext_reserve_va_h

#include <stdio.h>

void kmem_zone_kext_reserve_va(const struct sockaddr_in* smb_addr, uint num_pages, uint z_elem_size);

#endif /* zone_kext_reserve_va_h */

//
//  zone.h
//  libxe
//
//  Created by sreejith on 2/27/22.
//

#ifndef zone_h
#define zone_h

#include <stdio.h>

typedef struct xpl_util_zalloc* xpl_util_zalloc_t;

uintptr_t xpl_util_zalloc_find_zone_at_index(uint zone_index);
xpl_util_zalloc_t xpl_util_zalloc_create(uintptr_t zone, int num_allocs);
uintptr_t xpl_util_zalloc_alloc(xpl_util_zalloc_t util, int element_idx);
void xpl_util_zalloc_destroy(xpl_util_zalloc_t* util_p);

#endif /* zone_h */

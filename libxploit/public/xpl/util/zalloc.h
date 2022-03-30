//
//  zone.h
//  libxe
//
//  Created by sreejith on 2/27/22.
//

#ifndef zone_h
#define zone_h

#include <stdio.h>

typedef struct xpl_zalloc* xpl_zalloc_t;

/// Returns the address of zone at provided index
/// @param zone_index index of zone in `zone_array`
uintptr_t xpl_zalloc_find_zone_at_index(uint zone_index);

/// Create `xpl_zalloc_t` utility for allocating memory from an arbitary kernel zone
/// @param zone address of zone from which memory is to be allocated
/// @param num_allocs maximum number of allocations that will be made using created utility
xpl_zalloc_t xpl_zalloc_create(uintptr_t zone, int num_allocs);

/// Allocates memory from zone at provided element index
/// @param util `xpl_zalloc_t` created using `xpl_zalloc_create`
/// @param element_idx element index from which memory must be allocated
uintptr_t xpl_zalloc_alloc(xpl_zalloc_t util, int element_idx);

/// Release resources
/// @param util_p pointer to `xpl_zalloc_t` created using `xpl_zalloc_create`
void xpl_zalloc_destroy(xpl_zalloc_t* util_p);

#endif /* zone_h */

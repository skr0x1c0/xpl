//
//  util_zalloc.h
//  xe
//
//  Created by admin on 12/8/21.
//

#ifndef xe_util_zalloc_h
#define xe_util_zalloc_h

#include <stdio.h>

typedef struct xe_util_zalloc* xe_util_zalloc_t;

uintptr_t xe_util_zalloc_find_zone_at_index(uint zone_index);
xe_util_zalloc_t xe_util_zalloc_create(uintptr_t zone, uint num_pages);
uintptr_t xe_util_zalloc_alloc(xe_util_zalloc_t util);
uintptr_t xe_util_zalloc_alloc_at(xe_util_zalloc_t util, uintptr_t address);
uint16_t xe_util_zalloc_z_elem_size(xe_util_zalloc_t util);
void xe_util_zalloc_free(xe_util_zalloc_t util, uintptr_t address);

#endif /* xe_util_zalloc_h */

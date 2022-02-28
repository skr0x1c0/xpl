//
//  zone.h
//  libxe
//
//  Created by sreejith on 2/27/22.
//

#ifndef zone_h
#define zone_h

#include <stdio.h>

typedef struct xe_util_zalloc* xe_util_zalloc_t;

uintptr_t xe_util_zalloc_find_zone_at_index(uint zone_index);
xe_util_zalloc_t xe_util_zalloc_create(uintptr_t zone, int num_allocs);
uintptr_t xe_util_zalloc_alloc(xe_util_zalloc_t util, int element_idx);

#endif /* zone_h */

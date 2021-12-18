//
//  util_kalloc_heap.h
//  xe
//
//  Created by admin on 12/6/21.
//

#ifndef util_kalloc_heap_h
#define util_kalloc_heap_h

#include <stdio.h>

uintptr_t xe_util_kh_find_zone_for_size(uintptr_t heap, size_t size);

#endif /* util_kalloc_heap_h */

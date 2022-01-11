//
//  util_kalloc_heap.h
//  xe
//
//  Created by admin on 12/6/21.
//

#ifndef xe_util_kalloc_heap_h
#define xe_util_kalloc_heap_h

#include <stdio.h>

uintptr_t xe_util_kh_find_zone_for_size(uintptr_t heap, size_t size);

#endif /* xe_util_kalloc_heap_h */

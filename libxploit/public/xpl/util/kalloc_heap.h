//
//  util_kalloc_heap.h
//  xe
//
//  Created by admin on 12/6/21.
//

#ifndef xpl_util_kalloc_heap_h
#define xpl_util_kalloc_heap_h

#include <stdio.h>

uintptr_t xpl_util_kh_find_zone_for_size(uintptr_t heap, size_t size);

#endif /* xpl_util_kalloc_heap_h */

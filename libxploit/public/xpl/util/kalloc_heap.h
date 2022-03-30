//
//  util_kalloc_heap.h
//  xe
//
//  Created by admin on 12/6/21.
//

#ifndef xpl_kalloc_heap_h
#define xpl_kalloc_heap_h

#include <stdio.h>

/// Returns the address of zone in provided `kalloc_heap` from which allocations
/// of provided `size` will be done. Returns zero when size is greater than size of
/// largest zone in the provided `kalloc_heap`
/// @param heap address of `struct kalloc_heap`
/// @param size size of allocation
uintptr_t xpl_kheap_find_zone_for_size(uintptr_t heap, size_t size);

#endif /* xpl_kalloc_heap_h */

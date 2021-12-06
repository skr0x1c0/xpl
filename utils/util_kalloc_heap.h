//
//  util_kalloc_heap.h
//  xe
//
//  Created by admin on 12/6/21.
//

#ifndef util_kalloc_heap_h
#define util_kalloc_heap_h

#include <stdio.h>

int xe_util_kalloc_heap_alloc(uintptr_t heap, size_t size, uintptr_t* ptr_out);

#endif /* util_kalloc_heap_h */

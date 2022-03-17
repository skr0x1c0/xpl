//
//  io_array.c
//  xe
//
//  Created by admin on 12/6/21.
//

#include "iokit/os_array.h"
#include "memory/kmem.h"
#include "util/ptrauth.h"

#include <macos/kernel.h>


uint xe_os_array_count(uintptr_t array) {
    return xe_kmem_read_uint32(array, TYPE_OS_ARRAY_MEM_COUNT_OFFSET);
}

uintptr_t xe_os_array_value_at_index(uintptr_t array, int index) {
    uintptr_t values = xe_kmem_read_ptr(array, TYPE_OS_ARRAY_MEM_ARRAY_OFFSET);
    return xe_kmem_read_ptr(values, index * sizeof(uintptr_t));
}

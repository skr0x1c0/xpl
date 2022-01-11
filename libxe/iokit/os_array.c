//
//  io_array.c
//  xe
//
//  Created by admin on 12/6/21.
//

#include "iokit/os_array.h"
#include "memory/kmem.h"
#include "util/ptrauth.h"

#include "macos_params.h"


uint xe_os_array_count(uintptr_t array) {
    return xe_kmem_read_uint32(KMEM_OFFSET(array, TYPE_OS_ARRAY_MEM_COUNT_OFFSET));
}

uintptr_t xe_os_array_value_at_index(uintptr_t array, int index) {
    uintptr_t values = xe_kmem_read_uint64(KMEM_OFFSET(array, TYPE_OS_ARRAY_MEM_ARRAY_OFFSET));
    values = XE_PTRAUTH_STRIP(values);
    return xe_kmem_read_uint64(KMEM_OFFSET(values, index * sizeof(uintptr_t)));
}

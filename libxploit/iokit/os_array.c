
#include "iokit/os_array.h"
#include "memory/kmem.h"
#include "util/ptrauth.h"

#include <macos/kernel.h>


uint xpl_os_array_count(uintptr_t array) {
    return xpl_kmem_read_uint32(array, TYPE_OS_ARRAY_MEM_COUNT_OFFSET);
}

uintptr_t xpl_os_array_value_at_index(uintptr_t array, int index) {
    uintptr_t values = xpl_kmem_read_ptr(array, TYPE_OS_ARRAY_MEM_ARRAY_OFFSET);
    return xpl_kmem_read_ptr(values, index * sizeof(uintptr_t));
}

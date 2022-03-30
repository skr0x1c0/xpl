
#include "iokit/os_string.h"
#include "memory/kmem.h"
#include "util/misc.h"
#include "util/ptrauth.h"
#include "util/assert.h"

#include <macos/kernel.h>


size_t xpl_os_string_length(uintptr_t string) {
    uint32_t size = 0;
    xpl_kmem_read_bitfield(&size, string, TYPE_OS_STRING_MEM_LENGTH_BIT_OFFSET, TYPE_OS_STRING_MEM_LENGTH_BIT_SIZE);
    return size;
}

size_t xpl_os_string_read(char* dst, uintptr_t string, size_t dst_size) {
    size_t size = xpl_os_string_length(string);
    uintptr_t data = xpl_ptrauth_strip(xpl_kmem_read_uint64(string, TYPE_OS_STRING_MEM_STRING_OFFSET));
    xpl_kmem_read(dst, data, 0, xpl_min(size, dst_size - 1));
    dst[dst_size - 1] = '\0';
    return size;
}

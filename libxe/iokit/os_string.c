//
//  io_string.c
//  xe
//
//  Created by admin on 12/6/21.
//

#include "os_string.h"
#include "kmem.h"
#include "platform_params.h"
#include "util_misc.h"
#include "util_ptrauth.h"
#include "util_assert.h"


size_t xe_os_string_length(uintptr_t string) {
    uint32_t size = 0;
    xe_kmem_read_bitfield(&size, string, TYPE_OS_STRING_MEM_LENGTH_BIT_OFFSET, TYPE_OS_STRING_MEM_LENGTH_BIT_SIZE);
    return size;
}

size_t xe_os_string_read(char* dst, uintptr_t string, size_t dst_size) {
    size_t size = xe_os_string_length(string);
    uintptr_t data = XE_PTRAUTH_STRIP(xe_kmem_read_uint64(KMEM_OFFSET(string, TYPE_OS_STRING_MEM_STRING_OFFSET)));
    xe_kmem_read(dst, data, XE_MIN(size, dst_size - 1));
    dst[dst_size - 1] = '\0';
    return size;
}

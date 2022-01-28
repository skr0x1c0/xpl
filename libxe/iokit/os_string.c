//
//  io_string.c
//  xe
//
//  Created by admin on 12/6/21.
//

#include "iokit/os_string.h"
#include "memory/kmem.h"
#include "util/misc.h"
#include "util/ptrauth.h"
#include "util/assert.h"

#include "macos_params.h"


size_t xe_os_string_length(uintptr_t string) {
    uint32_t size = 0;
    xe_kmem_read_bitfield(&size, string, TYPE_OS_STRING_MEM_LENGTH_BIT_OFFSET, TYPE_OS_STRING_MEM_LENGTH_BIT_SIZE);
    return size;
}

size_t xe_os_string_read(char* dst, uintptr_t string, size_t dst_size) {
    size_t size = xe_os_string_length(string);
    uintptr_t data = xe_ptrauth_strip(xe_kmem_read_uint64(string, TYPE_OS_STRING_MEM_STRING_OFFSET));
    xe_kmem_read(dst, data, 0, xe_min(size, dst_size - 1));
    dst[dst_size - 1] = '\0';
    return size;
}

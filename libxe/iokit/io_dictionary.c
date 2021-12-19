//
//  io_dictionary.c
//  xe
//
//  Created by admin on 12/6/21.
//

#include <strings.h>
#include <limits.h>

#include <sys/errno.h>

#include "io_dictionary.h"
#include "io_string.h"
#include "platform_types.h"
#include "kmem.h"
#include "util_ptrauth.h"

uint xe_io_os_dictionary_count(uintptr_t dict) {
    return xe_kmem_read_uint32(KMEM_OFFSET(dict, TYPE_OS_DICTIONARY_MEM_COUNT_OFFSET));
}

uintptr_t xe_io_os_dictionary_key(uintptr_t dict_entry, int index) {
    return xe_kmem_read_uint64(KMEM_OFFSET(dict_entry, index * sizeof(uintptr_t) * 2));
}

uintptr_t xe_io_os_dictionary_value(uintptr_t dict_entry, int index) {
    return xe_kmem_read_uint64(KMEM_OFFSET(dict_entry, (index * 2 + 1) * sizeof(uintptr_t)));
}

uintptr_t xe_io_os_dictionary_dict_entry(uintptr_t dict) {
    return XE_PTRAUTH_STRIP(xe_kmem_read_uint64(KMEM_OFFSET(dict, TYPE_OS_DICTIONARY_MEM_DICT_ENTRY_OFFSET)));
}

uintptr_t xe_io_os_dictionary_key_at_index(uintptr_t dict, int index) {
    return xe_io_os_dictionary_key(xe_io_os_dictionary_dict_entry(dict), index);
}

uintptr_t xe_io_os_dictionary_value_at_index(uintptr_t dict, int index) {
    return xe_io_os_dictionary_value(xe_io_os_dictionary_dict_entry(dict), index);
}

void xe_io_os_dictionary_set_value_at_idx(uintptr_t dict, int idx, uintptr_t value) {
    uintptr_t dst = KMEM_OFFSET(xe_io_os_dictionary_dict_entry(dict), (idx * 2 + 1) * sizeof(uintptr_t));
    xe_kmem_write_uint64(dst, value);
}

int xe_io_os_dictionary_set_value_of_key(uintptr_t dict, char* key, uintptr_t value) {
    int idx;
    int error = xe_io_os_dictionary_find_value(dict, key, NULL, &idx);
    if (error) {
        return error;
    }
    xe_io_os_dictionary_set_value_at_idx(dict, idx, value);
    return 0;
}

int xe_io_os_dictionary_find_value(uintptr_t dict, const char* value, uintptr_t* ptr_out, int* idx_out) {
    uint count = xe_io_os_dictionary_count(dict);
    uintptr_t dict_entry = xe_io_os_dictionary_dict_entry(dict);
    
    char buffer[PATH_MAX];
    for (int i=0; i<count; i++) {
        size_t len = xe_io_os_string_read(buffer, xe_io_os_dictionary_key(dict_entry, i), sizeof(buffer));
        assert(len <= sizeof(buffer));
        
        if (strncmp(value, buffer, sizeof(buffer)) == 0) {
            if (ptr_out) *ptr_out = xe_io_os_dictionary_value(dict_entry, i);
            if (idx_out) *idx_out = i;
            return 0;
        }
    }
    
    return ENOENT;
}

void xe_io_os_dictionary_print_keys(uintptr_t dict) {
    uint count = xe_io_os_dictionary_count(dict);
    uintptr_t dictEntry = xe_io_os_dictionary_dict_entry(dict);
    for (uint i=0; i<count; i++) {
        uintptr_t key = xe_kmem_read_uint64(dictEntry + (i * 2 * sizeof(uintptr_t)));
        
        char key_str[PATH_MAX];
        size_t key_len = xe_io_os_string_read(key_str, key, sizeof(key_str));
        assert(key_len <= sizeof(key_str));
        
        printf("%s\n", key_str);
    }
}
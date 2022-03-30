
#include <strings.h>
#include <limits.h>

#include <sys/errno.h>

#include "iokit/os_dictionary.h"
#include "iokit/os_string.h"
#include "memory/kmem.h"
#include "util/ptrauth.h"
#include "util/assert.h"
#include "util/log.h"

#include <macos/kernel.h>


uint xpl_os_dictionary_count(uintptr_t dict) {
    return xpl_kmem_read_uint32(dict, TYPE_OS_DICTIONARY_MEM_COUNT_OFFSET);
}

uintptr_t xpl_os_dictionary_key(uintptr_t dict_entry, int index) {
    return xpl_kmem_read_uint64(dict_entry, index * sizeof(uintptr_t) * 2);
}

uintptr_t xpl_os_dictionary_value(uintptr_t dict_entry, int index) {
    return xpl_kmem_read_uint64(dict_entry, (index * 2 + 1) * sizeof(uintptr_t));
}

uintptr_t xpl_os_dictionary_dict_entry(uintptr_t dict) {
    return xpl_ptrauth_strip(xpl_kmem_read_uint64(dict, TYPE_OS_DICTIONARY_MEM_DICTIONARY_OFFSET));
}

uintptr_t xpl_os_dictionary_key_at_index(uintptr_t dict, int index) {
    return xpl_os_dictionary_key(xpl_os_dictionary_dict_entry(dict), index);
}

uintptr_t xpl_os_dictionary_value_at_index(uintptr_t dict, int index) {
    return xpl_os_dictionary_value(xpl_os_dictionary_dict_entry(dict), index);
}

void xpl_os_dictionary_set_value_at_idx(uintptr_t dict, int idx, uintptr_t value) {
    uintptr_t dst = xpl_os_dictionary_dict_entry(dict) + (idx * 2 + 1) * sizeof(uintptr_t);
    xpl_kmem_write_uint64(dst, 0, value);
}

int xpl_os_dictionary_set_value_of_key(uintptr_t dict, const char* key, uintptr_t value) {
    int idx;
    int error = xpl_os_dictionary_find_value(dict, key, NULL, &idx);
    if (error) {
        return error;
    }
    xpl_os_dictionary_set_value_at_idx(dict, idx, value);
    return 0;
}

int xpl_os_dictionary_find_value(uintptr_t dict, const char* value, uintptr_t* ptr_out, int* idx_out) {
    uint count = xpl_os_dictionary_count(dict);
    uintptr_t dict_entry = xpl_os_dictionary_dict_entry(dict);
    
    char buffer[PATH_MAX];
    for (int i=0; i<count; i++) {
        uintptr_t key = xpl_os_dictionary_key(dict_entry, i);
        if (!key) {
            xpl_log_warn("null key in dictionary %p", (void*)dict_entry);
            continue;
        }
        
        size_t len = xpl_os_string_read(buffer, key, sizeof(buffer));
        xpl_assert(len <= sizeof(buffer));
        
        if (strncmp(value, buffer, sizeof(buffer)) == 0) {
            if (ptr_out) *ptr_out = xpl_os_dictionary_value(dict_entry, i);
            if (idx_out) *idx_out = i;
            return 0;
        }
    }
    
    return ENOENT;
}

void xpl_os_dictionary_print_keys(uintptr_t dict) {
    uint count = xpl_os_dictionary_count(dict);
    uintptr_t dictEntry = xpl_os_dictionary_dict_entry(dict);
    for (uint i=0; i<count; i++) {
        uintptr_t key = xpl_kmem_read_uint64(dictEntry, (i * 2 * sizeof(uintptr_t)));
        
        char key_str[PATH_MAX];
        size_t key_len = xpl_os_string_read(key_str, key, sizeof(key_str));
        xpl_assert(key_len <= sizeof(key_str));
        
        printf("%s\n", key_str);
    }
}

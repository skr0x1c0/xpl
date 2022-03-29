//
//  io_registry_entry.c
//  xe
//
//  Created by admin on 12/6/21.
//

#include <strings.h>
#include <limits.h>
#include <sys/errno.h>

#include "iokit/io_registry_entry.h"
#include "iokit/os_dictionary.h"
#include "iokit/os_array.h"
#include "iokit/os_string.h"
#include "memory/kmem.h"
#include "slider/kernel.h"
#include "util/ptrauth.h"
#include "util/misc.h"
#include "util/assert.h"

#include <macos/kernel.h>


uintptr_t xpl_io_registry_entry_registry_table(uintptr_t entry) {
    return xpl_kmem_read_uint64(entry, TYPE_IO_REGISTRY_ENTRY_MEM_F_REGISTRY_TABLE_OFFSET);
}

uintptr_t xpl_io_registry_entry_property_table(uintptr_t entry) {
    return xpl_kmem_read_uint64(entry, TYPE_IO_REGISTRY_ENTRY_MEM_F_PROPERTY_TABLE_OFFSET);
}

int xpl_io_registry_filter_dict(uintptr_t dict, const struct xpl_io_registry_filter* filter) {
    uintptr_t value;
    int error = xpl_os_dictionary_find_value(dict, filter->key, &value, NULL);
    if (error) {
        return error;
    }
    
    uintptr_t vtable = xpl_slider_kernel_unslide(xpl_kmem_read_ptr(value, 0)) - 0x10;
    switch (filter->value_type) {
        case VALUE_TYPE_NUMBER: {
            xpl_assert_cond(vtable, ==, VAR_OS_NUMBER_VTABLE);
            uint64_t number = xpl_kmem_read_uint64(value, TYPE_OS_NUMBER_MEM_VALUE_OFFSET);
            if (number == filter->value.number) {
                return 0;
            }
            break;
        }
        case VALUE_TYPE_STRING: {
            if (vtable == VAR_OS_STRING_VTABLE || vtable == VAR_OS_SYMBOL_VTABLE) {
                size_t len = strlen(filter->value.string) + 1;
                char buffer[len];
                if (xpl_os_string_read(buffer, value, sizeof(buffer)) == len && memcmp(filter->value.string, buffer, sizeof(buffer)) == 0) {
                    return 0;
                }
            } else if (vtable == VAR_OS_DATA_VTABLE) {
                size_t len = strlen(filter->value.string) + 1;
                size_t data_len = xpl_kmem_read_uint32(value, TYPE_OS_DATA_MEM_LENGTH_OFFSET);
                if (len != data_len) {
                    break;
                }
                char buffer[len];
                xpl_kmem_read(buffer, xpl_kmem_read_ptr(value, TYPE_OS_DATA_MEM_DATA_OFFSET), 0, sizeof(buffer));
                if (memcmp(buffer, filter->value.string, sizeof(buffer)) == 0) {
                    return 0;
                }
            } else {
                xpl_abort();
            }
            break;
        }
        default:
            xpl_abort();
    }
    return ENOENT;
}


int xpl_io_registry_search(uintptr_t entry, const struct xpl_io_registry_filter* path, size_t num_segments, uintptr_t* found_entry) {
    xpl_assert_cond(num_segments, >=, 1);
    
    uintptr_t registry_table = xpl_io_registry_entry_registry_table(entry);
    uintptr_t child_array;
    int error = xpl_os_dictionary_find_value(registry_table, "IOServiceChildLinks", &child_array, NULL);
    if (error) {
        return error;
    }
    
    const struct xpl_io_registry_filter* filter = &path[0];
    
    uint count = xpl_os_array_count(child_array);
    for (int i=0; i<count; i++) {
        uintptr_t child = xpl_os_array_value_at_index(child_array, i);
        uintptr_t props = xpl_io_registry_entry_property_table(child);
        uintptr_t regs = xpl_io_registry_entry_registry_table(child);
        
        if (xpl_io_registry_filter_dict(props, filter) && xpl_io_registry_filter_dict(regs, filter)) {
            continue;
        }
        
        if (num_segments == 1) {
            *found_entry = child;
            return 0;
        }
        
        int error = xpl_io_registry_search(child, &path[1], num_segments - 1, found_entry);
        if (!error) {
            return 0;
        }
    }
    
    return ENOENT;
}

uintptr_t xpl_io_registry_entry_root(void) {
    return xpl_kmem_read_uint64(xpl_slider_kernel_slide(VAR_G_REGISTRY_ROOT), 0);
}

uintptr_t xpl_io_registry_get_children(uintptr_t entry) {
    uintptr_t registry_table = xpl_io_registry_entry_registry_table(entry);
    uintptr_t child_array;
    int error = xpl_os_dictionary_find_value(registry_table, "IOServiceChildLinks", &child_array, NULL);
    xpl_assert_err(error);
    return child_array;
}

void xpl_io_registry_set_boolean_prop(uintptr_t entry, const char* prop_key, _Bool value) {
    uintptr_t props = xpl_io_registry_entry_property_table(entry);
    if (value) {
        int error = xpl_os_dictionary_set_value_of_key(props, prop_key, xpl_kmem_read_ptr(xpl_kmem_read_ptr(xpl_slider_kernel_slide(VAR_K_OS_BOOLEAN_TRUE), 0), 0));
        xpl_assert_err(error);
    } else {
        int error = xpl_os_dictionary_set_value_of_key(props, prop_key, xpl_kmem_read_ptr(xpl_kmem_read_ptr(xpl_slider_kernel_slide(VAR_K_OS_BOOLEAN_FALSE), 0), 0));
        xpl_assert_err(error);
    }
}

_Bool xpl_io_registry_get_boolean_prop(uintptr_t entry, const char* prop_key) {
    uintptr_t props = xpl_io_registry_entry_property_table(entry);
    uintptr_t value;
    int error = xpl_os_dictionary_find_value(props, prop_key, &value, NULL);
    xpl_assert_err(error);
    return xpl_kmem_read_uint8(value, TYPE_OS_BOOLEAN_MEM_VALUE_OFFSET);
}

size_t xpl_io_registry_get_string_prop(uintptr_t entry, const char* key, char* dst, size_t dst_size) {
    uintptr_t props = xpl_io_registry_entry_property_table(entry);
    uintptr_t value;
    int error = xpl_os_dictionary_find_value(props, key, &value, NULL);
    xpl_assert_err(error);
    return xpl_os_string_read(dst, value, dst_size);
}

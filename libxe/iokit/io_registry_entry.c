//
//  io_registry_entry.c
//  xe
//
//  Created by admin on 12/6/21.
//

#include <strings.h>
#include <limits.h>
#include <sys/errno.h>

#include "io_registry_entry.h"
#include "io_dictionary.h"
#include "io_array.h"
#include "io_string.h"
#include "kmem.h"
#include "slider.h"
#include "platform_params.h"
#include "util_ptrauth.h"


uintptr_t xe_io_registry_entry_registry_table(uintptr_t entry) {
    return xe_kmem_read_uint64(KMEM_OFFSET(entry, TYPE_IO_REGISTRY_ENTRY_MEM_F_REGISTRY_TABLE_OFFSET));
}

uintptr_t xe_io_registry_entry_property_table(uintptr_t entry) {
    return xe_kmem_read_uint64(KMEM_OFFSET(entry, TYPE_IO_REGISTRY_ENTRY_MEM_F_PROPERTY_TABLE_OFFSET));
}

size_t xe_io_registry_entry_name(uintptr_t entry, char* name, size_t name_len) {
    uintptr_t registry_table = xe_io_registry_entry_registry_table(entry);
    uintptr_t prop_name;
    int error = xe_io_os_dictionary_find_value(registry_table, "IOName", &prop_name, NULL);
    if (error) {
        name[0] = '\0';
        return 0;
    }
    return xe_io_os_string_read(name, prop_name, name_len);
}

size_t xe_io_registry_entry_location(uintptr_t entry, char* location, size_t location_length) {
    uintptr_t registry_table = xe_io_registry_entry_registry_table(entry);
    xe_io_os_dictionary_print_keys(registry_table);
    uintptr_t prop;
    int error = xe_io_os_dictionary_find_value(registry_table, "IOLocation", &prop, NULL);
    if (error) {
        location[0] = '\0';
        return 0;
    }
    return xe_io_os_string_read(location, prop, location_length);
}

int xe_io_registry_entry_find_child_by_type(uintptr_t entry, uintptr_t type, uintptr_t* out) {    
    uintptr_t registry_table = xe_io_registry_entry_registry_table(entry);
    uintptr_t child_array;
    int error = xe_io_os_dictionary_find_value(registry_table, "IOServiceChildLinks", &child_array, NULL);
    if (error) {
        return error;
    }
    
    uint count = xe_io_os_array_count(child_array);
    for (int i=0; i<count; i++) {
        uintptr_t child = xe_io_os_array_value_at_index(child_array, i);
        uintptr_t class = XE_PTRAUTH_STRIP(xe_kmem_read_uint64(child));
        if (class == type) {
            *out = child;
            return 0;
        }
    }
    
    return ENOENT;
}

int xe_io_registry_entry_find_child_by_name(uintptr_t entry, char* name, uintptr_t* out) {
    uintptr_t registry_table = xe_io_registry_entry_registry_table(entry);
    uintptr_t child_array;
    int error = xe_io_os_dictionary_find_value(registry_table, "IOServiceChildLinks", &child_array, NULL);
    if (error) {
        return error;
    }
    
    uint count = xe_io_os_array_count(child_array);
    for (int i=0; i<count; i++) {
        uintptr_t child = xe_io_os_array_value_at_index(child_array, i);
        char buffer[PATH_MAX];
        size_t name_len = xe_io_registry_entry_name(child, buffer, sizeof(buffer));
        if (!name_len) {
            continue;
        }
        if (strncmp(buffer, name, sizeof(buffer)) == 0) {
            *out = child;
            return 0;
        }
    }
    
    return ENOENT;
}

uintptr_t xe_io_registry_entry_root(void) {
    return xe_kmem_read_uint64(xe_slider_slide(VAR_G_IO_REGISTRY_ROOT));
}

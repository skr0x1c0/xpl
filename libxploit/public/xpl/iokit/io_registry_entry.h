//
//  io_registry_entry.h
//  xe
//
//  Created by admin on 12/6/21.
//

#ifndef xpl_io_registry_entry_h
#define xpl_io_registry_entry_h

#include <stdio.h>

struct xpl_io_registry_filter {
    const char* key;
    enum {
        VALUE_TYPE_NUMBER,
        VALUE_TYPE_STRING,
    } value_type;
    union {
        uint64_t number;
        const char* string;
    } value;
};

uintptr_t xpl_io_registry_entry_registry_table(uintptr_t entry);
uintptr_t xpl_io_registry_entry_property_table(uintptr_t entry);
size_t xpl_io_registry_entry_name(uintptr_t entry, char* name, size_t name_len);
int xpl_io_registry_search(uintptr_t entry, const struct xpl_io_registry_filter* path, size_t num_segments, uintptr_t* found_entry);
uintptr_t xpl_io_registry_entry_root(void);
uintptr_t xpl_io_registry_get_children(uintptr_t entry);

void xpl_io_registry_set_boolean_prop(uintptr_t entry, const char* prop_key, _Bool value);
_Bool xpl_io_registry_get_boolean_prop(uintptr_t entry, const char* prop_key);
size_t xpl_io_registry_get_string_prop(uintptr_t entry, const char* key, char* dst, size_t dst_size);

#endif /* xpl_io_registry_entry_h */

//
//  io_registry_entry.h
//  xe
//
//  Created by admin on 12/6/21.
//

#ifndef xe_io_registry_entry_h
#define xe_io_registry_entry_h

#include <stdio.h>

uintptr_t xe_io_registry_entry_registry_table(uintptr_t entry);
uintptr_t xe_io_registry_entry_property_table(uintptr_t entry);
size_t xe_io_registry_entry_name(uintptr_t entry, char* name, size_t name_len);
int xe_io_registry_entry_find_child_by_type(uintptr_t entry, uintptr_t type, uintptr_t* out);
int xe_io_registry_entry_find_child_by_name(uintptr_t entry, char* name, uintptr_t* out);
uintptr_t xe_io_registry_entry_root(void);

#endif /* xe_io_registry_entry_h */

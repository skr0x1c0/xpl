//
//  io_dictionary.h
//  xe
//
//  Created by admin on 12/6/21.
//

#ifndef io_dictionary_h
#define io_dictionary_h

#include <stdio.h>

uint xe_io_os_dictionary_count(uintptr_t dict);
uintptr_t xe_io_os_dictionary_key_at_index(uintptr_t dict, int index);
uintptr_t xe_io_os_dictionary_value_at_index(uintptr_t dict, int index);

int xe_io_os_dictionary_find_value(uintptr_t dict, const char* value, uintptr_t* out);
void xe_io_os_dictionary_print_keys(uintptr_t dict);

#endif /* io_dictionary_h */

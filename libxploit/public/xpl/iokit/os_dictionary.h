
#ifndef xpl_io_dictionary_h
#define xpl_io_dictionary_h

#include <stdio.h>

uint xpl_os_dictionary_count(uintptr_t dict);
uintptr_t xpl_os_dictionary_key_at_index(uintptr_t dict, int index);

uintptr_t xpl_os_dictionary_value_at_index(uintptr_t dict, int index);
int xpl_os_dictionary_set_value_of_key(uintptr_t dict, const char* key, uintptr_t value);
void xpl_os_dictionary_set_value_at_idx(uintptr_t dict, int idx, uintptr_t value);

int xpl_os_dictionary_find_value(uintptr_t dict, const char* value, uintptr_t* out, int* idx_out);
void xpl_os_dictionary_print_keys(uintptr_t dict);

#endif /* xpl_io_dictionary_h */

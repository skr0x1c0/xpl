//
//  io_string.h
//  xe
//
//  Created by admin on 12/6/21.
//

#ifndef xpl_io_string_h
#define xpl_io_string_h

#include <stdio.h>

size_t xpl_os_string_length(uintptr_t string);
size_t xpl_os_string_read(char* dst, uintptr_t string, size_t dst_size);

#endif /* xpl_io_string_h */

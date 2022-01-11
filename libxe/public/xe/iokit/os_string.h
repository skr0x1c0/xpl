//
//  io_string.h
//  xe
//
//  Created by admin on 12/6/21.
//

#ifndef xe_io_string_h
#define xe_io_string_h

#include <stdio.h>

size_t xe_os_string_length(uintptr_t string);
size_t xe_os_string_read(char* dst, uintptr_t string, size_t dst_size);

#endif /* xe_io_string_h */

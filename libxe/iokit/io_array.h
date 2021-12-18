//
//  io_array.h
//  xe
//
//  Created by admin on 12/6/21.
//

#ifndef io_array_h
#define io_array_h

#include <stdio.h>

uint xe_io_os_array_count(uintptr_t array);
uintptr_t xe_io_os_array_value_at_index(uintptr_t array, int index);

#endif /* io_array_h */

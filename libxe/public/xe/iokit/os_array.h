//
//  io_array.h
//  xe
//
//  Created by admin on 12/6/21.
//

#ifndef xe_io_array_h
#define xe_io_array_h

#include <stdio.h>

uint xe_os_array_count(uintptr_t array);
uintptr_t xe_os_array_value_at_index(uintptr_t array, int index);

#endif /* xe_io_array_h */
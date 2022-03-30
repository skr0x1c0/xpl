//
//  util_binary.h
//  xe
//
//  Created by admin on 11/24/21.
//

#ifndef xpl_binary_h
#define xpl_binary_h

#include <stdio.h>

/// Print the hexdump of source data
/// @param data source data
/// @param data_size size of source data
int xpl_binary_hex_dump(const void* data, size_t data_size);

#endif /* xpl_binary_h */

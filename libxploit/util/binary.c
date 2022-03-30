//
//  util_binary.c
//  xe
//
//  Created by admin on 11/24/21.
//

#include "util/binary.h"


int xpl_binary_hex_dump(const void* data, size_t data_size) {
    for (size_t i=0; i<data_size; i++) {
        printf("%.2x ", (uint8_t)((char*)data)[i]);
        if ((i+1) % 8 == 0) {
            printf("\n");
        }
    }
    if (data_size % 8 != 0) {
        printf("\n");
    }
    return 0;
}


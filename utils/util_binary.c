//
//  util_binary.c
//  xe
//
//  Created by admin on 11/24/21.
//

#include "util_binary.h"


int xe_util_binary_hex_dump(const char* data, size_t data_size) {
    for (size_t i=0; i<data_size; i++) {
        printf("%.2x ", (uint8_t)data[i]);
        if ((i+1) % 8 == 0) {
            printf("\n");
        }
    }
    return 0;
}

void xe_util_binary_c_dump(const char* data, size_t data_size) {
    printf("{\n    ");
    for (int i=0; i<data_size; i++) {
        if ((i + 1) % 8 == 0) {
            printf("\n    ");
        }
        printf("0x%.2x, ", (uint8_t)data[i]);
    }
    printf("\n}");
}


//
//  slider_kas.c
//  xe
//
//  Created by admin on 11/25/21.
//

#include <stdlib.h>
#include <xib/kas.h>

#include "slider.h"

static struct xib_kas_address_slider xe_slider_kas;


void xe_slider_init(void) {
    int error = xib_kas_slider_init(&xe_slider_kas);
    if (error) {
        printf("[ERROR] xib kas slider init failed, err: %d\n", error);
        abort();
    }
}

uintptr_t xe_slider_slide(uintptr_t ptr) {
    uintptr_t out;
    int error = xib_kas_slider_i2k(&xe_slider_kas, ptr, &out);
    if (error) {
        printf("[ERROR] failed to slide address, err: %d\n", error);
        abort();
    }
    return out;
}


uintptr_t xe_slider_unslide(uintptr_t ptr) {
    uintptr_t out;
    int error = xib_kas_slider_k2i(&xe_slider_kas, ptr, &out);
    if (error) {
        printf("[ERROR] failed to slide address, err: %d\n", error);
        abort();
    }
    return out;
}

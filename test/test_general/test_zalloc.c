//
//  test_zalloc.c
//  test
//
//  Created by admin on 2/17/22.
//

#include <xe/util/zalloc.h>
#include <xe/util/kalloc_heap.h>
#include <xe/slider/kernel.h>
#include <macos_params.h>

#include "test_zalloc.h"


void test_zalloc(void) {
    xe_util_zalloc_t util = xe_util_zalloc_create(xe_util_kh_find_zone_for_size(xe_slider_kernel_slide(VAR_KHEAP_DEFAULT_ADDR), 1024), 1);
    uintptr_t ptr = xe_util_zalloc_alloc(util);
    printf("allocated memory at %p\n", (void*)ptr);
}

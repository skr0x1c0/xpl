//
//  test_zalloc.c
//  test
//
//  Created by admin on 2/17/22.
//

#include <xe/memory/kmem.h>
#include <xe/slider/kernel.h>
#include <xe/util/zalloc.h>
#include <xe/util/kalloc_heap.h>
#include <xe/util/misc.h>
#include <xe/util/log.h>
#include <xe/util/assert.h>
#include <xe/allocator/small_mem.h>

#include <macos_params.h>

#include "test_zalloc.h"


void test_zalloc(void) {
    uintptr_t zones[] = {
        VAR_ZONE_IO_EVENT_SOURCE,
        VAR_ZONE_IO_EVENT_SOURCE_COUNTER,
        VAR_ZONE_IO_EVENT_SOURCE_RESERVED
    };


    for (int i=0; i<xe_array_size(zones); i++) {
        uintptr_t zone = xe_kmem_read_uint64(xe_slider_kernel_slide(zones[i]), 0);

        xe_log_info("testing zalloc for zone %p", (void*)zone);
        xe_util_zalloc_t util = xe_util_zalloc_create(zone, 1);
        uintptr_t ptr = xe_util_zalloc_alloc(util, 0);
        xe_log_debug("allocated %p from zone %p", (void*)ptr, (void*)zone);
        xe_log_info("zalloc test for zone %p is ok", (void*)zone);
    }
    
    uint16_t kheap_zones[] = {
        16, 1024, 16384
    };
    
    for (int i=0; i<xe_array_size(kheap_zones); i++) {
        uintptr_t zone = xe_util_kh_find_zone_for_size(xe_slider_kernel_slide(VAR_KHEAP_DEFAULT_ADDR), kheap_zones[i]);
        
        xe_log_info("testing zalloc for zone %p", (void*)zone);
        xe_util_zalloc_t util = xe_util_zalloc_create(zone, 1);
        uintptr_t ptr = xe_util_zalloc_alloc(util, 0);
        xe_log_debug("allocated %p from zone %p", (void*)ptr, (void*)zone);
        xe_log_info("zalloc test for zone %p is ok", (void*)zone);
    }
}

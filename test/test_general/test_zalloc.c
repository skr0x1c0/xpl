//
//  test_zalloc.c
//  test
//
//  Created by admin on 2/17/22.
//

#include <xpl/memory/kmem.h>
#include <xpl/slider/kernel.h>
#include <xpl/util/zalloc.h>
#include <xpl/util/kalloc_heap.h>
#include <xpl/util/misc.h>
#include <xpl/util/log.h>
#include <xpl/util/assert.h>
#include <xpl/allocator/small_mem.h>

#include <macos/kernel.h>

#include "test_zalloc.h"


void test_zalloc(void) {
    uintptr_t kalloc_type_views[] = {
        VAR_IO_EVENT_SOURCE_KTV,
        VAR_IO_EVENT_SOURCE_COUNTER_KTV,
        VAR_IO_EVENT_SOURCE_RESERVED_KTV,
    };


    for (int i=0; i<xpl_array_size(kalloc_type_views); i++) {
        uintptr_t zone = xpl_kmem_read_uint64(xpl_slider_kernel_slide(kalloc_type_views[i]), TYPE_KALLOC_TYPE_VIEW_MEM_KT_ZV_OFFSET);

        xpl_log_info("testing zalloc for zone %p", (void*)zone);
        xpl_zalloc_t util = xpl_zalloc_create(zone, 1);
        uintptr_t ptr = xpl_zalloc_alloc(util, 0);
        xpl_log_debug("allocated %p from zone %p", (void*)ptr, (void*)zone);
        xpl_log_info("zalloc test for zone %p is ok", (void*)zone);
    }
    
    uint16_t kheap_zones[] = {
        16, 1024, 16384
    };
    
    for (int i=0; i<xpl_array_size(kheap_zones); i++) {
        uintptr_t zone = xpl_kheap_find_zone_for_size(xpl_slider_kernel_slide(VAR_KHEAP_DEFAULT_ADDR), kheap_zones[i]);
        
        xpl_log_info("testing zalloc for zone %p", (void*)zone);
        xpl_zalloc_t util = xpl_zalloc_create(zone, 1);
        uintptr_t ptr = xpl_zalloc_alloc(util, 0);
        xpl_log_debug("allocated %p from zone %p", (void*)ptr, (void*)zone);
        xpl_log_info("zalloc test for zone %p is ok", (void*)zone);
    }
}

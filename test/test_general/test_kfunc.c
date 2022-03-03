//
//  test_kfunc.c
//  test
//
//  Created by sreejith on 2/20/22.
//

#include "test_kfunc.h"

#include <xe/memory/kmem.h>
#include <xe/slider/kernel.h>
#include <xe/util/kfunc.h>
#include <xe/util/assert.h>
#include <xe/allocator/small_mem.h>

#include <macos_params.h>


void test_kfunc(void) {
    // This message should appear from kernel process in Console.app
    char message[] = "XE: xe_util_kfunc test run";
    
    uintptr_t address;
    xe_allocator_small_mem_t allocator = xe_allocator_small_mem_allocate(sizeof(message), &address);
    
    xe_kmem_write(address, 0, message, sizeof(message));
    
    xe_util_kfunc_t util = xe_util_kfunc_create(VAR_ZONE_ARRAY_LEN - 1);
    uint64_t args[8];
    bzero(args, sizeof(args));
    args[0] = address;
    
    for (int i=0; i<5; i++) {
        xe_util_kfunc_exec(util, xe_slider_kernel_slide(FUNC_OS_REPORT_WITH_BACKTRACE), args);
    }
    
    xe_allocator_small_mem_destroy(&allocator);
    xe_util_kfunc_destroy(&util);
}

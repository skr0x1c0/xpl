//
//  test_kfunc.c
//  test
//
//  Created by sreejith on 2/20/22.
//

#include <time.h>

#include <xpl/memory/kmem.h>
#include <xpl/slider/kernel.h>
#include <xpl/util/kfunc.h>
#include <xpl/util/assert.h>
#include <xpl/allocator/small_mem.h>

#include <macos/kernel.h>

#include "test_kfunc.h"


void test_kfunc(void) {
    // This message should appear from kernel process in Console.app
    char message[] = "XE: xpl_util_kfunc test run";
    
    uintptr_t address;
    xpl_allocator_small_mem_t allocator = xpl_allocator_small_mem_allocate(sizeof(message), &address);
    
    xpl_kmem_write(address, 0, message, sizeof(message));
    
    xpl_util_kfunc_t util = xpl_util_kfunc_create(VAR_ZONE_ARRAY_LEN - 1);
    
    uint64_t args[8];
    bzero(args, sizeof(args));
    args[0] = address;
    
    for (int i=0; i<5; i++) {
        xpl_util_kfunc_execute_simple(util, xpl_slider_kernel_slide(FUNC_OS_REPORT_WITH_BACKTRACE), args);
    }
    
    int num_samples = 100;
    uint64_t elapsed = 0;
    uintptr_t function = xpl_slider_kernel_slide(FUNC_VN_DEFAULT_ERROR);
    bzero(args, sizeof(args));

    xpl_log_info("kfunc test begin, num samples: %d", num_samples);
    
    for (int i=0; i<num_samples; i++) {
        uint64_t start = clock_gettime_nsec_np(CLOCK_MONOTONIC);
        xpl_util_kfunc_execute_simple(util, function, args);
        elapsed += clock_gettime_nsec_np(CLOCK_MONOTONIC) - start;
    }
    
    double samples_per_second = (double)num_samples / ((double)elapsed / 1e9);
    xpl_log_info("kfunc test OK, num samples: %d, total time: %llu ns, samples / second: %.2f", num_samples, elapsed, samples_per_second);
    
    xpl_allocator_small_mem_destroy(&allocator);
    xpl_util_kfunc_destroy(&util);
}

//
//  test_pacda.c
//  test_remote
//
//  Created by admin on 1/11/22.
//

#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <xe/xnu/proc.h>
#include <xe/util/pacda.h>
#include <xe/util/log.h>
#include <xe/util/assert.h>
#include <xe/allocator/large_mem.h>

#include "test_pacda.h"


void test_pacda(void) {
    uintptr_t proc = xe_xnu_proc_current_proc();
    xe_log_info("pid: %d", getpid());
    xe_log_info("proc: %p", (void*)proc);
    
    uint64_t elapsed = 0;
    int num_samples = 100;
    
    xe_log_info("PACDA test begin, num samples: %d", num_samples);
    
    for (int i=0; i<num_samples; i++) {
        uintptr_t addr = random();
        uintptr_t ctx = random();
    
        uint64_t start = clock_gettime_nsec_np(CLOCK_MONOTONIC);
        uintptr_t signed_ptr = xe_util_pacda_sign(addr, ctx);
        elapsed += clock_gettime_nsec_np(CLOCK_MONOTONIC) - start;
        xe_log_debug("signed pointer %p with context %p using PACDA, res: %p", (void*)addr, (void*)ctx, (void*)signed_ptr);        
    }
    
    double samples_per_second = (double)num_samples / ((double)elapsed / 1e9);
    
    xe_log_info("PACDA signing OK, num samples: %d, total time: %llu ns, samples / second: %.2f", num_samples, elapsed, samples_per_second);
}

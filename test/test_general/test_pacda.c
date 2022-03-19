//
//  test_pacda.c
//  test_remote
//
//  Created by admin on 1/11/22.
//

#include <stdlib.h>
#include <unistd.h>

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
    
    for (int i=0; i<3; i++) {
        uintptr_t ctx = 0xabcdef;
        
        uintptr_t addr;
        xe_allocator_large_mem_t allocator = xe_allocator_large_mem_allocate(16384 * 4, &addr);
        
        uintptr_t signed_ptr = xe_util_pacda_sign(addr, ctx);
        
        xe_log_info("signed ptr: %p", (void*)signed_ptr);
        xe_allocator_large_mem_free(&allocator);
    }
}

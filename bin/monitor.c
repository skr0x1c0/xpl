//
//  main.c
//  monitor
//
//  Created by admin on 11/25/21.
//

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <gym_client.h>

#include "kmem.h"
#include "slider.h"
#include "xnu_proc.h"
#include "util_binary.h"
#include "platform_types.h"
#include "platform_variables.h"

int dump_stack(uintptr_t thread) {
    uintptr_t kernel_stack = xe_kmem_read_uint64(KMEM_OFFSET(thread, TYPE_THREAD_MEM_KERNEL_STACK_OFFSET));
    
    uint stack_size = 0;
    size_t res_size = sizeof(stack_size);
    if (sysctlbyname("kern.stack_size", &stack_size, &res_size, NULL, 0)) {
        return errno;
    };
    
    char* stack = malloc(stack_size);
    xe_kmem_read(stack, kernel_stack, stack_size);
    
    xe_util_binary_hex_dump(stack, stack_size);
    free(stack);
    return 0;
}

int find_target_thread(uintptr_t proc, uintptr_t* ptr_out) {
    uintptr_t cursor = xe_kmem_read_uint64(KMEM_OFFSET(proc, TYPE_PROC_MEM_P_UTHLIST_OFFSET));
    while (cursor != 0) {
        printf("check cursor %p\n", (void*)cursor);
        uintptr_t thread = xe_kmem_read_uint64(KMEM_OFFSET(cursor, TYPE_UTHREAD_MEM_UU_THREAD_OFFSET));
        int state = xe_kmem_read_uint32(KMEM_OFFSET(thread, TYPE_THREAD_MEM_STATE_OFFSET));
        if (state == (0x01 /* TH_WAIT */ | 0x08 /* TH_UNINT */)) {
            *ptr_out = cursor;
            return 0;
        }
        cursor = xe_kmem_read_uint64(KMEM_OFFSET(cursor, TYPE_UTHREAD_MEM_UU_LIST_OFFSET));
    }
    return ENOENT;
}


int main(int argc, const char * argv[]) {
    if (argc != 2) {
        printf("[ERROR] invalid usage\n");
        abort();
    }
    
    char* endp;
    long pid = strtol(argv[1], &endp, 0);
    if (*endp != '\0' || pid > INT_MAX) {
        printf("[ERROR] invalid pid provided\n");
        abort();
    }
    
    xe_kmem_init();
    xe_slider_init();
    
    uintptr_t kernproc = xe_kmem_read_uint64(xe_slider_slide(VAR_KERNPROC_ADDR));
    uintptr_t proc;
    int error = xe_xnu_proc_find(kernproc, (pid_t)pid, &proc);
    if (error) {
        printf("[ERROR] failed to find proc for pid %ld\n", pid);
        abort();
    }
    
    getpass("press enter to start monitoring process\n");
    while (1) {
        uintptr_t thread;
        int error = find_target_thread(proc, &thread);
        if (error == ENOENT) {
            continue;
        }
        printf("dumping thread 0x%p stack\n", (void*)thread);
        dump_stack(thread);
        break;
    }
    
    return 0;
}

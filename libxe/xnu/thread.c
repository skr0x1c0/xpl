//
//  thread.c
//  libxe
//
//  Created by sreejith on 2/25/22.
//

#include <pthread.h>

#include "xnu/thread.h"
#include "xnu/proc.h"
#include "memory/kmem.h"
#include "util/ptrauth.h"
#include "util/assert.h"
#include "util/log.h"

#include "macos_params.h"


uintptr_t xe_xnu_thread_current_thread(void) {
    pthread_t pthread = pthread_self();
    uint64_t tid;
    int error = pthread_threadid_np(pthread, &tid);
    xe_assert_err(error);
    
    uintptr_t proc = xe_xnu_proc_current_proc();
    uintptr_t task = xe_ptrauth_strip(xe_kmem_read_uint64(proc, TYPE_PROC_MEM_TASK_OFFSET));
    uintptr_t thread = xe_kmem_read_uint64(task, TYPE_TASK_MEM_THREADS_OFFSET);
    while (thread != 0 && thread != task + TYPE_TASK_MEM_THREADS_OFFSET) {
        uint64_t thread_id = xe_kmem_read_uint64(thread, TYPE_THREAD_MEM_THREAD_ID_OFFSET);
        if (thread_id == tid) {
            return thread;
        }
        thread = xe_kmem_read_uint64(thread, TYPE_THREAD_MEM_TASK_THREADS_OFFSET);
    }
    
    xe_log_error("cannot find current thread with tid: %p", (void*)tid);
    abort();
}


int xe_xnu_thread_scan_stack(uintptr_t thread, uintptr_t value, uintptr_t mask, int scan_count, uintptr_t* found_address) {
    uintptr_t machine = thread + TYPE_THREAD_MEM_MACHINE_OFFSET;
    uintptr_t kstackptr = xe_kmem_read_ptr(machine, TYPE_MACHINE_THREAD_MEM_KSTACKPTR_OFFSET);
    uintptr_t kernel_stack = xe_kmem_read_ptr(thread, TYPE_THREAD_MEM_KERNEL_STACK_OFFSET);
    
    if (!kernel_stack || !kstackptr) {
        return EBADF;
    }
    
    int64_t max_size = kstackptr - kernel_stack;
    xe_assert_cond(max_size, >=, 0);
    xe_assert_cond(max_size, <=, KERNEL_STACK_SIZE);
    int scan_size = scan_count * sizeof(uintptr_t);
    if (scan_size > max_size) {
        scan_size = (int)max_size;
    }
    scan_count = scan_size / sizeof(uintptr_t);
    
    uintptr_t scan_values[scan_count];
    uintptr_t start_address = (kstackptr - scan_size) & ~7;
    xe_kmem_read(scan_values, start_address, 0, sizeof(scan_values));
    
    for (int i=0; i < scan_count; i++) {
        if ((scan_values[i] | mask) == value) {
            *found_address = start_address + i * sizeof(uintptr_t);
            return 0;
        }
    }
    
    return ENOENT;
}

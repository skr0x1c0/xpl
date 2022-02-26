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

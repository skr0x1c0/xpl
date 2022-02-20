//
//  test_large_mem.c
//  test
//
//  Created by admin on 2/15/22.
//

#include <xe/allocator/large_mem.h>
#include <xe/util/assert.h>
#include <xe/util/log.h>

#include "test_large_mem.h"


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


void test_large_mem(void) {
    uintptr_t addr;
    
    size_t size = (1 << 30);
    xe_allocator_large_mem_t allocator = xe_allocator_large_mem_allocate(size, &addr);
    xe_assert(addr != 0);
    xe_log_info("large mem allocated address: %p", (void*)addr);
    
    uintptr_t flags_disabled = (BLOCK_IS_GLOBAL);
    uintptr_t mask = (BLOCK_REFCOUNT_MASK | BLOCK_DEALLOCATING);
    uintptr_t flags_enabled = (BLOCK_SMALL_DESCRIPTOR | BLOCK_NEEDS_FREE | BLOCK_HAS_COPY_DISPOSE | 0x2);
    
    uintptr_t new_addr = addr & ~flags_disabled;
    new_addr &= ~mask;
    new_addr |= flags_enabled;
    
    if (new_addr < addr) {
        new_addr += (1 << 29);
    }
    
    xe_assert_cond(new_addr, >=, addr);
    xe_assert_cond(new_addr, <, addr + size);
    xe_assert_cond(new_addr + 900, <, addr + size);
    xe_assert_cond(new_addr & flags_enabled, ==, flags_enabled);
    xe_assert_cond(new_addr & flags_disabled, ==, 0);
    
    xe_log_info("new addr: %p", (void*)new_addr);
    
    xe_allocator_large_mem_free(&allocator);
}

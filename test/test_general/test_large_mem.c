
#include <xpl/allocator/large_mem.h>
#include <xpl/util/assert.h>
#include <xpl/util/log.h>

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
    xpl_allocator_large_mem_t allocator = xpl_allocator_large_mem_allocate(size, &addr);
    xpl_assert(addr != 0);
    xpl_log_info("large mem allocated address: %p", (void*)addr);
    
    xpl_allocator_large_mem_free(&allocator);
}

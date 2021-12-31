//
//  platform_constants.h
//  xe
//
//  Created by admin on 12/5/21.
//

#ifndef platform_constants_h
#define platform_constants_h

#define XE_PAGE_SIZE 16384
#define XE_PAGE_SHIFT 14

#define xe_ptoa(x) ((uint64_t)(x) << XE_PAGE_SHIFT)
#define xe_atop(x) ((uint64_t)(x) >> XE_PAGE_SHIFT)

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

#define XE_TiB(x) ((0ULL + (x)) << 40)
#define XE_GiB(x) ((0ULL + (x)) << 30)
#define XE_VM_MIN_KERNEL_ADDRESS   (0ULL - XE_TiB(2))
#define XE_VM_MAX_KERNEL_ADDRESS   (XE_VM_MIN_KERNEL_ADDRESS + XE_GiB(64) + XE_GiB(512) - 1)

#define XE_VM_KERNEL_ADDRESS_VALID(addr) (addr >= XE_VM_MIN_KERNEL_ADDRESS && addr <= XE_VM_MAX_KERNEL_ADDRESS)

#endif /* platform_constants_h */

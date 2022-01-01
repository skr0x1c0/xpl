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


#define XE_IMAGE_SEGMENT_TEXT_BASE 0xfffffe0007004000
#define XE_IMAGE_SEGMENT_TEXT_SIZE 0xe8000
#define XE_IMAGE_SEGMENT_DATA_CONST_BASE 0xfffffe00070ec000
#define XE_IMAGE_SEGMENT_DATA_CONST_SIZE 0x158000
#define XE_IMAGE_SEGMENT_TEXT_EXEC_BASE 0xfffffe0007244000
#define XE_IMAGE_SEGMENT_TEXT_EXEC_SIZE 0x85c000
#define XE_IMAGE_SEGMENT_KLD_BASE 0xfffffe0007aa0000
#define XE_IMAGE_SEGMENT_KLD_SIZE 0x8000
#define XE_IMAGE_SEGMENT_PPL_TEXT_BASE 0xfffffe0007aa8000
#define XE_IMAGE_SEGMENT_PPL_TEXT_SIZE 0x2c000
#define XE_IMAGE_SEGMENT_PPL_DATA_CONST_BASE 0xfffffe0007ad4000
#define XE_IMAGE_SEGMENT_PPL_DATA_CONST_SIZE 0x4000
#define XE_IMAGE_SEGMENT_LAST_DATA_CONST_BASE 0xfffffe0007ad8000
#define XE_IMAGE_SEGMENT_LAST_DATA_CONST_SIZE 0x4000
#define XE_IMAGE_SEGMENT_LAST_BASE 0xfffffe0007adc000
#define XE_IMAGE_SEGMENT_LAST_SIZE 0x4000
#define XE_IMAGE_SEGMENT_PPL_DATA_BASE 0xfffffe0007ae0000
#define XE_IMAGE_SEGMENT_PPL_DATA_SIZE 0x8000
#define XE_IMAGE_SEGMENT_KLD_DATA_BASE 0xfffffe0007ae8000
#define XE_IMAGE_SEGMENT_KLD_DATA_SIZE 0x8000
#define XE_IMAGE_SEGMENT_DATA_BASE 0xfffffe0007af0000
#define XE_IMAGE_SEGMENT_DATA_SIZE 0xf0000
#define XE_IMAGE_SEGMENT_HIB_DATA_BASE 0xfffffe0007be0000
#define XE_IMAGE_SEGMENT_HIB_DATA_SIZE 0x4000
#define XE_IMAGE_SEGMENT_BOOT_DATA_BASE 0xfffffe0007be4000
#define XE_IMAGE_SEGMENT_BOOT_DATA_SIZE 0x44000


#endif /* platform_constants_h */

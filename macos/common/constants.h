//
//  constants.h
//  
//
//  Created by admin on 1/4/22.
//

#ifndef macos_constants_h
#define macos_constants_h

#define XE_PAGE_SIZE 16384
#define XE_PAGE_SHIFT 14

#define xe_ptoa(x) ((uint64_t)(x) << XE_PAGE_SHIFT)
#define xe_atop(x) ((uint64_t)(x) >> XE_PAGE_SHIFT)

#define XE_TiB(x) ((0ULL + (x)) << 40)
#define XE_GiB(x) ((0ULL + (x)) << 30)
#define XE_VM_MIN_KERNEL_ADDRESS   (0ULL - XE_TiB(2))
#define XE_VM_MAX_KERNEL_ADDRESS   (XE_VM_MIN_KERNEL_ADDRESS + XE_GiB(64) + XE_GiB(512) - 1)

#define XE_VM_KERNEL_ADDRESS_VALID(addr) (addr >= XE_VM_MIN_KERNEL_ADDRESS && addr <= XE_VM_MAX_KERNEL_ADDRESS)

#endif /* macos_constants_h */

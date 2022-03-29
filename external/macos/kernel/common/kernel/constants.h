//
//  constants.h
//  
//
//  Created by admin on 1/4/22.
//

#ifndef macos_constants_h
#define macos_constants_h

#define xpl_PAGE_SIZE 16384
#define xpl_PAGE_SHIFT 14

#define xpl_ptoa(x) ((uint64_t)(x) << xpl_PAGE_SHIFT)
#define xpl_atop(x) ((uint64_t)(x) >> xpl_PAGE_SHIFT)

#define xpl_TiB(x) ((0ULL + (x)) << 40)
#define xpl_GiB(x) ((0ULL + (x)) << 30)
#define xpl_VM_MIN_KERNEL_ADDRESS   (0ULL - xpl_TiB(2))
#define xpl_VM_MAX_KERNEL_ADDRESS   (xpl_VM_MIN_KERNEL_ADDRESS + xpl_GiB(64) + xpl_GiB(512) - 1)

#define xpl_vm_kernel_address_valid(addr) (addr >= xpl_VM_MIN_KERNEL_ADDRESS && addr <= xpl_VM_MAX_KERNEL_ADDRESS)

#define PTOV_TABLE_SIZE 8
#define KERNEL_STACK_SIZE 16384

#endif /* macos_constants_h */

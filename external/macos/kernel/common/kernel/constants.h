
#ifndef macos_common_constants_h
#define macos_common_constants_h

#define XPL_PAGE_SIZE 16384
#define XPL_PAGE_SHIFT 14

#define xpl_ptoa(x) ((uint64_t)(x) << XPL_PAGE_SHIFT)
#define xpl_atop(x) ((uint64_t)(x) >> XPL_PAGE_SHIFT)

#define xpl_tib(x) ((0ULL + (x)) << 40)
#define xpl_gib(x) ((0ULL + (x)) << 30)
#define XPL_VM_MIN_KERNEL_ADDRESS   (0ULL - xpl_tib(2))
#define XPL_VM_MAX_KERNEL_ADDRESS   (XPL_VM_MIN_KERNEL_ADDRESS + xpl_gib(64) + xpl_gib(512) - 1)

#define xpl_vm_kernel_address_valid(addr) (addr >= XPL_VM_MIN_KERNEL_ADDRESS && addr <= XPL_VM_MAX_KERNEL_ADDRESS)

#define PTOV_TABLE_SIZE 8
#define KERNEL_STACK_SIZE 16384

#endif /* macos_common_constants_h */

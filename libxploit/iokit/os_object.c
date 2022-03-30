//
//  os_object.c
//  libxe
//
//  Created by sreejith on 3/10/22.
//

#include "iokit/os_object.h"
#include "memory/kmem.h"
#include "util/assert.h"

#include <macos/kernel.h>


/// Decodes the instructions in `getMetaClass` of the object and find its return value
uintptr_t xpl_os_object_get_meta_class(uintptr_t instance) {
    uintptr_t vtable = xpl_kmem_read_ptr(instance, 0) - 0x10;
    uintptr_t get_meta_class_method = xpl_kmem_read_ptr(vtable, TYPE_VTABLE_MEM_GET_META_CLASS_OFFSET);
    
    /// Expecting instructions of the form
    /// adrp x0, #imm
    /// add x0, x0, #imm
    /// ret
    uint32_t adrp_instr = xpl_kmem_read_uint32(get_meta_class_method, 0);
    uint32_t add_instr = xpl_kmem_read_uint32(get_meta_class_method, 4);
    uint32_t ret_instr = xpl_kmem_read_uint32(get_meta_class_method, 8);
    
    xpl_assert_cond(ret_instr, ==, 0xd65f03c0); // Make sure it is ret instruction
    
    xpl_assert_cond(adrp_instr & 0x9F000000, ==, 0x90000000); // Make sure it is adrp instruction
    xpl_assert_cond(adrp_instr & 0x1F, ==, 0x0); // Make sure Rd = x0
    uint32_t adrp_imm_lo = (adrp_instr & 0x60000000) >> 29;
    uint32_t adrp_imm_hi = (adrp_instr & 0xFFFFE0) >> 5;
    uint64_t base = (adrp_imm_lo << 12) | ((uint64_t)adrp_imm_hi << 14);
    if (base & 0x100000000) {
        base |= 0xFFFFFFFE00000000;
    }
    base += get_meta_class_method & ~4095;
    
    xpl_assert_cond(add_instr & 0xFFC00000, ==, 0x91000000); // 64-bit ADD (immediate) instr with no shift
    uint32_t offset = (add_instr & 0x3FFC00) >> 10;
    return base + offset;
}


_Bool xpl_os_object_is_instance_of(uintptr_t instance, uintptr_t meta_class) {
    uintptr_t g_meta_class = xpl_os_object_get_meta_class(instance);
    xpl_assert_kaddr(g_meta_class);
    while (g_meta_class != 0) {
        if (g_meta_class == meta_class) {
            return 1;
        } else {
            g_meta_class = xpl_kmem_read_ptr(g_meta_class, TYPE_OS_META_CLASS_MEM_SUPER_CLASS_LINK_OFFSET);
        }
    }
    return 0;
}

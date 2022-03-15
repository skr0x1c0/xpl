//
//  pmap.c
//  libxe
//
//  Created by sreejith on 3/8/22.
//

#include <string.h>
#include <mach/arm/vm_types.h>

#include <macos/macos.h>

#include "xnu/pmap.h"
#include "memory/kmem.h"
#include "slider/kernel.h"
#include "util/assert.h"


typedef struct {
    uintptr_t pa;
    uintptr_t va;
    vm_size_t len;
} ptov_table_entry;


uintptr_t xe_xnu_pmap_phystokv(uintptr_t pa) {
    ptov_table_entry ptov_table[PTOV_TABLE_SIZE];
    xe_kmem_read(ptov_table, xe_slider_kernel_slide(VAR_PTOV_TABLE), 0, sizeof(ptov_table));
    
    for (size_t i = 0; (i < PTOV_TABLE_SIZE) && (ptov_table[i].len != 0); i++) {
        if ((pa >= ptov_table[i].pa) && (pa < (ptov_table[i].pa + ptov_table[i].len))) {
            return pa - ptov_table[i].pa + ptov_table[i].va;
        }
    }
    
    uint64_t g_phys_base = xe_kmem_read_uint64(xe_slider_kernel_slide(VAR_G_PHYS_BASE), 0);
    xe_assert_cond(pa, >=, g_phys_base);
    uint64_t real_phys_size = xe_kmem_read_uint64(xe_slider_kernel_slide(VAR_REAL_PHYS_SIZE), 0);
    xe_assert_cond(pa - g_phys_base, <, real_phys_size);
    uint64_t g_virt_base = xe_kmem_read_uint64(xe_slider_kernel_slide(VAR_G_VIRT_BASE), 0);
    return pa - g_phys_base + g_virt_base;
}


uintptr_t xe_xnu_pmap_kvtophys(uintptr_t pmap, uintptr_t va) {
    uintptr_t ttp = xe_kmem_read_ptr(pmap, TYPE_PMAM_MEM_TTE_OFFSET);
    uintptr_t pt_attr = xe_kmem_read_ptr(pmap, TYPE_PMAP_MEM_PMAP_PT_ATTR_OFFSET);
    uintptr_t pta_level_info = xe_kmem_read_ptr(pt_attr, TYPE_PAGE_TABLE_ATTR_MEM_PTA_LEVEL_INFO_OFFSET);
    
    uint32_t root_level = xe_kmem_read_uint32(pt_attr, TYPE_PAGE_TABLE_ATTR_MEM_PTA_ROOT_LEVEL_OFFSET);
    uint32_t leaf_level = xe_kmem_read_uint32(pt_attr, TYPE_PAGE_TABLE_ATTR_MEM_PTA_MAX_LEVEL_OFFSET);
    
    for (uint32_t cur_level = root_level; cur_level <= leaf_level; cur_level++) {
        uintptr_t level_info = pta_level_info + TYPE_PAGE_TABLE_LEVEL_INFO_SIZE * cur_level;
        
        uint64_t index_mask = xe_kmem_read_uint64(level_info, TYPE_PAGE_TABLE_LEVEL_INFO_MEM_INDEX_MASK_OFFSET);
        uint64_t shift = xe_kmem_read_uint64(level_info, TYPE_PAGE_TABLE_LEVEL_INFO_MEM_SHIFT_OFFSET);
        uint64_t valid_mask = xe_kmem_read_uint64(level_info, TYPE_PAGE_TABLE_LEVEL_INFO_MEM_VALID_MASK_OFFSET);
        uint64_t type_block = xe_kmem_read_uint64(level_info, TYPE_PAGE_TABLE_LEVEL_INFO_MEM_TYPE_BLOCK_OFFSET);
        uint64_t type_mask = xe_kmem_read_uint64(level_info, TYPE_PAGE_TABLE_LEVEL_INFO_MEM_TYPE_MASK_OFFSET);
        uint64_t offmask = xe_kmem_read_uint64(level_info, TYPE_PAGE_TABLE_LEVEL_INFO_MEM_OFFMASK_OFFSET);
        
        uint64_t ttn_index = (va & index_mask) >> (shift & 0x3f) & 0xffffffff;
        uint64_t tte = xe_kmem_read_uint64(ttp, ttn_index * sizeof(uint64_t));
        
        if ((tte & valid_mask) != valid_mask) {
            return 0;
        }
        
        if ((tte & type_mask) == type_block) {
            return ((tte & 0xfffffffff000 & ~offmask) | (va & offmask));
        }
        
        ttp = xe_xnu_pmap_phystokv(tte & 0xfffffffff000);
    }
    
    return 0;
}

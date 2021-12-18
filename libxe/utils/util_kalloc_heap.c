//
//  util_kalloc_heap.c
//  xe
//
//  Created by admin on 12/6/21.
//

#include <sys/errno.h>

#include "util_kalloc_heap.h"
#include "platform_types.h"
#include "platform_variables.h"
#include "kmem.h"


uintptr_t xe_util_kh_find_zone_for_size(uintptr_t heap, size_t size) {
    uintptr_t kh_zones = xe_kmem_read_uint64(KMEM_OFFSET(heap, TYPE_KALLOC_HEAP_MEM_KH_ZONES_OFFSET));
    uint16_t max_k_zone = xe_kmem_read_uint16(KMEM_OFFSET(kh_zones, TYPE_KHEAP_ZONES_MEM_MAX_K_ZONE_OFFSET));
    uintptr_t cfg = xe_kmem_read_uint64(KMEM_OFFSET(kh_zones, TYPE_KHEAP_ZONES_MEM_CFG_OFFSET));
    uintptr_t k_zone = xe_kmem_read_uint64(KMEM_OFFSET(kh_zones, TYPE_KHEAP_ZONES_MEM_K_ZONE_OFFSET));
    for (int i=0; i<max_k_zone; i++) {
        uintptr_t z_cfg = cfg + i * TYPE_KALLOC_ZONE_CFG_SIZE;
        uint32_t kzc_size = xe_kmem_read_uint32(KMEM_OFFSET(z_cfg, TYPE_KALLOC_ZONE_CFG_MEM_KZC_SIZE_OFFSET));
        if (kzc_size >= size) {
            return xe_kmem_read_uint64(k_zone + i * sizeof(uintptr_t));
        }
    }
    return 0;
}


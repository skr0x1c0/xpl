
#include <sys/errno.h>

#include "util/kalloc_heap.h"
#include "memory/kmem.h"

#include <macos/kernel.h>


uintptr_t xpl_kheap_find_zone_for_size(uintptr_t heap, size_t size) {
    uintptr_t kh_zones = xpl_kmem_read_uint64(heap, TYPE_KALLOC_HEAP_MEM_KH_ZONES_OFFSET);
    uint16_t max_k_zone = xpl_kmem_read_uint16(kh_zones, TYPE_KHEAP_ZONES_MEM_MAX_K_ZONE_OFFSET);
    uintptr_t cfg = xpl_kmem_read_uint64(kh_zones, TYPE_KHEAP_ZONES_MEM_CFG_OFFSET);
    uintptr_t k_zone = xpl_kmem_read_uint64(kh_zones, TYPE_KHEAP_ZONES_MEM_K_ZONE_OFFSET);
    for (int i=0; i<max_k_zone; i++) {
        uintptr_t z_cfg = cfg + i * TYPE_KALLOC_ZONE_CFG_SIZE;
        uint32_t kzc_size = xpl_kmem_read_uint32(z_cfg, TYPE_KALLOC_ZONE_CFG_MEM_KZC_SIZE_OFFSET);
        if (kzc_size >= size) {
            return xpl_kmem_read_uint64(k_zone, i * sizeof(uintptr_t));
        }
    }
    return 0;
}


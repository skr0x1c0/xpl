//
//  io_surface.c
//  xe
//
//  Created by admin on 12/6/21.
//

#include <sys/errno.h>

#include "io_surface.h"
#include "io_dictionary.h"
#include "kmem.h"
#include "platform_types.h"


int xe_io_surface_find_surface_with_props_key(uintptr_t root_user_client, char* key, uintptr_t* out) {
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
        
        uintptr_t temp;
        int error = xe_io_os_dictionary_find_value(props_dict, key, &temp);
        if (!error) {
            *out = surface;
            return 0;
        }
    }
    
    return ENOENT;
}

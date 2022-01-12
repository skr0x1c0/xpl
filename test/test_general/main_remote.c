//
//  main_remote.c
//  test_remote
//
//  Created by admin on 1/11/22.
//

#include <stdio.h>

#include <xe/memory/kmem.h>
#include <xe/memory/kmem_remote.h>
#include <xe/slider/kernel.h>
#include <xe/util/log.h>
#include <xe/util/assert.h>
#include <xe/util/misc.h>


#include "registry.h"


int main(int argc, const char * argv[]) {
    xe_assert_cond(argc, >=, 2);
    const char* path = argv[1];
    const char* filter = argc > 2 ? argv[2] : "";
    
    struct xe_kmem_backend* backend = xe_kmem_remote_client_create(path);
    xe_kmem_use_backend(backend);
    xe_slider_kernel_init(xe_kmem_remote_client_get_mh_execute_header(backend));
    
    registry_run_tests(filter);
    return 0;
}

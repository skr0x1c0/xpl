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
    const char* uds_path = NULL;
    const char* test_name = NULL;
    
    int ch;
    while ((ch = getopt(argc, (char**)argv, "k:t:")) != -1) {
        switch (ch) {
            case 'k':
                uds_path = optarg;
                break;
            case 't':
                test_name = optarg;
                break;
            case '?':
            default:
                xe_log_info("usage: test_general_remote [-t test_name] [-k kmem_uds]");
                exit(1);
        }
    }
    
    xe_kmem_backend_t backend;
    int error = xe_kmem_remote_client_create(uds_path, &backend);
    if (error) {
        xe_log_error("failed to connect to kmem server, err: %s", strerror(error));
        exit(1);
    }
    
    xe_kmem_use_backend(backend);
    xe_slider_kernel_init(xe_kmem_remote_client_get_mh_execute_header(backend));
    
    registry_run_tests(test_name);
    return 0;
}

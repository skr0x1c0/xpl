//
//  main.c
//  test_kmem_gym
//
//  Created by admin on 2/7/22.
//

#include <stdio.h>

#include <xe/xe.h>
#include <xe/memory/kmem.h>
#include <xe/memory/kmem_remote.h>
#include <xe/slider/kernel.h>
#include <xe/util/assert.h>

#include <xe_dev/memory/gym.h>
#include <xe_dev/slider/kas.h>


int main(int argc, const char * argv[]) {
    const char* kmem_uds_path = NULL;
    
    int ch;
    while ((ch = getopt(argc, (char**)argv, "k:")) != -1) {
        switch (ch) {
            case 'k':
                kmem_uds_path = optarg;
                break;
            case '?':
            default:
                xe_log_info("usage: kmem_remote_gym [-k kmem_uds]");
                exit(1);
        }
    }
    
    
    xe_init();
    xe_kmem_backend_t gym_backend = xe_kmem_gym_create();
    xe_kmem_use_backend(gym_backend);
    xe_slider_kernel_init(xe_slider_kas_get_mh_execute_header());
    
    int error = xe_kmem_remote_server_start(xe_slider_kas_get_mh_execute_header(), kmem_uds_path);
    if (error) {
        xe_log_error("failed to start remote kmem server, err: %s", strerror(error));
        exit(1);
    }
    
    return 0;
}

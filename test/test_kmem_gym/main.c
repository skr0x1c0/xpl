//
//  main.c
//  test_kmem_gym
//
//  Created by admin on 2/7/22.
//

#include <stdio.h>

#include <xpl/xpl.h>
#include <xpl/memory/kmem.h>
#include <xpl/memory/kmem_remote.h>
#include <xpl/slider/kernel.h>
#include <xpl/util/assert.h>

#include <xpl_dev/memory/gym.h>
#include <xpl_dev/slider/kas.h>


int main(int argc, const char * argv[]) {
    const char* kmem_uds_path = xpl_DEFAULT_KMEM_SOCKET;
    
    int ch;
    while ((ch = getopt(argc, (char**)argv, "k:")) != -1) {
        switch (ch) {
            case 'k':
                kmem_uds_path = optarg;
                break;
            case '?':
            default:
                xpl_log_info("usage: kmem_remote_gym [-k kmem_uds]");
                exit(1);
        }
    }
    
    
    xpl_init();
    xpl_kmem_backend_t gym_backend = xpl_kmem_gym_create();
    xpl_kmem_use_backend(gym_backend);
    xpl_slider_kernel_init(xpl_slider_kas_get_mh_execute_header());
    
    int error = xpl_kmem_remote_server_start(xpl_slider_kas_get_mh_execute_header(), kmem_uds_path);
    if (error) {
        xpl_log_error("failed to start kmem server at socket %s, err: %s", kmem_uds_path, strerror(error));
        exit(1);
    }
    
    return 0;
}

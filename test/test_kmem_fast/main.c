//
//  main.c
//  kmem_msdosfs
//
//  Created by admin on 11/26/21.
//


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <signal.h>

#include <sys/errno.h>
#include <sys/fcntl.h>
#include <gym_client.h>

#include <xpl/xpl.h>
#include <xpl/memory/kmem.h>
#include <xpl/memory/kmem_remote.h>
#include <xpl/memory/kmem_fast.h>
#include <xpl/slider/kernel.h>
#include <xpl/util/assert.h>
#include <xpl/util/msdosfs.h>
#include <xpl/util/misc.h>

#include <xpl_dev/memory/gym.h>
#include <xpl_dev/slider/kas.h>
#include <xpl_dev/memory/tester.h>

#include <macos/kernel.h>


void bechmark(void) {
    uint32_t magic = xpl_kmem_read_uint32(xpl_slider_kernel_slide(XPL_IMAGE_SEGMENT_TEXT_BASE), 0);
    xpl_assert_cond(magic, ==, 0xfeedfacf);

    int test_limits[][2] = {
        { 1, 8 },
        { 8, 32 },
        { 32, 64 },
        { 64, 128 },
        { 128, 256 },
        { 256, 512 },
        { 512, 1024 },
        { 1024, 2048 },
        { 2048, 4096 },
        { 4096, 8192 },
        { 8192, 16384 },
        { 16384, 32768 },
    };

    double test_results[xpl_array_size(test_limits)][2];
    for (int i=0; i<xpl_array_size(test_limits); i++) {
        double* res = test_results[i];
        int* limit = test_limits[i];
        xpl_kmem_tester_run(10000, limit[0], limit[1], &res[0], &res[1]);
    }

    for (int i=0; i<xpl_array_size(test_limits); i++) {
        double* res = test_results[i];
        int* limit = test_limits[i];

        xpl_log_info("Min size: %d bytes, Max size: %d bytes, read: %.2f kB/s, write: %.2f kB/s", limit[0], limit[1], res[0] / 1024, res[1] / 1024);
    }
}


int main(int argc, const char* argv[]) {
    _Bool do_benchmark = FALSE;
    const char* uds_path = XPL_DEFAULT_KMEM_SOCKET;
    
    int ch;
    while ((ch = getopt(argc, (char**)argv, "bk:")) != -1) {
        switch (ch) {
            case 'b':
                do_benchmark = TRUE;
                break;
            case 'k':
                uds_path = optarg;
                break;
            case '?':
            default:
                xpl_log_info("usage: test_kmem_fast [-b] [-k kmem_uds]");
                exit(1);
                break;
        }
    }
    
    xpl_init();
    xpl_kmem_use_backend(xpl_kmem_gym_create());
    xpl_slider_kernel_init(xpl_slider_kas_get_mh_execute_header());

    xpl_kmem_backend_t kmem_fast =  xpl_memory_kmem_fast_create();
    xpl_kmem_use_backend(kmem_fast);

    if (do_benchmark) {
        bechmark();
    }
    
    int error = xpl_kmem_remote_server_start(xpl_slider_kas_get_mh_execute_header(), uds_path);
    if (error) {
        xpl_log_error("failed to start kmem server at socket %s, err: %s", uds_path, strerror(error));
        xpl_memory_kmem_fast_destroy(&kmem_fast);
        exit(1);
    }

    xpl_memory_kmem_fast_destroy(&kmem_fast);
    return 0;
}

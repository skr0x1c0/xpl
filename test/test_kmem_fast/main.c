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

#include <xe/xe.h>
#include <xe/memory/kmem.h>
#include <xe/memory/kmem_remote.h>
#include <xe/memory/kmem_fast.h>
#include <xe/slider/kernel.h>
#include <xe/util/assert.h>
#include <xe/util/msdosfs.h>
#include <xe/util/misc.h>

#include <xe_dev/memory/gym.h>
#include <xe_dev/slider/kas.h>
#include <xe_dev/memory/tester.h>

#include <macos/kernel.h>


void bechmark(void) {
    uint32_t magic = xe_kmem_read_uint32(xe_slider_kernel_slide(XE_IMAGE_SEGMENT_TEXT_BASE), 0);
    xe_assert_cond(magic, ==, 0xfeedfacf);

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

    double test_results[xe_array_size(test_limits)][2];
    for (int i=0; i<xe_array_size(test_limits); i++) {
        double* res = test_results[i];
        int* limit = test_limits[i];
        xe_kmem_tester_run(10000, limit[0], limit[1], &res[0], &res[1]);
    }

    for (int i=0; i<xe_array_size(test_limits); i++) {
        double* res = test_results[i];
        int* limit = test_limits[i];

        xe_log_info("Min size: %d bytes, Max size: %d bytes, read: %.2f kB/s, write: %.2f kB/s", limit[0], limit[1], res[0] / 1024, res[1] / 1024);
    }
}


int main(int argc, const char* argv[]) {
    _Bool do_benchmark = FALSE;
    const char* uds_path = XE_DEFAULT_KMEM_SOCKET;
    
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
                xe_log_info("usage: test_kmem_fast [-b] [-k kmem_uds]");
                exit(1);
                break;
        }
    }
    
    xe_init();
    xe_kmem_use_backend(xe_kmem_gym_create());
    xe_slider_kernel_init(xe_slider_kas_get_mh_execute_header());

    xe_kmem_backend_t kmem_fast =  xe_memory_kmem_fast_create();
    xe_kmem_use_backend(kmem_fast);

    if (do_benchmark) {
        bechmark();
    }
    
    int error = xe_kmem_remote_server_start(xe_slider_kas_get_mh_execute_header(), uds_path);
    if (error) {
        xe_log_error("failed to start kmem server at socket %s, err: %s", uds_path, strerror(error));
        xe_memory_kmem_fast_destroy(&kmem_fast);
        exit(1);
    }

    xe_memory_kmem_fast_destroy(&kmem_fast);
    return 0;
}

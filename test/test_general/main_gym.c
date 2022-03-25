//
//  main.c
//  test
//
//  Created by admin on 1/11/22.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

#include <xe/memory/kmem.h>
#include <xe/memory/kmem_remote.h>
#include <xe/slider/kernel.h>
#include <xe/util/log.h>
#include <xe/util/assert.h>
#include <xe/util/misc.h>

#include <xe_dev/memory/gym.h>
#include <xe_dev/slider/kas.h>


#include "registry.h"


int main(int argc, const char * argv[]) {
    const char* test_name = NULL;
    
    int ch;
    while ((ch = getopt(argc, (char**)argv, "t:")) != -1) {
        switch (ch) {
            case 't':
                test_name = optarg;
                break;
            case '?':
            default:
                xe_log_info("usage: test_general_gym [-t test_name]");
                exit(1);
        }
    }
    
    xe_assert_cond(getuid(), ==, 0);
    xe_kmem_use_backend(xe_kmem_gym_create());
    xe_slider_kernel_init(xe_slider_kas_get_mh_execute_header());
    
    registry_run_tests(test_name);
    return 0;
}

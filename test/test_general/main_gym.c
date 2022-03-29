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

#include <xpl/memory/kmem.h>
#include <xpl/memory/kmem_remote.h>
#include <xpl/slider/kernel.h>
#include <xpl/util/log.h>
#include <xpl/util/assert.h>
#include <xpl/util/misc.h>

#include <xpl_dev/memory/gym.h>
#include <xpl_dev/slider/kas.h>


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
                xpl_log_info("usage: test_general_gym [-t test_name]");
                exit(1);
        }
    }
    
    xpl_assert_cond(getuid(), ==, 0);
    xpl_kmem_use_backend(xpl_kmem_gym_create());
    xpl_slider_kernel_init(xpl_slider_kas_get_mh_execute_header());
    
    registry_run_tests(test_name);
    return 0;
}

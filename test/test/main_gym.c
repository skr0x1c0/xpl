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
    xe_assert_cond(getuid(), ==, 0);
    xe_kmem_use_backend(xe_kmem_gym_create());
    xe_slider_kernel_init(xe_slider_kas_get_mh_execute_header());
    
    const char* filter = argc > 1 ? argv[1] : "";
    registry_run_tests(filter);
    return 0;
}

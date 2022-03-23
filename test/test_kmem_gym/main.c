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
    xe_init();
    xe_kmem_backend_t gym_backend = xe_kmem_gym_create();
    xe_kmem_use_backend(gym_backend);
    xe_slider_kernel_init(xe_slider_kas_get_mh_execute_header());
    
    struct xe_kmem_remote_server_ctx ctx;
    ctx.mh_execute_header = xe_slider_kas_get_mh_execute_header();
    
    xe_kmem_remote_server_start(&ctx);
    return 0;
}

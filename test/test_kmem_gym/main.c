//
//  main.c
//  test_kmem_gym
//
//  Created by admin on 2/7/22.
//

#include <stdio.h>

#include <xe/memory/kmem.h>
#include <xe/memory/kmem_remote.h>
#include <xe/util/assert.h>

#include <xe_dev/memory/gym.h>
#include <xe_dev/slider/kas.h>


int main(int argc, const char * argv[]) {
    struct xe_kmem_backend* gym_backend = xe_kmem_gym_create();
    xe_kmem_use_backend(gym_backend);
    
    struct xe_kmem_remote_server_ctx ctx;
    ctx.mh_execute_header = xe_slider_kas_get_mh_execute_header();
    
    xe_kmem_remote_server_start(&ctx);
    return 0;
}

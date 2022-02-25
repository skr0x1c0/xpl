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

#include <xe/memory/kmem.h>
#include <xe/memory/kmem_remote.h>
#include <xe/memory/kmem_msdosfs.h>
#include <xe/slider/kernel.h>
#include <xe/util/assert.h>
#include <xe/util/msdosfs.h>

#include <xe_dev/memory/gym.h>
#include <xe_dev/slider/kas.h>

#include "macos_params.h"


int main(int argc, const char* argv[]) {
    xe_kmem_use_backend(xe_kmem_gym_create());
    xe_slider_kernel_init(xe_slider_kas_get_mh_execute_header());
    xe_util_msdosfs_loadkext();
    
    xe_kmem_backend_t msdosfs =  xe_kmem_msdosfs_create(KMEM_MSDOSFS_DEFAULT_BASE_IMAGE);
    xe_kmem_use_backend(msdosfs);
    
    uint32_t magic = xe_kmem_read_uint32(xe_slider_kernel_slide(XE_IMAGE_SEGMENT_TEXT_BASE), 0);
    xe_assert_cond(magic, ==, 0xfeedfacf);
    
    return 0;
}

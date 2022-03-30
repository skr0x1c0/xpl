
#include <stdio.h>

#include <xpl/memory/kmem.h>
#include <xpl/memory/kmem_remote.h>
#include <xpl/slider/kernel.h>
#include <xpl/util/log.h>
#include <xpl/util/assert.h>
#include <xpl/util/misc.h>


#include "registry.h"


int main(int argc, const char * argv[]) {
    const char* uds_path = XPL_DEFAULT_KMEM_SOCKET;
    const char* test_name = NULL;
    
    int ch;
    while ((ch = getopt(argc, (char**)argv, "k:t:")) != -1) {
        switch (ch) {
            case 'k':
                uds_path = optarg;
                break;
            case 't':
                test_name = optarg;
                break;
            case '?':
            default:
                xpl_log_info("usage: test_general_remote [-t test_name] [-k kmem_uds]");
                exit(1);
        }
    }
    
    xpl_kmem_backend_t backend;
    int error = xpl_kmem_remote_client_create(uds_path, &backend);
    if (error) {
        xpl_log_error("failed to connect to kmem server unix domain socket %s, err: %s", uds_path, strerror(error));
        exit(1);
    }
    
    xpl_kmem_use_backend(backend);
    xpl_slider_kernel_init(xpl_kmem_remote_client_get_mh_execute_header(backend));
    
    registry_run_tests(test_name);
    return 0;
}

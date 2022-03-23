//
//  main.c
//  kmem_client_test
//
//  Created by admin on 12/2/21.
//

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <gym_client.h>

#include <xe_dev/memory/tester.h>

#include <xe/memory/kmem.h>
#include <xe/memory/kmem_remote.h>
#include <xe/util/assert.h>
#include <xe/util/misc.h>


int main(int argc, const char * argv[]) {
    srandom(1221);
    
    xe_assert(argc == 2);
    const char* socket_path = argv[1];
    
    xe_kmem_backend_t remote_backend = xe_kmem_remote_client_create(socket_path);
    xe_kmem_use_backend(remote_backend);
    gym_init();
    
    int test_limits[][2] = {
        { 1, 8 },
        { 8, 32 },
        { 32, 64 },
        { 64, 128 },
        { 128, 256 },
        { 256, 512 },
        { 512, 1024 },
        { 1024, 2048 },
        { 2048, 4096 },
        { 4096, 8192 },
        { 8192, 16384 },
        { 16384, 32768 },
    };
    
    double test_results[xe_array_size(test_limits)][2];
    for (int i=0; i<xe_array_size(test_limits); i++) {
        double* res = test_results[i];
        int* limit = test_limits[i];
        xe_kmem_tester_run(10000, limit[0], limit[1], &res[0], &res[1]);
    }
    
    for (int i=0; i<xe_array_size(test_limits); i++) {
        double* res = test_results[i];
        int* limit = test_limits[i];
        
        printf("Min size: %d bytes, Max size: %d bytes, read: %.2f kB/s, write: %.2f kB/s\n", limit[0], limit[1], res[0] / 1024, res[1] / 1024);
    }
    
    xe_kmem_remote_client_destroy(&remote_backend);
    return 0;
}

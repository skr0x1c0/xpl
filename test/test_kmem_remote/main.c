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

#include <xe/memory/kmem.h>
#include <xe/memory/kmem_remote.h>
#include <xe_dev/memory/tester.h>
#include <xe/util/assert.h>

int main(int argc, const char * argv[]) {
    srandom(1221);
    
    xe_assert(argc == 2);
    const char* socket_path = argv[1];
    
    xe_kmem_backend_t remote_backend = xe_kmem_remote_client_create(socket_path);
    xe_kmem_use_backend(remote_backend);
    gym_init();
    
    xe_kmem_tester_run(100000, 32 * 1024);
    xe_kmem_remote_client_destroy(remote_backend);
    return 0;
}

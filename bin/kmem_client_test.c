//
//  main.c
//  kmem_client_test
//
//  Created by admin on 12/2/21.
//

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include "kmem.h"
#include "kmem_tester.h"
#include "kmem_remote.h"

int main(int argc, const char * argv[]) {
    srandom(1221);
    
    assert(argc == 2);
    const char* socket_path = argv[1];
    
    struct xe_kmem_backend* remote_backend = xe_kmem_remote_create(socket_path);
    xe_kmem_use_backend(remote_backend);
    xe_kmem_tester_run(100000, 1024);
    xe_kmem_remote_destroy(remote_backend);
    return 0;
}

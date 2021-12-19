//
//  kmem.c
//  xe
//
//  Created by admin on 11/20/21.
//

#include <unistd.h>
#include <stdlib.h>

#include "kmem.h"
#include "gym_client.h"


void xe_kmem_gym_read(void* ctx, void* dst, uintptr_t src, size_t size) {
    int error = gym_privileged_read((char*)dst, src, size);
    if (error) {
        printf("[ERROR] gym privileged read failed, src: %p, size: %lu, err: %d\n", (void*)src, size, error);
        abort();
    }
}

void xe_kmem_gym_write(void* ctx, uintptr_t dst, void* src, size_t size) {
    int error = gym_destructible_write(dst, (char*)src, size);
    if (error) {
        printf("[ERROR] gym destructible write failed, dst: %p, size: %lu, err: %d\n", (void*)dst, size, error);
        abort();
    }
}

static struct xe_kmem_ops xe_kmem_gym_ops = {
    .read = xe_kmem_gym_read,
    .write = xe_kmem_gym_write,
    
    .max_read_size = 16 * 1024,
    .max_write_size = 16 * 1024,
};

struct xe_kmem_backend* xe_kmem_gym_create(void) {
    if (getuid() != 0) {
        printf("[ERROR] root privileges are required for gym kmem read / write\n");
        abort();
    }
    gym_init();
    
    struct xe_kmem_backend* backend = malloc(sizeof(struct xe_kmem_backend));
    backend->ops = &xe_kmem_gym_ops;
    backend->ctx = NULL;
    
    return backend;
}
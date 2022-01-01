//
//  kmem_smbiod_rw.c
//  kmem_xepg
//
//  Created by admin on 1/1/22.
//

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "kmem_smbiod.h"
#include "kmem.h"
#include "smbiod_rw.h"


#define MAX_READ_SIZE UINT32_MAX
#define MAX_WRITE_SIZE UINT32_MAX


void xe_kmem_smbiod_read(void* ctx, void* dst, uintptr_t src, size_t size) {
    assert(size <= MAX_READ_SIZE);
    kmem_smbiod_rw_t rw = (kmem_smbiod_rw_t)ctx;
    kmem_smbiod_rw_read_data(rw, dst, src, size);
}

void xe_kmem_smbiod_write(void* ctx, uintptr_t dst, void* src, size_t size) {
    printf("[ERROR] write not supported by kmem_smbiod\n");
    abort();
}

static struct xe_kmem_ops xe_kmem_smbiod_ops = {
    .read = xe_kmem_smbiod_read,
    .max_read_size = MAX_READ_SIZE,
    .write = xe_kmem_smbiod_write,
    .max_write_size = MAX_WRITE_SIZE
};

struct xe_kmem_backend* xe_kmem_smbiod_create(kmem_smbiod_rw_t rw) {
    struct xe_kmem_backend* backend = malloc(sizeof(struct xe_kmem_backend));
    backend->ops = &xe_kmem_smbiod_ops;
    backend->ctx = rw;
    return backend;
}

void xe_kmem_smbiod_destroy(struct xe_kmem_backend** backend_p) {
    struct xe_kmem_backend* backend = *backend_p;
    free(backend);
    *backend_p = NULL;
}

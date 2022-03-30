
#include <unistd.h>
#include <stdlib.h>

#include <xpl/memory/kmem.h>
#include <xpl/memory/kmem_internal.h>
#include <xpl/util/log.h>

#include "gym_client.h"


void xpl_kmem_gym_read(void* ctx, void* dst, uintptr_t src, size_t size) {
    int error = gym_privileged_read((char*)dst, src, size);
    if (error) {
        xpl_log_error("gym privileged read failed, src: %p, size: %lu, err: %d", (void*)src, size, error);
        abort();
    }
}

void xpl_kmem_gym_write(void* ctx, uintptr_t dst, const void* src, size_t size) {
    int error = gym_destructible_write(dst, (char*)src, size);
    if (error) {
        xpl_log_error("gym destructible write failed, dst: %p, size: %lu, err: %d", (void*)dst, size, error);
        abort();
    }
}

static struct xpl_kmem_ops xpl_kmem_gym_ops = {
    .read = xpl_kmem_gym_read,
    .write = xpl_kmem_gym_write,
    
    .max_read_size = 16 * 1024,
    .max_write_size = 16 * 1024,
};

xpl_kmem_backend_t xpl_kmem_gym_create(void) {
    if (getuid() != 0) {
        xpl_log_error("root privileges are required for gym kmem read / write");
        abort();
    }
    gym_init();
    return xpl_kmem_backend_create(&xpl_kmem_gym_ops, NULL);
}

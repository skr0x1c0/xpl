//
//  kmem_common.c
//  xe
//
//  Created by admin on 11/20/21.
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "memory/kmem.h"
#include "util/misc.h"
#include "util/assert.h"

static struct xe_kmem_backend* kmem_backend;


void xe_kmem_use_backend(struct xe_kmem_backend* backend) {
    xe_assert(backend != NULL);
    kmem_backend = backend;
}

void xe_kmem_read(void* dst, uintptr_t base, uintptr_t off, size_t size) {
    xe_assert(kmem_backend != NULL);
    xe_assert_kaddr(base);
    xe_assert_cond(base + off, >=, base);
    size_t max_read_size = kmem_backend->ops->max_read_size;
    size_t done = 0;
    while (done < size) {
        size_t batch_size = xe_min(size - done, max_read_size);
        (*kmem_backend->ops->read)(kmem_backend->ctx, dst + done, base + off + done, batch_size);
        done += batch_size;
    }
}

void xe_kmem_write(uintptr_t base, uintptr_t off, void* src, size_t size) {
    xe_assert(kmem_backend != NULL);
    xe_assert_kaddr(base);
    xe_assert_cond(base + off, >=, base);
    size_t max_write_size = kmem_backend->ops->max_write_size;
    size_t done = 0;
    while (done < size) {
        size_t batch_size = xe_min(size - done, max_write_size);
        (*kmem_backend->ops->write)(kmem_backend->ctx, base + off + done, src + done, batch_size);
        done += batch_size;
    }
}

uint8_t xe_kmem_read_uint8(uintptr_t base, uintptr_t off) {
    uint8_t value;
    xe_kmem_read(&value, base, off, sizeof(value));
    return value;
}

uint16_t xe_kmem_read_uint16(uintptr_t base, uintptr_t off) {
    uint16_t value;
    xe_kmem_read(&value, base, off, sizeof(value));
    return value;
}

uint32_t xe_kmem_read_uint32(uintptr_t base, uintptr_t off) {
    uint32_t value;
    xe_kmem_read(&value, base, off, sizeof(value));
    return value;
}

uint64_t xe_kmem_read_uint64(uintptr_t base, uintptr_t off) {
    uint64_t value;
    xe_kmem_read(&value, base, off, sizeof(value));
    return value;
}

int8_t xe_kmem_read_int8(uintptr_t base, uintptr_t off) {
    int8_t value;
    xe_kmem_read(&value, base, off, sizeof(value));
    return value;
}

int16_t xe_kmem_read_int16(uintptr_t base, uintptr_t off) {
    int16_t value;
    xe_kmem_read(&value, base, off, sizeof(value));
    return value;
}

int32_t xe_kmem_read_int32(uintptr_t base, uintptr_t off) {
    int32_t value;
    xe_kmem_read(&value, base, off, sizeof(value));
    return value;
}

int64_t xe_kmem_read_int64(uintptr_t base, uintptr_t off) {
    int64_t value;
    xe_kmem_read(&value, base, off, sizeof(value));
    return value;
}

void xe_kmem_write_uint8(uintptr_t base, uintptr_t off, uint8_t value) {
    xe_kmem_write(base, off, &value, sizeof(value));
}

void xe_kmem_write_uint16(uintptr_t dst, uintptr_t off, uint16_t value) {
    xe_kmem_write(dst, off, &value, sizeof(value));
}

void xe_kmem_write_uint32(uintptr_t dst, uintptr_t off, uint32_t value) {
    xe_kmem_write(dst, off, &value, sizeof(value));
}

void xe_kmem_write_uint64(uintptr_t dst, uintptr_t off, uint64_t value) {
    xe_kmem_write(dst, off, &value, sizeof(value));
}

void xe_kmem_write_int8(uintptr_t dst, uintptr_t off, int8_t value) {
    xe_kmem_write(dst, off, &value, sizeof(value));
}

void xe_kmem_write_int16(uintptr_t dst, uintptr_t off, int16_t value) {
    xe_kmem_write(dst, off, &value, sizeof(value));
}

void xe_kmem_write_int32(uintptr_t dst, uintptr_t off, int32_t value) {
    xe_kmem_write(dst, off, &value, sizeof(value));
}

void xe_kmem_write_int64(uintptr_t dst, uintptr_t off, int64_t value) {
    xe_kmem_write(dst, off, &value, sizeof(value));
}

void xe_kmem_copy(uintptr_t dst, uintptr_t src, size_t size) {
    char buffer[128];
    size_t done = 0;
    while (done < size) {
        size_t batch_size = size - done > sizeof(buffer) ? sizeof(buffer) : size - done;
        xe_kmem_read(buffer, src, done, batch_size);
        xe_kmem_write(dst, done, buffer, batch_size);
        done += batch_size;
    }
}

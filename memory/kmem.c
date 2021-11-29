//
//  kmem_common.c
//  xe
//
//  Created by admin on 11/20/21.
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "kmem.h"

static struct xe_kmem_ops* kmem_ops;


void xe_kmem_setup(struct xe_kmem_ops* ops) {
    assert(ops != NULL);
    kmem_ops = ops;
}

void xe_kmem_read(void* dst, uintptr_t src, size_t size) {
    assert(kmem_ops != NULL);
    (*kmem_ops->read)(kmem_ops->ctx, dst, src, size);
}
void xe_kmem_write(uintptr_t dst, void* src, size_t size) {
    assert(kmem_ops != NULL);
    (*kmem_ops->write)(kmem_ops->ctx, dst, src, size);
}

uint8_t xe_kmem_read_uint8(uintptr_t src) {
    uint8_t value;
    xe_kmem_read(&value, src, sizeof(value));
    return value;
}

uint16_t xe_kmem_read_uint16(uintptr_t src) {
    uint16_t value;
    xe_kmem_read(&value, src, sizeof(value));
    return value;
}

uint32_t xe_kmem_read_uint32(uintptr_t src) {
    uint32_t value;
    xe_kmem_read(&value, src, sizeof(value));
    return value;
}

uint64_t xe_kmem_read_uint64(uintptr_t src) {
    uint64_t value;
    xe_kmem_read(&value, src, sizeof(value));
    return value;
}

int8_t xe_kmem_read_int8(uintptr_t src) {
    int8_t value;
    xe_kmem_read(&value, src, sizeof(value));
    return value;
}

int16_t xe_kmem_read_int16(uintptr_t src) {
    int16_t value;
    xe_kmem_read(&value, src, sizeof(value));
    return value;
}

int32_t xe_kmem_read_int32(uintptr_t src) {
    int32_t value;
    xe_kmem_read(&value, src, sizeof(value));
    return value;
}

int64_t xe_kmem_read_int64(uintptr_t src) {
    int64_t value;
    xe_kmem_read(&value, src, sizeof(value));
    return value;
}

void xe_kmem_write_uint8(uintptr_t dst, uint8_t value) {
    xe_kmem_write(dst, &value, sizeof(value));
}

void xe_kmem_write_uint16(uintptr_t dst, uint16_t value) {
    xe_kmem_write(dst, &value, sizeof(value));
}

void xe_kmem_write_uint32(uintptr_t dst, uint32_t value) {
    xe_kmem_write(dst, &value, sizeof(value));
}

void xe_kmem_write_uint64(uintptr_t dst, uint64_t value) {
    xe_kmem_write(dst, &value, sizeof(value));
}

void xe_kmem_write_int8(uintptr_t dst, int8_t value) {
    xe_kmem_write(dst, &value, sizeof(value));
}

void xe_kmem_write_int16(uintptr_t dst, int16_t value) {
    xe_kmem_write(dst, &value, sizeof(value));
}

void xe_kmem_write_int32(uintptr_t dst, int32_t value) {
    xe_kmem_write(dst, &value, sizeof(value));
}

void xe_kmem_write_int64(uintptr_t dst, int64_t value) {
    xe_kmem_write(dst, &value, sizeof(value));
}

void xe_kmem_copy(uintptr_t dst, uintptr_t src, size_t size) {
    char buffer[128];
    size_t done = 0;
    while (done < size) {
        size_t batch_size = size - done > sizeof(buffer) ? sizeof(buffer) : size - done;
        xe_kmem_read(buffer, src + done, batch_size);
        xe_kmem_write(dst + done, buffer, batch_size);
        done += batch_size;
    }
}

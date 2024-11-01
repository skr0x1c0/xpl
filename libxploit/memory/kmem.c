
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "memory/kmem.h"
#include "memory/kmem_internal.h"
#include "util/misc.h"
#include "util/assert.h"
#include "util/log.h"
#include "util/ptrauth.h"
#include "slider/kernel.h"


struct xpl_kmem_backend {
    const struct xpl_kmem_ops* ops;
    void* ctx;
};


static struct xpl_kmem_backend* kmem_backend;


xpl_kmem_backend_t xpl_kmem_use_backend(xpl_kmem_backend_t backend) {
    xpl_assert(backend != NULL);
    xpl_kmem_backend_t current = kmem_backend;
    kmem_backend = backend;
    return current;
}

xpl_kmem_backend_t xpl_kmem_backend_create(const struct xpl_kmem_ops* ops, void* ctx) {
    xpl_kmem_backend_t backend = malloc(sizeof(struct xpl_kmem_backend));
    backend->ctx = ctx;
    backend->ops = ops;
    return backend;
}

void* xpl_kmem_backend_get_ctx(xpl_kmem_backend_t backend) {
    return backend->ctx;
}

void xpl_kmem_backend_destroy(xpl_kmem_backend_t* backend_p) {
    free(*backend_p);
    *backend_p = NULL;
}

void xpl_kmem_validate_addr_range(uintptr_t base, size_t size) {
    uintptr_t end = base + size;
    if (xpl_vm_kernel_address_valid(base) && xpl_vm_kernel_address_valid(end)) {
        return;
    }
    
    xpl_log_error("address range [%p, %p) not in valid kernel address range [%p, %p)", (void*)base, (void*)end, (void*)XPL_VM_MIN_KERNEL_ADDRESS, (void*)XPL_VM_MAX_KERNEL_ADDRESS);
    xpl_assert(0);
}

void xpl_kmem_read(void* dst, uintptr_t base, ssize_t off, size_t size) {
    xpl_assert(kmem_backend != NULL);
    xpl_kmem_validate_addr_range(base + off, size);
    size_t max_read_size = kmem_backend->ops->max_read_size;
    size_t done = 0;
    while (done < size) {
        size_t batch_size = xpl_min(size - done, max_read_size);
        (*kmem_backend->ops->read)(kmem_backend->ctx, dst + done, base + off + done, batch_size);
        done += batch_size;
    }
}

void xpl_kmem_write(uintptr_t base, ssize_t off, const void* src, size_t size) {
    xpl_assert(kmem_backend != NULL);
    xpl_kmem_validate_addr_range(base + off, size);
    size_t max_write_size = kmem_backend->ops->max_write_size;
    size_t done = 0;
    while (done < size) {
        size_t batch_size = xpl_min(size - done, max_write_size);
        (*kmem_backend->ops->write)(kmem_backend->ctx, base + off + done, src + done, batch_size);
        done += batch_size;
    }
}

uint8_t xpl_kmem_read_uint8(uintptr_t base, ssize_t off) {
    uint8_t value;
    xpl_kmem_read(&value, base, off, sizeof(value));
    return value;
}

uint16_t xpl_kmem_read_uint16(uintptr_t base, ssize_t off) {
    uint16_t value;
    xpl_kmem_read(&value, base, off, sizeof(value));
    return value;
}

uint32_t xpl_kmem_read_uint32(uintptr_t base, ssize_t off) {
    uint32_t value;
    xpl_kmem_read(&value, base, off, sizeof(value));
    return value;
}

uint64_t xpl_kmem_read_uint64(uintptr_t base, ssize_t off) {
    uint64_t value;
    xpl_kmem_read(&value, base, off, sizeof(value));
    return value;
}

uintptr_t xpl_kmem_read_ptr(uintptr_t base, ssize_t off) {
    uintptr_t value = xpl_kmem_read_uint64(base, off);
    if (!value) {
        return 0;
    }
    uintptr_t ptr = value | XPL_PTRAUTH_MASK;
    xpl_assert_kaddr(ptr);
    return ptr;
}

int8_t xpl_kmem_read_int8(uintptr_t base, ssize_t off) {
    int8_t value;
    xpl_kmem_read(&value, base, off, sizeof(value));
    return value;
}

int16_t xpl_kmem_read_int16(uintptr_t base, ssize_t off) {
    int16_t value;
    xpl_kmem_read(&value, base, off, sizeof(value));
    return value;
}

int32_t xpl_kmem_read_int32(uintptr_t base, ssize_t off) {
    int32_t value;
    xpl_kmem_read(&value, base, off, sizeof(value));
    return value;
}

int64_t xpl_kmem_read_int64(uintptr_t base, ssize_t off) {
    int64_t value;
    xpl_kmem_read(&value, base, off, sizeof(value));
    return value;
}

void xpl_kmem_write_uint8(uintptr_t base, ssize_t off, uint8_t value) {
    xpl_kmem_write(base, off, &value, sizeof(value));
}

void xpl_kmem_write_uint16(uintptr_t dst, ssize_t off, uint16_t value) {
    xpl_kmem_write(dst, off, &value, sizeof(value));
}

void xpl_kmem_write_uint32(uintptr_t dst, ssize_t off, uint32_t value) {
    xpl_kmem_write(dst, off, &value, sizeof(value));
}

void xpl_kmem_write_uint64(uintptr_t dst, ssize_t off, uint64_t value) {
    xpl_kmem_write(dst, off, &value, sizeof(value));
}

void xpl_kmem_write_int8(uintptr_t dst, ssize_t off, int8_t value) {
    xpl_kmem_write(dst, off, &value, sizeof(value));
}

void xpl_kmem_write_int16(uintptr_t dst, ssize_t off, int16_t value) {
    xpl_kmem_write(dst, off, &value, sizeof(value));
}

void xpl_kmem_write_int32(uintptr_t dst, ssize_t off, int32_t value) {
    xpl_kmem_write(dst, off, &value, sizeof(value));
}

void xpl_kmem_write_int64(uintptr_t dst, ssize_t off, int64_t value) {
    xpl_kmem_write(dst, off, &value, sizeof(value));
}

#define bitfield_reader(name, type) \
    type xpl_kmem_read_bitfield_##name(uintptr_t base, ssize_t off, int bit_offset, int bit_size) { \
        xpl_assert_cond(bit_offset, <, NBBY * sizeof(type)); \
        xpl_assert_cond(bit_offset + bit_size, <=, NBBY * sizeof(type)); \
        type val = xpl_kmem_read_##name(base, off); \
        return (val >> bit_offset) & ((1ULL << bit_size) - 1); \
    }

bitfield_reader(uint8, uint8_t);
bitfield_reader(uint16, uint16_t);
bitfield_reader(uint32, uint32_t);
bitfield_reader(uint64, uint64_t);

#define bitfield_writer(name, type) \
    void xpl_kmem_write_bitfield_##name(uintptr_t dst, ssize_t off, type value, int bit_offset, int bit_size) { \
        xpl_assert_cond(bit_offset, <, NBBY * sizeof(type)); \
        xpl_assert_cond(bit_offset + bit_size, <=, NBBY * sizeof(type)); \
        type bitfield = xpl_kmem_read_##name(dst, off); \
        bitfield &= ~(((1ULL << bit_size) - 1) << bit_offset); \
        bitfield |= (value << bit_offset); \
        xpl_kmem_write_##name(dst, off, bitfield); \
    } \

bitfield_writer(uint8, uint8_t);
bitfield_writer(uint16, uint16_t);
bitfield_writer(uint32, uint32_t);
bitfield_writer(uint64, uint64_t);

void xpl_kmem_copy(uintptr_t dst, uintptr_t src, size_t size) {
    char buffer[128];
    size_t done = 0;
    while (done < size) {
        size_t batch_size = size - done > sizeof(buffer) ? sizeof(buffer) : size - done;
        xpl_kmem_read(buffer, src, done, batch_size);
        xpl_kmem_write(dst, done, buffer, batch_size);
        done += batch_size;
    }
}


#ifndef xpl_memory_kmem_h
#define xpl_memory_kmem_h

#include <assert.h>
#include <stdio.h>

typedef struct xpl_kmem_backend* xpl_kmem_backend_t;

xpl_kmem_backend_t xpl_kmem_use_backend(xpl_kmem_backend_t backend);
void xpl_kmem_read(void* dst, uintptr_t base, ssize_t off, size_t size);
void xpl_kmem_write(uintptr_t base, ssize_t off, const void* src, size_t size);

uint8_t xpl_kmem_read_uint8(uintptr_t base, ssize_t off);
uint16_t xpl_kmem_read_uint16(uintptr_t base, ssize_t off);
uint32_t xpl_kmem_read_uint32(uintptr_t base, ssize_t off);
uint64_t xpl_kmem_read_uint64(uintptr_t base, ssize_t off);
uintptr_t xpl_kmem_read_ptr(uintptr_t base, ssize_t off);
int8_t xpl_kmem_read_int8(uintptr_t base, ssize_t off);
int16_t xpl_kmem_read_int16(uintptr_t base, ssize_t off);
int32_t xpl_kmem_read_int32(uintptr_t base, ssize_t off);
int64_t xpl_kmem_read_int64(uintptr_t base, ssize_t off);

void xpl_kmem_write_uint8(uintptr_t base, ssize_t off, uint8_t value);
void xpl_kmem_write_uint16(uintptr_t base, ssize_t off, uint16_t value);
void xpl_kmem_write_uint32(uintptr_t base, ssize_t off, uint32_t value);
void xpl_kmem_write_uint64(uintptr_t base, ssize_t off, uint64_t value);
void xpl_kmem_write_int8(uintptr_t base, ssize_t off, int8_t value);
void xpl_kmem_write_int16(uintptr_t base, ssize_t off, int16_t value);
void xpl_kmem_write_int32(uintptr_t base, ssize_t off, int32_t value);
void xpl_kmem_write_int64(uintptr_t base, ssize_t off, int64_t value);

uint8_t xpl_kmem_read_bitfield_uint8(uintptr_t base, ssize_t off, int bit_offset, int bit_size);
uint16_t xpl_kmem_read_bitfield_uint16(uintptr_t base, ssize_t off, int bit_offset, int bit_size);
uint32_t xpl_kmem_read_bitfield_uint32(uintptr_t base, ssize_t off, int bit_offset, int bit_size);
uint64_t xpl_kmem_read_bitfield_uint64(uintptr_t base, ssize_t off, int bit_offset, int bit_size);

void xpl_kmem_write_bitfield_uint8(uintptr_t dst, ssize_t off, uint8_t value, int bit_offset, int bit_size);
void xpl_kmem_write_bitfield_uint16(uintptr_t dst, ssize_t off, uint16_t value, int bit_offset, int bit_size);
void xpl_kmem_write_bitfield_uint32(uintptr_t dst, ssize_t off, uint32_t value, int bit_offset, int bit_size);
void xpl_kmem_write_bitfield_uint64(uintptr_t dst, ssize_t off, uint64_t value, int bit_offset, int bit_size);


void xpl_kmem_copy(uintptr_t dst, uintptr_t src, size_t size);


#define xpl_kmem_read_bitfield(dst, src, bit_offset, bit_size) \
{ \
    size_t byte_size = ((bit_size) + NBBY - 1) / NBBY;\
    xpl_assert(sizeof(*dst) >= byte_size);\
    xpl_kmem_read(dst, (src) + ((bit_offset) / NBBY), 0, byte_size); \
    (*dst) = (((*(dst)) >> ((bit_offset) % NBBY)) & ((1 << (bit_size)) - 1));\
}

#endif /* xpl_memory_kmem_h */

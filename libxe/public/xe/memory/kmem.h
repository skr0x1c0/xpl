//
//  kmem.h
//  xe
//
//  Created by admin on 11/20/21.
//

#ifndef xe_memory_kmem_h
#define xe_memory_kmem_h

#include <assert.h>
#include <stdio.h>

typedef struct xe_kmem_backend* xe_kmem_backend_t;

xe_kmem_backend_t xe_kmem_use_backend(xe_kmem_backend_t backend);
void xe_kmem_read(void* dst, uintptr_t base, uintptr_t off, size_t size);
void xe_kmem_write(uintptr_t base, uintptr_t off, void* src, size_t size);

uint8_t xe_kmem_read_uint8(uintptr_t base, uintptr_t off);
uint16_t xe_kmem_read_uint16(uintptr_t base, uintptr_t off);
uint32_t xe_kmem_read_uint32(uintptr_t base, uintptr_t off);
uint64_t xe_kmem_read_uint64(uintptr_t base, uintptr_t off);
int8_t xe_kmem_read_int8(uintptr_t base, uintptr_t off);
int16_t xe_kmem_read_int16(uintptr_t base, uintptr_t off);
int32_t xe_kmem_read_int32(uintptr_t base, uintptr_t off);
int64_t xe_kmem_read_int64(uintptr_t base, uintptr_t off);

void xe_kmem_write_uint8(uintptr_t base, uintptr_t off, uint8_t value);
void xe_kmem_write_uint16(uintptr_t base, uintptr_t off, uint16_t value);
void xe_kmem_write_uint32(uintptr_t base, uintptr_t off, uint32_t value);
void xe_kmem_write_uint64(uintptr_t base, uintptr_t off, uint64_t value);
void xe_kmem_write_int8(uintptr_t base, uintptr_t off, int8_t value);
void xe_kmem_write_int16(uintptr_t base, uintptr_t off, int16_t value);
void xe_kmem_write_int32(uintptr_t base, uintptr_t off, int32_t value);
void xe_kmem_write_int64(uintptr_t base, uintptr_t off, int64_t value);

void xe_kmem_copy(uintptr_t dst, uintptr_t src, size_t size);


#define xe_kmem_read_bitfield(dst, src, bit_offset, bit_size) \
{ \
    size_t byte_size = ((bit_size) + NBBY - 1) / NBBY;\
    xe_assert(sizeof(*dst) >= byte_size);\
    xe_kmem_read(dst, (src) + ((bit_offset) / NBBY), 0, byte_size); \
    (*dst) = (((*(dst)) >> ((bit_offset) % NBBY)) & ((1 << (bit_size)) - 1));\
}

#endif /* xe_memory_kmem_h */

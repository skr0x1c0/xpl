//
//  kmem.h
//  xe
//
//  Created by admin on 11/20/21.
//

#ifndef memory_kmem_h
#define memory_kmem_h

#include <stdio.h>

#define KMEM_OFFSET(base, offset) (base + offset)

struct xe_kmem_ops {
    void(*read)(void* ctx, void* dst, uintptr_t src, size_t size);
    void(*write)(void* ctx, uintptr_t dst, void* src, size_t size);
    
    size_t max_read_size;
    size_t max_write_size;
};

struct xe_kmem_backend {
    struct xe_kmem_ops* ops;
    void* ctx;
};

void xe_kmem_use_backend(struct xe_kmem_backend* backend);
void xe_kmem_read(void* dst, uintptr_t src, size_t size);
void xe_kmem_write(uintptr_t dst, void* src, size_t size);

uint8_t xe_kmem_read_uint8(uintptr_t src);
uint16_t xe_kmem_read_uint16(uintptr_t src);
uint32_t xe_kmem_read_uint32(uintptr_t src);
uint64_t xe_kmem_read_uint64(uintptr_t src);
int8_t xe_kmem_read_int8(uintptr_t src);
int16_t xe_kmem_read_int16(uintptr_t src);
int32_t xe_kmem_read_int32(uintptr_t src);
int64_t xe_kmem_read_int64(uintptr_t src);

void xe_kmem_write_uint8(uintptr_t dst, uint8_t value);
void xe_kmem_write_uint16(uintptr_t dst, uint16_t value);
void xe_kmem_write_uint32(uintptr_t dst, uint32_t value);
void xe_kmem_write_uint64(uintptr_t dst, uint64_t value);
void xe_kmem_write_int8(uintptr_t dst, int8_t value);
void xe_kmem_write_int16(uintptr_t dst, int16_t value);
void xe_kmem_write_int32(uintptr_t dst, int32_t value);
void xe_kmem_write_int64(uintptr_t dst, int64_t value);

void xe_kmem_copy(uintptr_t dst, uintptr_t src, size_t size);


#endif /* memory_kmem_h */

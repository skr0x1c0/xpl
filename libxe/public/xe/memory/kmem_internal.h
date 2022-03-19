//
//  kmem_internal.h
//  libxe
//
//  Created by sreejith on 2/24/22.
//

#ifndef kmem_internal_h
#define kmem_internal_h

struct xe_kmem_ops {
    void(*read)(void* ctx, void* dst, uintptr_t src, size_t size);
    void(*write)(void* ctx, uintptr_t dst, const void* src, size_t size);
    
    size_t max_read_size;
    size_t max_write_size;
};

xe_kmem_backend_t xe_kmem_backend_create(const struct xe_kmem_ops* ops, void* ctx);
void* xe_kmem_backend_get_ctx(xe_kmem_backend_t backend);
void xe_kmem_backend_destroy(xe_kmem_backend_t* backend_p);

#endif /* kmem_internal_h */

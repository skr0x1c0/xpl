//
//  kmem_internal.h
//  libxe
//
//  Created by sreejith on 2/24/22.
//

#ifndef xpl_kmem_internal_h
#define xpl_kmem_internal_h

struct xpl_kmem_ops {
    void(*read)(void* ctx, void* dst, uintptr_t src, size_t size);
    void(*write)(void* ctx, uintptr_t dst, const void* src, size_t size);
    
    size_t max_read_size;
    size_t max_write_size;
};

xpl_kmem_backend_t xpl_kmem_backend_create(const struct xpl_kmem_ops* ops, void* ctx);
void* xpl_kmem_backend_get_ctx(xpl_kmem_backend_t backend);
void xpl_kmem_backend_destroy(xpl_kmem_backend_t* backend_p);

#endif /* xpl_kmem_internal_h */

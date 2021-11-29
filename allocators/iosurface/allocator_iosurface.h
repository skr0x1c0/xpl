#ifndef iokit_iosurface_allocator_h
#define iokit_iosurface_allocator_h

#include <stdio.h>

typedef struct iokit_iosurface_allocator* iokit_iosurface_allocator_t;

int iokit_iosurface_allocator_create(iokit_iosurface_allocator_t* allocator_out);
int iokit_iosurface_allocator_allocate(iokit_iosurface_allocator_t allocator, int size, int count, size_t* alloc_idx_out);
int iokit_iosurface_allocator_release(iokit_iosurface_allocator_t allocator, size_t alloc_idx);
int iokit_iosurface_allocator_destroy(iokit_iosurface_allocator_t* allocator_p);

#endif /* iokit_iosurface_allocator_h */

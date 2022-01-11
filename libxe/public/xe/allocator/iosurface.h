#ifndef xe_iosurface_allocator_h
#define xe_iosurface_allocator_h

#include <stdio.h>

typedef struct xe_iosurface_allocator* xe_iosurface_allocator_t;

int xe_iosurface_allocator_create(xe_iosurface_allocator_t* allocator_out);
int xe_iosurface_allocator_allocate(xe_iosurface_allocator_t allocator, int size, int count, size_t* alloc_idx_out);
int xe_iosurface_allocator_release(xe_iosurface_allocator_t allocator, size_t alloc_idx);
int xe_iosurface_allocator_destroy(xe_iosurface_allocator_t* allocator_p);

#endif /* xe_iosurface_allocator_h */

#ifndef allocator_rw_h
#define allocator_rw_h

#include <sys/types.h>
#include <arpa/inet.h>

/*
 * Allocator with read write capability
 * -- Reading of allocated memory can be done anytime
 * -- Writing to memory can only be done initially during allocation
 * -- Memory allocated from `KHEAP_KEXT`
 */

typedef struct kmem_allocator_rw* kmem_allocator_rw_t;

typedef void(^kmem_allocator_rw_data_reader)(void* ctx, char** data1_out, uint32_t* data1_size_out, char** data2_out, uint32_t* data2_size_out, size_t index);

typedef _Bool(^kmem_allocator_rw_data_filter)(void* ctx, char* data1, char* data2, size_t index);

kmem_allocator_rw_t kmem_allocator_rw_create(const struct sockaddr_in* addr, int count);
int kmem_allocator_rw_allocate(kmem_allocator_rw_t allocator, int count, kmem_allocator_rw_data_reader data_reader, void* data_reader_ctx);
int kmem_allocator_rw_filter(kmem_allocator_rw_t allocator, int offset, int count, uint32_t data1_size, uint32_t data2_size, kmem_allocator_rw_data_filter filter, void* filter_ctx, int64_t* found_idx_out);
int kmem_allocator_rw_read(kmem_allocator_rw_t allocator, int index, char* data1_out, uint32_t data1_size, char* data2_out, uint32_t data2_size);
int kmem_allocator_rw_disown_backend(kmem_allocator_rw_t allocator, int index);
int kmem_allocator_rw_release_backends(kmem_allocator_rw_t allocator, int offset, int count);
int kmem_allocator_rw_grow_backend_count(kmem_allocator_rw_t allocator, int count);
int kmem_allocator_rw_get_backend_count(kmem_allocator_rw_t allocator);
int kmem_allocator_rw_destroy(kmem_allocator_rw_t* allocator);

#endif /* allocator_rw_h */

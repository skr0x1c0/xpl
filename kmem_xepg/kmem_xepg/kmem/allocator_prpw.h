#ifndef kmem_allocator_prpw_h
#define kmem_allocator_prpw_h

#include <sys/types.h>
#include <arpa/inet.h>

typedef struct kmem_allocator_prpw* kmem_allocator_prpw_t;
typedef void(^kmem_allocator_prpw_data_reader)(void* ctx, uint8_t* len, sa_family_t* family, char** data, size_t* data_len, size_t index);
typedef _Bool(^kmem_allocator_prpw_data_filter)(void* ctx, sa_family_t family, size_t index);

kmem_allocator_prpw_t kmem_allocator_prpw_create(const struct sockaddr_in* addr, size_t num_allocs);
int kmem_allocator_prpw_allocate(kmem_allocator_prpw_t id, size_t count, kmem_allocator_prpw_data_reader reader, void* reader_ctx);
size_t kmem_allocator_prpw_get_capacity(kmem_allocator_prpw_t allocator);
int kmem_allocator_prpw_filter(kmem_allocator_prpw_t allocator, size_t offset, size_t count, kmem_allocator_prpw_data_filter filter, void* filter_ctx, int64_t* found_idx_out);
int kmem_allocator_prpw_read(kmem_allocator_prpw_t allocator, size_t alloc_index, sa_family_t* family);
int kmem_allocator_prpw_trim_backend_count(kmem_allocator_prpw_t allocator, size_t offset, size_t count);
int kmem_allocator_prpw_release_containing_backend(kmem_allocator_prpw_t allocator, int64_t alloc_index);
int kmem_allocator_prpw_destroy(kmem_allocator_prpw_t* id);

#endif /* kmem_allocator_prpw_h */

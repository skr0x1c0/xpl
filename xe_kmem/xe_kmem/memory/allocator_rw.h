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

typedef struct xe_allocator_rw* xe_allocator_rw_t;

typedef void(^xe_allocator_rw_data_reader)(void* ctx, char** data1_out, uint32_t* data1_size_out, char** data2_out, uint32_t* data2_size_out, size_t index);

typedef _Bool(^xe_allocator_rw_data_filter)(void* ctx, char* data1, char* data2, size_t index);

xe_allocator_rw_t xe_allocator_rw_create(const struct sockaddr_in* addr, int count);
int xe_allocator_rw_allocate(xe_allocator_rw_t allocator, int count, xe_allocator_rw_data_reader data_reader, void* data_reader_ctx);
int xe_allocator_rw_filter(xe_allocator_rw_t allocator, int offset, int count, uint32_t data1_size, uint32_t data2_size, xe_allocator_rw_data_filter filter, void* filter_ctx, int* found_idx_out);
int xe_allocator_rw_read(xe_allocator_rw_t allocator, int index, char* data1_out, uint32_t data1_size, char* data2_out, uint32_t data2_size);
int xe_allocator_rw_disown_backend(xe_allocator_rw_t allocator, int index);
int xe_allocator_rw_release_backends(xe_allocator_rw_t allocator, int offset, int count);
int xe_allocator_rw_grow_backend_count(xe_allocator_rw_t allocator, int count);
int xe_allocator_rw_get_backend_count(xe_allocator_rw_t allocator);
int xe_allocator_rw_destroy(xe_allocator_rw_t* allocator);

#endif /* allocator_rw_h */

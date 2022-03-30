#ifndef xpl_allocator_rw_h
#define xpl_allocator_rw_h

#include <sys/types.h>
#include <arpa/inet.h>

/**
 * Allocator with read write capability
 * -- Reading of allocated memory can be done anytime
 * -- Writing to memory can only be done initially during allocation
 * -- Memory allocated from `KHEAP_KEXT`
 */

typedef struct xpl_allocator_rw* xpl_allocator_rw_t;

typedef void(^xpl_allocator_rw_data_reader)(void* ctx, char** data1_out, uint32_t* data1_size_out, char** data2_out, uint32_t* data2_size_out, size_t index);

typedef _Bool(^xpl_allocator_rw_data_filter)(void* ctx, char* data1, char* data2, size_t index);

xpl_allocator_rw_t xpl_allocator_rw_create(const struct sockaddr_in* addr, int count);
int xpl_allocator_rw_allocate(xpl_allocator_rw_t allocator, int count, xpl_allocator_rw_data_reader data_reader, void* data_reader_ctx);
int xpl_allocator_rw_filter(xpl_allocator_rw_t allocator, int offset, int count, uint32_t data1_size, uint32_t data2_size, xpl_allocator_rw_data_filter filter, void* filter_ctx, int* found_idx_out);
int xpl_allocator_rw_read(xpl_allocator_rw_t allocator, int index, char* data1_out, uint32_t data1_size, char* data2_out, uint32_t data2_size);
int xpl_allocator_rw_disown_backend(xpl_allocator_rw_t allocator, int index);
int xpl_allocator_rw_release_backends(xpl_allocator_rw_t allocator, int offset, int count);
int xpl_allocator_rw_grow_backend_count(xpl_allocator_rw_t allocator, int count);
int xpl_allocator_rw_get_backend_count(xpl_allocator_rw_t allocator);
int xpl_allocator_rw_destroy(xpl_allocator_rw_t* allocator);

#endif /* xpl_allocator_rw_h */

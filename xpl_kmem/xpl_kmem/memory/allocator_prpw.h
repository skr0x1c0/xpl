#ifndef xpl_allocator_prpw_h
#define xpl_allocator_prpw_h

#include <sys/types.h>
#include <arpa/inet.h>

/**
 * Allocator with partial read write capability
 * -- Data allocated as a socket address
 * -- Maximum length of data is UINT8_MAX
 * -- Only socket address family (2nd byte of allocated memory) can be read
 * -- For IPv4 socket addresses, memory after field `sin_addr` can be arbitary data
 * -- For IPv6 socket addresses, memory after field `sin6_addr` can be arbitary data
 * -- For other socket address family, memory starting from field `sa_data` can be arbitary data
 * -- Writing to memory can only be done once during allocation
 * -- Memory allocated from `KHEAP_KEXT`
 */

typedef struct xpl_allocator_prpw* xpl_allocator_prpw_t;
typedef _Bool(^xpl_allocator_prpw_data_filter)(void* ctx, sa_family_t address_family, size_t index);

xpl_allocator_prpw_t xpl_allocator_prpw_create(const struct sockaddr_in* addr, size_t num_allocs);
int xpl_allocator_prpw_allocate(xpl_allocator_prpw_t allocator, uint8_t address_len, sa_family_t address_family, const char* arbitary_data, int arbitary_data_len, int arbitary_data_offset);
size_t xpl_allocator_prpw_get_capacity(xpl_allocator_prpw_t allocator);
int xpl_allocator_prpw_filter(xpl_allocator_prpw_t allocator, size_t offset, size_t count, xpl_allocator_prpw_data_filter filter, void* filter_ctx, int64_t* found_idx_out);
void xpl_allocator_prpw_iter_backends(xpl_allocator_prpw_t allocator, void(^iterator)(int));
int xpl_allocator_prpw_release_containing_backend(xpl_allocator_prpw_t allocator, int64_t alloc_index);
int xpl_allocator_prpw_destroy(xpl_allocator_prpw_t* id);

#endif /* xpl_allocator_prpw_h */

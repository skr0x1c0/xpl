#ifndef kmem_allocator_prpw_h
#define kmem_allocator_prpw_h

#include <sys/types.h>
#include <arpa/inet.h>

/*
 * Allocator with partial read write capability
 * -- Data allocated as a socket address
 * -- Maximum length of data is UINT8_MAX
 * -- Only socket address family (2nd byte of allocated memory) can be read
 * -- For IPv4 socket addresses, memory after field `sin_addr` can be arbitary data
 * -- For IPv6 socket addresses, memory after field `sin6_addr` can be arbitary data
 * -- For other socket address family, memory after field `sa_data` can be arbitary data
 * -- Writing to memory can only be done once during allocation
 * -- Memory allocated from `KHEAP_KEXT`
 */

typedef struct kmem_allocator_prpw* kmem_allocator_prpw_t;
typedef _Bool(^kmem_allocator_prpw_data_filter)(void* ctx, sa_family_t address_family, size_t index);

kmem_allocator_prpw_t kmem_allocator_prpw_create(const struct sockaddr_in* addr, size_t num_allocs);
int kmem_allocator_prpw_allocate(kmem_allocator_prpw_t allocator, uint8_t address_len, sa_family_t address_family, const char* arbitary_data, int arbitary_data_len, int arbitary_data_offset);
size_t kmem_allocator_prpw_get_capacity(kmem_allocator_prpw_t allocator);
int kmem_allocator_prpw_filter(kmem_allocator_prpw_t allocator, size_t offset, size_t count, kmem_allocator_prpw_data_filter filter, void* filter_ctx, int64_t* found_idx_out);
void kmem_allocator_prpw_iter_backends(kmem_allocator_prpw_t allocator, void(^iterator)(int));
int kmem_allocator_prpw_release_containing_backend(kmem_allocator_prpw_t allocator, int64_t alloc_index);
int kmem_allocator_prpw_destroy(kmem_allocator_prpw_t* id);

#endif /* kmem_allocator_prpw_h */

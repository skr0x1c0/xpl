//
//  allocator_nrnw.h
//  xe_kmem
//
//  Created by admin on 2/22/22.
//

#ifndef allocator_nrnw_h
#define allocator_nrnw_h

#include <stdio.h>
#include <arpa/inet.h>

/// Value of sin_port member in allocated `struct sockaddr_in` entries
#define ALLOCATOR_NRNW_SOCK_ADDR_PORT 23478
/// Value of sin_family member in allocated `struct sockaddr_in` entries
#define ALLOCATOR_NRNW_SOCK_ADDR_FAMILY AF_INET

typedef struct xe_allocator_nrnw* xe_allocator_nrnw_t;

/// Create a new allocator for allocating elements on kext.* zones with size upto 256 bytes
/// Data in the allocated elements cannot be read or controlled
/// @param smb_addr Socket address of the xe_smbx server
xe_allocator_nrnw_t xe_allocator_nrnw_create(const struct sockaddr_in* smb_addr);

/// Allocate elements in kext.* zone. Allocated elements are of type `struct sockaddr_in` with sin_len = size
/// sin_port = ALLOCATOR_NRNW_SOCK_ADDR_PORT and sin_family = ALLOCATOR_NRNW_SOCK_ADDR_FAMILY
/// @param allocator allocator created using xe_allocator_nrnw_create
/// @param size size of allocated element (maximum size is 256 bytes)
/// @param num_allocs number of elements allocated
void xe_allocator_nrnw_allocate(xe_allocator_nrnw_t allocator, size_t size, size_t num_allocs);

/// Destory the allocator. All the elements allocated on kext.* zones will also be removed
/// when the allocator is destroyed
/// @param allocator_p allocator created using xe_allocator_nrnw_create
void xe_allocator_nrnw_destroy(xe_allocator_nrnw_t* allocator_p);

#endif /* allocator_nrnw_h */

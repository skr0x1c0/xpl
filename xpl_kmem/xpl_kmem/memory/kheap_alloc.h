
#ifndef xpl_zalloc_kext_small_h
#define xpl_zalloc_kext_small_h

#include <stdio.h>

#include "../smb/nic_allocator.h"

#include "allocator_prpw.h"
#include "oob_reader_base.h"


/**
 * --- Allocates memory (upto 256 bytes) from `KHEAP_KEXT`
 * --- Address of allocated memory will be returned
 */


struct xpl_kheap_alloc_entry {
    /// Address of allocated socket address
    uintptr_t address;
    /// `xpl_allocator_prpw` used for allocating the socket address
    xpl_allocator_prpw_t element_allocator;
};

/// Allocate the provided data on a zone in `KHEAP_KEXT` (determined by `alloc_size`)
/// @param smb_addr socket address of xpl_smbx server
/// @param alloc_size size of allocated socket address (`sa_len`)
/// @param sa_family address family of allocated socket address (`sa_family`)
/// @param data arbitary data to be written at offset `data_offset` from start of socket address
/// @param data_size size of arbitary data
/// @param data_offset offset of arbitary data (min offset for sa_family
///                   AF_INET is 8,  AF_INET6 is 24 and others is 2)
struct xpl_kheap_alloc_entry xpl_kheap_alloc(const struct sockaddr_in* smb_addr, uint8_t alloc_size, uint8_t sa_family, const char* data, uint8_t data_size, uint8_t data_offset);

#endif /* xpl_zalloc_kext_small_h */

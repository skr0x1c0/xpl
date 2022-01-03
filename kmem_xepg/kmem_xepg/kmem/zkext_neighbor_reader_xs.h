//
//  zkext_neighbor_reader_xs.h
//  kmem_xepg
//
//  Created by admin on 1/3/22.
//

#ifndef zkext_neighbor_reader_xs_h
#define zkext_neighbor_reader_xs_h

#include <stdio.h>

typedef struct kmem_zkext_neighbor_reader_xs* kmem_zkext_neighbor_reader_xs_t;

kmem_zkext_neighbor_reader_xs_t kmem_zkext_neighbor_reader_xs_create(const struct sockaddr_in* smb_addr);
void kmem_zkext_neighbor_reader_xs_read(kmem_zkext_neighbor_reader_xs_t reader, uint8_t saddr_snb_len, uint32_t saddr_ioc_len, uint8_t saddr_snb_name, uint8_t laddr_snb_len, uint32_t laddr_ioc_len, uint8_t laddr_snb_name, char* dst, uint32_t dst_size);
void kmem_zkext_neighbor_reader_xs_destroy(kmem_zkext_neighbor_reader_xs_t* reader_p);

#endif /* zkext_neighbor_reader_xs_h */

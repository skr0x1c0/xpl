//
//  zkext_neighbor_reader_xs.h
//  kmem_xepg
//
//  Created by admin on 1/3/22.
//

#ifndef zkext_neighbor_reader_xs_h
#define zkext_neighbor_reader_xs_h

#include <stdio.h>

typedef struct kmem_neighbor_reader* kmem_neighbor_reader_t;

kmem_neighbor_reader_t kmem_neighbor_reader_create(const struct sockaddr_in* smb_addr);
void kmem_neighbor_reader_read(kmem_neighbor_reader_t reader, uint8_t saddr_snb_len, uint32_t saddr_ioc_len, uint8_t saddr_snb_name, uint8_t laddr_snb_len, uint32_t laddr_ioc_len, uint8_t laddr_snb_name, char* dst, uint32_t dst_size);
void kmem_neighbor_reader_destroy(kmem_neighbor_reader_t* reader_p);

#endif /* zkext_neighbor_reader_xs_h */

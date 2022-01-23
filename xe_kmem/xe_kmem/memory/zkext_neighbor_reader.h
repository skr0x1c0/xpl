//
//  neighbor_reader.h
//  kmem_xepg
//
//  Created by admin on 12/20/21.
//

#ifndef neighbor_reader_h
#define neighbor_reader_h

#include <stdio.h>

typedef struct kmem_zkext_neighbor_reader* kmem_zkext_neighour_reader_t;

kmem_zkext_neighour_reader_t kmem_zkext_neighbor_reader_create(const struct sockaddr_in* smb_addr);
int kmem_zkext_neighbor_reader_read(kmem_zkext_neighour_reader_t reader, uint8_t zone_size, char* data, size_t data_size);
void kmem_zkext_neighbor_reader_attempt_cleanup(kmem_zkext_neighour_reader_t reader);
void kmem_zkext_neighbor_reader_reset(kmem_zkext_neighour_reader_t reader);
void kmem_zkext_neighbor_reader_destroy(kmem_zkext_neighour_reader_t* reader_p);

#endif /* neighbor_reader_h */

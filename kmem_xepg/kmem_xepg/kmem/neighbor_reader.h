//
//  neighbor_reader.h
//  kmem_xepg
//
//  Created by admin on 12/20/21.
//

#ifndef neighbor_reader_h
#define neighbor_reader_h

#include <stdio.h>

typedef struct kmem_neighbor_reader* kmem_neighour_reader_t;

kmem_neighour_reader_t kmem_neighbor_reader_create(void);

void kmem_neighbor_reader_prepare(kmem_neighour_reader_t reader, char* data, size_t data_size);
int kmem_neighbor_reader_read_modified(kmem_neighour_reader_t reader, struct socket_fdinfo* infos, size_t* count);
int kmem_neighbor_reader_prepare_read_modified(kmem_neighour_reader_t reader, char* out, uint8_t size);

void kmem_neighbor_reader_reset(kmem_neighour_reader_t reader);
void kmem_neighbor_reader_destroy(kmem_neighour_reader_t* reader_p);

#endif /* neighbor_reader_h */

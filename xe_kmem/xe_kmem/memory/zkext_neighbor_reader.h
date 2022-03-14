//
//  neighbor_reader.h
//  kmem_xepg
//
//  Created by admin on 12/20/21.
//

#ifndef neighbor_reader_h
#define neighbor_reader_h

#include <stdio.h>

int kmem_zkext_neighbor_reader_read(const struct sockaddr_in* smb_addr, uint8_t zone_size, char* data, size_t data_size);

#endif /* neighbor_reader_h */

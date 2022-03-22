//
//  neighbor_reader.h
//  kmem_xepg
//
//  Created by admin on 12/20/21.
//

#ifndef neighbor_reader_h
#define neighbor_reader_h

#include <stdio.h>

/// Perform safe OOB read from zones in `KHEAP_DEFAULT` with element size greater than 32
/// @param smb_addr socket address of xe_smbx server
/// @param zone_size element size of zone from which OOB read is to be performed
/// @param data buffer to store OOB read data
/// @param data_size size of `data` buffer
int oob_reader_ovf_read(const struct sockaddr_in* smb_addr, uint8_t zone_size, char* data, size_t data_size);

#endif /* neighbor_reader_h */

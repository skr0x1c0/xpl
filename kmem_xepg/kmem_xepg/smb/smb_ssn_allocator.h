#ifndef smb_ssn_allocator_h
#define smb_ssn_allocator_h

#include <stdio.h>

typedef int smb_ssn_allocator;

int smb_ssn_allocator_create(const struct sockaddr_in* addr, uint32_t ioc_saddr_len, smb_ssn_allocator* id_out);
int smb_ssn_allocator_allocate(smb_ssn_allocator id, const char* data1, uint32_t data1_size, const char* data2, uint32_t data2_size, uint32_t user_size, uint32_t password_size, uint32_t domain_size);
int smb_ssn_allocator_read(smb_ssn_allocator id, char* data1_out, uint32_t data1_size, char* data2_out, uint32_t data2_size);
int smb_ssn_allocator_destroy(smb_ssn_allocator* id);

#endif /* smb_ssn_allocator_h */

//
//  zone_prime.h
//  kmem_xepg
//
//  Created by admin on 12/27/21.
//

#ifndef zone_prime_util_h
#define zone_prime_util_h

#include <stdio.h>
#include <arpa/inet.h>

typedef struct kmem_zkext_prime_util* kmem_zkext_prime_util_t;

kmem_zkext_prime_util_t kmem_zkext_prime_util_create(const struct sockaddr_in* smb_addr);
void kmem_zkext_prime_util_prime(kmem_zkext_prime_util_t util, uint num_elements, uint z_elem_size);
void kmem_zkext_prime_util_destroy(kmem_zkext_prime_util_t* util_p);

#endif /* zone_prime_util_h */

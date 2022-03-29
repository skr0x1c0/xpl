//
//  util_ptrauth.h
//  xe
//
//  Created by admin on 11/25/21.
//

#ifndef xpl_util_ptrauth_h
#define xpl_util_ptrauth_h

#define xpl_PTRAUTH_MASK 0xfffff00000000000
#define xpl_ptrauth_strip(addr) ((addr == 0) ? 0 : (addr | xpl_PTRAUTH_MASK))
#define xpl_ptrauth_blend_discriminator_with_address(desc, addr) (((addr) & ((1ULL << 48) - 1)) | (((desc) & ((1ULL << 16) - 1)) << 48))

#endif /* xpl_util_ptrauth_h */

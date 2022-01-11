//
//  util_ptrauth.h
//  xe
//
//  Created by admin on 11/25/21.
//

#ifndef util_ptrauth_h
#define util_ptrauth_h

#define XE_PTRAUTH_MASK 0xfffff00000000000
#define XE_PTRAUTH_STRIP(addr) ((addr == 0) ? 0 : (addr | XE_PTRAUTH_MASK))
#define XE_PTRAUTH_BLEND_DISCRIMINATOR_WITH_ADDRESS(desc, addr) (((addr) & ((1ULL << 48) - 1)) | (((desc) & ((1ULL << 16) - 1)) << 48))

#endif /* util_ptrauth_h */

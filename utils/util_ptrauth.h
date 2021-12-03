//
//  util_ptrauth.h
//  xe
//
//  Created by admin on 11/25/21.
//

#ifndef util_ptrauth_h
#define util_ptrauth_h

#define XE_PTRAUTH_MASK 0xfffff00000000000
#define XE_UTIL_PTRAUTH_STRIP(addr) ((addr == 0) ? 0 : (addr | XE_PTRAUTH_MASK))

#endif /* util_ptrauth_h */

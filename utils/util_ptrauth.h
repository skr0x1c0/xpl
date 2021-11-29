//
//  util_ptrauth.h
//  xe
//
//  Created by admin on 11/25/21.
//

#ifndef util_ptrauth_h
#define util_ptrauth_h

#define UTIL_PTRAUTH_STRIP(addr) ((addr == 0) ? 0 : (addr | 0xfffff00000000000))

#endif /* util_ptrauth_h */

//
//  util_pacda.h
//  xe
//
//  Created by admin on 12/3/21.
//

#ifndef xpl_pacda_h
#define xpl_pacda_h

#include <stdio.h>

/// Signs the value `ptr` with modifier `ctx` using PACDA and returns the signed value
/// @param ptr value to be signed
/// @param ctx modifier to be used while signing
uintptr_t xpl_pacda_sign(uintptr_t ptr, uint64_t ctx);

/// Signs the value `ptr` with modifier constructed from `ctx` and `descriminator` using PACDA
/// @param ptr value to be signed
/// @param ctx 0 to 47 bits of modifier
/// @param descriminator 48 to 63 bits of modifier
uintptr_t xpl_pacda_sign_with_descriminator(uintptr_t ptr, uintptr_t ctx, uint16_t descriminator);

#endif /* xpl_pacda_h */

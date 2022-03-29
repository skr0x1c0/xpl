//
//  util_pacda.h
//  xe
//
//  Created by admin on 12/3/21.
//

#ifndef xpl_util_pacda_h
#define xpl_util_pacda_h

#include <stdio.h>

uintptr_t xpl_util_pacda_sign(uintptr_t ptr, uint64_t ctx);
uintptr_t xpl_util_pacda_sign_with_descriminator(uintptr_t ptr, uintptr_t ctx, uint16_t descriminator);

#endif /* xpl_util_pacda_h */

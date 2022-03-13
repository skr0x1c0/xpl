//
//  platform_params.h
//  
//
//  Created by admin on 1/4/22.
//

#ifndef macos_params_h
#define macos_params_h

#define IDENT(x) x
#define XSTR(x) #x
#define STR(x) XSTR(x)
#define PATH(version, variant, file) STR(IDENT(version)IDENT(_)IDENT(variant)IDENT(/)IDENT(file))

#define MACOS_VERSION v21E5212f
#define MACOS_KERNEL_VARIANT T6000

#include PATH(MACOS_VERSION, MACOS_KERNEL_VARIANT, functions.h)
#include PATH(MACOS_VERSION, MACOS_KERNEL_VARIANT, variables.h)
#include PATH(MACOS_VERSION, MACOS_KERNEL_VARIANT, image.h)
#include PATH(MACOS_VERSION, MACOS_KERNEL_VARIANT, types.h)
#include PATH(MACOS_VERSION, MACOS_KERNEL_VARIANT, constants.h)

#endif /* macos_params_h */

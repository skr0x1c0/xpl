//
//  platform_params.h
//  
//
//  Created by admin on 1/4/22.
//

#ifndef macos_params_h
#define macos_params_h

#define _MACOS_XSTR(x) #x
#define _MACOS_STR(x) _MACOS_XSTR(x)

#define MACOS_v21E230
#define KERNEL_T6000

#if defined(MACOS_v21E230)
#define MACOS_VERSION v21E230
#else
#error unknown macos version
#endif

#if defined(KERNEL_T6000)
#define MACOS_KERNEL_VARIANT T6000
#elif defined(KERNEL_T8101)
#define MACOS_KERNEL_VARIANT T8101
#else
#error unknown kernel variant
#endif

#define _MACOS_KERNEL_INCLUDES_DIR kernel/MACOS_VERSION/MACOS_KERNEL_VARIANT

#include _MACOS_STR(_MACOS_KERNEL_INCLUDES_DIR/functions.h)
#include _MACOS_STR(_MACOS_KERNEL_INCLUDES_DIR/variables.h)
#include _MACOS_STR(_MACOS_KERNEL_INCLUDES_DIR/image.h)
#include _MACOS_STR(_MACOS_KERNEL_INCLUDES_DIR/types.h)
#include _MACOS_STR(_MACOS_KERNEL_INCLUDES_DIR/constants.h)

#endif /* macos_params_h */

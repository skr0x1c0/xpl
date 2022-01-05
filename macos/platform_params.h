//
//  platform_params.h
//  
//
//  Created by admin on 1/4/22.
//

#ifndef platform_params_h
#define platform_params_h

#define MACOS_21A559

#if defined(MACOS_21A559)
#include "21A559/functions.h"
#include "21A559/variables.h"
#include "21A559/image.h"
#include "21A559/types.h"
#include "21A559/constants.h"
#elif defined(MACOS_21AXXX)
#include "21AXXX/functions.h"
#include "21AXXX/variables.h"
#include "21AXXX/image.h"
#include "21AXXX/types.h"
#include "21AXXX/constants.h"
#else
#error "Unsupported platform"
#endif


#endif /* platform_params_h */

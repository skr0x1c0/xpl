//
//  platform_params.h
//  
//
//  Created by admin on 1/4/22.
//

#ifndef macos_params_h
#define macos_params_h

#define MACOS_21E5212f

#if defined(MACOS_21A559)
#include "21A559/functions.h"
#include "21A559/variables.h"
#include "21A559/image.h"
#include "21A559/types.h"
#include "21A559/constants.h"
#elif defined(MACOS_21C52)
#include "21C52/functions.h"
#include "21C52/variables.h"
#include "21C52/image.h"
#include "21C52/types.h"
#include "21C52/constants.h"
#elif defined(MACOS_21D49)
#include "21D49/functions.h"
#include "21D49/variables.h"
#include "21D49/image.h"
#include "21D49/types.h"
#include "21D49/constants.h"
#elif defined(MACOS_21E5212f)
#include "21E5212f/functions.h"
#include "21E5212f/variables.h"
#include "21E5212f/image.h"
#include "21E5212f/types.h"
#include "21E5212f/constants.h"
#else
#error "Unsupported platform"
#endif


#endif /* macos_params_h */

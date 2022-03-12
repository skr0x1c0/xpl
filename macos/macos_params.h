//
//  platform_params.h
//  
//
//  Created by admin on 1/4/22.
//

#ifndef macos_params_h
#define macos_params_h

#define MACOS_21E230_T8101

#if defined(MACOS_21E5212f_T6000)
#include "21E5212f_T6000/functions.h"
#include "21E5212f_T6000/variables.h"
#include "21E5212f_T6000/image.h"
#include "21E5212f_T6000/types.h"
#include "21E5212f_T6000/constants.h"
#elif defined(MACOS_21E230_T8101)
#include "21E230_T8101/functions.h"
#include "21E230_T8101/variables.h"
#include "21E230_T8101/image.h"
#include "21E230_T8101/types.h"
#include "21E230_T8101/constants.h"
#else
#error "Unsupported platform"
#endif


#endif /* macos_params_h */

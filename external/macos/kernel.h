
#ifndef macos_params_h
#define macos_params_h

#if !defined(MACOS_21E230_T6000_RELEASE)
#if !defined(MACOS_21E230_T8101_RELEASE)
/// Use default build configuration from `xpl/env.h` in none provided
#include "../../env.h"
#endif /* MACOS_21E230_T6000_RELEASE */
#endif /* MACOS_21E230_T8101_RELEASE */

#if MACOS_21E230_T6000_RELEASE

#define MACOS_OSVERSION              "21E230"
#define MACOS_OSVERSION_21E230       1
#define MACOS_KERNEL_MACHINE         "T6000"
#define MACOS_KERNEL_MACHINE_T6000   1
#define MACOS_KERNEL_VARIANT         "RELEASE"
#define MACOS_KERNEL_VARIANT_RELEASE 1
#define MACOS_KERNEL_UUID            "178AA913-FCA9-33FD-A81C-DF08315F458D"

#include "kernel/21E230/T6000/functions.h"
#include "kernel/21E230/T6000/variables.h"
#include "kernel/21E230/T6000/image.h"
#include "kernel/21E230/T6000/types.h"
#include "kernel/21E230/T6000/constants.h"

#elif MACOS_21E230_T8101_RELEASE

#define MACOS_OSVERSION              "21E230"
#define MACOS_OSVERSION_21E230       1
#define MACOS_KERNEL_MACHINE         "T8101"
#define MACOS_KERNEL_MACHINE_T8101   1
#define MACOS_KERNEL_VARIANT         "RELEASE"
#define MACOS_KERNEL_VARIANT_RELEASE 1
#define MACOS_KERNEL_UUID            "F027EB2E-26A7-3A4E-87FA-6A745379530C"

#include "kernel/21E230/T8101/functions.h"
#include "kernel/21E230/T8101/variables.h"
#include "kernel/21E230/T8101/image.h"
#include "kernel/21E230/T8101/types.h"
#include "kernel/21E230/T8101/constants.h"

#else
#error valid configuration not selected
#endif

#endif /* macos_params_h */

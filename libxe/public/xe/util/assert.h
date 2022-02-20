//
//  util_assert.h
//  libxe
//
//  Created by admin on 1/2/22.
//

#ifndef xe_util_assert_h
#define xe_util_assert_h

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "macos_params.h"

#define xe_assert(val) if (!(val)) { printf("[ERROR] assertion %s failed at %s:%d\n", #val, __FILE__, __LINE__); getpass("press enter to exit\n"); abort(); }
#define xe_assert_err(val) if ((val) != 0) { printf("[ERROR] error check failed at %s:%d, err: %s(%d)\n", __FILE__, __LINE__, strerror(val), val); getpass("press enter to exit\n"); abort(); }
#define xe_assert_errno(res) if ((res) != 0) { printf("[ERROR] error check failed at %s:%d, err: %s(%d)\n", __FILE__, __LINE__, strerror(errno), errno); getpass("press enter to exit\n"); abort(); }
#define xe_assert_cond(lhs, cond, rhs) if (!((lhs) cond (rhs))) { printf("[ERROR] assert condition %llu %s %llu failed at %s::%d\n", (uint64_t)(lhs), #cond, (uint64_t)(rhs), __FILE__, __LINE__); getpass("press enter to continue\n"); abort(); }
#define xe_assert_kaddr(val) if (!xe_vm_kernel_address_valid((val))) { printf("[ERROR] assertion for valid kernel address failed for %p at %s:%d\n", (void*)val, __FILE__, __LINE__); getpass("press enter to continue"); abort(); }

#endif /* xe_util_assert_h */

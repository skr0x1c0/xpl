//
//  util_assert.h
//  libxe
//
//  Created by admin on 1/2/22.
//

#ifndef xpl_util_assert_h
#define xpl_util_assert_h

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <macos/kernel.h>

#include "./log.h"


#if defined(DEBUG)
#define xpl_abort() xpl_log_print_backtrace(); getpass("press enter to abort. this may cause kernel panic\n"); abort()
#else
#define xpl_abort() getpass("press enter to abort. this may cause kernel panic\n"); abort()
#endif

#define xpl_assert(val) if (!(val)) { printf("[ERROR] assertion %s failed at %s:%d\n", #val, __FILE__, __LINE__); xpl_abort(); }
#define xpl_assert_err(val) if ((val) != 0) { printf("[ERROR] error check failed at %s:%d, err: %s(%d)\n", __FILE__, __LINE__, strerror(val), val); xpl_abort(); }
#define xpl_assert_errno(res) if ((res) != 0) { printf("[ERROR] error check failed at %s:%d, err: %s(%d)\n", __FILE__, __LINE__, strerror(errno), errno); xpl_abort(); }
#define xpl_assert_cond(lhs, cond, rhs) if (!((lhs) cond (rhs))) { printf("[ERROR] assert condition %llu %s %llu failed at %s::%d\n", (uint64_t)(lhs), #cond, (uint64_t)(rhs), __FILE__, __LINE__); xpl_abort(); }
#define xpl_assert_kaddr(val) if (!xpl_vm_kernel_address_valid((val))) { printf("[ERROR] assertion for valid kernel address failed for %p at %s:%d\n", (void*)val, __FILE__, __LINE__); xpl_abort(); }

#endif /* xpl_util_assert_h */

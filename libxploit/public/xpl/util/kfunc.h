//
//  kfunc.h
//  libxe
//
//  Created by admin on 2/16/22.
//

#ifndef kfunc_h
#define kfunc_h

#include <stdio.h>

#include <macos/kernel/xnu/osfmk/mach/arm/thread_status.h>

struct xpl_util_kfunc_register_state {
    uint64_t x19, x20, x21, x22;
    uint64_t x23, x24, x25, x26;
    uint64_t fp, sp, lr;
};

struct xpl_util_kfunc_args {
    uint64_t  x[29];
    uint64_t  fp;
    uint64_t  lr;
    uint64_t  sp;
    uint128_t q[31];
};

typedef struct xpl_util_kfunc* xpl_util_kfunc_t;

xpl_util_kfunc_t xpl_util_kfunc_create(uint free_zone_idx);

struct xpl_util_kfunc_register_state xpl_util_kfunc_pre_execute(xpl_util_kfunc_t util);
void xpl_util_kfunc_execute(xpl_util_kfunc_t util, uintptr_t target_func, const struct xpl_util_kfunc_args* args);

void xpl_util_kfunc_execute_simple(xpl_util_kfunc_t util, uintptr_t target_func, uint64_t args[8]);
void xpl_util_kfunc_destroy(xpl_util_kfunc_t* util_p);

#endif /* kfunc_h */

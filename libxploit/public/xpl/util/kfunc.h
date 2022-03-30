//
//  kfunc.h
//  libxe
//
//  Created by admin on 2/16/22.
//

#ifndef xpl_kfunc_h
#define xpl_kfunc_h

#include <stdio.h>

#include <macos/kernel/xnu/osfmk/mach/arm/thread_status.h>

struct xpl_kfunc_register_state {
    uint64_t x19, x20, x21, x22;
    uint64_t x23, x24, x25, x26;
    uint64_t fp, sp, lr;
};

struct xpl_kfunc_args {
    uint64_t  x[29];
    uint64_t  fp;
    uint64_t  lr;
    uint64_t  sp;
    uint128_t q[31];
};

typedef struct xpl_kfunc* xpl_kfunc_t;

/// Create `xpl_kfunc_t` utility for executing arbitary kernel functions
/// @param free_zone_idx zone index of a free zone which will be used for allocating
///        small block descriptor.
xpl_kfunc_t xpl_kfunc_create(uint free_zone_idx);

/// Prepares `xpl_kfunc_t` utility for executing an arbitary kernel function. Returns the
/// values for registers that must restored when the arbitary kernel function returns. Call to
/// this method must be accompanied by `xpl_kfunc_execute` method call
/// @param util `xpl_kfunc_t` created using `xpl_kfunc_create`
struct xpl_kfunc_register_state xpl_kfunc_pre_execute(xpl_kfunc_t util);

/// Execute the arbitary kernel function. This method must be called after calling `xpl_kfunc_pre_execute`
/// @param util `xpl_kfunc_t` created using `xpl_kfunc_create`
/// @param target_func address of kernel function to be called
/// @param args values of registers x0 - x30, sp and q0 - q31 with which `target_func` must be called
void xpl_kfunc_execute(xpl_kfunc_t util, uintptr_t target_func, const struct xpl_kfunc_args* args);

/// Simplified API for executing arbitary kernel functions with upto 8 parameters. Do not call
/// `xpl_kfunc_pre_execute` before calling this method
/// @param util `xpl_kfunc_t` created using `xpl_kfunc_create`
/// @param target_func address of kernel function to be called
void xpl_kfunc_execute_simple(xpl_kfunc_t util, uintptr_t target_func, uint64_t args[8]);

/// Releases resources
/// @param util_p pointer to `xpl_kfunc_t` created using `xpl_kfunc_create`
void xpl_kfunc_destroy(xpl_kfunc_t* util_p);

#endif /* xpl_kfunc_h */

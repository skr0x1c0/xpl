//
//  sandbox.h
//  playground
//
//  Created by sreejith on 3/8/22.
//

#ifndef xpl_util_sandbox_h
#define xpl_util_sandbox_h

#include <stdio.h>

typedef struct xpl_util_sandbox* xpl_util_sandbox_t;

xpl_util_sandbox_t xpl_util_sandbox_create(void);
void xpl_util_sandbox_disable_fs_restrictions(xpl_util_sandbox_t util);
void xpl_util_sandbox_disable_signal_check(xpl_util_sandbox_t util);
void xpl_util_sandbox_destroy(xpl_util_sandbox_t* util_p);

#endif /* xpl_util_sandbox_h */

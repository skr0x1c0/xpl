//
//  sandbox.h
//  playground
//
//  Created by sreejith on 3/8/22.
//

#ifndef xe_util_sandbox_h
#define xe_util_sandbox_h

#include <stdio.h>

typedef struct xe_util_sandbox* xe_util_sandbox_t;

xe_util_sandbox_t xe_util_sandbox_create(void);
void xe_util_sandbox_disable_fs_restrictions(xe_util_sandbox_t util);
void xe_util_sandbox_destroy(xe_util_sandbox_t* util_p);

#endif /* xe_util_sandbox_h */

//
//  suid.h
//  libxe
//
//  Created by sreejith on 3/10/22.
//

#ifndef suid_h
#define suid_h

#include <stdio.h>

typedef struct xe_util_sudo* xe_util_sudo_t;

xe_util_sudo_t xe_util_sudo_create(void);
int xe_util_sudo_run(xe_util_sudo_t util, const char* cmd, const char* argv[], size_t argc);
void xe_util_sudo_destroy(xe_util_sudo_t* util_p);

#endif /* suid_h */

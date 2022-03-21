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

/// Create a new `xe_util_sudo_t` which can be used to execute commands with root privilege
xe_util_sudo_t xe_util_sudo_create(void);

/// Run a binary with root privilege
/// @param util `xe_util_sudo_t` created using `xe_util_sudo_create`
/// @param cmd path to binary
/// @param argv arguments to be passed to the binary
/// @param argc number of arguments in argv
int xe_util_sudo_run(xe_util_sudo_t util, const char* cmd, const char* argv[], size_t argc);

/// Release resource
/// @param util_p pointer to `xe_util_sudo_t` created using `xe_util_sudo_create`
void xe_util_sudo_destroy(xe_util_sudo_t* util_p);

#endif /* suid_h */

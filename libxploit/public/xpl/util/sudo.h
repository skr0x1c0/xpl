//
//  suid.h
//  libxe
//
//  Created by sreejith on 3/10/22.
//

#ifndef suid_h
#define suid_h

#include <stdio.h>

typedef struct xpl_util_sudo* xpl_util_sudo_t;

/// Create a new `xpl_util_sudo_t` which can be used to execute commands with root privilege
xpl_util_sudo_t xpl_util_sudo_create(void);

/// Run a binary with root privilege
/// @param util `xpl_util_sudo_t` created using `xpl_util_sudo_create`
/// @param cmd path to binary
/// @param argv arguments to be passed to the binary
/// @param argc number of arguments in argv
int xpl_util_sudo_run(xpl_util_sudo_t util, const char* cmd, const char* argv[], size_t argc);

/// Release resource
/// @param util_p pointer to `xpl_util_sudo_t` created using `xpl_util_sudo_create`
void xpl_util_sudo_destroy(xpl_util_sudo_t* util_p);

#endif /* suid_h */

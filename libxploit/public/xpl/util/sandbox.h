
#ifndef xpl_sandbox_h
#define xpl_sandbox_h

#include <stdio.h>

typedef struct xpl_sandbox* xpl_sandbox_t;

/// Creates `xpl_sandbox_t` utility for modifying `Sandbox.kext` mac_policy_conf
xpl_sandbox_t xpl_sandbox_create(void);

/// Disables all file system restrictions imposed by `Sandbox.kext`
/// @param util `xpl_sandbox_t` created using `xpl_sandbox_create`
void xpl_sandbox_disable_fs_restrictions(xpl_sandbox_t util);

/// Disables `mac_proc_check_signal` of `Sandbox.kext`
/// @param util `xpl_sandbox_t` created using `xpl_sandbox_create`
void xpl_sandbox_disable_signal_check(xpl_sandbox_t util);

/// Restores the original `Sandbox.kext` mac_policy_conf
/// @param util_p pointer to `xpl_sandbox_t` created using `xpl_sandbox_create`
void xpl_sandbox_destroy(xpl_sandbox_t* util_p);

#endif /* xpl_sandbox_h */

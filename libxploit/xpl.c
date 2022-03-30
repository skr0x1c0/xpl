
#include <stdio.h>
#include <limits.h>

#include <sys/sysctl.h>
#include <macos/kernel.h>
#include <CoreFoundation/CoreFoundation.h>

#include "util/assert.h"
#include "util/msdosfs.h"


void xpl_dump_build_configuration(void) {
    xpl_log_debug("build configuration: \n===");
    xpl_log_debug("osversion: %s", MACOS_OSVERSION);
    xpl_log_debug("machine config: %s", MACOS_KERNEL_MACHINE);
    xpl_log_debug("kernel variant: %s", MACOS_KERNEL_VARIANT);
    xpl_log_debug("kernel uuid: %s", MACOS_KERNEL_UUID);
}


void xpl_verify_kernel_uuid(void) {
    char buffer[48];
    size_t buffer_size = sizeof(buffer);
    int res = sysctlbyname("kern.uuid", buffer, &buffer_size, NULL, 0);
    xpl_assert_errno(res);
    
    if (strncmp(buffer, MACOS_KERNEL_UUID, sizeof(buffer)) != 0) {
        xpl_log_error("kern.uuid does not match with MACOS_KERNEL_UUID used during build time. if you are building from within xcode, check build configuration selected in \"xpl/env.h\". if you are using build.sh script, make sure correct build configuration was used");
        exit(1);
    }
}


void xpl_init(void) {
    xpl_dump_build_configuration();
    xpl_verify_kernel_uuid();
}

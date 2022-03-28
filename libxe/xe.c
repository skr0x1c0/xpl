//
//  xe.c
//  libxe
//
//  Created by sreejith on 3/22/22.
//

#include <stdio.h>
#include <limits.h>

#include <sys/sysctl.h>
#include <macos/kernel.h>
#include <CoreFoundation/CoreFoundation.h>

#include "util/assert.h"
#include "util/msdosfs.h"


void xe_dump_build_configuration(void) {
    xe_log_debug("build configuration: \n===");
    xe_log_debug("osversion: %s", MACOS_OSVERSION);
    xe_log_debug("machine config: %s", MACOS_KERNEL_MACHINE);
    xe_log_debug("kernel variant: %s", MACOS_KERNEL_VARIANT);
    xe_log_debug("kernel uuid: %s", MACOS_KERNEL_UUID);
}


void xe_verify_kernel_uuid(void) {
    char buffer[48];
    size_t buffer_size = sizeof(buffer);
    int res = sysctlbyname("kern.uuid", buffer, &buffer_size, NULL, 0);
    xe_assert_errno(res);
    
    if (strncmp(buffer, MACOS_KERNEL_UUID, sizeof(buffer)) != 0) {
        xe_log_error("kern.uuid does not match with MACOS_KERNEL_UUID used during build time. if you are building from within xcode, check build configuration selected in \"xe/env.h\". if you are using build.sh script, make sure correct build configuration was used");
        exit(1);
    }
}


void xe_init(void) {
    xe_dump_build_configuration();
    xe_verify_kernel_uuid();
}

//
//  test_sudo.c
//  test
//
//  Created by sreejith on 3/10/22.
//

#include <limits.h>
#include <spawn.h>
#include <pwd.h>

#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <xe/memory/kmem.h>
#include <xe/util/msdosfs.h>
#include <xe/util/sudo.h>
#include <xe/util/log.h>
#include <xe/util/assert.h>
#include <xe/util/misc.h>

#include "test_sudo.h"


void test_sudo(void) {
    if (getuid() == 0) {
        xe_log_warn("skipping test_sudo since test is run with root privileges");
        return;
    }
        
    xe_util_msdosfs_loadkext();
    xe_util_sudo_t util = xe_util_sudo_create();
    
    char test_directory[] = "/tmp/test_sudo_XXXXXX";
    xe_assert(mkdtemp(test_directory) != NULL);
    
    char test_file[PATH_MAX];
    snprintf(test_file, sizeof(test_file), "%s/test_file", test_directory);
    
    int fd = open(test_file, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    xe_assert_errno(fd < 0);
    write(fd, "test", sizeof("test"));
    
    const char* argv[] = {
        "root:admin",
        test_file
    };
    int result = xe_util_sudo_run(util, "/usr/sbin/chown", argv, xe_array_size(argv));
    xe_assert_cond(result, ==, 0);

    struct stat test_file_stat;
    int res = stat(test_file, &test_file_stat);
    xe_assert_errno(res);
    xe_assert_cond(test_file_stat.st_uid, ==, 0);

    xe_util_sudo_destroy(&util);
    unlink(test_file);
    rmdir(test_directory);
}

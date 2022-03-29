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

#include <xpl/memory/kmem.h>
#include <xpl/util/msdosfs.h>
#include <xpl/util/sudo.h>
#include <xpl/util/log.h>
#include <xpl/util/assert.h>
#include <xpl/util/misc.h>

#include "test_sudo.h"


void test_sudo(void) {
    if (getuid() == 0) {
        xpl_log_warn("skipping test_sudo since test is run with root privileges");
        return;
    }
        
    xpl_util_msdosfs_loadkext();
    xpl_util_sudo_t util = xpl_util_sudo_create();
    
    char test_directory[] = "/tmp/test_sudo_XXXXXX";
    xpl_assert(mkdtemp(test_directory) != NULL);
    
    char test_file[PATH_MAX];
    snprintf(test_file, sizeof(test_file), "%s/test_file", test_directory);
    
    int fd = open(test_file, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    xpl_assert_errno(fd < 0);
    write(fd, "test", sizeof("test"));
    
    const char* argv[] = {
        "root:admin",
        test_file
    };
    int result = xpl_util_sudo_run(util, "/usr/sbin/chown", argv, xpl_array_size(argv));
    xpl_assert_cond(result, ==, 0);

    struct stat test_file_stat;
    int res = stat(test_file, &test_file_stat);
    xpl_assert_errno(res);
    xpl_assert_cond(test_file_stat.st_uid, ==, 0);

    xpl_util_sudo_destroy(&util);
    unlink(test_file);
    rmdir(test_directory);
}

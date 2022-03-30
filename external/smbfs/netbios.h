/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2010 Apple Inc. All rights reserved.
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
#ifndef EXTERNAL_SMBFS_NETSMB_NETBIOS_H_
#define EXTERNAL_SMBFS_NETSMB_NETBIOS_H_

#define    NB_NAMELEN       16
#define    NB_ENCNAMELEN    NB_NAMELEN * 2

#include <sys/types.h>
#include <netinet/in.h>

/*
 * Session packet types
 */
#define    NB_SSN_MESSAGE        0x0
#define    NB_SSN_REQUEST        0x81
#define    NB_SSN_POSRESP        0x82
#define    NB_SSN_NEGRESP        0x83
#define    NB_SSN_RTGRESP        0x84
#define    NB_SSN_KEEPALIVE      0x85

/*
 * Socket address
 */
struct sockaddr_nb {
    u_char                snb_len;
    u_char                snb_family;
    struct sockaddr_in    snb_addrin;
    u_char                snb_name[1 + NB_ENCNAMELEN + 1];    /* encoded */
};

#endif /* EXTERNAL_SMBFS_NETSMB_NETBIOS_H_ */

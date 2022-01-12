/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2015 Apple Inc. All rights reserved.
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
#ifndef _SMB_CONN_H_
#define _SMB_CONN_H_

#include <bsm/audit.h>
#include <sys/queue.h>

#include "kern_types.h"

#define kClientIfBlacklistMaxLen 32 /* max len of client interface blacklist */

/*
 * network IO daemon states
 */
enum smbiod_state {
    SMBIOD_ST_NOTCONN,        /* no connect request was made */
    SMBIOD_ST_CONNECT,        /* a connect attempt is in progress */
    SMBIOD_ST_TRANACTIVE,     /* TCP transport level is connected */
    SMBIOD_ST_NEGOACTIVE,     /* completed negotiation */
    SMBIOD_ST_SSNSETUP,       /* started (a) session setup */
    SMBIOD_ST_SESSION_ACTIVE, /* SMB session established */
    SMBIOD_ST_DEAD,           /* connection broken, transport is down */
    SMBIOD_ST_RECONNECT_AGAIN,/* We need to attempt to reconnect again */
    SMBIOD_ST_RECONNECT       /* Currently in reconnect */
};

struct smb_rqhead {
    void* next;
    void** prev;
};

typedef enum gssd_nametype {
    GSSD_STRING_NAME = 0,
    GSSD_EXPORT,
    GSSD_ANONYMOUS,
    GSSD_HOSTBASED,
    GSSD_USER,
    GSSD_MACHINE_UID,
    GSSD_STRING_UID,
    GSSD_KRB5_PRINCIPAL,
    GSSD_KRB5_REFERRAL,
    GSSD_NTLM_PRINCIPAL,
    GSSD_NTLM_BLOB,
    GSSD_UUID
} gssd_nametype;

#define SMB3_KEY_LEN 16
#define SHA512_DIGEST_LENGTH 64

struct smb_gss {
    au_asid_t     gss_asid;        /* Audit session id to find gss_mp */
    gssd_nametype gss_target_nt;   /* Service's principal's name type */
    uint32_t      gss_spn_len;     /* Service's principal's length */
    uint8_t *     gss_spn;         /* Service's principal name */
    gssd_nametype gss_client_nt;   /* Client's principal's name type */
    uint32_t      gss_cpn_len;     /* Client's principal's length */
    uint8_t *     gss_cpn;         /* Client's principal name */
    char *        gss_cpn_display; /* String representation of client principal */
    uint32_t      gss_tokenlen;    /* Gss token length */
    uint8_t *     gss_token;       /* Gss token */
    uint64_t      gss_ctx;         /* GSS opaque context handle */
    uint64_t      gss_cred;        /* GSS opaque cred handle */
    uint32_t      gss_rflags;      /* Flags returned from gssd */
    uint32_t      gss_major;       /* GSS major error code */
    uint32_t      gss_minor;       /* GSS minor (mech) error code */
    uint32_t      gss_smb_error;   /* Last error returned by smb SetUpAndX */
};

struct conn_params {
    struct session_con_entry  *con_entry;
    uint64_t client_if_idx;                     // IF index of the client NIC
    struct sock_addr_entry    *selected_saddr;  // The currently selected server IP address
};

struct smbiod {
    int                 iod_id;
    int                 iod_flags;
    enum smbiod_state   iod_state;
    lck_mtx_t           iod_flagslock;        /* iod_flags */
    /* number of active outstanding requests (keep it signed!) */
    int32_t             iod_muxcnt;
    /* number of active outstanding async requests (keep it signed!) */
    int32_t             iod_asynccnt;
    struct timespec     iod_sleeptimespec;
    struct smb_session *iod_session;
    lck_mtx_t           iod_rqlock;           /* iod_rqlist, iod_muxwant */
    struct smb_rqhead   iod_rqlist;           /* list of outstanding requests */
    int                 iod_muxwant;
    void*       iod_context;
    lck_mtx_t           iod_evlock;           /* iod_evlist */
    STAILQ_HEAD(,smbiod_event) iod_evlist;
    struct timespec     iod_lastrqsent;
    struct timespec     iod_lastrecv;
    int                 iod_workflag;         /* should be protected with lock */
    struct timespec     reconnectStartTime;   /* Time when the reconnect was started */
    
    /* MultiChannel Support */
    TAILQ_ENTRY(smbiod) tailq;                  /* list of iods per session */
    struct sockaddr     *iod_saddr;             /* server addr */
    struct sockaddr     *iod_laddr;             /* local addr, if any, only used for port 139 */
    lck_mtx_t           iod_lck_unknown;        /* unknown lock mtx added */
    void                *iod_tdata;             /* transport control block */
    uint32_t            iod_ref_cnt;            /* counts references to this iod, protected by iod_tailq_lock */
    struct smb_tran_desc *iod_tdesc;            /* transport functions */
    struct smb_gss      iod_gss;                /* Parameters for gssd */
    uint64_t            iod_message_id;         /* SMB 2/3 request message id */
    uint32_t            negotiate_tokenlen;     /* negotiate token length */
    uint8_t             *negotiate_token;       /* negotiate token */
    uint32_t            iod_credits_granted;    /* SMB 2/3 credits granted */
    uint32_t            iod_credits_ss_granted; /* SMB 2/3 credits granted from session setup replies */
    uint32_t            iod_credits_max;        /* SMB 2/3 max amount of credits server has granted us */
    int32_t             iod_credits_wait;       /* SMB 2/3 credit wait */
    uint32_t            iod_req_pending;        /* SMB 2/3 set if there is a pending request */
    uint64_t            iod_oldest_message_id;  /* SMB 2/3 oldest pending request message id */
    lck_mtx_t           iod_credits_lock;

    /* Alternate channels have their own signing key */
    uint8_t             *iod_mackey;            /* MAC key */
    uint32_t            iod_mackeylen;          /* length of MAC key */
    uint8_t             iod_smb3_signing_key[SMB3_KEY_LEN]; /* Channel.SigningKey */
    uint32_t            iod_smb3_signing_key_len;

    /* SMB 3.1.1 PreAuthIntegrity fields */
    uint8_t             iod_pre_auth_int_salt[32]; /* Random number */
    uint8_t             iod_pre_auth_int_hash_neg[SHA512_DIGEST_LENGTH] __attribute__((aligned(4)));
    uint8_t             iod_pre_auth_int_hash[SHA512_DIGEST_LENGTH] __attribute__((aligned(4)));

    struct conn_params  iod_conn_entry;
    uint64_t            iod_total_tx_bytes;     /* The total number of bytes transmitted by this iod */
    uint64_t            iod_total_rx_bytes;     /* The total number of bytes received by this iod */
    struct timespec     iod_session_setup_time;
    
    /* Saved last session setup reply */
    uint8_t             *iod_sess_setup_reply;  /* Used for pre auth and alt channels */
    size_t              iod_sess_setup_reply_len;
};

#endif /* _SMB_CONN_H_ */

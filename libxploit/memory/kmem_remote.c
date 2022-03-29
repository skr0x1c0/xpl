//
//  kmem_remote.c
//  xe
//
//  Created by admin on 12/2/21.
//

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <poll.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/errno.h>
#include <sys/stat.h>

#include "memory/kmem_remote.h"
#include "memory/kmem.h"
#include "memory/kmem_internal.h"
#include "util/misc.h"
#include "util/assert.h"
#include "util/log.h"


#define DEFAULT_KMEM_SOCKET_PATH "/tmp/xpl_kmem_server.sock"


struct xpl_kmem_remote_ctx {
    uintptr_t mh_execute_header;
};


// MARK: server

#define MAX_PEER_CONNECTIONS 10
#define MAX_READ_SIZE (16 * 1024)
#define MAX_WRITE_SIZE (16 * 1024)

#define xpl_KMEM_REMOTE_CMD_READ 12
#define xpl_KMEM_REMOTE_CMD_WRITE 13
#define xpl_KMEM_REMOTE_GET_MH_EXECUTE_HEADER 14


struct cmd_read {
    uintptr_t src;
    size_t size;
};

struct cmd_write {
    uintptr_t dst;
    size_t size;
};


int xpl_kmem_server_send_response(int fd, int status, void* data, size_t data_size) {
    struct iovec iov[2];
    iov[0].iov_base = &status;
    iov[0].iov_len = sizeof(status);
    iov[1].iov_base = data;
    iov[1].iov_len = data_size;
    
    struct msghdr msg;
    bzero(&msg, sizeof(msg));
    msg.msg_iov = iov;
    msg.msg_iovlen = xpl_array_size(iov);
    
    ssize_t tf_len = sendmsg(fd, &msg, MSG_WAITALL);
    if (tf_len != sizeof(status) + data_size) {
        return EIO;
    }
    return 0;
}


int xpl_kmem_server_handle_cmd_write(const struct xpl_kmem_remote_ctx* ctx, int fd) {
    struct cmd_write cmd;
    ssize_t size = recv(fd, &cmd, sizeof(cmd), MSG_WAITALL);
    if (size != sizeof(cmd)) {
        xpl_log_warn("write cmd failed to recv req");
        return EIO;
    }
    
    if (cmd.size > MAX_WRITE_SIZE) {
        return xpl_kmem_server_send_response(fd, E2BIG, NULL, 0);
    }
    
    xpl_log_verbose("writing data of size %lu to %p",cmd.size, (void*)cmd.dst);
    
    char* buffer = malloc(cmd.size);
    size = recv(fd, buffer, cmd.size, MSG_WAITALL);
    if (size != cmd.size) {
        xpl_log_warn("write cmd failed to recv data");
        free(buffer);
        return EIO;
    }
    
    xpl_kmem_write(cmd.dst, 0, buffer, cmd.size);
    free(buffer);
    
    return xpl_kmem_server_send_response(fd, 0, NULL, 0);
}

int xpl_kmem_server_handle_cmd_read(const struct xpl_kmem_remote_ctx* ctx, int fd) {
    struct cmd_read cmd;
    bzero(&cmd, sizeof(cmd));
    
    ssize_t size = recv(fd, &cmd, sizeof(cmd), MSG_WAITALL);
    if (size != sizeof(cmd)) {
        xpl_log_warn("read cmd failed to recv req");
        return EIO;
    }
    
    if (cmd.size > MAX_READ_SIZE) {
        return xpl_kmem_server_send_response(fd, E2BIG, NULL, 0);
    }
    
    xpl_log_verbose("reading data of size %lu from %p", cmd.size, (void*)cmd.src);
    
    char* buffer = malloc(cmd.size);
    xpl_kmem_read(buffer, cmd.src, 0, cmd.size);
    
    int error = xpl_kmem_server_send_response(fd, 0, buffer, cmd.size);
    free(buffer);
    return error;
}

int xpl_kmem_server_handle_cmd_get_mh_execute_header(const struct xpl_kmem_remote_ctx* ctx, int fd) {
    uintptr_t value = ctx->mh_execute_header;
    return xpl_kmem_server_send_response(fd, 0, (void*)&value, sizeof(value));
}

void xpl_kmem_server_handle_incoming(const struct xpl_kmem_remote_ctx* ctx, int fd) {
    uint8_t cmd;
    
    ssize_t size = recv(fd, &cmd, sizeof(cmd), MSG_WAITALL);
    if (size != sizeof(cmd)) {
        xpl_log_warn("ignoring request with no cmd data");
        close(fd);
        return;
    }
    
    int error = ENOENT;
    switch (cmd) {
        case xpl_KMEM_REMOTE_CMD_READ: {
            error = xpl_kmem_server_handle_cmd_read(ctx, fd);
            break;
        }
        case xpl_KMEM_REMOTE_CMD_WRITE: {
            error = xpl_kmem_server_handle_cmd_write(ctx, fd);
            break;
        case xpl_KMEM_REMOTE_GET_MH_EXECUTE_HEADER: {
            error = xpl_kmem_server_handle_cmd_get_mh_execute_header(ctx, fd);
            break;
        }
        }
    }
    
    if (error) {
        xpl_log_error("error %s handling request", strerror(error));
        close(fd);
    }
}

void xpl_kmem_server_spin_once(const struct xpl_kmem_remote_ctx* ctx, struct pollfd* fds, nfds_t* num_fds, _Bool* stop) {
    for (int i=0; i<*num_fds; i++) {
        if (fds[i].revents == 0) {
            continue;
        }
        
        short events = fds[i].revents;
        fds[i].revents = 0;
        
        if ((events & POLLERR) || (events & POLLHUP) || (events & POLLNVAL)) {
            xpl_log_debug("dropping connection at fd: %d", fds[i].fd);
            // remove dropped connection
            close(fds[i].fd);
            memcpy(&fds[i], &fds[i+1], sizeof(fds[0]) * (*num_fds - i - 1));
            (*num_fds)--;
        } else if (events & POLLIN && i == 0) {
            // accept incoming connection
            int fd = accept(fds[0].fd, NULL, NULL);
            if (*num_fds >= MAX_PEER_CONNECTIONS) {
                xpl_log_warn("dropped peer connection due to connection max limit");
                close(fd);
                continue;
            }
            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            int res = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            xpl_assert(res == 0);
            res = setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
            xpl_assert(res == 0);
            fds[*num_fds].fd = fd;
            fds[*num_fds].events = POLLIN;
            (*num_fds)++;
        } else if (events & POLLIN && i == 1) {
            *stop = 1;
            break;
        } else if (events & POLLIN) {
            // read incoming request
            xpl_kmem_server_handle_incoming(ctx, fds[i].fd);
        } else {
            xpl_log_warn("[ERROR] unexpected event %d\n", events);
            xpl_abort();
        }
    }
}

void xpl_kmem_server_listen(const struct xpl_kmem_remote_ctx* ctx, int sock_fd) {
    struct pollfd fds[MAX_PEER_CONNECTIONS];
    bzero(&fds, sizeof(fds));
    
    nfds_t num_poll_fds = 2;
    fds[0].fd = sock_fd;
    fds[0].events = POLLIN;
    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;
    
    _Bool stop = 0;
    while (!stop) {
        int num_fds_ready = poll(fds, num_poll_fds, -1);
        if (num_fds_ready) {
            xpl_kmem_server_spin_once(ctx, fds, &num_poll_fds, &stop);
        } else {
            xpl_log_warn("poll returned error: %s", strerror(errno));
        }
    }
}

int xpl_kmem_remote_server_start(uintptr_t mh_execute_header, const char* uds_path) {
    int fd = socket(PF_UNIX, SOCK_STREAM, 0);
    xpl_assert(fd >= 0);

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    addr.sun_len = sizeof(addr);
    strlcpy(addr.sun_path, uds_path, sizeof(addr.sun_path));
    
    int res = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (res) {
        return errno;
    }
    
    res = listen(fd, 10);
    xpl_assert(res == 0);
    
    xpl_log_info("starting remote kmem server with socket %s", addr.sun_path);
    xpl_log_info("press any key to stop");
    struct xpl_kmem_remote_ctx ctx;
    ctx.mh_execute_header = mh_execute_header;
    xpl_kmem_server_listen(&ctx, fd);
    
    unlink(addr.sun_path);
    return 0;
}


// MARK: client
struct xpl_kmem_remote_client {
    int sock;
};


void xpl_kmem_remote_client_read(void* ctx, void* dst, uintptr_t src, size_t size) {
    struct xpl_kmem_remote_client* client = (struct xpl_kmem_remote_client*)ctx;
    int fd = client->sock;
    
    uint8_t cmd = xpl_KMEM_REMOTE_CMD_READ;
    
    struct cmd_read req;
    req.size = size;
    req.src = src;
    
    struct iovec iov[2];
    iov[0].iov_base = &cmd;
    iov[0].iov_len = sizeof(cmd);
    iov[1].iov_base = &req;
    iov[1].iov_len = sizeof(req);
    
    struct msghdr msg;
    bzero(&msg, sizeof(msg));
    msg.msg_iov = iov;
    msg.msg_iovlen = xpl_array_size(iov);
    
    ssize_t tf_size = sendmsg(fd, &msg, MSG_WAITALL);
    xpl_assert(tf_size == sizeof(cmd) + sizeof(req));
    
    int status = 0;
    tf_size = recv(fd, &status, sizeof(status), MSG_WAITALL);
    xpl_assert(tf_size == sizeof(status));
    xpl_assert(status == 0);
    
    tf_size = recv(fd, dst, size, MSG_WAITALL);
    xpl_assert(tf_size == size);
}


void xpl_kmem_remote_client_write(void* ctx, uintptr_t dst, const void* src, size_t size) {
    struct xpl_kmem_remote_client* client = (struct xpl_kmem_remote_client*)ctx;
    int fd = client->sock;
    
    uint8_t cmd = xpl_KMEM_REMOTE_CMD_WRITE;
    
    struct cmd_write req;
    req.size = size;
    req.dst = dst;
    
    struct iovec iov[3];
    iov[0].iov_base = &cmd;
    iov[0].iov_len = sizeof(cmd);
    iov[1].iov_base = &req;
    iov[1].iov_len = sizeof(req);
    iov[2].iov_base = (void*)src;
    iov[2].iov_len = size;
    
    struct msghdr msg;
    bzero(&msg, sizeof(msg));
    msg.msg_iov = iov;
    msg.msg_iovlen = xpl_array_size(iov);
    
    ssize_t tf_size = sendmsg(fd, &msg, MSG_WAITALL);
    xpl_assert(tf_size == sizeof(cmd) + sizeof(req) + size);
    
    int status = 0;
    tf_size = read(fd, &status, sizeof(status));
    xpl_assert(tf_size == sizeof(status));
    xpl_assert(status == 0);
}

static struct xpl_kmem_ops xpl_kmem_remote_client_ops = {
    .read = xpl_kmem_remote_client_read,
    .write = xpl_kmem_remote_client_write,
    .max_read_size = MAX_READ_SIZE,
    .max_write_size = MAX_WRITE_SIZE,
};

int xpl_kmem_remote_client_create(const char* uds_path, xpl_kmem_backend_t* backend) {
    int fd = socket(PF_UNIX, SOCK_STREAM, 0);
        
    struct sockaddr_un addr;
    bzero(&addr, sizeof(addr));
    addr.sun_family = AF_UNIX;
    addr.sun_len = sizeof(addr);
    strlcpy(addr.sun_path, uds_path, sizeof(addr.sun_path));
    int res = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (res) {
        return errno;
    }
    
    struct xpl_kmem_remote_client* client = (struct xpl_kmem_remote_client*)malloc(sizeof(struct xpl_kmem_remote_client));
    client->sock = fd;
    
    *backend = xpl_kmem_backend_create(&xpl_kmem_remote_client_ops, client);
    return 0;
}

uintptr_t xpl_kmem_remote_client_get_mh_execute_header(xpl_kmem_backend_t backend) {
    struct xpl_kmem_remote_client* client = (struct xpl_kmem_remote_client*)xpl_kmem_backend_get_ctx(backend);
    int fd = client->sock;
    
    uint8_t cmd = xpl_KMEM_REMOTE_GET_MH_EXECUTE_HEADER;
    
    struct iovec iov_req[1];
    iov_req[0].iov_base = &cmd;
    iov_req[0].iov_len = sizeof(cmd);
    
    struct msghdr msg_req;
    bzero(&msg_req, sizeof(msg_req));
    msg_req.msg_iov = iov_req;
    msg_req.msg_iovlen = xpl_array_size(iov_req);
    
    ssize_t tf_size = sendmsg(fd, &msg_req, MSG_WAITALL);
    xpl_assert(tf_size == sizeof(cmd));
    
    int status = 0;
    tf_size = recv(fd, &status, sizeof(status), MSG_WAITALL);
    xpl_assert(tf_size == sizeof(status));
    xpl_assert(status == 0);
    
    uintptr_t value;
    tf_size = recv(fd, &value, sizeof(value), MSG_WAITALL);
    xpl_assert(tf_size == sizeof(value));
    return value;
}

void xpl_kmem_remote_client_destroy(xpl_kmem_backend_t* backend_p) {
    struct xpl_kmem_remote_client* client = (struct xpl_kmem_remote_client*)xpl_kmem_backend_get_ctx(*backend_p);
    close(client->sock);
    free(client);
    xpl_kmem_backend_destroy(backend_p);
}

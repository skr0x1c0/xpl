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

#include "kmem_remote.h"
#include "kmem.h"
#include "util_misc.h"
#include "util_assert.h"
#include "util_log.h"


// MARK: server

#define MAX_PEER_CONNECTIONS 10
#define MAX_READ_SIZE (16 * 1024)
#define MAX_WRITE_SIZE (16 * 1024)

#define XE_KMEM_REMOTE_CMD_PING 11
#define XE_KMEM_REMOTE_CMD_READ 12
#define XE_KMEM_REMOTE_CMD_WRITE 13
#define XE_KMEM_REMOTE_GET_MH_EXECUTE_HEADER 14


struct cmd_read {
    uintptr_t src;
    size_t size;
};

struct cmd_write {
    uintptr_t dst;
    size_t size;
};


int xe_kmem_server_send_response(int fd, int status, void* data, size_t data_size) {
    struct iovec iov[2];
    iov[0].iov_base = &status;
    iov[0].iov_len = sizeof(status);
    iov[1].iov_base = data;
    iov[1].iov_len = data_size;
    
    struct msghdr msg;
    bzero(&msg, sizeof(msg));
    msg.msg_iov = iov;
    msg.msg_iovlen = XE_ARRAY_SIZE(iov);
    
    ssize_t tf_len = sendmsg(fd, &msg, MSG_WAITALL);
    if (tf_len != sizeof(status) + data_size) {
        return EIO;
    }
    return 0;
}


int xe_kmem_server_handle_cmd_write(const struct xe_kmem_remote_server_ctx* ctx, int fd) {
    struct cmd_write cmd;
    ssize_t size = recv(fd, &cmd, sizeof(cmd), MSG_WAITALL);
    if (size != sizeof(cmd)) {
        xe_log_warn("write cmd failed to recv req");
        return EIO;
    }
    
    if (cmd.size > MAX_WRITE_SIZE) {
        return xe_kmem_server_send_response(fd, E2BIG, NULL, 0);
    }
    
    xe_log_info("writing data of size %lu to %p",cmd.size, (void*)cmd.dst);
    
    char* buffer = malloc(cmd.size);
    size = recv(fd, buffer, cmd.size, MSG_WAITALL);
    if (size != cmd.size) {
        xe_log_warn("write cmd failed to recv data");
        free(buffer);
        return EIO;
    }
    
    xe_kmem_write(cmd.dst, buffer, cmd.size);
    free(buffer);
    
    return xe_kmem_server_send_response(fd, 0, NULL, 0);
}

int xe_kmem_server_handle_cmd_read(const struct xe_kmem_remote_server_ctx* ctx, int fd) {
    struct cmd_read cmd;
    bzero(&cmd, sizeof(cmd));
    
    ssize_t size = recv(fd, &cmd, sizeof(cmd), MSG_WAITALL);
    if (size != sizeof(cmd)) {
        xe_log_warn("read cmd failed to recv req");
        return EIO;
    }
    
    if (cmd.size > MAX_READ_SIZE) {
        return xe_kmem_server_send_response(fd, E2BIG, NULL, 0);
    }
    
    xe_log_info("reading data of size %lu from %p", cmd.size, (void*)cmd.src);
    
    char* buffer = malloc(cmd.size);
    xe_kmem_read(buffer, cmd.src, cmd.size);
    
    int error = xe_kmem_server_send_response(fd, 0, buffer, cmd.size);
    free(buffer);
    return error;
}

int xe_kmem_server_handle_cmd_ping(const struct xe_kmem_remote_server_ctx* ctx, int fd) {
    uint8_t resp = XE_KMEM_REMOTE_CMD_PING;
    return xe_kmem_server_send_response(fd, 0, &resp, sizeof(resp));
}

int xe_kmem_server_handle_cmd_get_mh_execute_header(const struct xe_kmem_remote_server_ctx* ctx, int fd) {
    uintptr_t value = ctx->mh_execute_header;
    return xe_kmem_server_send_response(fd, 0, (void*)&value, sizeof(value));
}

void xe_kmem_server_handle_incoming(const struct xe_kmem_remote_server_ctx* ctx, int fd) {
    uint8_t cmd;
    
    ssize_t size = recv(fd, &cmd, sizeof(cmd), MSG_WAITALL);
    if (size != sizeof(cmd)) {
        xe_log_warn("ignoring request with no cmd data");
        close(fd);
        return;
    }
    
    int error = ENOENT;
    switch (cmd) {
        case XE_KMEM_REMOTE_CMD_PING: {
            error = xe_kmem_server_handle_cmd_ping(ctx, fd);
            break;
        }
        case XE_KMEM_REMOTE_CMD_READ: {
            error = xe_kmem_server_handle_cmd_read(ctx, fd);
            break;
        }
        case XE_KMEM_REMOTE_CMD_WRITE: {
            error = xe_kmem_server_handle_cmd_write(ctx, fd);
            break;
        case XE_KMEM_REMOTE_GET_MH_EXECUTE_HEADER: {
            error = xe_kmem_server_handle_cmd_get_mh_execute_header(ctx, fd);
            break;
        }
        }
    }
    
    if (error) {
        xe_log_error("error %s handling request", strerror(error));
        close(fd);
    }
}

void xe_kmem_server_spin_once(const struct xe_kmem_remote_server_ctx* ctx, struct pollfd* fds, nfds_t* num_fds, _Bool* stop) {
    for (int i=0; i<*num_fds; i++) {
        if (fds[i].revents == 0) {
            continue;
        }
        
        short events = fds[i].revents;
        fds[i].revents = 0;
        
        if ((events & POLLERR) || (events & POLLHUP) || (events & POLLNVAL)) {
            // remove dropped connection
            close(fds[i].fd);
            memcpy(&fds[i], &fds[i+1], sizeof(fds[0]) * (*num_fds - i - 1));
            (*num_fds)--;
        } else if (events & POLLIN && i == 0) {
            // accept incoming connection
            int fd = accept(fds[0].fd, NULL, NULL);
            if (*num_fds >= MAX_PEER_CONNECTIONS) {
                xe_log_warn("dropped peer connection due to connection max limit");
                close(fd);
                continue;
            }
            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            int res = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            xe_assert(res == 0);
            res = setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
            xe_assert(res == 0);
            fds[*num_fds].fd = fd;
            fds[*num_fds].events = POLLIN;
            (*num_fds)++;
        } else if (events & POLLIN && i == 1) {
            *stop = 1;
            break;
        } else if (events & POLLIN) {
            // read incoming request
            xe_kmem_server_handle_incoming(ctx, fds[i].fd);
        } else {
            xe_log_warn("[ERROR] unexpected event %d\n", events);
            abort();
        }
    }
}

void xe_kmem_server_listen(const struct xe_kmem_remote_server_ctx* ctx, int sock_fd) {
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
        xe_assert(num_fds_ready >= 0);
        xe_kmem_server_spin_once(ctx, fds, &num_poll_fds, &stop);
    }
}

void xe_kmem_remote_server_start(const struct xe_kmem_remote_server_ctx* ctx) {
    int fd = socket(PF_UNIX, SOCK_STREAM, 0);
    xe_assert(fd >= 0);
    
    char data_directory[PATH_MAX] = "/tmp/xe_XXXXXXXX";
    mkdtemp(data_directory);
    
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    addr.sun_len = sizeof(addr);
    size_t path_size = snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/socket", data_directory);
    xe_assert(path_size < sizeof(addr.sun_path));
    
    int res = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    xe_assert(res == 0);
    
    res = listen(fd, 10);
    xe_assert(res == 0);
    
    xe_log_info("starting remote kmem server with socket %s", addr.sun_path);
    xe_log_info("press any key to stop");
    xe_kmem_server_listen(ctx, fd);
}


// MARK: client
struct xe_kmem_remote_client {
    int sock;
};


void xe_kmem_remote_client_read(void* ctx, void* dst, uintptr_t src, size_t size) {
    struct xe_kmem_remote_client* client = (struct xe_kmem_remote_client*)ctx;
    int fd = client->sock;
    
    uint8_t cmd = XE_KMEM_REMOTE_CMD_READ;
    
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
    msg.msg_iovlen = XE_ARRAY_SIZE(iov);
    
    ssize_t tf_size = sendmsg(fd, &msg, MSG_WAITALL);
    xe_assert(tf_size == sizeof(cmd) + sizeof(req));
    
    int status = 0;
    tf_size = recv(fd, &status, sizeof(status), MSG_WAITALL);
    xe_assert(tf_size == sizeof(status));
    xe_assert(status == 0);
    
    tf_size = recv(fd, dst, size, MSG_WAITALL);
    xe_assert(tf_size == size);
}


void xe_kmem_remote_client_write(void* ctx, uintptr_t dst, void* src, size_t size) {
    struct xe_kmem_remote_client* client = (struct xe_kmem_remote_client*)ctx;
    int fd = client->sock;
    
    uint8_t cmd = XE_KMEM_REMOTE_CMD_WRITE;
    
    struct cmd_write req;
    req.size = size;
    req.dst = dst;
    
    struct iovec iov[3];
    iov[0].iov_base = &cmd;
    iov[0].iov_len = sizeof(cmd);
    iov[1].iov_base = &req;
    iov[1].iov_len = sizeof(req);
    iov[2].iov_base = src;
    iov[2].iov_len = size;
    
    struct msghdr msg;
    bzero(&msg, sizeof(msg));
    msg.msg_iov = iov;
    msg.msg_iovlen = XE_ARRAY_SIZE(iov);
    
    ssize_t tf_size = sendmsg(fd, &msg, MSG_WAITALL);
    xe_assert(tf_size == sizeof(cmd) + sizeof(req) + size);
    
    int status = 0;
    tf_size = read(fd, &status, sizeof(status));
    xe_assert(tf_size == sizeof(status));
    xe_assert(status == 0);
}

static struct xe_kmem_ops xe_kmem_remote_client_ops = {
    .read = xe_kmem_remote_client_read,
    .write = xe_kmem_remote_client_write,
    .max_read_size = MAX_READ_SIZE,
    .max_write_size = MAX_WRITE_SIZE,
};

struct xe_kmem_backend* xe_kmem_remote_client_create(const char* socket_path) {
    int fd = socket(PF_UNIX, SOCK_STREAM, 0);
    
    struct sockaddr_un addr;
    bzero(&addr, sizeof(addr));
    addr.sun_family = AF_UNIX;
    addr.sun_len = sizeof(addr);
    strlcpy(addr.sun_path, socket_path, sizeof(addr.sun_path));
    int res = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    xe_assert(res == 0);
    
    struct xe_kmem_remote_client* client = (struct xe_kmem_remote_client*)malloc(sizeof(struct xe_kmem_remote_client));
    client->sock = fd;
    
    struct xe_kmem_backend* backend = (struct xe_kmem_backend*)malloc(sizeof(struct xe_kmem_backend));
    backend->ctx = client;
    backend->ops = &xe_kmem_remote_client_ops;
    
    return backend;
}

uintptr_t xe_kmem_remote_client_get_mh_execute_header(struct xe_kmem_backend* backend) {
    struct xe_kmem_remote_client* client = (struct xe_kmem_remote_client*)backend->ctx;
    int fd = client->sock;
    
    uint8_t cmd = XE_KMEM_REMOTE_GET_MH_EXECUTE_HEADER;
    
    struct iovec iov_req[1];
    iov_req[0].iov_base = &cmd;
    iov_req[0].iov_len = sizeof(cmd);
    
    struct msghdr msg_req;
    bzero(&msg_req, sizeof(msg_req));
    msg_req.msg_iov = iov_req;
    msg_req.msg_iovlen = XE_ARRAY_SIZE(iov_req);
    
    ssize_t tf_size = sendmsg(fd, &msg_req, MSG_WAITALL);
    xe_assert(tf_size == sizeof(cmd));
    
    int status = 0;
    tf_size = recv(fd, &status, sizeof(status), MSG_WAITALL);
    xe_assert(tf_size == sizeof(status));
    xe_assert(status == 0);
    
    uintptr_t value;
    tf_size = recv(fd, &value, sizeof(value), MSG_WAITALL);
    xe_assert(tf_size == sizeof(value));
    return value;
}

void xe_kmem_remote_client_destroy(struct xe_kmem_backend* backend) {
    struct xe_kmem_remote_client* client = (struct xe_kmem_remote_client*)backend->ctx;
    close(client->sock);
    free(client);
    free(backend);
}

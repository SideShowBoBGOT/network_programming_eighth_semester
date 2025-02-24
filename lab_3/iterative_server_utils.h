#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <endian.h>
#include <alloca.h>

#include "protocol.h"

typedef struct {
    const char *address;
    uint16_t port;
    const char *dir_path;
} IterativeServerConfig;

static void handle_client(
    const int client_sock,
    const char* const dir_path
) {
    {
        printf("[Client_sock: %d] [Start handling client]\n", client_sock);
        uint8_t client_protocol_version;
        if(not readn(client_sock, &client_protocol_version, sizeof(client_protocol_version), NULL)) {
            printf("[Client_sock: %d] [Failed to receive request] [errno: %d] [strerror: %s]\n", client_sock, errno, strerror(errno));
            return;
        }
        printf("[Client_sock: %d] [Client protocol version: %d]\n", client_sock, client_protocol_version);
        {
            const bool is_protocol_match = client_protocol_version == PROTOCOL_VERSION;
            if(not writen(client_sock, &is_protocol_match, sizeof(is_protocol_match), NULL)) {
                printf("[Client_sock: %d] [Failed to send protocol match] [errno: %d] [strerror: %s]\n", client_sock, errno, strerror(errno));
                return;
            }
            printf("[Client_sock: %d] [Sent protocol match: %d]\n", client_sock, is_protocol_match);
            if(not is_protocol_match) {
                return;
            }
        }
    }
    struct fd_size_t { int fd; size_t size;};
    const struct fd_size_t fd_size = ({
        bool is_file_size_ok = false;
        const struct fd_size_t fd_size = ({
            const char *const buffer = ({
                filename_buff_t filename_buffer;
                if(not readn(client_sock, filename_buffer, ARRAY_SIZE(filename_buffer), NULL)) {
                    printf("[Client_sock: %d] [Failed to read filename buffer] [errno: %d] [strerror: %s]\n", client_sock, errno, strerror(errno));
                    return;
                }

                if(memchr(filename_buffer, '\0', ARRAY_SIZE(filename_buffer)) == NULL) {
                    printf("[Client_sock: %d] [Error filename not valid]\n", client_sock);
                    if(not writen(client_sock, &is_file_size_ok, sizeof(is_file_size_ok), NULL)) {
                        printf("[Client_sock: %d] [Error filename not valid] [errno: %d] [strerror: %s]\n", client_sock, errno, strerror(errno));
                    }
                    return;
                }
                const size_t dir_path_strlen = strlen(dir_path);
                const size_t filename_strlen = strlen(filename_buffer);
                const size_t buffer_byte_count = dir_path_strlen + 1 + filename_strlen + 1;
                char *const buffer = alloca(buffer_byte_count);
                snprintf(buffer, buffer_byte_count, "%s/%s", dir_path, filename_buffer);
                buffer;
            });
            const int fd = open(buffer, O_RDONLY);
            if(fd == -1) {
                printf("[Client_sock: %d] [Error open file: %s] [errno: %d] [strerror: %s]\n", client_sock, buffer, errno, strerror(errno));
                if(not writen(client_sock, &is_file_size_ok, 1, NULL)) {
                    printf("[Client_sock: %d] [Error open file: %s] [errno: %d] [strerror: %s]\n", client_sock, buffer, errno, strerror(errno));
                }
                return;
            }

            const size_t file_size = ({
                struct stat st;
                if(fstat(fd, &st) == -1) {
                    printf("[Client_sock: %d] [Error stat: %s] [errno: %d] [strerror: %s]\n", client_sock, buffer, errno, strerror(errno));
                    if(not writen(client_sock, &is_file_size_ok, sizeof(is_file_size_ok), NULL)) {
                        printf("[Client_sock: %d] [Error stat: %s] [errno: %d] [strerror: %s]\n", client_sock, buffer, errno, strerror(errno));
                    }
                    return;
                }
                (size_t)st.st_size;
            });
            (struct fd_size_t){fd, file_size};
        });
        is_file_size_ok = true;
        if(not writen(client_sock, &is_file_size_ok, 1, NULL)) {
            printf("[Client_sock: %d] [Failed to send file size ready flag] [errno: %d] [strerror: %s]\n", client_sock, errno, strerror(errno));
            return;
        }
        const size_t network_file_size = htobe64(fd_size.size);
        if(not writen(client_sock, &network_file_size, sizeof(network_file_size), NULL)) {
            printf("[Client_sock: %d] [Failed to send file size] [errno: %d] [strerror: %s]\n", client_sock, errno, strerror(errno));
            return;
        }
        fd_size;
    });
    {
        bool is_client_ready;
        if(not readn(client_sock, &is_client_ready, sizeof(is_client_ready), NULL)) {
            printf("[Client_sock: %d] [Failed to receive clients file approval] [errno: %d] [strerror: %s]\n", client_sock, errno, strerror(errno));
            return;
        }
        if(not is_client_ready) {
            printf("[Client_sock: %d] [Client rejected file receiving]\n", client_sock);
            return;
        }
    }
    printf("[Ready to send file]\n");

    // {
    //     off_t offset = 0;
    //     while(offset < fd_size.size) {
    //         const ssize_t nsendfile = sendfile(client_sock, fd_size.fd, &offset, fd_size.size - offset);
    //         if(nsendfile < 0) {
    //             printf("[Client_sock: %d] [Failed to sendfile] [errno: %d] [strerror: %s]\n", client_sock, errno, strerror(errno));
    //             return;
    //         } else if(nsendfile == 0) {
    //             break;
    //         }
    //     }
    // }
    printf("[Client_sock: %d] [Finished sending file]\n", client_sock);
    close(fd_size.fd);
}

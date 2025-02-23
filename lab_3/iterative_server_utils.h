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
#include "protocol.h"
#include <endian.h>

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
    const int fd = ({
        FileSizeResult file_size_result;
        const int fd = ({
            filename_buff_t filename_buffer;
            if(not readn(client_sock, filename_buffer, ARRAY_SIZE(filename_buffer), NULL)) {
                printf("[Client_sock: %d] [Failed to read filename buffer] [errno: %d] [strerror: %s]\n", client_sock, errno, strerror(errno));
                return;
            }

            if(memchr(filename_buffer, '\0', ARRAY_SIZE(filename_buffer)) == NULL) {
                printf("[Client_sock: %d] [FileSizeResult_ERROR_FILENAME_INVALID]\n", client_sock);
                file_size_result = FileSizeResult_ERROR_FILENAME_INVALID;
                if(not writen(client_sock, &file_size_result, 1, NULL)) {
                    printf("[Client_sock: %d] [Failed to send FileSizeResult_ERROR_FILENAME_INVALID] [errno: %d] [strerror: %s]\n", client_sock, errno, strerror(errno));
                }
                return;
            }
            const size_t dir_path_strlen = strlen(dir_path);
            const size_t filename_strlen = strlen(filename_buffer);
            const size_t buffer_byte_count = dir_path_strlen + 1 + filename_strlen + 1;
            char buffer[buffer_byte_count];
            snprintf(buffer, buffer_byte_count, "%s/%s", dir_path, filename_buffer);
            printf("[Client_sock: %d] [Directory path: %s]\n", client_sock, dir_path);
            const int fd = open(buffer, O_RDONLY);
            if(fd == -1) {
                printf("[Client_sock: %d] [FileSizeResult_ERROR_OPEN: %s] [errno: %d] [strerror: %s]\n", client_sock, buffer, errno, strerror(errno));
                file_size_result = FileSizeResult_ERROR_OPEN;
                if(not writen(client_sock, &file_size_result, 1, NULL)) {
                    printf("[Client_sock: %d] [Failed to send FileSizeResult_ERROR_OPEN] [errno: %d] [strerror: %s]\n", client_sock, errno, strerror(errno));
                }
                return;
            }
            fd;
        });
        
        const size_t network_file_size = ({
            struct stat st;
            if(fstat(fd, &st) == -1) {
                printf("[Client_sock: %d] [FileSizeResult_ERROR_STAT] [errno: %d] [strerror: %s]\n", client_sock, errno, strerror(errno));
                file_size_result = FileSizeResult_ERROR_STAT;
                if(not writen(client_sock, &file_size_result, 1, NULL)) {
                    printf("[Client_sock: %d] [Failed to send FileSizeResult_ERROR_STAT] [errno: %d] [strerror: %s]\n", client_sock, errno, strerror(errno));
                }
                return;
            }
            htobe64(st.st_size);
        });
        file_size_result = FileSizeResult_OK;
        if(not writen(client_sock, &file_size_result, 1, NULL)) {
            printf("[Client_sock: %d] [Failed to send FileSizeResult_OK] [errno: %d] [strerror: %s]\n", client_sock, errno, strerror(errno));
            return;
        }
        if(not writen(client_sock, &network_file_size, sizeof(network_file_size), NULL)) {
            printf("[Client_sock: %d] [Failed to send file size] [errno: %d] [strerror: %s]\n", client_sock, errno, strerror(errno));
            return;
        }
        fd;
    });
    {
        bool is_file_size_ok;
        if(not readn(client_sock, &is_file_size_ok, sizeof(is_file_size_ok), NULL)) {
            printf("[Client_sock: %d] [Failed to receive is_file_size_ok] [errno: %d] [strerror: %s]\n", client_sock, errno, strerror(errno));
            return;
        }
        if(not is_file_size_ok) {
            printf("[Client_sock: %d] [File size is not ok]\n", client_sock);
            return;
        }
    }
    printf("[Ready to send file]\n");
    close(fd);
}

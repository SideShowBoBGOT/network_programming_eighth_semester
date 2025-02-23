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
        printf("pre client_protocol_version\n");
        uint8_t client_protocol_version;
        if(not readn(client_sock, &client_protocol_version, sizeof(client_protocol_version), NULL)) {        
            perror("Failed to receive request");
            return;
        }
        printf("client_sock: %d, client_protocol_version: %d\n", client_sock, client_protocol_version);
        {
            const bool is_protocol_match = client_protocol_version == PROTOCOL_VERSION;
            if(not writen(client_sock, &is_protocol_match, sizeof(is_protocol_match), NULL)) {
                perror("Failed to send protocol match");
                return;
            }
            printf("Sent protocol match: %d\n", is_protocol_match);
            if(not is_protocol_match) {
                return;
            }
        }
    }
    // const int fd = ({
    //     FileSizeResult file_size_result;
    //     const int fd = ({
    //         filename_buff_t filename_buffer;
    //         if(not readn(client_sock, filename_buffer, ARRAY_SIZE(filename_buffer), NULL)) {
    //             perror("Failed to read filename buffer");
    //             return;
    //         }

    //         if(memchr(filename_buffer, '\0', ARRAY_SIZE(filename_buffer)) == NULL) {
    //             printf("Filename is not a valid ");
    //             file_size_result = FileSizeResult_ERROR_FILENAME_INVALID;
    //             if(not writen(client_sock, &file_size_result, 1, NULL)) {
    //                 perror("Failed to send file_size_result file name not valid");
    //             }
    //             return;
    //         }
    //         const size_t dir_path_strlen = strlen(dir_path);
    //         const size_t filename_strlen = strlen(filename_buffer);
    //         const size_t buffer_byte_count = dir_path_strlen + filename_strlen + 1;
    //         char buffer[buffer_byte_count];
    //         buffer[0] = 0;
    //         strncpy(buffer, dir_path, dir_path_strlen);
    //         strncat(buffer, filename_buffer, filename_strlen);
    //         open(buffer, O_RDONLY);
    //     });
    //     if(fd == -1) {
    //         file_size_result = FileSizeResult_ERROR_OPEN;
    //         if(not writen(client_sock, &file_size_result, 1, NULL)) {
    //             perror("Failed to send open file_size_result");
    //             return;
    //         }
    //     }
    //     const size_t network_file_size = ({
    //         struct stat st;
    //         if(fstat(fd, &st) == -1) {
    //             file_size_result = FileSizeResult_ERROR_STAT;
    //             if(not writen(client_sock, &file_size_result, 1, NULL)) {
    //                 perror("Failed to send stat file_size_result");
    //                 return;
    //             }
    //         }
    //         htobe64(st.st_size);
    //     });
    //     file_size_result = FileSizeResult_OK;
    //     if(not writen(client_sock, &file_size_result, 1, NULL)) {
    //         perror("Failed to send ok file_size_result");
    //         return;
    //     }
    //     if(not writen(client_sock, &network_file_size, sizeof(network_file_size), NULL)) {
    //         perror("Failed to send file size");
    //         return;
    //     }
    //     fd;
    // });
    // close(fd);
}

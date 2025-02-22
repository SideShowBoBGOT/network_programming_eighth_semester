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

typedef struct {
    const char *address;
    int port;
    const char *dir_path;
} IterativeServerConfig;

void exit_err(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void warn_err(const char *msg) {
    perror(msg);
}

static bool receive_send_check_protocol_version(const int client_sock) {
    int client_protocol_version = 0;
    recv(client_sock, &client_protocol_version, sizeof(client_protocol_version), 0);
    const int server_protocol_version = 1;
    const bool protocol_version_match = client_protocol_version == server_protocol_version;
    send(client_sock, &protocol_version_match, sizeof(protocol_version_match), 0);
    if(!protocol_version_match) {
        printf("Protocol version mismatch.\n");
        close(client_sock);
        return false;
    }
    return true;
}

#define MAX_FILENAME_LENGTH 256

static bool check_file_existance_and_permissions(
    const int client_sock,
    const char *const dir_path,
    const char *const filename
) {
    

    struct stat st;
    if (stat(buffer, &st) == -1 || !S_ISREG(st.st_mode)) {
        const file_existence_t file_existence = FILE_NOT_FOUND;
        send(client_sock, &file_existence, sizeof(file_existence), 0);
        close(client_sock);
        return false;
    }
    const file_existence_t file_existence = FILE_FOUND;
    send(client_sock, &file_existence, sizeof(file_existence), 0);
    const file_size_t file_size = {.size = st.st_size};
    printf("File size: %lu\n", file_size.size);
    send(client_sock, &file_size, sizeof(file_size), 0);
    return true;
}

const size_t CHUNK_SIZE = 1;

static bool check_file_readiness(const int client_sock) {
    file_receive_readiness_t file_receive_readiness;
    recv(client_sock, &file_receive_readiness, sizeof(file_receive_readiness), 0);
    if (file_receive_readiness == REFUSE_TO_RECEIVE) {
        printf("Client refused to receive file.\n");
        close(client_sock);
        return false;
    }
    send(client_sock, &CHUNK_SIZE, sizeof(CHUNK_SIZE), 0);
    return true;
}

void handle_client(
    const int client_sock,
    const char* const dir_path
) {
    {
        uint16_t client_protocol_version;
        if(readn(client_sock, &client_protocol_version, sizeof(client_protocol_version)) == -1) {        
            perror("Failed to receive request");
            return;
        }
        {
            const bool is_protocol_match = ntohs(client_protocol_version) == PROTOCOL_VERSION;
            if(writen(client_sock, &is_protocol_match, 1) == -1) {
                perror("Failed to send protocol match");
                return;
            }
            if(not is_protocol_match) {
                return;
            }
        }
    }
    
    filename_buff_t filename_buffer;
    if(readn(client_sock, filename_buffer, ARRAY_SIZE(filename_buffer)) == -1) {
        perror("Failed to read filename buffer");
        return;
    }
    if(memchr(filename_buffer, '\0', ARRAY_SIZE(filename_buffer)) == NULL) {
        printf("Filename is not a valid ");
    }
    {
        int fd;
        {
            const size_t dir_path_strlen = strlen(dir_path);
            const size_t filename_strlen = strlen(filename_buffer);
            const size_t buffer_byte_count = dir_path_strlen + filename_strlen + 1;
            uint8_t buffer[buffer_byte_count];
            buffer[0] = 0;
            strncpy(buffer, dir_path, dir_path_strlen);
            strncat(buffer, filename_buffer, filename_strlen);

            fd = open(buffer, O_RDONLY);
        }
        if(fd == -1) {
            return;
        }
        {
            struct stat st;
            if(fstat(fd, &st) == -1) {
                return false;
            }

            st.st_size;
        }

        close(fd);
    }
    
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        perror("Failed to open file");
        close(client_sock);
        return;
    }

    char buffer[CHUNK_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        // printf("Process %d read bytes: %lu\n", getpid(), bytes_read);
        send(client_sock, buffer, bytes_read, 0);
    }

    fclose(file);
    close(client_sock);
}
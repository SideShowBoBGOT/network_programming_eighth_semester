#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <netinet/in.h>
#include "protocol.h"

typedef struct {
    const char *address;
    uint16_t port;
    const char *filename;
    size_t max_file_size;
} ClientConfig;

static void print_config(const ClientConfig *config) {
    printf("Client Configuration:\n");
    printf("  Address: %s\n", config->address);
    printf("  Port: %d\n", config->port);
    printf("  Filename: %s\n", config->filename);
    printf("  Maximum Children: %ld\n", config->max_file_size);
}

static ClientConfig handle_cmd_args(const int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <server_address> <server_port> <filename> <max_file_size>\n", argv[0]);
        exit(1);
    }
    const ClientConfig config = {
        .address = argv[1],
        .port = (uint16_t)atoi(argv[2]),
        .filename = argv[3],
        .max_file_size = (uint64_t)atoi(argv[4])
    };

    print_config(&config);

    return config;
}

static void func(const ClientConfig *const config, const int sock) {
    {
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(config->port);
        if (inet_pton(AF_INET, config->address, &server_addr.sin_addr) <= 0) {
            printf("[Failed inet_pton] [errno: %d] [strerror: %s]\n", errno, strerror(errno));
            return;
        }
        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            printf("[Connection failed] [errno: %d] [strerror: %s]\n", errno, strerror(errno));
            return;
        }
    }
    const size_t file_size = ({
        const uint8_t protocol_version = PROTOCOL_VERSION;
        if(not writen(sock, &protocol_version, sizeof(protocol_version), NULL)) {
            printf("[Failed to send protocol version] [errno: %d] [strerror: %s]\n", errno, strerror(errno));
            return;
        }
        printf("[Written protocol version: %d]\n", PROTOCOL_VERSION);
        {
            bool is_protocol_version_ok;
            if(not readn(sock, &is_protocol_version_ok, sizeof(is_protocol_version_ok), NULL)) {
                printf("[Failed to receive protocol version ok] [errno: %d] [strerror: %s]\n", errno, strerror(errno));
                return;
            }
            if(not is_protocol_version_ok) {
                printf("[Protocol version mismatch]\n");
                return;
            }
            printf("[Protocol version match]\n");
        }
        filename_buff_t filename_buffer;
        strncpy(filename_buffer, config->filename, ARRAY_SIZE(filename_buffer));
        if(not writen(sock, filename_buffer, ARRAY_SIZE(filename_buffer), NULL)) {
            printf("[Failed to send filename buffer] [errno: %d] [strerror: %s]\n", errno, strerror(errno));
            return;
        }
        {
            bool is_file_size_ok;
            if(not readn(sock, &is_file_size_ok, sizeof(is_file_size_ok), NULL)) {
                printf("[Failed to receive is_file_size_ok] [errno: %d] [strerror: %s]\n", errno, strerror(errno));
                return;
            }
            if(not is_file_size_ok) {
                printf("[File size is not ok]\n");
                return;
            }
        }
        size_t file_size;
        if(not readn(sock, &file_size, sizeof(file_size), NULL)) {
            printf("[Failed to receive file size] [errno: %d] [strerror: %s]\n", errno, strerror(errno));
            return;
        }
        be64toh(file_size);
    });
    printf("[File size: %ld]\n", file_size);
    
    int file_fd = -1;
    int pipefd[2] = {0};
    {
        __label__ send_is_client_ready;
        bool is_client_ready = file_size <= config->max_file_size;
        if(not is_client_ready) {
            printf("[Server file size is too large: %lu]\n", file_size);
            goto send_is_client_ready;
        }
        if(pipe(pipefd) < 0) {
            printf("[Can not create pipe] [errno: %d] [strerror: %s]\n", errno, strerror(errno));
            is_client_ready = false;
            goto send_is_client_ready;
        }
        file_fd = open(config->filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if(file_fd < 0) {
            is_client_ready = false;
            printf("[Failed to open file for writing] [errno: %d] [strerror: %s]\n", errno, strerror(errno));
            goto send_is_client_ready;
        }
        
        send_is_client_ready:
        if(not writen(sock, &is_client_ready, sizeof(is_client_ready), NULL)) {
            printf("[Failed to send is_client_ready: %d] [errno: %d] [strerror: %s]\n", is_client_ready, errno, strerror(errno));
            return;
        }
        if(not is_client_ready) {
            printf("[Client is not able to receive the file]\n");
            return;
        }
    }
    printf("[Ready to receive file]\n");
    size_t nread = 0;
    off_t write_offset = 0;
    while((size_t)write_offset < file_size) {
        {
            const ssize_t local_read = splice(sock, NULL, pipefd[1], NULL, file_size - nread, 0);
            if(local_read < 0) {
                printf("[Failed to read splice] [errno: %d] [strerror: %s]\n", errno, strerror(errno));
                return;
            }
            nread += (size_t)local_read;
        }
        {
            const ssize_t local_write = splice(pipefd[0], NULL, file_fd, &write_offset, file_size - (size_t)write_offset, 0);
            if(local_write < 0) {
                printf("[Failed to write splice] [errno: %d] [strerror: %s]\n", errno, strerror(errno));
                return;
            }
        }
    }
    close(file_fd);
    printf("[finished receiving file file]\n");
}

int main(const int argc, char *argv[]) {
    const ClientConfig config = handle_cmd_args(argc, argv);

    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
    } else {
        func(&config, sock);
    }

    return EXIT_SUCCESS;
}

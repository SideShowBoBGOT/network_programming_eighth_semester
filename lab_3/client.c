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
            perror("Invalid address");
            return;
        }
        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Connection failed");
            return;
        }
    }
    {
        const uint8_t protocol_version = PROTOCOL_VERSION;
        if(not writen(sock, &protocol_version, sizeof(protocol_version), NULL)) {
            perror("Failed to send protocol version");
            return;
        }
        printf("writen protocol version: %d\n", PROTOCOL_VERSION);
        {
            bool is_protocol_version_ok;
            if(not readn(sock, &is_protocol_version_ok, sizeof(is_protocol_version_ok), NULL)) {
                perror("Failed to receive protocol version ok");
                return;
            }
            if(not is_protocol_version_ok) {
                printf("Protocol version mismatch");
                return;
            }
            printf("is_protocol_version_ok: %d\n", is_protocol_version_ok);
        }
        // filename_buff_t filename_buffer;
        // strncpy(filename_buffer, config->filename, ARRAY_SIZE(filename_buffer));
        // if(not writen(sock, filename_buffer, ARRAY_SIZE(filename_buffer), NULL)) {
        //     perror("Failed to send filename buffer");
        //     return;
        // }
        // {
        //     FileSizeResult file_size_result;
        //     if(not readn(sock, &file_size_result, sizeof(file_size_result), NULL)) {
        //         perror("Failed to receive FileSizeResponseType");
        //         return;
        //     }
        //     if(file_size_result != FileSizeResult_OK) {
        //         printf("FileSizeResponseType: %d\n", file_size_result);
        //         return;
        //     }
        // }
        // size_t file_size;
        // if(not readn(sock, &file_size, sizeof(file_size), NULL)) {
        //     perror("Failed to receive file size");
        //     return;
        // }
        // file_size = be64toh(file_size);
        // printf("File size: %ld\n", file_size);
    }
    
    // {
    //     FileReceivingReadinessMessage message;
    //     message.data = file_data.host_file_size <= config->max_file_size;
        
    //     if() {
    //         fprintf(stderr, "File too large: %ld\n", file_data.host_file_size);
    //         if(send(sock, &readiness, sizeof(readiness), 0) == -1) {
    //             perror("Failed to send readiness");
    //             return;
    //         }
    //         return;
    //     }
    //     readiness = true;
    //     if(send(sock, &readiness, sizeof(readiness), 0) == -1) {
    //         perror("Failed to send readiness");
    //         return;
    //     }
    // }
    

    // {
    //     const int file_fd = open(config->filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    //     if (file_fd < 0) {
    //         perror("open");
    //         return;
    //     }
        
    //     const ssize_t res = splice(sock, NULL, file_fd, NULL, file_data.host_file_size, 0);
    //     if(res < 0) {
    //         perror("Failed to receive file data");
    //     } else if((size_t)res != file_data.host_file_size) {
    //         perror("Incomplete file transfer");
    //     }
    //     printf("Successfuly received file");
    //     close(file_fd);
    // }
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

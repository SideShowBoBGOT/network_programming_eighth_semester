#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdbool.h>
#include "protocol.h"

typedef struct {
    const char *address;
    int port;
    const char *filename;
    off_t max_file_size;
} ClientConfig;

static void print_config(const ClientConfig *config) {
    printf("Client Configuration:\n");
    printf("  Address: %s\n", config->address);
    printf("  Port: %d\n", config->port);
    printf("  Filename: %s\n", config->filename);
    printf("  Maximum file size: %lu\n", config->max_file_size);
}

static ClientConfig handle_cmd_args(const int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <server_address> <server_port> <filename> <max_file_size>\n", argv[0]);
        exit(1);
    }

    const ClientConfig config = {
        .address = argv[1],
        .port = atoi(argv[2]),
        .filename = argv[3],
        .max_file_size = atol(argv[4])
    };

    print_config(&config);

    return config;
}

static int create_and_connect_socket(const int port, const char* const address) {
    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        exit_err("socket() failed");
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, address, &server_addr.sin_addr) <= 0) {
        exit_err("inet_pton() failed");
    }

    if (connect(sock, (const struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        exit_err("connect() failed");
    }

    return sock;
}

static void send_receive_check_protocol_version(const int sock) {
    const int protocol_version = 1;
    send(sock, &protocol_version, sizeof(protocol_version), 0);

    bool match;
    recv(sock, &match, sizeof(match), 0);

    if(!match) {
        exit_err("Protocol version mismatch");
    }
}

static void send_filename_length_max_size(const int sock, const char* const filename, const off_t file_max_size) {
    const size_t filename_length = strlen(filename);
    const FileNameLengthMaxSize file_name_length_max_size = {filename_length, file_max_size};
    send(sock, &file_name_length_max_size, sizeof(file_name_length_max_size), 0);
    send(sock, filename, filename_length, 0);
}

static void check_operation_possibility(const int sock) {
    enum OperationPossibility operation_possibility;
    recv(sock, &operation_possibility, sizeof(operation_possibility), 0);
    switch (operation_possibility) {
        case FILE_NOT_FOUND: {
            exit_err("File not found");
        }
        case FILE_SIZE_GREATER_THAN_MAX_SIZE: {
            exit_err("File size greater than max size");
        }
        case FAILED_TO_OPEN_FILE: {
            exit_err("Failed to open file");
        }
        case OPERATION_POSSIBLE: break;
    }
}

static FileAndChunkSize receive_file_and_chunk_size(const int sock) {
    FileAndChunkSize file_and_chunk_size;
    recv(sock, &file_and_chunk_size, sizeof(file_and_chunk_size), 0);
    return file_and_chunk_size;
}

static void receive_file(
    const int sock,
    const FileAndChunkSize file_and_chunk_size,
    const char* const filename
) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file for writing");
        close(sock);
        exit(EXIT_FAILURE);
    }

    off_t received = 0;
    while (received < file_and_chunk_size.file_size) {
        char buffer[file_and_chunk_size.chunk_size];
        const int bytes = recv(sock, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;
        fwrite(buffer, 1, bytes, file);
        received += bytes;
    }

    fclose(file);
    close(sock);

    if (received == file_and_chunk_size.file_size) {
        printf("File received successfully.\n");
    } else {
        printf("Error: Incomplete file transfer.\n");
    }
}

int main(const int argc, char *argv[]) {
    const ClientConfig config = handle_cmd_args(argc, argv);
    const int sock = create_and_connect_socket(config.port, config.address);

    send_receive_check_protocol_version(sock);
    send_filename_length_max_size(sock, config.filename, config.max_file_size);
    check_operation_possibility(sock);
    const FileAndChunkSize file_and_chunk_size = receive_file_and_chunk_size(sock);
    receive_file(sock, file_and_chunk_size, config.filename);
    return EXIT_SUCCESS;
}
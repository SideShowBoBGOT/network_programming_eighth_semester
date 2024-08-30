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
    uint32_t max_file_size;
} ClientConfig;

static void print_config(const ClientConfig *config) {
    printf("Client Configuration:\n");
    printf("  Address: %s\n", config->address);
    printf("  Port: %d\n", config->port);
    printf("  Filename: %s\n", config->filename);
    printf("  Maximum Children: %u\n", config->max_file_size);
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

static void send_filename_length(const int sock, const char* const filename) {
    const FileNameLength send_info = {strlen(filename)};
    send(sock, &send_info, sizeof(send_info), 0);
    send(sock, filename, send_info.file_name_length, 0);
}

static FileSize receive_file_existence_and_size(const int sock) {
    FileExistence file_existence;
    recv(sock, &file_existence, sizeof(file_existence), 0);
    if (file_existence == FILE_NOT_FOUND) {
        printf("File not found on server.\n");
        close(sock);
        exit(1);
    }
    FileSize file_size;
    recv(sock, &file_size, sizeof(file_size), 0);
    printf("File size: %lu\n", file_size.size);
    return file_size;
}

static size_t send_receive_readiness(const int sock, const FileSize file_info, const uint32_t max_file_size) {
    const FileReceiveReadiness readiness = file_info.size > max_file_size ? REFUSE_TO_RECEIVE : READY_TO_RECEIVE;
    send(sock, &readiness, sizeof(readiness), 0);
    if(readiness == REFUSE_TO_RECEIVE) {
        printf("File size exceeds maximum allowed size.\n");
        close(sock);
        exit(1);
    }
    size_t chunk_size;
    recv(sock, &chunk_size, sizeof(chunk_size), 0);
    return chunk_size;
}

static void receive_file(
    const int sock,
    const FileSize file_info,
    const char* const filename,
    const size_t chunk_size
) {
    // Receive and save file content
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file for writing");
        close(sock);
        exit(1);
    }

    uint64_t received = 0;
    while (received < file_info.size) {
        char buffer[chunk_size];
        const int bytes = recv(sock, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;
        fwrite(buffer, 1, bytes, file);
        received += bytes;
    }

    fclose(file);
    close(sock);

    if (received == file_info.size) {
        printf("File received successfully.\n");
    } else {
        printf("Error: Incomplete file transfer.\n");
    }
}

int main(const int argc, char *argv[]) {
    const ClientConfig config = handle_cmd_args(argc, argv);
    const int sock = create_and_connect_socket(config.port, config.address);
    send_receive_check_protocol_version(sock);
    send_filename_length(sock, config.filename);
    const FileSize file_size = receive_file_existence_and_size(sock);
    const size_t chunk_size = send_receive_readiness(sock, file_size, config.max_file_size);
    receive_file(sock, file_size, config.filename, chunk_size);
    return EXIT_SUCCESS;
}
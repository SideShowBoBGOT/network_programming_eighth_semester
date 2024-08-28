#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <protocol/protocol.h>

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

static int create_and_bind_socket(const int port, const char* const address) {
    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, address, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        exit(1);
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(1);
    }

    return sock;
}

static void send_receive_check_protocol_version(const int sock) {
    const int protocol_version = 1;
    send(sock, &protocol_version, sizeof(protocol_version), 0);

    bool match;
    recv(sock, &match, sizeof(match), 0);

    if(!match) {
        printf("Protocol version mismatch");
        exit(1);
    }
}

static void send_filename_length(const int sock, const char* const filename) {
    const send_info_t send_info = {strlen(filename)};
    send(sock, &send_info, sizeof(send_info), 0);
    send(sock, filename, send_info.file_name_length, 0);
}

static file_size_t receive_file_info(const int sock) {
    file_existence_t file_existence;
    recv(sock, &file_existence, sizeof(file_existence), 0);
    if (file_existence == FILE_NOT_FOUND) {
        printf("File not found on server.\n");
        close(sock);
        exit(1);
    }
    file_size_t file_size;
    recv(sock, &file_size, sizeof(file_size), 0);
    printf("File size: %lu\n", file_size.size);
    return file_size;
}

static size_t send_receive_readiness(const int sock, const file_size_t file_info, const uint32_t max_file_size) {
    const file_receive_readiness_t readiness = file_info.size > max_file_size ? REFUSE_TO_RECEIVE : READY_TO_RECEIVE;
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
    const file_size_t file_info,
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
    const int sock = create_and_bind_socket(config.port, config.address);
    send_receive_check_protocol_version(sock);
    send_filename_length(sock, config.filename);
    const file_size_t file_size = receive_file_info(sock);
    const size_t chunk_size = send_receive_readiness(sock, file_size, config.max_file_size);
    receive_file(sock, file_size, config.filename, chunk_size);
    return EXIT_SUCCESS;
}
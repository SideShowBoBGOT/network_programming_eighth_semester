#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <iterative_server_utils/iterative_server_utils.h>

int main(const int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <server_address> <server_port> <filename> <max_file_size>\n", argv[0]);
        exit(1);
    }

    const char *server_address = argv[1];
    const int server_port = atoi(argv[2]);
    const char *filename = argv[3];
    const uint64_t max_file_size = strtoull(argv[4], NULL, 10);

    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_address, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        exit(1);
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(1);
    }

    // Send file request
    struct message_header header = {PROTOCOL_VERSION, REQUEST_FILE, strlen(filename)};
    send(sock, &header, sizeof(header), 0);
    send(sock, filename, header.length, 0);

    // Receive file info
    recv(sock, &header, sizeof(header), 0);
    if (header.type == FILE_NOT_FOUND) {
        printf("File not found on server.\n");
        close(sock);
        return 1;
    }
    if (header.type == ERROR) {
        struct error_info error;
        recv(sock, &error, sizeof(error), 0);
        printf("Server error: %d\n", error.error_code);
        close(sock);
        return 1;
    }

    struct file_info file_info;
    recv(sock, &file_info, sizeof(file_info), 0);

    if (file_info.size > max_file_size) {
        header.type = REFUSE_TO_RECEIVE;
        send(sock, &header, sizeof(header), 0);
        printf("File size exceeds maximum allowed size.\n");
        close(sock);
        return 1;
    }

    header.type = READY_TO_RECEIVE;
    send(sock, &header, sizeof(header), 0);

    // Receive and save file content
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file for writing");
        close(sock);
        return 1;
    }

    uint64_t received = 0;
    while (received < file_info.size) {
        char buffer[CHUNK_SIZE];
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

    return 0;
}
#include <iterative_server_utils/iterative_server_utils.h>

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

void exit_err(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void warn_err(const char *msg) {
    perror(msg);
}

void handle_client(const int client_sock, const char* const dir_path) {
    struct message_header header;
    recv(client_sock, &header, sizeof(header), 0);

    if (header.version != PROTOCOL_VERSION) {
        printf("Protocol version mismatch.\n");
        close(client_sock);
        return;
    }

    if (header.type != REQUEST_FILE) {
        printf("Unexpected message type.\n");
        close(client_sock);
        return;
    }

    char filename[MAX_FILENAME_LENGTH + 1];
    recv(client_sock, filename, header.length, 0);
    filename[header.length] = '\0';

    // Check if filename is valid
    if (strchr(filename, '/') != NULL) {
        header.type = ERROR;
        send(client_sock, &header, sizeof(header), 0);
        struct error_info error = {1}; // 1 for invalid filename
        send(client_sock, &error, sizeof(error), 0);
        close(client_sock);
        return;
    }

    char filepath[PATH_MAX];
    snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, filename);

    struct stat st;
    if (stat(filepath, &st) == -1 || !S_ISREG(st.st_mode)) {
        header.type = FILE_NOT_FOUND;
        send(client_sock, &header, sizeof(header), 0);
        close(client_sock);
        return;
    }

    header.type = FILE_INFO;
    send(client_sock, &header, sizeof(header), 0);

    struct file_info file_info = {st.st_size};
    send(client_sock, &file_info, sizeof(file_info), 0);

    recv(client_sock, &header, sizeof(header), 0);
    if (header.type != READY_TO_RECEIVE) {
        close(client_sock);
        return;
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
        printf("Process %d read bytes: %lu\n", getpid(), bytes_read);
        send(client_sock, buffer, bytes_read, 0);
    }

    fclose(file);
    close(client_sock);
}

#define MAX_BACKLOG 10

int create_and_bind_socket(const IterativeServerConfig *config) {
    const int listenfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
    if (listenfd < 0)
        exit_err("socket()");

    struct sockaddr_in srv_sin4 = {0};
    srv_sin4.sin_family = AF_INET;
    srv_sin4.sin_port = htons(config->port);
    srv_sin4.sin_addr.s_addr = inet_addr(config->address);

    if (bind(listenfd, (struct sockaddr *)&srv_sin4, sizeof(srv_sin4)) < 0)
        exit_err("bind()");

    if (listen(listenfd, MAX_BACKLOG) < 0)
        exit_err("listen()");

    printf("Server listening on %s:%d\n", config->address, config->port);
    return listenfd;
}

int accept_connection(const int listenfd) {
    struct sockaddr_in cln_sin4;
    socklen_t addrlen = sizeof(cln_sin4);
    const int connfd = accept(listenfd, (struct sockaddr *)&cln_sin4, &addrlen);
    if (connfd < 0) {
        return -1;
    }
    printf("New connection from %s:%d\n", inet_ntoa(cln_sin4.sin_addr), ntohs(cln_sin4.sin_port));
    return connfd;
}
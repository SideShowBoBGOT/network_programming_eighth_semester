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
#include <stdbool.h>

#include <protocol/protocol.h>

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

static bool check_filename(
    const int client_sock,
    const send_info_t send_info,
    const char* const dir_path,
    char* filepath,
    const size_t file_path_max_length
) {
    char filename[MAX_FILENAME_LENGTH];
    recv(client_sock, filename, send_info.file_name_length, 0);
    printf("Received filename: %s\n", filename);
    filename[send_info.file_name_length] = '\0';

    snprintf(filepath, file_path_max_length, "%s/%s", dir_path, filename);

    struct stat st;
    if (stat(filepath, &st) == -1 || !S_ISREG(st.st_mode)) {
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
    receive_send_check_protocol_version(client_sock);

    send_info_t send_info;
    recv(client_sock, &send_info, sizeof(send_info), 0);

    char filepath[PATH_MAX];
    if(!check_filename(client_sock, send_info, dir_path, filepath, sizeof(filepath))) {
        return;
    }
    if(!check_file_readiness(client_sock)) {
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
        // printf("Process %d read bytes: %lu\n", getpid(), bytes_read);
        send(client_sock, buffer, bytes_read, 0);
    }

    fclose(file);
    close(client_sock);
}

#define MAX_BACKLOG 10

int create_and_bind_socket(const int port, const char* const address) {
    const int listenfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
    if (listenfd < 0)
        exit_err("socket()");

    // Проблема була у тому, що після перезапуску сервера я не міг перевикористати той самий сокет.
    // Як пояснила документація: треба явно прописати перевикористання.
    {
        const int optval = 1;
        if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
            exit_err("setsockopt(SO_REUSEADDR)");
    }

    struct sockaddr_in srv_sin4 = {0};
    srv_sin4.sin_family = AF_INET;
    srv_sin4.sin_port = htons(port);
    srv_sin4.sin_addr.s_addr = inet_addr(address);

    if (bind(listenfd, (struct sockaddr *)&srv_sin4, sizeof(srv_sin4)) < 0)
        exit_err("bind()");

    if (listen(listenfd, MAX_BACKLOG) < 0)
        exit_err("listen()");

    printf("Server listening on %s:%d\n", address, port);
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
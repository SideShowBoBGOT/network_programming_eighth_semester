#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include "protocol.h"

typedef struct {
    const char *address;
    int port;
    const char *dir_path;
    uint32_t max_clients;
} IterativeServerConfig;



// static bool receive_send_check_protocol_version(const int client_sock) {
//     int client_protocol_version = 0;
//     recv(client_sock, &client_protocol_version, sizeof(client_protocol_version), 0);
//     const int server_protocol_version = 1;
//     const bool protocol_version_match = client_protocol_version == server_protocol_version;
//     send(client_sock, &protocol_version_match, sizeof(protocol_version_match), 0);
//     if(!protocol_version_match) {
//         printf("Protocol version mismatch.\n");
//         close(client_sock);
//         return false;
//     }
//     return true;
// }
//
// #define MAX_FILENAME_LENGTH 256
//
// static bool check_filename(
//     const int client_sock,
//     const send_info_t send_info,
//     const char* const dir_path,
//     char* filepath,
//     const size_t file_path_max_length
// ) {
//     char filename[MAX_FILENAME_LENGTH];
//     recv(client_sock, filename, send_info.file_name_length, 0);
//     printf("Received filename: %s\n", filename);
//     filename[send_info.file_name_length] = '\0';
//
//     snprintf(filepath, file_path_max_length, "%s/%s", dir_path, filename);
//
//     struct stat st;
//     if (stat(filepath, &st) == -1 || !S_ISREG(st.st_mode)) {
//         const file_existence_t file_existence = FILE_NOT_FOUND;
//         send(client_sock, &file_existence, sizeof(file_existence), 0);
//         close(client_sock);
//         return false;
//     }
//     const file_existence_t file_existence = FILE_FOUND;
//     send(client_sock, &file_existence, sizeof(file_existence), 0);
//     const file_size_t file_size = {.size = st.st_size};
//     printf("File size: %lu\n", file_size.size);
//     send(client_sock, &file_size, sizeof(file_size), 0);
//     return true;
// }
//
// const int CHUNK_SIZE = 4096;
//
// static bool check_file_readiness(const int client_sock) {
//     file_receive_readiness_t file_receive_readiness;
//     recv(client_sock, &file_receive_readiness, sizeof(file_receive_readiness), 0);
//     if (file_receive_readiness == REFUSE_TO_RECEIVE) {
//         printf("Client refused to receive file.\n");
//         close(client_sock);
//         return false;
//     }
//     send(client_sock, &CHUNK_SIZE, sizeof(CHUNK_SIZE), 0);
//     return true;
// }
//
// static void handle_client(
//     const int client_sock,
//     const char* const dir_path
// ) {
//     receive_send_check_protocol_version(client_sock);
//
//     send_info_t send_info;
//     recv(client_sock, &send_info, sizeof(send_info), 0);
//
//     char filepath[PATH_MAX];
//     if(!check_filename(client_sock, send_info, dir_path, filepath, sizeof(filepath))) {
//         return;
//     }
//     if(!check_file_readiness(client_sock)) {
//         return;
//     }
//
//     FILE *file = fopen(filepath, "rb");
//     if (!file) {
//         perror("Failed to open file");
//         close(client_sock);
//         return;
//     }
//
//     char buffer[CHUNK_SIZE];
//     size_t bytes_read;
//     while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
//         // printf("Process %d read bytes: %lu\n", getpid(), bytes_read);
//         send(client_sock, buffer, bytes_read, 0);
//     }
//
//     fclose(file);
//     close(client_sock);
// }
//
static int accept_connection(const int listenfd) {
    struct sockaddr_in cln_sin4;
    socklen_t addrlen = sizeof(cln_sin4);
    const int conn_fd = accept(listenfd, (struct sockaddr *)&cln_sin4, &addrlen);
    if (conn_fd < 0) {
        return -1;
    }
    printf("New connection from %s:%d\n", inet_ntoa(cln_sin4.sin_addr), ntohs(cln_sin4.sin_port));
    return conn_fd;
}

static void print_config(const IterativeServerConfig *config) {
    printf("Server Configuration:\n");
    printf("  Address: %s\n", config->address);
    printf("  Port: %d\n", config->port);
    printf("  Directory Path: %s\n", config->dir_path);
    printf("  Max clients: %u\n", config->max_clients);
}

static IterativeServerConfig handle_cmd_args(const int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <server_address> <server_port> <directory_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const IterativeServerConfig config = {
        .address = argv[1],
        .port = atoi(argv[2]),
        .dir_path = argv[3],
        .max_clients = atoi(argv[4])
    };

    print_config(&config);

    return config;
}

static int create_and_bind_socket(const int port, const char* const address, const int backlog) {
    struct sockaddr_in srv_sin4 = {0};
    srv_sin4.sin_family = AF_INET;
    srv_sin4.sin_port = htons(port);

    const int listenfd = socket(srv_sin4.sin_family, SOCK_STREAM, IPPROTO_TCP);
    if (listenfd < 0)
        exit_err("socket()");

    if(inet_pton( srv_sin4.sin_family, address, &srv_sin4.sin_addr) < 0) {
        exit_err("inet_pton()");
    }

    if (bind(listenfd, (const struct sockaddr *)&srv_sin4, sizeof(srv_sin4)) < 0)
        exit_err("bind()");

    if (listen(listenfd, backlog) < 0)
        exit_err("listen()");

    printf("Server listening on %s:%d\n", address, port);
    return listenfd;
}

volatile sig_atomic_t keep_running = 1;
static void handle_sigint(const int) { keep_running = 0; }

typedef struct {} ClientState_Invalid;
typedef struct { int client_fd; } ClientState_ReceiveProtocolVersion;
typedef struct { int client_fd; } ClientState_SendMatchProtocolVersion;
typedef struct { int client_fd; } ClientState_ReceiveFileNameLength;
typedef struct { int client_fd; } ClientState_SendFileExistence;

typedef enum {
    ClientStateTag_INVALID,
    ClientState_RECEIVE_PROTOCOL_VERSION,
    ClientState_SEND_MATCH_PROTOCOL_VERSION,
    ClientState_SEND_FILE_NAME_LENGTH,
    ClientState_SEND_FILE_EXISTENCE,

} ClientStateTag;

typedef struct {
    ClientStateTag tag;
    union {
        ClientState_Invalid invalid;
        ClientState_ReceiveProtocolVersion receive_protocol_version;
        ClientState_SendMatchProtocolVersion send_match_protocol_version;
    } value;
} ClientState;

int ClientState_client_fd(const ClientState* client_state) {
    switch (client_state->tag) {
        case ClientStateTag_INVALID: {
            exit_err("Can not get client_fd from INVALID state");
        }
        case ClientState_RECEIVE_PROTOCOL_VERSION: return client_state->value.receive_protocol_version.client_fd;
        case ClientState_SEND_MATCH_PROTOCOL_VERSION: return client_state->value.send_match_protocol_version.client_fd;
    }
}

ClientState ClientState_transition(
    const ClientState* const generic_state,
    const fd_set* const readfds,
    const fd_set* const writefds
) {
    switch (generic_state->tag) {
        case ClientStateTag_INVALID: return *generic_state;
        case ClientState_RECEIVE_PROTOCOL_VERSION: {
            const ClientState_ReceiveProtocolVersion* cur_state = &generic_state->value.receive_protocol_version;
            if(FD_ISSET(cur_state->client_fd, readfds)) {
                int client_protocol_version = 0;
                recv(cur_state->client_fd, &client_protocol_version, sizeof(client_protocol_version), 0);
                ClientState next_state;
                next_state.tag = ClientState_SEND_MATCH_PROTOCOL_VERSION;
                next_state.value.send_match_protocol_version.client_fd = cur_state->client_fd;
                return next_state;
            }
            return *generic_state;
        }
    }
}

int set_read_write_max_fds(
    fd_set *readfds,
    fd_set *writefds,
    const int serverfd,
    const ClientState* client_states,
    const size_t client_sockets_size
) {
    FD_ZERO(readfds);
    FD_ZERO(writefds);
    FD_SET(serverfd, readfds);
    int max_sd = serverfd;
    for (int i = 0; i < client_sockets_size; ++i) {
        const ClientState* state = &client_states[i];
        if (state->tag != ClientStateTag_INVALID) {
            const int client_fd = ClientState_client_fd(state);
            FD_SET(client_fd, readfds);
            FD_SET(client_fd, writefds);
            if (client_fd > max_sd) {
                max_sd = client_fd;
            }
        }
    }
    return max_sd;
}

static void check_accept_connection(
    const fd_set *readfds,
    const int serverfd,
    ClientState* client_states,
    const size_t client_states_size
) {
    if (FD_ISSET(serverfd, readfds)) {
        struct sockaddr_in address;
        socklen_t addr_len = sizeof(address);
        const int client_fd = accept(serverfd, (struct sockaddr *)&address, &addr_len);
        if (client_fd < 0) {
            exit_err("accept()");
        }

        printf("New connection, socket fd is %d, IP is: %s, port: %d\n",
               client_fd, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

        for (int i = 0; i < client_states_size; i++) {
            if (client_states[i].tag == ClientStateTag_INVALID) {
                client_states[i].tag = ClientState_RECEIVE_PROTOCOL_VERSION;
                client_states[i].value.receive_protocol_version.client_fd = client_fd;
                printf("Adding to list of sockets as %d\n", i);
                break;
            }
        }
    }
}

static void update_sets(
    fd_set *readfds,
    fd_set *writefds,
    const int serverfd,
    const ClientState* client_sockets,
    const size_t client_sockets_size
) {
    const int max_fd = set_read_write_max_fds(readfds, writefds, serverfd, client_sockets, client_sockets_size);
    const int activity = select(max_fd + 1, readfds, writefds, NULL, NULL);
    if (activity < 0 && errno != EINTR) {
        printf("select error");
    }
}

int main(const int argc, const char *argv[]) {
    signal(SIGINT, handle_sigint);
    const IterativeServerConfig config = handle_cmd_args(argc, argv);
    const int server_fd = create_and_bind_socket(config.port, config.address, config.max_clients);

    fd_set readfds;
    fd_set writefds;
    ClientState client_sockets[config.max_clients];
    for (int i = 0; i < config.max_clients; ++i) {
        client_sockets[i].tag = ClientStateTag_INVALID;
    }

    while (keep_running) {
        update_sets(&readfds, &writefds, server_fd, client_sockets, config.max_clients);
        check_accept_connection(&readfds, server_fd, client_sockets, config.max_clients);

        for (int i = 0; i < config.max_clients; i++) {
            ClientState* state = &client_sockets[i];
            *state = ClientState_transition(state, &readfds, &writefds);
        }
    }

    close(server_fd);
    return EXIT_SUCCESS;
}


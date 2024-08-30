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

static void print_config(const IterativeServerConfig *config) {
    printf("Server Configuration:\n");
    printf("  Address: %s\n", config->address);
    printf("  Port: %d\n", config->port);
    printf("  Directory Path: %s\n", config->dir_path);
    printf("  Max clients: %u\n", config->max_clients);
}

static IterativeServerConfig handle_cmd_args(const int argc, const char * const * const argv) {
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

    const int listenfd = socket(srv_sin4.sin_family, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
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

struct OwningString {
    char* buffer;
    size_t size;
};

struct FilePath { struct OwningString value; };

struct ClientState_Invalid {};
struct ClientState_ReceiveProtocolVersion {
    int client_fd;
};
struct ClientState_SendMatchProtocolVersion {
    int client_fd;
    int client_protocol_version;
};
struct ClientState_ReceiveFileNameLengthMaxSize {
    int client_fd;
} ;
struct ClientState_ReceiveFileName {
    int client_fd;
    FileNameLengthMaxSize file_name_length_max_size;
};
struct ClientState_SendFileOperationPossibility {
    int client_fd;
    enum OperationPossibility operation_possibility;
    FILE* file;
    off_t file_size;
};

struct ClientState_SendFileAndChunkSize {
    int client_fd;
    FILE* file;
    off_t file_size;
};

struct ClientState_SendFileChunk {
    int client_fd;
    FILE* file;
};

struct ClientState_DropConnection {
    int client_fd;
};

typedef enum {
    ClientStateTag_INVALID,
    ClientState_RECEIVE_PROTOCOL_VERSION,
    ClientState_SEND_MATCH_PROTOCOL_VERSION,
    ClientState_RECEIVE_FILE_NAME_LENGTH_MAX_SIZE,
    ClientState_RECEIVE_FILE_NAME,
    ClientState_SEND_FILE_OPERATION_POSSIBILITY,
    ClientState_SEND_FILE_AND_CHUNK_SIZE,
    ClientState_SEND_FILE_CHUNK,
    ClientState_DROP_CONNECTION
} ClientStateTag;

typedef struct {
    ClientStateTag tag;
    union {
        struct ClientState_Invalid invalid;
        struct ClientState_ReceiveProtocolVersion receive_protocol_version;
        struct ClientState_SendMatchProtocolVersion send_match_protocol_version;
        struct ClientState_ReceiveFileNameLengthMaxSize receive_file_name_length;
        struct ClientState_ReceiveFileName receive_file_name;
        struct ClientState_SendFileOperationPossibility send_file_operation_possibility;
        struct ClientState_SendFileAndChunkSize send_file_and_chunk_size;
        struct ClientState_SendFileChunk send_file_chunk;
        struct ClientState_DropConnection drop_connection;
    } value;
} ClientState;

int ClientState_client_fd(const ClientState* client_state) {
    switch (client_state->tag) {
        case ClientStateTag_INVALID: {
            exit_err("Can not get client_fd from INVALID state");
        }
        case ClientState_RECEIVE_PROTOCOL_VERSION:
            return client_state->value.receive_protocol_version.client_fd;
        case ClientState_SEND_MATCH_PROTOCOL_VERSION:
            return client_state->value.send_match_protocol_version.client_fd;
        case ClientState_RECEIVE_FILE_NAME_LENGTH_MAX_SIZE:
            return client_state->value.receive_file_name_length.client_fd;
        case ClientState_RECEIVE_FILE_NAME:
            return client_state->value.receive_file_name.client_fd;
        case ClientState_SEND_FILE_OPERATION_POSSIBILITY:
            return client_state->value.send_file_operation_possibility.client_fd;
        case ClientState_SEND_FILE_AND_CHUNK_SIZE:
            return client_state->value.send_file_and_chunk_size.client_fd;
        case ClientState_SEND_FILE_CHUNK:
            return client_state->value.send_file_chunk.client_fd;
        case ClientState_DROP_CONNECTION:
            return client_state->value.drop_connection.client_fd;
        default:
            __builtin_unreachable();
    }
}

#define NAME_MAX_WITHOUT_NULL_TERMINATOR NAME_MAX
#define PATH_MAX_WITHOUT_NULL_TERMINATOR PATH_MAX

static ClientState construct_drop_connection(const int client_fd) {
    ClientState new_state;
    new_state.tag = ClientState_DROP_CONNECTION;
    new_state.value.drop_connection.client_fd = client_fd;
    return new_state;
}

#define CHUNK_SIZE 1

ClientState ClientState_transition(
    const ClientState* const generic_state,
    const fd_set* const readfds,
    const fd_set* const writefds,
    const char* const dir_path
) {
    switch (generic_state->tag) {
        case ClientStateTag_INVALID: return *generic_state;
        case ClientState_RECEIVE_PROTOCOL_VERSION: {
            const struct ClientState_ReceiveProtocolVersion* const cur_state = &generic_state->value.receive_protocol_version;
            if(FD_ISSET(cur_state->client_fd, readfds)) {
                ClientState next_state;
                next_state.tag = ClientState_SEND_MATCH_PROTOCOL_VERSION;
                next_state.value.send_match_protocol_version.client_fd = cur_state->client_fd;
                recv(cur_state->client_fd,
                    &next_state.value.send_match_protocol_version.client_protocol_version,
                    sizeof(next_state.value.send_match_protocol_version.client_protocol_version),
                    0
                );
                return next_state;
            }
            return *generic_state;
        }
        case ClientState_SEND_MATCH_PROTOCOL_VERSION: {
            const struct ClientState_SendMatchProtocolVersion* const cur_state = &generic_state->value.send_match_protocol_version;
            if(FD_ISSET(cur_state->client_fd, writefds)) {
                const int server_protocol_version = 1;
                const bool protocol_version_match = cur_state->client_protocol_version == server_protocol_version;
                if(send(cur_state->client_fd, &protocol_version_match, sizeof(protocol_version_match), 0) == -1) {
                    return construct_drop_connection(cur_state->client_fd);
                }
                if(!protocol_version_match) {
                    return construct_drop_connection(cur_state->client_fd);
                }
                ClientState next_state;
                next_state.tag = ClientState_RECEIVE_FILE_NAME_LENGTH_MAX_SIZE;
                next_state.value.receive_file_name_length.client_fd = cur_state->client_fd;
                return next_state;
            }
            return *generic_state;
        }
        case ClientState_RECEIVE_FILE_NAME_LENGTH_MAX_SIZE: {
            const struct ClientState_ReceiveFileNameLengthMaxSize* const cur_state = &generic_state->value.receive_file_name_length;
            if(FD_ISSET(cur_state->client_fd, readfds)) {
                ClientState next_state;
                next_state.tag = ClientState_RECEIVE_FILE_NAME;
                next_state.value.receive_file_name.client_fd = cur_state->client_fd;

                recv(
                    cur_state->client_fd,
                    &next_state.value.receive_file_name.file_name_length_max_size,
                    sizeof(next_state.value.receive_file_name.file_name_length_max_size),
                    0
                );
                return next_state;
            }
            return *generic_state;
        }
        case ClientState_RECEIVE_FILE_NAME: {
            const struct ClientState_ReceiveFileName* const cur_state = &generic_state->value.receive_file_name;
            if(FD_ISSET(cur_state->client_fd, readfds)) {
                char name_buffer[NAME_MAX_WITHOUT_NULL_TERMINATOR + 1] = {0};
                recv(cur_state->client_fd, name_buffer, sizeof(name_buffer), 0);
                name_buffer[NAME_MAX_WITHOUT_NULL_TERMINATOR] = '\0';

                char path_buffer[PATH_MAX_WITHOUT_NULL_TERMINATOR] = {0};
                snprintf(path_buffer, PATH_MAX_WITHOUT_NULL_TERMINATOR, "%s/%s", dir_path, name_buffer);

                ClientState next_state;
                next_state.tag = ClientState_SEND_FILE_OPERATION_POSSIBILITY;
                next_state.value.send_file_operation_possibility.client_fd = cur_state->client_fd;
                next_state.value.send_file_operation_possibility.file = NULL;
                next_state.value.send_file_operation_possibility.operation_possibility = OPERATION_POSSIBLE;

                struct stat st;
                if(stat(path_buffer, &st) == -1 || !S_ISREG(st.st_mode)) {
                    next_state.value.send_file_operation_possibility.operation_possibility = FILE_NOT_FOUND;
                } else if(cur_state->file_name_length_max_size.file_max_size < st.st_size) {
                    next_state.value.send_file_operation_possibility.operation_possibility = FILE_SIZE_GREATER_THAN_MAX_SIZE;
                } else {
                    next_state.value.send_file_operation_possibility.file = fopen(path_buffer, "rb");
                    if(!next_state.value.send_file_operation_possibility.file) {
                        next_state.value.send_file_operation_possibility.operation_possibility = FAILED_TO_OPEN_FILE;
                    }
                }
                next_state.value.send_file_operation_possibility.file_size = st.st_size;
                return next_state;
            }
            return *generic_state;
        }
        case ClientState_SEND_FILE_OPERATION_POSSIBILITY: {
            const struct ClientState_SendFileOperationPossibility* const cur_state = &generic_state->value.send_file_operation_possibility;
            if(FD_ISSET(cur_state->client_fd, writefds)) {
                if(send(cur_state->client_fd, &cur_state->operation_possibility, sizeof(cur_state->operation_possibility), 0) == -1) {
                    return construct_drop_connection(cur_state->client_fd);
                }
                if(cur_state->operation_possibility != OPERATION_POSSIBLE) {
                    return construct_drop_connection(cur_state->client_fd);
                }
                ClientState next_state;
                next_state.tag = ClientState_SEND_FILE_AND_CHUNK_SIZE;
                next_state.value.send_file_and_chunk_size.client_fd = cur_state->client_fd;
                next_state.value.send_file_and_chunk_size.file = cur_state->file;
                next_state.value.send_file_and_chunk_size.file_size = cur_state->file_size;
                return next_state;
            }
            return *generic_state;
        }
        case ClientState_SEND_FILE_AND_CHUNK_SIZE: {
            const struct ClientState_SendFileAndChunkSize* const cur_state = &generic_state->value.send_file_and_chunk_size;
            if(FD_ISSET(cur_state->client_fd, writefds)) {
                const FileAndChunkSize file_and_chunk_size = {cur_state->file_size, CHUNK_SIZE};
                if(send(cur_state->client_fd, &file_and_chunk_size, sizeof(file_and_chunk_size), 0) == -1) {
                    return construct_drop_connection(cur_state->client_fd);
                }
                ClientState next_state;
                next_state.tag = ClientState_SEND_FILE_CHUNK;
                next_state.value.send_file_chunk.client_fd = cur_state->client_fd;
                next_state.value.send_file_chunk.file = cur_state->file;
                return next_state;
            }
            return *generic_state;
        }
        case ClientState_SEND_FILE_CHUNK: {
            const struct ClientState_SendFileChunk* const cur_state = &generic_state->value.send_file_chunk;
            if(FD_ISSET(cur_state->client_fd, writefds)) {
                char buffer[CHUNK_SIZE];
                size_t bytes_read;
                while ((bytes_read = fread(buffer, 1, sizeof(buffer), cur_state->file)) > 0) {
                    // printf("Read bytes: %lu\n", bytes_read);
                    if(send(cur_state->client_fd, buffer, bytes_read, 0) == -1) {
                        return construct_drop_connection(cur_state->client_fd);
                    }
                }
                fclose(cur_state->file);
                return construct_drop_connection(cur_state->client_fd);
            }
            return *generic_state;
        }
        case ClientState_DROP_CONNECTION: {
            const struct ClientState_DropConnection* const cur_state = &generic_state->value.drop_connection;
            close(cur_state->client_fd);
            ClientState next_state;
            next_state.tag = ClientStateTag_INVALID;
            return next_state;
        }
        default:
            __builtin_unreachable();
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
    for (size_t i = 0; i < client_sockets_size; ++i) {
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
        if (client_fd == -1) {
            if(errno != EAGAIN && errno != EWOULDBLOCK) {
                exit_err("accept()");
            }
            return;
        }

        printf("New connection, socket fd is %d, IP is: %s, port: %d\n",
               client_fd, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

        for (size_t i = 0; i < client_states_size; ++i) {
            if (client_states[i].tag == ClientStateTag_INVALID) {
                client_states[i].tag = ClientState_RECEIVE_PROTOCOL_VERSION;
                client_states[i].value.receive_protocol_version.client_fd = client_fd;
                printf("Adding to list of sockets as %lu\n", i);
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

int main(const int argc, const char* const argv[]) {
    signal(SIGINT, handle_sigint);
    signal(SIGPIPE, SIG_IGN);
    const IterativeServerConfig config = handle_cmd_args(argc, argv);
    const int server_fd = create_and_bind_socket(config.port, config.address, config.max_clients);

    fd_set readfds;
    fd_set writefds;
    ClientState client_sockets[config.max_clients];
    for (size_t i = 0; i < config.max_clients; ++i) {
        client_sockets[i].tag = ClientStateTag_INVALID;
    }

    while (keep_running) {
        update_sets(&readfds, &writefds, server_fd, client_sockets, config.max_clients);
        check_accept_connection(&readfds, server_fd, client_sockets, config.max_clients);

        for (size_t i = 0; i < config.max_clients; ++i) {
            ClientState* state = &client_sockets[i];
            *state = ClientState_transition(state, &readfds, &writefds, config.dir_path);
        }
    }

    close(server_fd);
    return EXIT_SUCCESS;
}
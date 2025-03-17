#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>

#include "client_utils.h"

struct ClientState_Invalid {};
struct ClientState_ReceiveProtocolVersion {
    int32_t client_fd;
};
struct ClientState_SendMatchProtocolVersion {
    int32_t client_fd;
    int32_t client_protocol_version;
};
struct ClientState_ReceiveFileName {
    int32_t client_fd;
};
struct ClientState_SendFileOperationPossibility {
    int32_t client_fd;
    filename_buff_t filename;
};
struct ClientState_SendChunkAndFileSize {
    int32_t client_fd;
    int32_t fd;
    off_t file_size;
};
struct ClientState_ReceiveClientReady {
    int32_t client_fd;
    int32_t fd;
};


struct ClientState_SendFileChunk {
    int32_t client_fd;
    int32_t fd;
};

struct ClientState_DropConnection {
    int32_t client_fd;
};

typedef enum {
    ClientStateTag_INVALID,
    ClientState_RECEIVE_PROTOCOL_VERSION,
    ClientState_SEND_MATCH_PROTOCOL_VERSION,
    ClientState_RECEIVE_FILE_NAME,
    ClientState_SEND_FILE_OPERATION_POSSIBILITY,
    ClientState_SEND_CHUNK_AND_FILE_SIZE,
    ClientState_RECEIVE_CLIENT_READY,

    ClientState_SEND_FILE_CHUNK,
    ClientState_DROP_CONNECTION
} ClientStateTag;

typedef struct {
    ClientStateTag tag;
    union {
        struct ClientState_Invalid invalid;
        struct ClientState_ReceiveProtocolVersion receive_protocol_version;
        struct ClientState_SendMatchProtocolVersion send_match_protocol_version;
        struct ClientState_ReceiveFileName receive_file_name;
        struct ClientState_SendFileOperationPossibility send_file_operation_possibility;
        struct ClientState_SendChunkAndFileSize send_chunk_and_file_size;
        struct ClientState_ReceiveClientReady receive_client_ready;

        struct ClientState_SendFileChunk send_file_chunk;
        struct ClientState_DropConnection drop_connection;
    } value;
} ClientState;

int ClientState_client_fd(const ClientState* client_state) {
    switch (client_state->tag) {
        case ClientStateTag_INVALID: {
            __builtin_unreachable();
        }
        case ClientState_RECEIVE_PROTOCOL_VERSION:
            return client_state->value.receive_protocol_version.client_fd;
        case ClientState_SEND_MATCH_PROTOCOL_VERSION:
            return client_state->value.send_match_protocol_version.client_fd;
        case ClientState_RECEIVE_FILE_NAME:
            return client_state->value.receive_file_name.client_fd;
        case ClientState_SEND_FILE_OPERATION_POSSIBILITY:
            return client_state->value.send_file_operation_possibility.client_fd;
        case ClientState_SEND_CHUNK_AND_FILE_SIZE:
            return client_state->value.send_chunk_and_file_size.client_fd;
        case ClientState_RECEIVE_CLIENT_READY:
            return client_state->value.receive_client_ready.client_fd;
        case ClientState_SEND_FILE_CHUNK:
            return client_state->value.send_file_chunk.client_fd;
        case ClientState_DROP_CONNECTION:
            return client_state->value.drop_connection.client_fd;
        default:
            __builtin_unreachable();
    }
}

static void drop_connection(
    ClientStateTag *const tag,
    const int client_fd
) {
    *tag = ClientStateTag_INVALID;
    checked_close(client_fd);
}

#define CHUNK_SIZE 4096

void ClientState_transition(
    ClientState* const state,
    const fd_set* const readfds,
    const fd_set* const writefds,
    const char* const dir_path
) {
    switch (state->tag) {
        case ClientStateTag_INVALID: {
            return;
        }
        case ClientState_RECEIVE_PROTOCOL_VERSION: {
            const struct ClientState_ReceiveProtocolVersion* const cur_state = &state->value.receive_protocol_version;
            if(!FD_ISSET(cur_state->client_fd, readfds)) {
                return;
            }
            printf("Client fd %d is at ClientState_RECEIVE_PROTOCOL_VERSION\n", cur_state->client_fd);

            uint8_t client_protocol_version;
            if(not checked_read(cur_state->client_fd, &client_protocol_version, sizeof(client_protocol_version), NULL)) {
                printf("[Client_sock: %d] [Failed to receive request] [errno: %d] [strerror: %s]\n", cur_state->client_fd, errno, strerror(errno));
                drop_connection(&state->tag, cur_state->client_fd);
                return;
            }

            state->tag = ClientState_SEND_MATCH_PROTOCOL_VERSION;
            state->value.send_match_protocol_version.client_fd = cur_state->client_fd;
            return;
        }
        case ClientState_SEND_MATCH_PROTOCOL_VERSION: {
            const struct ClientState_SendMatchProtocolVersion* const cur_state = &state->value.send_match_protocol_version;
            if(!FD_ISSET(cur_state->client_fd, writefds)) {
                return;
            }
            printf("[Client_sock: %d] [ClientState_SEND_MATCH_PROTOCOL_VERSION]\n", cur_state->client_fd);
            const bool is_protocol_match = cur_state->client_protocol_version == PROTOCOL_VERSION;
            if(not checked_write(cur_state->client_fd, &is_protocol_match, sizeof(is_protocol_match), NULL)) {
                printf("[Client_sock: %d] [Failed to send protocol match] [errno: %d] [strerror: %s]\n", cur_state->client_fd, errno, strerror(errno));
                drop_connection(&state->tag, cur_state->client_fd);
                return;
            }
            printf("[Client_sock: %d] [Sent protocol match: %d]\n", cur_state->client_fd, is_protocol_match);
            if(not is_protocol_match) {
                drop_connection(&state->tag, cur_state->client_fd);
                return;
            }
            state->tag = ClientState_RECEIVE_FILE_NAME;
            state->value.receive_file_name.client_fd = cur_state->client_fd;
            return;
        }
        case ClientState_RECEIVE_FILE_NAME: {
            const struct ClientState_ReceiveFileName* const cur_state = &state->value.receive_file_name;
            if(!FD_ISSET(cur_state->client_fd, readfds)) {
                return;
            }
            printf("Client fd %d is at ClientState_RECEIVE_FILE_NAME\n", cur_state->client_fd);

            state->tag = ClientState_SEND_FILE_OPERATION_POSSIBILITY;
            state->value.send_file_operation_possibility.client_fd = cur_state->client_fd;
            if(
                not checked_read(
                    cur_state->client_fd,
                    state->value.send_file_operation_possibility.filename,
                    ARRAY_SIZE(state->value.send_file_operation_possibility.filename),
                    NULL
                )
            ) {
                printf("[Client_sock: %d] [Failed to read filename buffer] [errno: %d] [strerror: %s]\n", cur_state->client_fd, errno, strerror(errno));
                drop_connection(&state->tag, cur_state->client_fd);
            }
            return;
        }
        case ClientState_SEND_FILE_OPERATION_POSSIBILITY: {
            const struct ClientState_SendFileOperationPossibility* const cur_state = &state->value.send_file_operation_possibility;
            if(!FD_ISSET(cur_state->client_fd, writefds)) {
                return;
            }
            if(memchr(cur_state->filename, '\0', ARRAY_SIZE(cur_state->filename)) == NULL) {
                printf("[Client_sock: %d] [Error filename not valid]\n", cur_state->client_fd);
                const bool is_file_size_ok = false;
                if(not checked_write(cur_state->client_fd, &is_file_size_ok, sizeof(is_file_size_ok), NULL)) {
                    printf("[Client_sock: %d] [Error filename not valid] [errno: %d] [strerror: %s]\n", cur_state->client_fd, errno, strerror(errno));
                }
                return drop_connection(&state->tag, cur_state->client_fd);
            }
            const size_t dir_path_strlen = strlen(dir_path);
            const size_t filename_strlen = strlen(cur_state->filename);
            const size_t buffer_byte_count = dir_path_strlen + 1 + filename_strlen + 1;
            
            char *const buffer = malloc(buffer_byte_count);
            snprintf(buffer, buffer_byte_count, "%s/%s", dir_path, cur_state->filename);
            const int fd = open(buffer, O_RDONLY);
            
            if(fd == -1) {
                printf("[Client_sock: %d] [Error open file: %s] [errno: %d] [strerror: %s]\n", cur_state->client_fd, buffer, errno, strerror(errno));
                const bool is_file_size_ok = false;
                if(not checked_write(cur_state->client_fd, &is_file_size_ok, sizeof(is_file_size_ok), NULL)) {
                    printf("[Client_sock: %d] [Failed to inform failure file size not ok] [errno: %d] [strerror: %s]\n", cur_state->client_fd, errno, strerror(errno));
                }
                drop_connection(&state->tag, cur_state->client_fd);
            } else {
                struct stat st;
                if(fstat(fd, &st) == -1) {
                    printf("[Client_sock: %d] [Error stat: %s] [errno: %d] [strerror: %s]\n", cur_state->client_fd, buffer, errno, strerror(errno));
                    const bool is_file_size_ok = false;
                    if(not checked_write(cur_state->client_fd, &is_file_size_ok, sizeof(is_file_size_ok), NULL)) {
                        printf("[Client_sock: %d] [Failed to inform file size not ok: %s] [errno: %d] [strerror: %s]\n", cur_state->client_fd, buffer, errno, strerror(errno));
                    }
                    if(not checked_close(fd)) {
                        printf("[Client_sock: %d] [Failed close file: %s] [errno: %d] [strerror: %s]\n", cur_state->client_fd, buffer, errno, strerror(errno));
                    }
                    drop_connection(&state->tag, cur_state->client_fd);
                } else {
                    const bool is_file_size_ok = true;
                    if(not checked_write(cur_state->client_fd, &is_file_size_ok, sizeof(is_file_size_ok), NULL)) {
                        printf("[Client_sock: %d] [Failed to inform failure file size ok] [errno: %d] [strerror: %s]\n", cur_state->client_fd, errno, strerror(errno));
                        if(not checked_close(fd)) {
                            printf("[Client_sock: %d] [Failed close file: %s] [errno: %d] [strerror: %s]\n", cur_state->client_fd, buffer, errno, strerror(errno));
                        }
                        drop_connection(&state->tag, cur_state->client_fd);
                    } else {
                        state->tag = ClientState_SEND_CHUNK_AND_FILE_SIZE;
                        state->value.send_chunk_and_file_size.client_fd = cur_state->client_fd;
                        state->value.send_chunk_and_file_size.fd = fd;
                        state->value.send_chunk_and_file_size.file_size = st.st_size;
                    }
                }
            }
            free(buffer);
            return;
        }
        case ClientState_SEND_CHUNK_AND_FILE_SIZE: {
            const struct ClientState_SendChunkAndFileSize* const cur_state = &state->value.send_chunk_and_file_size;
            if(!FD_ISSET(cur_state->client_fd, writefds)) {
                return;
            }
            printf("Client fd %d is at ClientState_SEND_FILE_AND_CHUNK_SIZE\n", cur_state->client_fd);
            const ChunkAndFileSize file_and_chunk_size = {htons(CHUNK_SIZE), htonl((uint32_t)cur_state->file_size)};
            if(not checked_write(cur_state->client_fd, &file_and_chunk_size, sizeof(file_and_chunk_size), 0)) {
                printf("[Client_sock: %d] [Failed to send chunk and file size] [errno: %d] [strerror: %s]\n", cur_state->client_fd, errno, strerror(errno));
                if(not checked_close(cur_state->client_fd)) {
                    printf("[Client_sock: %d] [Failed close file: %d] [errno: %d] [strerror: %s]\n", cur_state->client_fd, cur_state->client_fd, errno, strerror(errno));
                }
                return drop_connection(&state->tag, cur_state->client_fd);
            }
            state->tag = ClientState_RECEIVE_CLIENT_READY;
            state->value.receive_client_ready.client_fd = cur_state->client_fd;
            state->value.receive_client_ready.fd = cur_state->fd;
            return;
        }
        case ClientState_RECEIVE_CLIENT_READY: {
            const struct ClientState_ReceiveClientReady* const cur_state = &state->value.receive_client_ready;
            if(!FD_ISSET(cur_state->client_fd, readfds)) {
                return;
            }

            return;
        }



        case ClientState_SEND_FILE_CHUNK: {


            const struct ClientState_SendFileChunk* const cur_state = &state->value.send_file_chunk;
            if(FD_ISSET(cur_state->client_fd, writefds)) {
                printf("Client fd %d is at ClientState_SEND_FILE_CHUNK\n", cur_state->client_fd);

                char buffer[CHUNK_SIZE];
                size_t bytes_read;
                while ((bytes_read = fread(buffer, 1, sizeof(buffer), cur_state->file)) > 0) {
                    // printf("Read bytes: %lu\n", bytes_read);
                    if(send(cur_state->client_fd, buffer, bytes_read, 0) == -1) {
                        return construct_drop_connection(cur_state->client_fd);
                    }
                }
                fclose(cur_state->file);

                printf("Client fd %d sent file successfuly\n", cur_state->client_fd);
                return construct_drop_connection(cur_state->client_fd);
            }
            return *state;
        }
        case ClientState_DROP_CONNECTION: {
            const struct ClientState_DropConnection* const cur_state = &state->value.drop_connection;
            printf("Client fd %d is at ClientState_DROP_CONNECTION\n", cur_state->client_fd);

            close(cur_state->client_fd);
            ClientState next_state;
            next_state.tag = ClientStateTag_INVALID;
            return next_state;
        }
        default:
            __builtin_unreachable();
    }
}

static volatile sig_atomic_t keep_running = 1;
static void handle_sigint(const int) { keep_running = 0; }

typedef struct {
    in_addr_t address;
    in_port_t port;
    const char* dir_path;
    uint16_t max_clients_count;
} Config;

static Config parse_config(
    const int argc,
    const char* const argv[]
) {
    Config config;

    assert(argc == 5);

    config.address = inet_addr(argv[1]);
    assert(config.address);

    errno = 0;
    const uint32_t port = strtoul(argv[2], NULL, 10);
    assert(errno == 0);
    assert(port == (long)((in_port_t)port));
    config.port = (in_port_t)port;

    config.dir_path = argv[3];

    const uint32_t max_clients_count = strtoul(argv[4], NULL, 10);
    assert(errno == 0);
    static const uint16_t FD_SETSIZE_ACCOUNTING_FOR_SERVER_FD = FD_SETSIZE - 1;
    assert(max_clients_count <= FD_SETSIZE_ACCOUNTING_FOR_SERVER_FD);
    config.max_clients_count = (uint16_t)max_clients_count;

    return config;
}

typedef uint16_t clients_count_t;

static int init_file_descriptors(
    fd_set *readfds,
    fd_set *writefds,
    const int serverfd,
    const ClientState *const client_state_array,
    const clients_count_t client_state_array_count
) {
    FD_ZERO(readfds);
    FD_ZERO(writefds);
    FD_SET(serverfd, readfds);
    int max_sd = serverfd;
    for (size_t i = 0; i < client_state_array_count; ++i) {
        const ClientState* state = &client_state_array[i];
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

int main(
    const int argc,
    const char* const argv[]
) {
    {
        struct sigaction sa;
        sa.sa_handler = handle_sigint;
        ASSERT_POSIX(sigemptyset(&sa.sa_mask));
        sa.sa_flags = 0;
        ASSERT_POSIX(sigaction(SIGINT, &sa, NULL));
    }
    const Config config = parse_config(argc, argv);

    const int listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_POSIX(listenfd);
    {
        struct sockaddr_in srv_sin4 = {
            .sin_family  = AF_INET,
            .sin_port = config.port,
            .sin_addr.s_addr = config.address
        };
        
        ASSERT_POSIX(bind(listenfd, (struct sockaddr *)&srv_sin4, sizeof(srv_sin4)));
        static const uint8_t MAX_BACKLOG = 10;
        ASSERT_POSIX(listen(listenfd, MAX_BACKLOG));
    }

    fd_set readfds, writefds;
    clients_count_t clients_count = 0;

    ClientState *const client_state_array = calloc(config.max_clients_count, sizeof(ClientState));
    clients_count_t client_state_array_count = 0;
    for(size_t i = 0; i < config.max_clients_count; ++i) {
        client_state_array[i].tag = ClientStateTag_INVALID;
    }

    while(keep_running) {
        const int max_fd = init_file_descriptors(
            &readfds, &writefds, listenfd, client_state_array, client_state_array_count
        );
        if(select(max_fd + 1, &readfds, &writefds, NULL, NULL) == -1) {
            printf("[select] [errno: %d] [strerror: %s]\n", errno, strerror(errno));
            continue;
        }

        if(FD_ISSET(listenfd, &readfds)) {
            struct sockaddr_in address;
            socklen_t addr_len = sizeof(address);
            const int client_fd = accept(listenfd, (struct sockaddr *)&address, &addr_len);
            if (client_fd == -1) {
                printf("[accept] [errno: %d] [strerror: %s]\n", errno, strerror(errno));
                continue;
            }
    
            printf("[New connection] [client_fd: %d] [IP: %s] [port: %d]\n",
                client_fd, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
    
            for (size_t i = 0; i < client_state_array_count; ++i) {
                if (client_state_array[i].tag == ClientStateTag_INVALID) {
                    client_state_array[i].tag = ClientState_RECEIVE_PROTOCOL_VERSION;
                    client_state_array[i].value.receive_protocol_version.client_fd = client_fd;
                    break;
                }
            }
        }

        for (size_t i = 0; i < config.max_clients_count; ++i) {
            ClientState* state = &client_state_array[i];
            *state = ClientState_transition(state, &readfds, &writefds, config.dir_path);
        }
    }

    free(client_state_array);
    assert(checked_close(listenfd));
    return EXIT_SUCCESS;
}
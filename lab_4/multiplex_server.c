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
#include <sys/sendfile.h>

#include "client_utils.h"

typedef enum {
    ClientStateTag_INVALID,
    ClientStateTag_RECEIVE_PROTOCOL_VERSION,
    ClientStateTag_SEND_MATCH_PROTOCOL_VERSION,
    ClientStateTag_RECEIVE_FILE_NAME,
    ClientStateTag_SEND_FILE_OPERATION_POSSIBILITY,
    ClientStateTag_SEND_FILE_SIZE,
    ClientStateTag_RECEIVE_CLIENT_READY,
    ClientStateTag_SEND_CHUNK,
    ClientStateTag_RECEIVE_FINISH,
} ClientStateTag;

typedef struct {
    ClientStateTag tag;
    union {
        struct ClientState_ReceiveProtocolVersion {
            int32_t client_fd;
        } receive_protocol_version;
        struct ClientState_SendMatchProtocolVersion {
            int32_t client_fd;
            uint8_t client_protocol_version;
        } send_match_protocol_version;
        struct ClientState_ReceiveFilename {
            int32_t client_fd;
        } receive_filename;
        struct ClientState_SendFileOperationPossibility {
            int32_t client_fd;
            int32_t fd;
        } send_file_operation_possibility;
        struct ClientState_SendChunkAndFileSize {
            int32_t client_fd;
            int32_t fd;
            off_t file_size;
        } send_file_size;
        struct ClientState_ReceiveClientReady {
            int32_t client_fd;
            int32_t fd;
            off_t file_size;
        } receive_client_ready;
        struct ClientState_SendChunk {
            int32_t client_fd;
            int32_t fd;
            off_t file_size;
            off_t file_offset;
        } send_chunk;
        struct ClientState_ReceiveFinish {
            int32_t client_fd;
        } receive_finish;
    } value;
} ClientState;

typedef uint16_t clients_count_t;

static ClientState construct_drop_connection(
    clients_count_t *const clients_count,
    const int32_t client_fd
) {
    printf("[client_fd: %d] [drop connection]\n", client_fd);

    --(*clients_count);
    checked_close(client_fd);
    ClientState state;
    state.tag = ClientStateTag_INVALID;
    return state;
}

static ClientState ClientState_transition(
    clients_count_t *const clients_count,
    const ClientState* const state,
    const fd_set* const readfds,
    const fd_set* const writefds,
    char* const filepath_buffer,
    const size_t filepath_buffer_offset
) {
    switch (state->tag) {
        case ClientStateTag_INVALID: {
            return *state;
        }
        case ClientStateTag_RECEIVE_PROTOCOL_VERSION: {
            const struct ClientState_ReceiveProtocolVersion* const cur_state = &state->value.receive_protocol_version;
            if(!FD_ISSET(cur_state->client_fd, readfds)) {
                return *state;
            }
            printf("[client_fd: %d] [ClientStateTag_RECEIVE_PROTOCOL_VERSION]\n", cur_state->client_fd);

            uint8_t client_protocol_version;
            if(
                not checked_read(cur_state->client_fd, &client_protocol_version, sizeof(client_protocol_version), NULL)
            ) {
                return construct_drop_connection(clients_count, cur_state->client_fd);
            }

            ClientState new_state;
            new_state.tag = ClientStateTag_SEND_MATCH_PROTOCOL_VERSION;
            new_state.value.send_match_protocol_version.client_fd = cur_state->client_fd;
            new_state.value.send_match_protocol_version.client_protocol_version = client_protocol_version;
            return new_state;
        }
        case ClientStateTag_SEND_MATCH_PROTOCOL_VERSION: {
            const struct ClientState_SendMatchProtocolVersion* const cur_state = &state->value.send_match_protocol_version;
            if(!FD_ISSET(cur_state->client_fd, writefds)) {
                return *state;
            }
            printf("[client_fd: %d] [ClientStateTag_SEND_MATCH_PROTOCOL_VERSION]\n", cur_state->client_fd);
            
            const bool is_protocol_match = cur_state->client_protocol_version == PROTOCOL_VERSION;
            if(not checked_write(cur_state->client_fd, &is_protocol_match, sizeof(is_protocol_match), NULL)) {
                return construct_drop_connection(clients_count, cur_state->client_fd);
            }
            if(not is_protocol_match) {
                return construct_drop_connection(clients_count, cur_state->client_fd);
            }
            ClientState new_state;
            new_state.tag = ClientStateTag_RECEIVE_FILE_NAME;
            new_state.value.receive_filename.client_fd = cur_state->client_fd;
            return new_state;
        }
        case ClientStateTag_RECEIVE_FILE_NAME: {
            const struct ClientState_ReceiveFilename* const cur_state = &state->value.receive_filename;
            if(!FD_ISSET(cur_state->client_fd, readfds)) {
                return *state;
            }
            printf("[client_fd: %d] [ClientStateTag_RECEIVE_FILE_NAME]\n", cur_state->client_fd);

            if(not checked_read(cur_state->client_fd, filepath_buffer + filepath_buffer_offset, NAME_MAX, NULL)) {
                return construct_drop_connection(clients_count, cur_state->client_fd);
            }
            filepath_buffer[filepath_buffer_offset + NAME_MAX] = '\0';
            printf("[client_fd: %d] [filepath_buffer: %s]\n", cur_state->client_fd, filepath_buffer);
            
            ClientState new_state;
            new_state.tag = ClientStateTag_SEND_FILE_OPERATION_POSSIBILITY;
            new_state.value.send_file_operation_possibility.client_fd = cur_state->client_fd;
            new_state.value.send_file_operation_possibility.fd = open(filepath_buffer, O_RDONLY);
            return new_state;
        }
        case ClientStateTag_SEND_FILE_OPERATION_POSSIBILITY: {
            const struct ClientState_SendFileOperationPossibility *const cur_state = &state->value.send_file_operation_possibility;
            if(!FD_ISSET(cur_state->client_fd, writefds)) {
                return *state;
            }
            printf("[client_fd: %d] [ClientStateTag_SEND_FILE_OPERATION_POSSIBILITY]\n", cur_state->client_fd);

            bool is_possible = cur_state->client_fd != -1;
            if(not is_possible) {
                checked_write(cur_state->client_fd, &is_possible, sizeof(is_possible), NULL);
                return construct_drop_connection(clients_count, cur_state->client_fd);
            }
            struct stat st;
            is_possible = fstat(cur_state->fd, &st) != -1; 
            if(not is_possible) {
                checked_close(cur_state->fd);
                checked_write(cur_state->client_fd, &is_possible, sizeof(is_possible), NULL);
                return construct_drop_connection(clients_count, cur_state->client_fd);
            }
            if(not checked_write(cur_state->client_fd, &is_possible, sizeof(is_possible), NULL)) {
                checked_close(cur_state->fd);
                return construct_drop_connection(clients_count, cur_state->client_fd);
            }
            ClientState new_state;
            new_state.tag = ClientStateTag_SEND_FILE_SIZE;
            new_state.value.send_file_size.client_fd = cur_state->client_fd;
            new_state.value.send_file_size.fd = cur_state->fd;
            new_state.value.send_file_size.file_size = st.st_size;
            return new_state;
        }
        case ClientStateTag_SEND_FILE_SIZE: {
            const struct ClientState_SendChunkAndFileSize *const cur_state = &state->value.send_file_size;
            if(!FD_ISSET(cur_state->client_fd, writefds)) {
                return *state;
            }
            printf("[client_fd: %d] [ClientStateTag_SEND_FILE_SIZE]\n", cur_state->client_fd);

            printf("[client_fd: %d] [cur_state->file_size: %ld]\n", cur_state->client_fd, cur_state->file_size);
            const uint32_t network_file_size = htonl((uint32_t)cur_state->file_size);
            checked_write(cur_state->client_fd, &network_file_size, sizeof(network_file_size), NULL);

            ClientState new_state;
            new_state.tag = ClientStateTag_RECEIVE_CLIENT_READY;
            new_state.value.receive_client_ready.client_fd = cur_state->client_fd;
            new_state.value.receive_client_ready.fd = cur_state->fd;
            new_state.value.receive_client_ready.file_size = cur_state->file_size;
            return new_state;
        }
        case ClientStateTag_RECEIVE_CLIENT_READY: {
            const struct ClientState_ReceiveClientReady *const cur_state = &state->value.receive_client_ready;
            if(!FD_ISSET(cur_state->client_fd, readfds)) {
                return *state;
            }
            printf("[client_fd: %d] [ClientStateTag_RECEIVE_CLIENT_READY]\n", cur_state->client_fd);

            bool is_client_ready;
            if(not checked_read(cur_state->client_fd, &is_client_ready, sizeof(is_client_ready), NULL)) {
                checked_close(cur_state->fd);
                return construct_drop_connection(clients_count, cur_state->client_fd);
            }
            printf("[client_fd: %d] [is_client_ready: %d]\n", cur_state->client_fd, is_client_ready);
            if(not is_client_ready) {
                checked_close(cur_state->fd);
                return construct_drop_connection(clients_count, cur_state->client_fd);
            }
            ClientState new_state;
            new_state.tag = ClientStateTag_SEND_CHUNK;
            new_state.value.send_chunk.client_fd = cur_state->client_fd;
            new_state.value.send_chunk.fd = cur_state->fd;
            new_state.value.send_chunk.file_size = cur_state->file_size;
            new_state.value.send_chunk.file_offset = 0;
            return new_state;
        }
        case ClientStateTag_SEND_CHUNK: {
            ClientState new_generic_state = *state;
            struct ClientState_SendChunk *const new_cur_state = &new_generic_state.value.send_chunk;
            if(!FD_ISSET(new_cur_state->client_fd, writefds)) {
                return new_generic_state;
            }
            printf("[client_fd: %d] [ClientStateTag_SEND_CHUNK]\n", new_cur_state->client_fd);

            const off_t old_offset = new_cur_state->file_offset;
            while(true) {
                printf("[client_fd: %d] [new_cur_state->file_offset: %ld]\n", new_cur_state->client_fd, new_cur_state->file_offset);
                if(new_cur_state->file_offset >= new_cur_state->file_size) {
                    checked_close(new_cur_state->fd);
                    new_generic_state.tag = ClientStateTag_RECEIVE_FINISH;
                    new_generic_state.value.receive_finish.client_fd = state->value.send_chunk.client_fd;
                    return new_generic_state;
                }
                const off_t diff_offset = CHUNK_SIZE - (new_cur_state->file_offset - old_offset);
                if(diff_offset <= 0) {
                    break;
                }
                const ssize_t nsendfile = sendfile(new_cur_state->client_fd, new_cur_state->fd, &new_cur_state->file_offset, (size_t)diff_offset);
                if(nsendfile == -1) {
                    printf("[Failed to sendfile] [errno: %d] [strerror: %s]\n", errno, strerror(errno));
                    break;
                }
            }            
            return new_generic_state;
        }
        case ClientStateTag_RECEIVE_FINISH: {
            const struct ClientState_ReceiveFinish *const cur_state = &state->value.receive_finish;
            if(!FD_ISSET(cur_state->client_fd, readfds)) {
                return *state;
            }
            printf("[client_fd: %d] [ClientStateTag_RECEIVE_FINISH]\n", cur_state->client_fd);

            uint8_t signal_end_byte;
            checked_read(cur_state->client_fd, &signal_end_byte, sizeof(signal_end_byte), NULL);

            printf("[client_fd: %d] [received signal_end_byte]\n", cur_state->client_fd);

            return construct_drop_connection(clients_count, cur_state->client_fd);
        }
        default: {
            __builtin_unreachable();
        }
    }
}

static volatile sig_atomic_t keep_running = 1;
static void handle_sigint(const int value __attribute_maybe_unused__) { keep_running = 0; }

static in_addr_t parse_address(const char *const value) {
    const in_addr_t address = inet_addr(value);
    ASSERT_POSIX(address);
    return address;
}
static in_port_t parse_port(const char *const value) {
    errno = 0;
    const uint64_t port = strtoul(value, NULL, 10);
    assert(errno == 0);
    assert(port == (uint64_t)((in_port_t)port));
    return htons((in_port_t)port);
}
static uint16_t parse_max_clients_count(const char *const value) {
    const uint64_t max_clients_count = strtoul(value, NULL, 10);
    assert(errno == 0);
    static const uint16_t FD_SETSIZE_ACCOUNTING_FOR_SERVER_FD = FD_SETSIZE - 1;
    assert(max_clients_count <= FD_SETSIZE_ACCOUNTING_FOR_SERVER_FD);
    return (uint16_t)max_clients_count;
}
static int32_t ClientState_client_fd(const ClientState *const state) {
    switch(state->tag) {
        case ClientStateTag_INVALID: {
            __builtin_unreachable();
        }
        case ClientStateTag_RECEIVE_PROTOCOL_VERSION: {
            return state->value.receive_protocol_version.client_fd;
        }
        case ClientStateTag_SEND_MATCH_PROTOCOL_VERSION: {
            return state->value.send_match_protocol_version.client_fd;
        }
        case ClientStateTag_RECEIVE_FILE_NAME: {
            return state->value.receive_filename.client_fd;
        }
        case ClientStateTag_SEND_FILE_OPERATION_POSSIBILITY: {
            return state->value.send_file_operation_possibility.client_fd;
        }
        case ClientStateTag_SEND_FILE_SIZE: {
            return state->value.send_file_size.client_fd;
        }
        case ClientStateTag_RECEIVE_CLIENT_READY: {
            return state->value.receive_client_ready.client_fd;
        }
        case ClientStateTag_SEND_CHUNK: {
            return state->value.send_chunk.client_fd;
        }
        case ClientStateTag_RECEIVE_FINISH: {
            return state->value.receive_finish.client_fd;
        }
        default: {
            __builtin_unreachable();
        }
    }
}

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
    for(size_t i = 0; i < client_state_array_count; ++i) {
        const ClientState* state = &client_state_array[i];
        if(state->tag != ClientStateTag_INVALID) {
            const int client_fd = ClientState_client_fd(state);
            FD_SET(client_fd, readfds);
            FD_SET(client_fd, writefds);
            if(client_fd > max_sd) {
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
    assert(argc == 5);
    
    const int listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_POSIX(listenfd);
    {
        struct sockaddr_in srv_sin4 = {
            .sin_family  = AF_INET,
            .sin_addr.s_addr = parse_address(argv[1]),
            .sin_port = parse_port(argv[2]),
        };
        ASSERT_POSIX(bind(listenfd, (struct sockaddr *)&srv_sin4, sizeof(srv_sin4)));
        static const uint8_t MAX_BACKLOG = 10;
        ASSERT_POSIX(listen(listenfd, MAX_BACKLOG));
    }
    char filepath_buffer[PATH_MAX];
    uint16_t filepath_buffer_offset;
    {
        static const uint8_t slash_character_addition = 1;
        const size_t len_with_slash = strlen(argv[3]) + slash_character_addition;
        assert((len_with_slash + NAME_MAX) <= PATH_MAX);
        filepath_buffer_offset = (uint16_t)len_with_slash;
        strcpy(filepath_buffer, argv[3]);
        strcat(filepath_buffer, "/");
    }

    const uint16_t max_clients_count = parse_max_clients_count(argv[4]);

    fd_set readfds, writefds;
    ClientState *const client_state_array = calloc(max_clients_count, sizeof(ClientState));
    clients_count_t client_state_array_count = 0;
    for(size_t i = 0; i < max_clients_count; ++i) {
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
        // printf("[select triggered]\n");


        if(FD_ISSET(listenfd, &readfds) && client_state_array_count < max_clients_count) {
            struct sockaddr_in address;
            socklen_t addr_len = sizeof(address);
            const int client_fd = accept(listenfd, (struct sockaddr *)&address, &addr_len);
            if (client_fd == -1) {
                printf("[accept] [errno: %d] [strerror: %s]\n", errno, strerror(errno));
                continue;
            }
    
            printf("[New connection] [client_fd: %d] [IP: %s] [port: %d]\n",
                client_fd, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
            
            client_state_array[client_state_array_count].tag = ClientStateTag_RECEIVE_PROTOCOL_VERSION;
            client_state_array[client_state_array_count].value.receive_protocol_version.client_fd = client_fd;
            ++client_state_array_count;
        }

        for(size_t i = 0; i < client_state_array_count; ++i) {
            ClientState* state = &client_state_array[i];
            *state = ClientState_transition(&client_state_array_count, state, &readfds, &writefds, filepath_buffer, filepath_buffer_offset);
        }
    }

    free(client_state_array);
    assert(checked_close(listenfd));
    return EXIT_SUCCESS;
}

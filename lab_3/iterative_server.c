#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include "iterative_server_utils.h"

static void print_config(const IterativeServerConfig *config) {
    printf("Server Configuration:\n");
    printf("  Address: %s\n", config->address);
    printf("  Port: %d\n", config->port);
    printf("  Directory Path: %s\n", config->dir_path);
}

static IterativeServerConfig handle_cmd_args(const int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server_address> <server_port> <directory_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const IterativeServerConfig config = {
        .address = argv[1],
        .port = (uint16_t)atoi(argv[2]),
        .dir_path = argv[3],
    };

    print_config(&config);

    return config;
}

static volatile sig_atomic_t keep_running = 1;
static void handle_sigint(const int val __attribute__((unused))) {
    keep_running = 0;
}

#define MAX_BACKLOG 10

static void inner_function(const int listenfd, const IterativeServerConfig *const config) {
    struct sockaddr_in srv_sin4 = {
        .sin_family  = AF_INET,
        .sin_port = htons(config->port),
        .sin_addr.s_addr = inet_addr(config->address)
    };
    if(bind(listenfd, (struct sockaddr *)&srv_sin4, sizeof(srv_sin4)) < 0) {
        perror("bind failed");
        return;
    }

    if(listen(listenfd, MAX_BACKLOG) < 0) {
        perror("listen");
        return;
    }
    printf("[Server listening on %s:%d]\n", config->address, config->port);
    while (keep_running) {
        struct sockaddr_in client_in;
        socklen_t addrlen = sizeof(client_in);
        const int connection_fd = accept(listenfd, (struct sockaddr *)&client_in, &addrlen);
        if (connection_fd < 0) {
            continue;
        }
        printf("[New connection from %s:%d]\n", inet_ntoa(client_in.sin_addr), ntohs(client_in.sin_port));
        handle_client(connection_fd, config->dir_path);
        close(connection_fd);
    }
}

int main(const int argc, char *argv[]) {
    signal(SIGINT, handle_sigint);
    const IterativeServerConfig config = handle_cmd_args(argc, argv);
    const int listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenfd < 0) {
        perror("socket failed");
        return EXIT_FAILURE;
    }
    inner_function(listenfd, &config);
    close(listenfd);
    return EXIT_SUCCESS;
}

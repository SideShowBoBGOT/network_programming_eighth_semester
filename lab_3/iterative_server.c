#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include <iterative_server_utils/iterative_server_utils.h>

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
        .port = atoi(argv[2]),
        .dir_path = argv[3],
    };

    print_config(&config);

    return config;
}

volatile sig_atomic_t keep_running = 1;

static void handle_sigint(const int) {
    keep_running = 0;
}

static void run_server(const IterativeServerConfig *config) {
    const int listenfd = create_and_bind_socket(config->port, config->address);
    signal(SIGINT, handle_sigint);
    while (keep_running) {
        const int connfd = accept_connection(listenfd);
        if (connfd < 0) {
            continue;
        }
        handle_client(connfd, config->dir_path);
        close(connfd);
    }

    close(listenfd);
}

int main(const int argc, char *argv[]) {
    const IterativeServerConfig config = handle_cmd_args(argc, argv);
    run_server(&config);
    return EXIT_SUCCESS;
}
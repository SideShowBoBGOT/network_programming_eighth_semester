#include "iterative_server_utils.h"

#include <signal.h>

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

    iterative_server_print_config(&config);

    return config;
}

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
        if(not closen(connection_fd)) {
            printf("[Failed to close client connection: %d]\n", connection_fd);
        }
    }
}



int main(const int argc, char *argv[]) {
    {
        struct sigaction sa;
        sa.sa_handler = handle_sigint;
        assert(sigemptyset(&sa.sa_mask) != -1);
        sa.sa_flags = 0;
        assert(sigaction(SIGINT, &sa, NULL) != -1);
    }

    const IterativeServerConfig config = handle_cmd_args(argc, argv);
    const int listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenfd < 0) {
        perror("socket failed");
        return EXIT_FAILURE;
    }
    inner_function(listenfd, &config);
    if(not closen(listenfd)) {
        printf("[Failed to close listenfd: %d]\n", listenfd);
    }
    return EXIT_SUCCESS;
}

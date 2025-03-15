#include "iterative_server_utils_two.h"

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
    ASSERT_POSIX(bind(listenfd, (struct sockaddr *)&srv_sin4, sizeof(srv_sin4)));
    ASSERT_POSIX(listen(listenfd, MAX_BACKLOG));

    printf("[Server listening on %s:%d]\n", config->address, config->port);
    iterative_server_main_loop(listenfd, config->dir_path);
}

int main(const int argc, char *argv[]) {
    {
        struct sigaction sa;
        sa.sa_handler = handle_sigint;
        ASSERT_POSIX(sigemptyset(&sa.sa_mask));
        sa.sa_flags = 0;
        ASSERT_POSIX(sigaction(SIGINT, &sa, NULL));
    }

    const IterativeServerConfig config = handle_cmd_args(argc, argv);
    const int listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_POSIX(listenfd);
    inner_function(listenfd, &config);
    assert(checked_close(listenfd));
    return EXIT_SUCCESS;
}

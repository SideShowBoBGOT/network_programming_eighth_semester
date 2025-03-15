#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>
#include <err.h>
#include <stdatomic.h>
#include "iterative_server_utils_two.h"

typedef struct {
    IterativeServerConfig config;
    int32_t max_children;
} ParallelServerConfig;

static void parallel_server_print_config(const ParallelServerConfig *config) {
    iterative_server_print_config(&config->config);
    printf("\tMaximum Children: %d\n", config->max_children);
}

static ParallelServerConfig handle_cmd_args(const int argc, const char **argv) {
    if (argc != 5) {
        printf("Usage: %s <server_address> <server_port> <directory_path> <max_children>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const ParallelServerConfig config = {
        .config.address = argv[1],
        .config.port = (uint16_t)atoi(argv[2]),
        .config.dir_path = argv[3],
        .max_children = atoi(argv[4])
    };
    assert(config.max_children > 0);
    parallel_server_print_config(&config);
    return config;
}

static void wait_finish_child_processes(void) {
    while (true) {
        const pid_t pid = waitpid(-1, NULL, WNOHANG);
        switch (pid) {
            case -1: {
                if (errno != ECHILD) {
                    printf("[Failed waitpid]");
                }
                return;
            }
            case 0: {
                return;
            }
            default: {
                printf("[Process %jd exited]\n", (intmax_t)pid);
            }
        }    
    }
}

static void handle_sigchld(const int value __attribute__((unused))) {
    wait_finish_child_processes();
}

static void socketfd_valid(const ParallelServerConfig *config, const int socketfd) {
    {
        const struct sockaddr_in srv_sin4 = {
            .sin_family  = AF_INET,
            .sin_port = htons(config->config.port),
            .sin_addr.s_addr = inet_addr(config->config.address)
        };
        ASSERT_POSIX(bind(socketfd, (const struct sockaddr *)&srv_sin4, sizeof(srv_sin4)));
    }
    ASSERT_POSIX(listen(socketfd, MAX_BACKLOG));
    printf("[Server listening on %s:%d]\n", config->config.address, config->config.port);

    sigset_t sigcld_unblock_mask;
    sigprocmask(SIG_BLOCK, NULL, &sigcld_unblock_mask);
    sigdelset(&sigcld_unblock_mask, SIGCLD);

    sigset_t sigcld_block_mask;    
    sigprocmask(SIG_BLOCK, NULL, &sigcld_block_mask);
    sigaddset(&sigcld_block_mask, SIGCHLD);

    sigprocmask(SIG_BLOCK, &sigcld_block_mask, NULL);

    for(uint32_t i = 0; i < (uint32_t)config->max_children; ++i) {
        const pid_t pid = fork();
        ASSERT_POSIX(pid);
        if(pid == 0) {
            iterative_server_main_loop(socketfd, config->config.dir_path);
            return;
        }
    }
    iterative_server_main_loop(socketfd, config->config.dir_path);
    wait_finish_child_processes();
}

int main(const int argc, const char *argv[]) {
    {
        struct sigaction sa;
        ASSERT_POSIX(sigemptyset(&sa.sa_mask));
        sa.sa_flags = 0;
        
        sa.sa_handler = handle_sigint;
        ASSERT_POSIX(sigaction(SIGINT, &sa, NULL));

        sa.sa_handler = handle_sigchld;
        ASSERT_POSIX(sigaction(SIGCLD, &sa, NULL));
    }
    const ParallelServerConfig config = handle_cmd_args(argc, argv);
    const int listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_POSIX(listenfd);
    socketfd_valid(&config, listenfd);
    assert(checked_close(listenfd));
    return EXIT_SUCCESS;
}

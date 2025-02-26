#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#define __USE_POSIX
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>
#include <err.h>
#include "iterative_server_utils.h"

typedef struct {
    IterativeServerConfig config;
    size_t max_children;
} ParallelServerConfig;

static void print_config(const ParallelServerConfig *config) {
    printf("Server Configuration:\n");
    printf("  Address: %s\n", config->config.address);
    printf("  Port: %d\n", config->config.port);
    printf("  Directory Path: %s\n", config->config.dir_path);
    printf("  Maximum Children: %zu\n", config->max_children);
}

static ParallelServerConfig handle_cmd_args(const int argc, const char **argv) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <server_address> <server_port> <directory_path> <max_children>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const ParallelServerConfig config = {
        .config.address = argv[1],
        .config.port = (uint16_t)atoi(argv[2]),
        .config.dir_path = argv[3],
        .max_children = (size_t)atoi(argv[4])
    };

    print_config(&config);

    return config;
}

static bool keep_running = 1;
static size_t active_children = 0;

static void handle_sigint(const int value __attribute__((unused))) {
    keep_running = 0;
}

static void wait_finish_child_processes(void) {
    while (true) {
        const pid_t pid = waitpid(-1, NULL, WNOHANG);
        switch (pid) {
            case -1: {
                if (errno != ECHILD) {
                    warn("[Failed waitpid]");
                }
                return;
            }
            case 0: {
                return;
            }
            default: {
                printf("[Process %jd exited]\n", (intmax_t)pid);
                active_children--;
            }
        }    
    }
}

static void handle_sigchld(const int value __attribute__((unused))) {
    wait_finish_child_processes();
}

#define MAX_BACKLOG 10

static void socketfd_valid(const ParallelServerConfig *config, const int socketfd) {
    {
        const struct sockaddr_in srv_sin4 = {
            .sin_family  = AF_INET,
            .sin_port = htons(config->config.port),
            .sin_addr.s_addr = inet_addr(config->config.address)
        };
        if(bind(socketfd, (const struct sockaddr *)&srv_sin4, sizeof(srv_sin4)) < 0) {
            perror("bind failed");
            return;
        }
    }
    if(listen(socketfd, MAX_BACKLOG) < 0) {
        perror("listen");
        return;
    }
    printf("Server listening on %s:%d\n", config->config.address, config->config.port);

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigprocmask(SIG_BLOCK, &set, NULL);

    while(keep_running) {
        const int connection_fd = accept(socketfd, NULL, NULL);
        if(connection_fd < 0) {
            continue;
        }
        
        const pid_t pid = fork();
        if(pid < 0) {
            perror("[Failed to fork]");
        } else if (pid == 0) {
            handle_client(connection_fd, config->config.dir_path);
            goto exit_label;
        } else {
            active_children++;
            while(active_children == config->max_children) {
                sigsuspend()
            }
        }

        if(not close(connection_fd)) {
            warn("[Failed to close connection_fd: %d]", connection_fd);
        }
    }
    wait_finish_child_processes();
    exit_label:;
}

int main(const int argc, const char *argv[]) {
    if(signal(SIGINT, handle_sigint) == SIG_ERR) {
        warn("Failed to establish signal handler for SIGINT");
        return EXIT_FAILURE;
    }
    if(signal(SIGCHLD, handle_sigchld) == SIG_ERR) {
        warn("Failed to establish signal handler for SIGCHLD");
        return EXIT_FAILURE;
    }
    const ParallelServerConfig config = handle_cmd_args(argc, argv);
    const int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_fd < 0) {
        perror("socket failed");
        return EXIT_FAILURE;
    }
    socketfd_valid(&config, socket_fd);
    if(not close(socket_fd)) {
        warn("[Can not close socket_fd: %d]", socket_fd);
        exit(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

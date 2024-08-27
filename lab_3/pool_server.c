#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>

#include <iterative_server_utils/iterative_server_utils.h>
#include <parallel_server_utils/parallel_server_utils.h>

#define MAX_BACKLOG 10

volatile sig_atomic_t keep_running = 1;

static void handle_sigint(const int) {
    keep_running = 0;
}

static void child_process(const int listenfd, const char* const dir_path) {
    while (keep_running) {
        const int connfd = accept_connection(listenfd);
        if (connfd < 0) {
            // printf("No connectiosdfsdfdsfdsfn\n");
            continue;
        }
        handle_client(connfd, dir_path);
        close(connfd);
    }
}

static void run_server(const ParallelServerConfig* const config) {
    const int listenfd = create_and_bind_socket(&config->config);

    pid_t pids[config->max_children];
    for (int i = 0; i < config->max_children; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            warn_err("fork()");
            exit(EXIT_FAILURE);
        }
        if (pids[i] == 0) {
            child_process(listenfd, config->config.dir_path);
            exit(EXIT_SUCCESS);
        }
    }

    printf("Created %d child processes\n", config->max_children);

    signal(SIGINT, handle_sigint);

    while (keep_running) {
        sleep(1);
    }

    printf("Shutting down...\n");

    for (int i = 0; i < config->max_children; i++) {
        kill(pids[i], SIGTERM);
    }

    for (int i = 0; i < config->max_children; i++) {
        waitpid(pids[i], NULL, 0);
    }

    close(listenfd);
    printf("Server shut down successfully\n");
}

int main(const int argc, char *argv[]) {
    const ParallelServerConfig config = handle_cmd_args(argc, argv);
    run_server(&config);
    return EXIT_SUCCESS;
}
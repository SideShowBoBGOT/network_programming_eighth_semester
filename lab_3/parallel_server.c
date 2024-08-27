#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>

#include <parallel_server_utils/parallel_server_utils.h>
#include <iterative_server_utils/iterative_server_utils.h>

static void reap_child_processes(volatile sig_atomic_t *active_children) {
    while (1) {
        const pid_t pid = waitpid(-1, NULL, WNOHANG);
        if (pid == -1)
            break;
        printf("Process %jd exited\n", (intmax_t)pid);
        (*active_children)--;
    }
    if (errno != ECHILD)
        exit_err("waitpid()");
}

static void handle_child_process(int listenfd, int connfd, const char *dir_path) {
    if (close(listenfd) < 0)
        exit_err("close()");
    handle_client(connfd, dir_path);
    close(connfd);
    exit(EXIT_SUCCESS);
}

static void handle_parent_process(int connfd, volatile sig_atomic_t *active_children) {
    if (close(connfd) < 0)
        warn_err("close()");
    (*active_children)++;
}

static void run_server(const ServerConfig *config) {
    const int listenfd = create_and_bind_socket(config);
    volatile sig_atomic_t active_children = 0;

    while(1) {
        reap_child_processes(&active_children);

        if (active_children >= config->max_children) {
            sleep(1);
            continue;
        }

        const int connfd = accept_connection(listenfd);
        if (connfd < 0)
            continue;

        pid_t pid = fork();
        if (pid < 0) {
            warn_err("fork()");
        } else if (pid == 0) {
            handle_child_process(listenfd, connfd, config->dir_path);
        } else {
            handle_parent_process(connfd, &active_children);
        }
    }

    close(listenfd);
}

int main(const int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <server_address> <server_port> <directory_path> <max_children>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    ServerConfig config = {
        .address = argv[1],
        .port = atoi(argv[2]),
        .dir_path = argv[3],
        .max_children = atoi(argv[4])
    };

    run_server(&config);
    return EXIT_SUCCESS;
}
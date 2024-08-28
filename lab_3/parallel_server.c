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

static void handle_child_process(const int listenfd, const int connfd, const char* const dir_path) {
    if (close(listenfd) < 0)
        exit_err("close()");
    handle_client(connfd, dir_path);
    close(connfd);
    exit(EXIT_SUCCESS);
}

static void handle_parent_process(const int connfd, int* active_children) {
    if (close(connfd) < 0)
        warn_err("close()");
    (*active_children)++;
    printf("Working child processes: %d\n", *active_children);
}

volatile sig_atomic_t keep_running = 1;

static void handle_sigint(const int) {
    keep_running = 0;
}

static void reap_child_processes(int* active_children) {
    while (1) {
        const pid_t pid = waitpid(-1, NULL, WNOHANG);
        switch (pid) {
            case -1: {
                if (errno != ECHILD)
                    exit_err("waitpid()");
                return;
            }
            case 0: {
                return;
            }
            default: {
                printf("Process %jd exited\n", (intmax_t)pid);
                (*active_children)--;
            }
        }
    }
}

static void wait_finish_child_processes(int* active_children) {
    while (1) {
        const pid_t pid = waitpid(-1, NULL, WNOHANG);
        if(pid == -1)
            break;
        if(pid > 0) {
            printf("Process %jd exited\n", (intmax_t)pid);
            (*active_children)--;
        }
    }
    if (errno != ECHILD)
        exit_err("waitpid()");
}

static void run_server(const ParallelServerConfig *config) {
    const int listenfd = create_and_bind_socket(config->config.port, config->config.address);
    int active_children = 0;
    signal(SIGINT, handle_sigint);

    while(keep_running) {
        reap_child_processes(&active_children);

        const int connfd = accept_connection(listenfd);
        if (connfd < 0)
            continue;

        if (active_children >= config->max_children) {
            printf("No processes available\n");
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            warn_err("fork()");
        } else if (pid == 0) {
            handle_child_process(listenfd, connfd, config->config.dir_path);
        } else {
            handle_parent_process(connfd, &active_children);
        }
    }
    wait_finish_child_processes(&active_children);

    close(listenfd);
}

int main(const int argc, char *argv[]) {
    const ParallelServerConfig config = handle_cmd_args(argc, argv);
    run_server(&config);
    return EXIT_SUCCESS;
}
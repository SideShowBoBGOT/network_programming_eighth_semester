#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>

#include <iterative_server_utils/iterative_server_utils.h>
#include <parallel_server_utils/parallel_server_utils.h>

#define MAX_BACKLOG 10

volatile sig_atomic_t keep_running = 1;

void handle_sigint(const int) {
    keep_running = 0;
}

void child_process(const int listenfd, const char* const dir_path) {
    while (keep_running) {
        const int connfd =

        printf("Process %d: New connection from %s:%d\n", 
               getpid(), inet_ntoa(cln_sin4.sin_addr), ntohs(cln_sin4.sin_port));

        handle_client(connfd, dir_path);
        close(connfd);
    }
}

int main(const int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <server_address> <server_port> <directory_path> <num_processes>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *server_address = argv[1];
    const int server_port = atoi(argv[2]);
    const char *dir_path = argv[3];
    const int num_processes = atoi(argv[4]);

    const int listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenfd < 0)
        exit_err("socket()");

    struct sockaddr_in srv_sin4 = {0};
    srv_sin4.sin_family = AF_INET;
    srv_sin4.sin_port = htons(server_port);
    srv_sin4.sin_addr.s_addr = inet_addr(server_address);

    if (bind(listenfd, (struct sockaddr *)&srv_sin4, sizeof(srv_sin4)) < 0)
        exit_err("bind()");

    if (listen(listenfd, MAX_BACKLOG) < 0)
        exit_err("listen()");

    printf("Server listening on %s:%d\n", server_address, server_port);

    signal(SIGINT, handle_sigint);

    pid_t pids[num_processes];
    for (int i = 0; i < num_processes; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            warn_err("fork()");
            exit(EXIT_FAILURE);
        }
        if (pids[i] == 0) {
            // Child process
            child_process(listenfd, dir_path);
            exit(EXIT_SUCCESS);
        }
    }

    printf("Created %d child processes\n", num_processes);

    // Wait for SIGINT
    while (keep_running) {
        sleep(1);
    }

    printf("Shutting down...\n");

    // Send SIGTERM to all child processes
    for (int i = 0; i < num_processes; i++) {
        kill(pids[i], SIGTERM);
    }

    // Wait for all child processes to exit
    for (int i = 0; i < num_processes; i++) {
        waitpid(pids[i], NULL, 0);
    }

    close(listenfd);
    printf("Server shut down successfully\n");
    return EXIT_SUCCESS;
}
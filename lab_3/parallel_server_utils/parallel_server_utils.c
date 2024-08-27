#include <parallel_server_utils/parallel_server_utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <errno.h>

#define MAX_BACKLOG 10

int create_and_bind_socket(const ServerConfig *config) {
    const int listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenfd < 0)
        exit_err("socket()");

    struct sockaddr_in srv_sin4 = {0};
    srv_sin4.sin_family = AF_INET;
    srv_sin4.sin_port = htons(config->port);
    srv_sin4.sin_addr.s_addr = inet_addr(config->address);

    if (bind(listenfd, (struct sockaddr *)&srv_sin4, sizeof(srv_sin4)) < 0)
        exit_err("bind()");

    if (listen(listenfd, MAX_BACKLOG) < 0)
        exit_err("listen()");

    printf("Server listening on %s:%d\n", config->address, config->port);
    return listenfd;
}

int accept_connection(const int listenfd) {
    struct sockaddr_in cln_sin4;
    socklen_t addrlen = sizeof(cln_sin4);
    const int connfd = accept(listenfd, (struct sockaddr *)&cln_sin4, &addrlen);
    if (connfd < 0) {
        if (errno == EINTR)
            return -1;
        warn_err("accept()");
        return -1;
    }
    printf("New connection from %s:%d\n", inet_ntoa(cln_sin4.sin_addr), ntohs(cln_sin4.sin_port));
    return connfd;
}

void exit_err(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void warn_err(const char *msg) {
    perror(msg);
}
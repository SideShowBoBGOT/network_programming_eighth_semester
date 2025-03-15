#include "iterative_server_utils_one.h"

static void iterative_server_main_loop(
    const int listenfd,
    const char *const dir_path
) {
    while (keep_running) {
        struct sockaddr_in client_in;
        socklen_t addrlen = sizeof(client_in);
        const int connection_fd = accept(listenfd, (struct sockaddr *)&client_in, &addrlen);
        if (connection_fd < 0) {
            continue;
        }
        printf("[New connection from %s:%d]\n", inet_ntoa(client_in.sin_addr), ntohs(client_in.sin_port));
        handle_client(connection_fd, dir_path);
        if(not checked_close(connection_fd)) {
            printf("[Failed to close client connection: %d]\n", connection_fd);
        }
    }
}

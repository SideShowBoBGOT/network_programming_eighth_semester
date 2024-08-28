#pragma once

typedef struct {
    const char *address;
    int port;
    const char *dir_path;
} IterativeServerConfig;

void exit_err(const char *msg);
void warn_err(const char *msg);
void handle_client(int client_sock, const char* dir_path);
int create_and_bind_socket(int port, const char* address);
int accept_connection(int listenfd);
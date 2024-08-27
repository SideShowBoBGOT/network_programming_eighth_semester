#pragma once

typedef struct {
    const char *address;
    int port;
    const char *dir_path;
    int max_children;
} ServerConfig;

int create_and_bind_socket(const ServerConfig *config);
int accept_connection(int listenfd);
void exit_err(const char *msg);
void warn_err(const char *msg);

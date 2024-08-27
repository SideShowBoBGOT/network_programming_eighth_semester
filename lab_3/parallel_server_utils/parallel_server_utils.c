#include <parallel_server_utils/parallel_server_utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

static void print_config(const ParallelServerConfig *config) {
    printf("Server Configuration:\n");
    printf("  Address: %s\n", config->config.address);
    printf("  Port: %d\n", config->config.port);
    printf("  Directory Path: %s\n", config->config.dir_path);
    printf("  Maximum Children: %d\n", config->max_children);
}

ParallelServerConfig handle_cmd_args(const int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <server_address> <server_port> <directory_path> <max_children>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const ParallelServerConfig config = {
        .config.address = argv[1],
        .config.port = atoi(argv[2]),
        .config.dir_path = argv[3],
        .max_children = atoi(argv[4])
    };

    print_config(&config);

    return config;
}
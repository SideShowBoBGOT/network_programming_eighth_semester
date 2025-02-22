#pragma once

#include <iterative_server_utils/iterative_server_utils.h>

typedef struct {
    IterativeServerConfig config;
    int max_children;
} ParallelServerConfig;

ParallelServerConfig handle_cmd_args(int argc, char **argv);
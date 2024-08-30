#pragma once

#include <stdint.h>

typedef enum {
    READY_TO_RECEIVE,
    REFUSE_TO_RECEIVE,
} FileReceiveReadiness;

typedef enum {
    FILE_FOUND,
    FILE_NOT_FOUND,
} FileExistence;

typedef struct {
    uint16_t file_name_length;
} FileNameLength;

typedef struct {
    uint64_t size;
} FileSize;

inline void exit_err(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}
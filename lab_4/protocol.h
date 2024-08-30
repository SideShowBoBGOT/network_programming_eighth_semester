#pragma once

#include <stdint.h>

typedef enum {
    READY_TO_RECEIVE,
    REFUSE_TO_RECEIVE,
} file_receive_readiness_t;

typedef enum {
    FILE_FOUND,
    FILE_NOT_FOUND,
} file_existence_t;

typedef struct {
    uint16_t file_name_length;
} send_info_t;

typedef struct {
    uint64_t size;
} file_size_t;

inline void exit_err(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}
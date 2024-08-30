#pragma once

#include <stdint.h>

typedef struct {
    uint8_t file_name_length;
    off_t file_max_size;
} FileNameLengthMaxSize;

enum OperationPossibility {
    OPERATION_POSSIBLE,
    FILE_NOT_FOUND,
    FILE_SIZE_GREATER_THAN_MAX_SIZE,
    FAILED_TO_OPEN_FILE
};


typedef struct {
    uint64_t size;
} FileSize;

inline void exit_err(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}
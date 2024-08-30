#pragma once

#include <stdint.h>

typedef struct {
    uint8_t file_name_length;
    off_t file_max_size;
} FileNameLengthMaxSize;

typedef struct {
    off_t file_size;
    size_t chunk_size;
} FileAndChunkSize;

enum OperationPossibility {
    OPERATION_POSSIBLE,
    FILE_NOT_FOUND,
    FILE_SIZE_GREATER_THAN_MAX_SIZE,
    FAILED_TO_OPEN_FILE
};

typedef struct {
    uint64_t size;
} FileSize;

static _Noreturn void exit_err(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}
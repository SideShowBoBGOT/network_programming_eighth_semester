#pragma once

#include <stdint.h>

#define PROTOCOL_VERSION 1
#define MAX_FILENAME_LENGTH 255
#define CHUNK_SIZE 4096

enum message_type {
    REQUEST_FILE,
    FILE_INFO,
    FILE_NOT_FOUND,
    ERROR,
    READY_TO_RECEIVE,
    REFUSE_TO_RECEIVE,
    FILE_CONTENT
};

struct message_header {
    uint8_t version;
    uint8_t type;
    uint16_t length;
};

struct file_info {
    uint64_t size;
};

struct error_info {
    uint32_t error_code;
};

void handle_client(int client_sock, const char* dir_path);

#pragma once

#include <unistd.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <iso646.h>
#include <assert.h>

#define ARRAY_SIZE(data) sizeof((data)) / sizeof(data[0])

static bool readn(const int fd, void *const vptr, const size_t n, size_t *nread) {
    if(nread == NULL) {
        nread = alloca(sizeof(*nread));
    }
    *nread = 0;
    while(*nread < n) {
        ssize_t local_nread = read(fd, (char*)vptr + *nread, n - *nread);
        // printf("local_nread: %lu\n", local_nread);
        if(local_nread < 0) {
            if(errno == EINTR) {
                continue;
            }
            return false;
        } else if(local_nread == 0) {
            break;
        }
        *nread += (size_t)local_nread;
    }
    return true;
}

static bool writen(const int fd, const void *vptr, const size_t n, size_t *nwrite) {
    if(nwrite == NULL) {
        nwrite = alloca(sizeof(*nwrite));
    }
    *nwrite = 0;
    while(*nwrite < n) {
        ssize_t local_nwrite = write(fd, (const char*)vptr + *nwrite, n - *nwrite);
        // printf("local_nwrite: %lu\n", local_nwrite);
        if(local_nwrite <= 0) {
            if (local_nwrite < 0 and errno == EINTR) {
                continue;
            }
            return false;
        }
        *nwrite += (size_t)local_nwrite;
    }
    return true;
}

#define PROTOCOL_VERSION 17
typedef char filename_buff_t[255];

typedef enum __attribute__((packed)) {
    FileSizeResult_OK,
    FileSizeResult_ERROR_FILENAME_INVALID,
    FileSizeResult_ERROR_OPEN,  
    FileSizeResult_ERROR_STAT,  
} FileSizeResult;

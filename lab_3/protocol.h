#pragma once
#define _GNU_SOURCE
#include <fcntl.h>
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


#define ALWAYS_INLINE static inline __attribute((always_inline))

#define CONCAT_IMPL(x, y) x##y
#define CONCAT(x, y) CONCAT_IMPL(x, y)

#define UNIQUE_NAME_LINE(prefix) CONCAT(prefix, __LINE__)
#define UNIQUE_NAME_COUNTER(prefix) CONCAT(prefix, __COUNTER__)
#define UNIQUE_NAME(prefix) UNIQUE_NAME_COUNTER(UNIQUE_NAME_LINE(prefix))

#define ARRAY_SIZE(data) sizeof((data)) / sizeof(data[0])
#define DEFER(block) for(char UNIQUE_NAME_LINE(flag) = 1; UNIQUE_NAME_LINE(flag); UNIQUE_NAME_LINE(flag) = 0, (block))


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
            // EOF
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

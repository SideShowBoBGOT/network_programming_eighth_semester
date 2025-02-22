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

static ssize_t readn(int fd, void *vptr, size_t n) {
    size_t nleft;
    ssize_t nread;
    char *ptr = (char *)vptr;

    nleft = n;
    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) < 0) {
            if (errno == EINTR)
                nread = 0; /* and call read() again */
            else
                return (-1);
        } else if (nread == 0)
            break; /* EOF */

        nleft -= nread;
        ptr += nread;
    }
    return (n - nleft); /* return >= 0 */
}

static ssize_t writen(int fd, const void *vptr, size_t n) {
    size_t nleft;
    ssize_t nwritten;
    const char *ptr = (const char *)vptr;
    nleft = n;
    while (nleft > 0) {
        if ((nwritten = write(fd, ptr, nleft)) <= 0) {
            if (nwritten < 0 && errno == EINTR)
                nwritten = 0; /* and call write() again */
            else
                return (-1); /* error */
        }
        nleft -= nwritten;
        ptr += nwritten;
    }
    return (n);
}

#define PROTOCOL_VERSION 1345
typedef uint8_t filename_buff_t[255];

typedef enum __attribute__((packed)) {
    FileSizeResponseType_OK,
    FileSizeResponseType_FILE_NOT_FOUND,  
    FileSizeResponseType_FILENAME_INVALID,  
} FileSizeResponseType;

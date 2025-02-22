#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <sys/stat.h> 
#include <fcntl.h>

#define CONCAT_IMPL(x, y) x##y
#define CONCAT(x, y) CONCAT_IMPL(x, y)
#define ARRAY_SIZE(data) sizeof((data)) / sizeof(data[0])
#define UNIQUE_NAME_LINE(prefix) CONCAT(prefix, __LINE__)
#define ENUM_NAME_VALUE_NAME(enum_name, value_name) enum_name##_##value_name
#define DECLARE_ENUM(enum_name, enum_values)\
    typedef enum { enum_values(enum_name, ENUM_NAME_VALUE_NAME) } enum_name


#define ENUM_VALUES(enum_name, macro)\
    macro(enum_name, V1),\
    macro(enum_name, V2),\

    DECLARE_ENUM(ProtocolVersion, ENUM_VALUES);
#undef ENUM_VALUES

typedef struct {
    const char *address;
    uint16_t port;
    const char *filename;
    size_t max_file_size;
} ClientConfig;

static void print_config(const ClientConfig *config) {
    printf("Client Configuration:\n");
    printf("  Address: %s\n", config->address);
    printf("  Port: %d\n", config->port);
    printf("  Filename: %s\n", config->filename);
    printf("  Maximum Children: %ld\n", config->max_file_size);
}

static ClientConfig handle_cmd_args(const int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <server_address> <server_port> <filename> <max_file_size>\n", argv[0]);
        exit(1);
    }

    const ClientConfig config = {
        .address = argv[1],
        .port = (uint16_t)atoi(argv[2]),
        .filename = argv[3],
        .max_file_size = (uint64_t)(argv[4])
    };

    print_config(&config);

    return config;
}

typedef struct {
    ProtocolVersion protocol_version;
    uint8_t filename[255];
} RequestFileMessage;

#define ENUM_VALUES(enum_name, macro)\
    macro(enum_name, ERROR_PROTOCOL_MISMATCH),\
    macro(enum_name, ERROR_FILE_NOT_FOUND),\
    macro(enum_name, OK),\

    DECLARE_ENUM(RequestFileResponseMessage_Type, ENUM_VALUES);
#undef ENUM_VALUES

typedef struct {
    ProtocolVersion host_version;
} RequestFileResponseMessage_ErrorProtocolMismatch;

typedef struct {
    size_t host_file_size; 
} RequestFileResponseMessage_Ok;

typedef struct {
    RequestFileResponseMessage_Type type;
    union {
        RequestFileResponseMessage_ErrorProtocolMismatch error_protocol_mismatch;
        RequestFileResponseMessage_Ok ok;
    } data;
} RequestFileResponseMessage;

static void func(const ClientConfig *const config, const int sock) {
    {
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(config->port);
        if (inet_pton(AF_INET, config->address, &server_addr.sin_addr) <= 0) {
            perror("Invalid address");
            return;
        }
        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Connection failed");
            return;
        }
    }
    {
        RequestFileMessage message;
        message.protocol_version = ProtocolVersion_V1;
        memcpy(message.filename, config->filename, ARRAY_SIZE(message.filename));
        if(send(sock, &message, sizeof(message), 0) == -1) {
            perror("Failed to send request");
            return;
        }
    }
    const RequestFileResponseMessage_Ok file_data = ({
        RequestFileResponseMessage message;
        if(recv(sock, &message, sizeof(message), 0) == -1) {
            perror("Failed to receive response");
            return;
        }
        if(message.type != RequestFileResponseMessage_Type_OK) {
            printf("RequestFileResponseMessageType %d\n", message.type);
            return;
        }
        bool readiness;
        if(message.data.ok.host_file_size > config->max_file_size) {
            fprintf(stderr, "File too large: %ld\n", message.data.ok.host_file_size);
            readiness = false;
            if(send(sock, &readiness, sizeof(readiness), 0) == -1) {
                perror("Failed to send readiness");
                return;
            }
            return;
        }
        readiness = true;
        if(send(sock, &readiness, sizeof(readiness), 0) == -1) {
            perror("Failed to send readiness");
            return;
        }
        message.data.ok;
    });

    {
        const int file_fd = open(config->filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (file_fd < 0) {
            perror("open");
            return;
        }
        
        const ssize_t res = splice(sock, NULL, file_fd, NULL, file_data.host_file_size, 0);
        if(res < 0) {
            perror("Failed to receive file data");
        } else if((size_t)res != file_data.host_file_size) {
            perror("Incomplete file transfer");
        }
        printf("Successfuly received file");
        close(file_fd);
    }
}

int main(const int argc, char *argv[]) {
    const ClientConfig config = handle_cmd_args(argc, argv);

    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
    } else {
        func(&config, sock);
    }
    return EXIT_SUCCESS;
}

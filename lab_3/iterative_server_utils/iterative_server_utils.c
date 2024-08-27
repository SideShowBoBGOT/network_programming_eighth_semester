#include <iterative_server_utils/iterative_server_utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdint.h>

void handle_client(const int client_sock, const char* const dir_path) {
    struct message_header header;
    recv(client_sock, &header, sizeof(header), 0);

    if (header.version != PROTOCOL_VERSION) {
        printf("Protocol version mismatch.\n");
        close(client_sock);
        return;
    }

    if (header.type != REQUEST_FILE) {
        printf("Unexpected message type.\n");
        close(client_sock);
        return;
    }

    char filename[MAX_FILENAME_LENGTH + 1];
    recv(client_sock, filename, header.length, 0);
    filename[header.length] = '\0';

    // Check if filename is valid
    if (strchr(filename, '/') != NULL) {
        header.type = ERROR;
        send(client_sock, &header, sizeof(header), 0);
        struct error_info error = {1}; // 1 for invalid filename
        send(client_sock, &error, sizeof(error), 0);
        close(client_sock);
        return;
    }

    char filepath[PATH_MAX];
    snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, filename);

    struct stat st;
    if (stat(filepath, &st) == -1 || !S_ISREG(st.st_mode)) {
        header.type = FILE_NOT_FOUND;
        send(client_sock, &header, sizeof(header), 0);
        close(client_sock);
        return;
    }

    header.type = FILE_INFO;
    send(client_sock, &header, sizeof(header), 0);

    struct file_info file_info = {st.st_size};
    send(client_sock, &file_info, sizeof(file_info), 0);

    recv(client_sock, &header, sizeof(header), 0);
    if (header.type != READY_TO_RECEIVE) {
        close(client_sock);
        return;
    }

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        perror("Failed to open file");
        close(client_sock);
        return;
    }

    char buffer[CHUNK_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(client_sock, buffer, bytes_read, 0);
    }

    fclose(file);
    close(client_sock);
}
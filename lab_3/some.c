#include <stdio.h>
#include <string.h>
#include <inttypes.h>

int main(const int argc, char** argv) {
    const char *const dir_path = "dsfklsdfjsd/dsf/sdfdsfsd/./dddd";
    const char *const filename = "hello yopta";
    const size_t dir_path_strlen = strlen(dir_path);
    const size_t filename_strlen = strlen(filename);
    const size_t buffer_byte_count = dir_path_strlen + filename_strlen + 1;
    char buffer[buffer_byte_count];
    buffer[0] = 0;
    strncpy(buffer, dir_path, dir_path_strlen);
    puts(buffer);
    strncat(buffer, filename, filename_strlen);
    printf("strlen: %s\n", buffer);
    return 0;
}

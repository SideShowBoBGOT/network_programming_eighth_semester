cmake_minimum_required(VERSION 3.10)
project(lab_4 C)

set(CMAKE_C_STANDARD 11)

set(strict_compiler_options -Wall -Wextra -Werror)

add_executable(client client.c protocol.h)
target_compile_options(client PRIVATE ${strict_compiler_options})

add_executable(iterative_server iterative_server.c protocol.h)
target_compile_options(iterative_server PRIVATE ${strict_compiler_options})

add_executable(server server.c)
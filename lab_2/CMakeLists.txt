cmake_minimum_required(VERSION 3.10)
project(lab_1 C)

set(CMAKE_C_STANDARD 11)

add_executable(lab_1 main.c)
target_compile_options(lab_1 PRIVATE -Werror -Wall -Wextra)
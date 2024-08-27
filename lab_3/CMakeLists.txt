cmake_minimum_required(VERSION 3.10)
project(lab_3 C)

set(CMAKE_C_STANDARD 11)

set(strict_compiler_options -Wall -Wextra -Werror)

add_library(iterative_server_utils STATIC
        iterative_server_utils/iterative_server_utils/iterative_server_utils.h
        iterative_server_utils/iterative_server_utils.c
)
target_include_directories(iterative_server_utils PUBLIC iterative_server_utils)
target_compile_options(iterative_server_utils PRIVATE ${strict_compiler_options})

add_library(parallel_server_utils STATIC
        parallel_server_utils/parallel_server_utils/parallel_server_utils.h
        parallel_server_utils/parallel_server_utils.c
)
target_include_directories(parallel_server_utils PUBLIC parallel_server_utils)
target_compile_options(parallel_server_utils PRIVATE ${strict_compiler_options})

add_executable(client client.c)
target_link_libraries(client PRIVATE iterative_server_utils)
target_compile_options(client PRIVATE ${strict_compiler_options})

add_executable(iterative_server iterative_server.c)
target_link_libraries(iterative_server PRIVATE iterative_server_utils)
target_compile_options(iterative_server PRIVATE ${strict_compiler_options})

add_executable(parallel_server parallel_server.c)
target_link_libraries(parallel_server PRIVATE iterative_server_utils parallel_server_utils)
target_compile_options(parallel_server PRIVATE ${strict_compiler_options})

add_executable(pool_server pool_server.c)
target_link_libraries(pool_server PRIVATE iterative_server_utils parallel_server_utils)
target_compile_options(pool_server PRIVATE ${strict_compiler_options})
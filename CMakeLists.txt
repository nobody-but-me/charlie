
cmake_minimum_required(VERSION 3.21)
project(CHARLIE C)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_executable(charlie)

target_sources(charlie PRIVATE ${CMAKE_SOURCE_DIR}/main.c)
target_compile_options(charlie PRIVATE -Wall -Werror -Wextra -pedantic) # I like to torture myself

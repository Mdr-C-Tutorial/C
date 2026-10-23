cmake_minimum_required(VERSION 3.20)
project(sensord C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(sensord sensord.c evl.c sdnotify.c)
target_compile_options(sensord PRIVATE -Wall -Wextra)

install(TARGETS sensord DESTINATION bin)
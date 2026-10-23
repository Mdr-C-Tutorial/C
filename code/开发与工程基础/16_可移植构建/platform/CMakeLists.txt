cmake_minimum_required(VERSION 3.20)
project(platform_demo LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(platform_demo main.c)
if(WIN32)
    target_sources(platform_demo PRIVATE platform_win32.c)
else()
    target_sources(platform_demo PRIVATE platform_posix.c)
endif()

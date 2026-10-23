cmake_minimum_required(VERSION 3.20)
project(lfs_host_demo C)

include(FetchContent)
FetchContent_Declare(
    littlefs
    GIT_REPOSITORY https://github.com/littlefs-project/littlefs.git
    GIT_TAG        v2.11.3
)
FetchContent_MakeAvailable(littlefs)

add_executable(lfs_host_demo
    main.c
    "${littlefs_SOURCE_DIR}/lfs.c"
    "${littlefs_SOURCE_DIR}/lfs_util.c"
    "${littlefs_SOURCE_DIR}/bd/lfs_rambd.c")
target_include_directories(lfs_host_demo PRIVATE "${littlefs_SOURCE_DIR}")
target_compile_options(lfs_host_demo PRIVATE -std=c17 -Wall -Wextra)

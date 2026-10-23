cmake_minimum_required(VERSION 3.20)
project(fatfs_host_demo C)

include(FetchContent)
FetchContent_Declare(
    fatfs
    GIT_REPOSITORY https://github.com/abbrev/fatfs.git
    GIT_TAG        R0.16
)
FetchContent_MakeAvailable(fatfs)
configure_file("${CMAKE_CURRENT_LIST_DIR}/ffconf.h" "${fatfs_SOURCE_DIR}/source/ffconf.h" COPYONLY)

add_executable(fatfs_host_demo
    main.c
    ram_diskio.c
    "${fatfs_SOURCE_DIR}/source/ff.c")
target_include_directories(fatfs_host_demo PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}"
    "${fatfs_SOURCE_DIR}/source")
target_compile_options(fatfs_host_demo PRIVATE -std=c17 -Wall -Wextra)

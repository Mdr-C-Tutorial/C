cmake_minimum_required(VERSION 3.20)

project(sqlite_demo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# 合并源码不入库。配置时用 -DSQLITE_DIR=... 指向解压出的
# sqlite-amalgamation-3460100 目录，里面要有 sqlite3.c 与 sqlite3.h。
if (NOT SQLITE_DIR)
    message(FATAL_ERROR "请用 -DSQLITE_DIR=<合并源码目录> 指定 sqlite3.c 所在目录")
endif ()
if (NOT EXISTS "${SQLITE_DIR}/sqlite3.c")
    message(FATAL_ERROR "SQLITE_DIR 里找不到 sqlite3.c：${SQLITE_DIR}")
endif ()
get_filename_component(SQLITE_DIR "${SQLITE_DIR}" ABSOLUTE)
set(SQLITE_INC "${SQLITE_DIR}")

add_library(sqlite3_core STATIC "${SQLITE_DIR}/sqlite3.c")
target_include_directories(sqlite3_core PUBLIC "${SQLITE_INC}")
if (MSVC)
    target_compile_definitions(sqlite3_core PUBLIC _CRT_SECURE_NO_WARNINGS)
endif ()

if (MSVC)
    set(DEMO_WARNINGS /W4 /utf-8)
else ()
    set(DEMO_WARNINGS -Wall -Wextra)
endif ()

# 本目录新写的代码要过 -Wall -Wextra；合并源码保持上游原样。
function(demo_executable name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE sqlite3_core)
    target_compile_options(${name} PRIVATE ${DEMO_WARNINGS})
    if (MINGW)
        # MinGW 默认按微软运行库检查 printf 格式，%zu 会报未知转换。
        target_compile_definitions(${name} PRIVATE __USE_MINGW_ANSI_STDIO=1)
    endif ()
endfunction()

demo_executable(test_users test_users.c)
demo_executable(mdr_reverse mdr_reverse.c)

# 可加载扩展：Windows 上得到 mdrreverse.dll，其余平台得到 libmdrreverse.so。
add_library(mdrreverse MODULE mdrreverse.c)
target_link_libraries(mdrreverse PRIVATE sqlite3_core)
target_compile_options(mdrreverse PRIVATE ${DEMO_WARNINGS})
set_target_properties(mdrreverse PROPERTIES PREFIX "")

demo_executable(static_result static_result.c)

demo_executable(vfsdemo vfs/vfsdemo.c vfs/vfslog.c)
target_include_directories(vfsdemo PRIVATE vfs)

# 调试版命令行工具：第 1 节的六个 -D 选项。
add_executable(sqlite3d "${SQLITE_DIR}/shell.c" "${SQLITE_DIR}/sqlite3.c")
target_include_directories(sqlite3d PRIVATE "${SQLITE_INC}")
target_compile_definitions(sqlite3d PRIVATE
    SQLITE_DEBUG
    SQLITE_ENABLE_EXPLAIN_COMMENTS
    SQLITE_ENABLE_TREETRACE
    SQLITE_ENABLE_WHERETRACE
    SQLITE_ENABLE_DBPAGE_VTAB
    SQLITE_ENABLE_DBSTAT_VTAB
)
if (NOT MSVC)
    target_compile_options(sqlite3d PRIVATE -O0 -g)
endif ()

enable_testing()
add_test(NAME users COMMAND test_users)
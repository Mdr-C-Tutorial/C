cmake_minimum_required(VERSION 3.20)

project(mdrdb C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(MDRDB_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

if (MSVC)
    set(MDRDB_WARNINGS /W4 /utf-8)
    set(MDRDB_WERROR /WX)
else ()
    set(MDRDB_WARNINGS -Wall -Wextra -Wpedantic)
    set(MDRDB_WERROR -Werror)
endif ()
if (MDRDB_WARNINGS_AS_ERRORS)
    list(APPEND MDRDB_WARNINGS ${MDRDB_WERROR})
endif ()

# 数据库本身编成静态库，命令行程序和各个测试都链接它。
add_library(mdrdb_core STATIC
    src/btree.c
    src/db.c
    src/file.c
    src/journal.c
    src/node.c
    src/pager.c
    src/record.c
    src/repl.c
)
target_include_directories(mdrdb_core
    PUBLIC include
    PRIVATE src
)
target_compile_options(mdrdb_core PRIVATE ${MDRDB_WARNINGS})
if (MSVC)
    target_compile_definitions(mdrdb_core PUBLIC _CRT_SECURE_NO_WARNINGS)
endif ()

add_executable(mdrdb src/main.c)
target_link_libraries(mdrdb PRIVATE mdrdb_core)
target_include_directories(mdrdb PRIVATE src)
target_compile_options(mdrdb PRIVATE ${MDRDB_WARNINGS})

# 每个测试是一个独立的程序，在构建目录里运行，临时文件也留在那里。
enable_testing()
foreach (name record node journal pager btree crash repl)
    add_executable(test_${name} tests/test_${name}.c)
    target_link_libraries(test_${name} PRIVATE mdrdb_core)
    target_include_directories(test_${name} PRIVATE src)
    target_compile_options(test_${name} PRIVATE ${MDRDB_WARNINGS})
    add_test(NAME ${name} COMMAND test_${name})
endforeach ()

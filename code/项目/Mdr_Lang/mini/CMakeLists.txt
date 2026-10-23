cmake_minimum_required(VERSION 3.20)

project(mdr_mini C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(mdr_mini
    src/main.c
    src/mdr_mini.c
    src/ast.c
    src/lexer.c
    src/parser.c
    src/interp.c
    src/emit_c.c
)

target_include_directories(mdr_mini PRIVATE include src)

if (MSVC)
    target_compile_options(mdr_mini PRIVATE /W4 /WX)
else ()
    target_compile_options(mdr_mini PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif ()

if (MINGW)
    target_compile_definitions(mdr_mini PRIVATE __USE_MINGW_ANSI_STDIO=1)
endif ()
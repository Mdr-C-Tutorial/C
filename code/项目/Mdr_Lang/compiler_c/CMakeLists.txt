cmake_minimum_required(VERSION 3.20)

project(mdr_lang_compiler_c C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(mdrcc
    src/main.c
    src/mdr_compiler.c
)

target_include_directories(mdrcc PRIVATE include)

if (MSVC)
    target_compile_options(mdrcc PRIVATE /W4 /WX)
else ()
    target_compile_options(mdrcc PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif ()

cmake_minimum_required(VERSION 3.20)

project(mdr_lang_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

enable_testing()

set(MDR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

# 被测的源文件在上一级目录里。Ninja 给源码树之外的源文件生成的目标文件名带绝对
# 路径，路径含中文时无法创建。配置时把两个被测目录复制进构建目录并保持相对布局，
# 目标文件就只带相对路径，仓库放在中文目录下也能构建。
set(MDR_STAGE "${CMAKE_CURRENT_BINARY_DIR}/staged")
file(REMOVE_RECURSE "${MDR_STAGE}")
file(COPY "${MDR_ROOT}/compiler_c/src" DESTINATION "${MDR_STAGE}/compiler_c")
file(COPY "${MDR_ROOT}/compiler_c/include" DESTINATION "${MDR_STAGE}/compiler_c")
file(COPY "${MDR_ROOT}/mini/src" DESTINATION "${MDR_STAGE}/mini")
file(COPY "${MDR_ROOT}/mini/include" DESTINATION "${MDR_STAGE}/mini")

add_library(mdr_frontend STATIC "${MDR_STAGE}/compiler_c/src/mdr_compiler.c")
target_include_directories(mdr_frontend PUBLIC "${MDR_ROOT}/compiler_c/include")

add_library(mdr_mini_lib STATIC
    "${MDR_STAGE}/mini/src/ast.c"
    "${MDR_STAGE}/mini/src/lexer.c"
    "${MDR_STAGE}/mini/src/parser.c"
    "${MDR_STAGE}/mini/src/interp.c"
    "${MDR_STAGE}/mini/src/emit_c.c"
    "${MDR_STAGE}/mini/src/mdr_mini.c"
)
target_include_directories(mdr_mini_lib PUBLIC "${MDR_ROOT}/mini/include")
target_include_directories(mdr_mini_lib PRIVATE "${MDR_STAGE}/mini/src")

add_executable(test_semantics test_semantics.c)
target_link_libraries(test_semantics PRIVATE mdr_frontend)

add_executable(test_run test_run.c)
target_link_libraries(test_run PRIVATE mdr_mini_lib)

add_executable(fuzz_truncate fuzz_truncate.c)
target_link_libraries(fuzz_truncate PRIVATE mdr_frontend)

if (MINGW)
    add_compile_definitions(__USE_MINGW_ANSI_STDIO=1)
endif ()

if (MSVC)
    add_compile_options(/W4)
else ()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif ()

add_test(NAME semantics COMMAND test_semantics "${CMAKE_CURRENT_SOURCE_DIR}/cases")
add_test(NAME run COMMAND test_run "${CMAKE_CURRENT_SOURCE_DIR}/cases"
                              "${CMAKE_CURRENT_BINARY_DIR}/run_tmp")
add_test(NAME truncate COMMAND fuzz_truncate "${CMAKE_CURRENT_SOURCE_DIR}/cases")
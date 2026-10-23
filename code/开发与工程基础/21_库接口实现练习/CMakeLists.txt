cmake_minimum_required(VERSION 3.20)
project(mdr VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MDR_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(MDR_NO_BUILTINS "Use plain C fallbacks instead of compiler builtins" OFF)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

if(MINGW)
    add_compile_definitions(__USE_MINGW_ANSI_STDIO=1)
endif()

if(MDR_SANITIZE AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# 库的源文件：每一节新增的 mdr_*.c 自动进入，不需要改这个文件。
file(GLOB MDR_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

add_library(mdr STATIC ${MDR_SOURCES})
target_include_directories(mdr PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(MDR_NO_BUILTINS)
    target_compile_definitions(mdr PUBLIC MDR_NO_BUILTINS=1)
endif()

# 21.6：同一批源文件再编一个强制走普通 C 回退的库，测试两条路径结果一致。
# 整个构建已经用 MDR_NO_BUILTINS 时不必重复。
if(NOT MDR_NO_BUILTINS)
    add_library(mdr_fallback STATIC ${MDR_SOURCES})
    target_include_directories(mdr_fallback PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_compile_definitions(mdr_fallback PUBLIC MDR_NO_BUILTINS=1)
endif()

enable_testing()

# 单元测试：tests/test_<模块>.c 各自链接库与 tests/check.c，注册成同名的 ctest 测试。
file(GLOB MDR_UNIT_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.c")
foreach(test_src IN LISTS MDR_UNIT_TESTS)
    get_filename_component(test_name "${test_src}" NAME_WE)
    add_executable(${test_name}
        "${test_src}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/check.c"
    )
    target_include_directories(${test_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_link_libraries(${test_name} PRIVATE mdr)
    add_test(NAME ${test_name} COMMAND ${test_name})

    if(TARGET mdr_fallback)
        add_executable(${test_name}_fallback
            "${test_src}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/check.c"
        )
        target_include_directories(${test_name}_fallback PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        target_link_libraries(${test_name}_fallback PRIVATE mdr_fallback)
        add_test(NAME ${test_name}_fallback COMMAND ${test_name}_fallback)
    endif()
endforeach()

# 差分测试：tests/diff_<模块>.c 额外链接 tests/diff_util.c 中的比较与转储工具。
# 这些文件由 21.7 建立；diff_util.c 到齐之前不生成任何差分测试目标，但通配符照常登记，
# 这样后来添加的 diff_*.c 会触发重新配置。
file(GLOB MDR_DIFF_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/diff_*.c")
set(MDR_DIFF_SUPPORT "${CMAKE_CURRENT_SOURCE_DIR}/tests/diff_util.c")
if(EXISTS "${MDR_DIFF_SUPPORT}")
    list(REMOVE_ITEM MDR_DIFF_TESTS "${MDR_DIFF_SUPPORT}")
    foreach(diff_src IN LISTS MDR_DIFF_TESTS)
        get_filename_component(diff_name "${diff_src}" NAME_WE)
        add_executable(${diff_name}
            "${diff_src}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/check.c"
            "${MDR_DIFF_SUPPORT}"
        )
        target_include_directories(${diff_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        target_link_libraries(${diff_name} PRIVATE mdr)
        add_test(NAME ${diff_name} COMMAND ${diff_name})
        set_tests_properties(${diff_name} PROPERTIES LABELS diff)
    endforeach()
endif()
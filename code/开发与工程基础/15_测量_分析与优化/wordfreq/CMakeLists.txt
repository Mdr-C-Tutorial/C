cmake_minimum_required(VERSION 3.20)
project(wordfreq VERSION 0.2.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(WF_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(WF_COVERAGE "Build with gcov instrumentation" OFF)
option(WF_FUZZ "Build the libFuzzer target and the corpus replay program" OFF)

# 推荐给开发构建的一组警告；WF_WERROR 打开时把警告当错误处理
set(WF_WARNINGS -Wall -Wextra -Wshadow -Wformat=2 -Wmissing-prototypes -Wstrict-prototypes -Wvla)

add_library(wf STATIC
    src/token.c
    src/counter.c
    src/report.c
)
target_include_directories(wf PUBLIC include)
target_compile_options(wf PRIVATE ${WF_WARNINGS})

# MinGW 的 -Wformat 不认识 %zu（运行结果正确），加上这个宏改用 UCRT 的格式化函数
if(MINGW)
    target_compile_definitions(wf PUBLIC __USE_MINGW_ANSI_STDIO=1)
endif()

add_executable(wordfreq app/main.c)
target_link_libraries(wordfreq PRIVATE wf)
target_compile_options(wordfreq PRIVATE ${WF_WARNINGS})

enable_testing()

# 单元测试：每个测试程序链接同一个 libwf，通过公开接口检查行为
add_executable(test_token tests/test_token.c tests/check.c)
target_link_libraries(test_token PRIVATE wf)
target_compile_options(test_token PRIVATE ${WF_WARNINGS})
add_test(NAME token COMMAND test_token)
set_tests_properties(token PROPERTIES LABELS unit)

add_executable(test_counter tests/test_counter.c tests/check.c)
target_link_libraries(test_counter PRIVATE wf)
target_compile_options(test_counter PRIVATE ${WF_WARNINGS})
add_test(NAME counter COMMAND test_counter)
set_tests_properties(counter PROPERTIES LABELS unit)

# 端到端测试：运行程序，把输出与 tests/data/sample.expected 比较
add_test(NAME e2e
    COMMAND ${CMAKE_COMMAND}
        -DWORDFREQ=$<TARGET_FILE:wordfreq>
        -DDATA=${CMAKE_CURRENT_SOURCE_DIR}/tests/data
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_e2e.cmake)
set_tests_properties(e2e PROPERTIES LABELS e2e)

# 模糊测试目标由 clang 的 -fsanitize=fuzzer 驱动；重放程序用普通 gcc 就能构建，
# 因此没有 clang 的 CI 也能把语料与崩溃输入当作回归测试跑。
if(WF_FUZZ)
    add_executable(replay_token fuzz/replay_main.c fuzz/fuzz_token.c)
    target_link_libraries(replay_token PRIVATE wf)
    target_compile_options(replay_token PRIVATE ${WF_WARNINGS})
    file(GLOB WF_REGRESS_INPUTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/regress/*)
    if(WF_REGRESS_INPUTS)
        add_test(NAME replay COMMAND replay_token ${WF_REGRESS_INPUTS})
        set_tests_properties(replay PROPERTIES LABELS e2e)
    endif()
endif()

# Sanitizer：编译和链接都要加同一组选项
if(WF_SANITIZE)
    foreach(target wf wordfreq test_token test_counter)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endforeach()
endif()

# 覆盖率：--coverage 等价于 -fprofile-arcs -ftest-coverage
if(WF_COVERAGE)
    set(wf_coverage_targets wf wordfreq test_token test_counter)
    if(WF_FUZZ)
        list(APPEND wf_coverage_targets replay_token)
    endif()
    foreach(target ${wf_coverage_targets})
        target_compile_options(${target} PRIVATE --coverage -O0)
        target_link_options(${target} PRIVATE --coverage)
    endforeach()
endif()

if(WF_WERROR)
    foreach(target wf wordfreq test_token test_counter)
        target_compile_options(${target} PRIVATE -Werror)
    endforeach()
endif()

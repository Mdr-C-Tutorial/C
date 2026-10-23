cmake_minimum_required(VERSION 3.20)
project(wordfreq VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(WF_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(WF_COVERAGE "Build with gcov instrumentation" OFF)
option(WF_FUZZ "Build the libFuzzer target and the corpus replay program" OFF)
option(WF_WERROR "Treat warnings as errors" OFF)
option(WF_HAVE_GETLINE "Assume getline is available (set OFF to force the fgets path)" ON)

include(CheckSymbolExists)

# 按编译器选择警告选项：MSVC 用 /W4，其余用 GCC/Clang 的选项
if(MSVC)
    set(WF_WARNINGS /W4)
else()
    set(WF_WARNINGS -Wall -Wextra -Wshadow -Wformat=2 -Wmissing-prototypes
        -Wstrict-prototypes -Wvla)
endif()

# 配置时检测 getline：MSVC 与 MinGW 不提供，config.h 里据此选择实现。
# -DWF_HAVE_GETLINE=OFF 时跳过检测，强制走 fgets 回退路径。
set(WF_CONFIG_HAVE_GETLINE OFF)
if(WF_HAVE_GETLINE)
    set(CMAKE_REQUIRED_DEFINITIONS -D_POSIX_C_SOURCE=200809L)
    check_symbol_exists(getline "stdio.h" WF_HAVE_GETLINE_FOUND)
    unset(CMAKE_REQUIRED_DEFINITIONS)
    if(WF_HAVE_GETLINE_FOUND)
        set(WF_CONFIG_HAVE_GETLINE ON)
    endif()
endif()
# 生成的头文件放进一个与源码树同构的目录，代码里写 #include "wf/config.h"
configure_file(src/config.h.in ${PROJECT_BINARY_DIR}/include/wf/config.h)
# 版本号只在 project(VERSION) 定义这一次，version.h 由它生成
configure_file(include/wf/version.h.in ${PROJECT_BINARY_DIR}/include/wf/version.h)

add_library(wf
    src/token.c
    src/counter.c
    src/report.c
    src/version.c
)
target_include_directories(wf PUBLIC include ${PROJECT_BINARY_DIR}/include)
target_compile_options(wf PRIVATE ${WF_WARNINGS})
target_compile_definitions(wf PRIVATE WF_BUILDING)

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

# 模糊测试目标由 clang 的 -fsanitize=fuzzer 驱动；重放程序用普通 gcc 就能构建。
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
    if(MSVC)
        set(WF_WERROR_FLAG /WX)
    else()
        set(WF_WERROR_FLAG -Werror)
    endif()
    foreach(target wf wordfreq test_token test_counter)
        target_compile_options(${target} PRIVATE ${WF_WERROR_FLAG})
    endforeach()
endif()

# 构建共享库时定义 WF_SHARED，导出标记才生效；使用方通过 target_link_libraries
# 继承这个定义（PUBLIC），所以 wordfreq 与测试程序都能链接到导出符号。
if(BUILD_SHARED_LIBS)
    target_compile_definitions(wf PUBLIC WF_SHARED)
endif()
# 共享库的版本与 ABI 版本：Linux 上生成 libwf.so.1.0.0 与 libwf.so.1
if(BUILD_SHARED_LIBS)
    set_target_properties(wf PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
endif()

# 安装规则：头文件、库与程序，供 CPack 打包
include(GNUInstallDirs)
install(TARGETS wf wordfreq
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h")
install(FILES ${PROJECT_BINARY_DIR}/include/wf/config.h
              ${PROJECT_BINARY_DIR}/include/wf/version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/wf)
install(FILES README.md CHANGELOG.md LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})

set(CPACK_PACKAGE_NAME wordfreq)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Word frequency counter")
set(CPACK_PACKAGE_CONTACT "wordfreq contributors")
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE)
include(CPack)

# Doxygen 可选：默认不找。打开 WF_BUILD_DOCS 且配置时找到 Doxygen 才生成 docs 目标。
option(WF_BUILD_DOCS "Generate API reference with Doxygen" OFF)
if(WF_BUILD_DOCS)
    find_package(Doxygen REQUIRED)
    doxygen_add_docs(docs
        include/wf
        COMMENT "Generate API reference"
    )
endif()

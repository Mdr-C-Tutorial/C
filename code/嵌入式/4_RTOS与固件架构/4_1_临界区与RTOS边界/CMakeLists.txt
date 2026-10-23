cmake_minimum_required(VERSION 3.20)
project(rtos_critical_section C)

# 本节的 Cortex-M3 片段（race_disasm.c、basepri_lock.c、basepri_nest.c、
# atomic_inc.c、cp64.c、mpsc_reserve.c）只做编译与反汇编，编译命令写在
# 各自文件开头。这里只构建能在 PC 上运行的三个程序。

if(WIN32)
    set(_threads "")
else()
    find_package(Threads REQUIRED)
    set(_threads Threads::Threads)
endif()

add_executable(superloop superloop_vs_tasks/superloop.c)
add_executable(mpsc_test test_mpsc.c mpsc_reserve.c)
target_link_libraries(mpsc_test PRIVATE ${_threads})

# 习题起始代码，只构建、不运行（任务在 TODO 里）
add_executable(mpsc_ringbuf_starter exercises/mpsc_ringbuf_starter.c)
target_link_libraries(mpsc_ringbuf_starter PRIVATE ${_threads})
add_executable(clock64_starter exercises/clock64_starter.c)

foreach(_t superloop mpsc_test mpsc_ringbuf_starter clock64_starter)
    target_compile_options(${_t} PRIVATE -std=c17 -O2 -Wall -Wextra)
endforeach()

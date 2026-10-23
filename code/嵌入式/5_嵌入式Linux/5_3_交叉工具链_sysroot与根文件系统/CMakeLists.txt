cmake_minimum_required(VERSION 3.20)
project(mdr_calc C)

add_executable(calc calc.c)
target_link_libraries(calc PRIVATE m)
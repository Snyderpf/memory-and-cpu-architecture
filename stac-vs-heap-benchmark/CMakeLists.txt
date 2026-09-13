cmake_minimum_required(VERSION 3.10)
project(StackVsHeapBenchmark)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-O2 -Wall -Wextra)

add_executable(main main.cpp)
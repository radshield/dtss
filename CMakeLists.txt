cmake_minimum_required(VERSION 3.18)
project(double-trouble-seu-smasher)

set(CMAKE_CXX_STANDARD 20)

find_package(LLVM REQUIRED CONFIG)

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

add_subdirectory(dtss)

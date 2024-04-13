cmake_minimum_required(VERSION 3.19)
project(double-trouble-seu-smasher)

set(CMAKE_CXX_STANDARD 20)

find_package(Boost 1.45.0 REQUIRED)
find_package(Threads REQUIRED)

add_subdirectory(crypt)
add_subdirectory(dnn)
add_subdirectory(img)
add_subdirectory(packet)
add_subdirectory(zip)

cmake_minimum_required(VERSION 3.18)
project(double-trouble-seu-smasher)

set(CMAKE_CXX_STANDARD 20)

find_package(Boost 1.45.0 REQUIRED)
find_package(Threads REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})

add_library(dtss src/dtss.cpp)
target_link_libraries(dtss
	${Boost_LIBRARIES}
	Threads::Threads
)

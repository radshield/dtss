cmake_minimum_required(VERSION 3.19)
project(double-trouble-seu-smasher)

set(CMAKE_CXX_STANDARD 20)

find_package(Boost 1.45.0 REQUIRED)
find_package(Threads REQUIRED)
find_package(ZLIB)

include_directories(${Boost_INCLUDE_DIRS})

add_executable(dtss_zip src/dtss_zip.cpp)
target_link_libraries(dtss_zip
	${Boost_LIBRARIES}
	Threads::Threads
	ZLIB::ZLIB
)

add_executable(dtss_zip_disk src/dtss_zip_disk.cpp)
target_link_libraries(dtss_zip_disk
	${Boost_LIBRARIES}
	Threads::Threads
	ZLIB::ZLIB
)

add_executable(zip src/zip.cpp)
target_link_libraries(zip
	ZLIB::ZLIB
)

add_executable(zip_disk src/zip_disk.cpp)
target_link_libraries(zip_disk
	ZLIB::ZLIB
)


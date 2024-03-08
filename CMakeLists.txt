cmake_minimum_required(VERSION 3.19)
project(double-trouble-seu-smasher)

set(CMAKE_CXX_STANDARD 20)

find_package(Boost 1.45.0 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})

add_executable(dtss_crypt src/dtss_crypt.cpp)
target_link_libraries(dtss_crypt
	${Boost_LIBRARIES}
	Threads::Threads
	OpenSSL::SSL
)

add_executable(dtss_crypt_disk src/dtss_crypt_disk.cpp)
target_link_libraries(dtss_crypt_disk
	${Boost_LIBRARIES}
	Threads::Threads
	OpenSSL::SSL
)

add_executable(crypt src/crypt.cpp)
target_link_libraries(crypt
	OpenSSL::SSL
)

add_executable(crypt_disk src/crypt_disk.cpp)
target_link_libraries(crypt_disk
	OpenSSL::SSL
)


find_package(OpenSSL REQUIRED)

add_executable(dtss_crypt dtss_crypt.cpp)
target_include_directories(dtss_crypt PUBLIC
	${Boost_INCLUDE_DIRS}
)
target_link_libraries(dtss_crypt
	${Boost_LIBRARIES}
	Threads::Threads
	OpenSSL::SSL
)

add_executable(dtss_crypt_disk dtss_crypt_disk.cpp)
target_include_directories(dtss_crypt PUBLIC
	${Boost_INCLUDE_DIRS}
)
target_link_libraries(dtss_crypt_disk
	${Boost_LIBRARIES}
	Threads::Threads
	OpenSSL::SSL
)

add_executable(crypt crypt.cpp)
target_include_directories(crypt PUBLIC
	${Boost_INCLUDE_DIRS}
)
target_link_libraries(crypt
	OpenSSL::SSL
)

add_executable(crypt_disk crypt_disk.cpp)
target_include_directories(crypt_disk PUBLIC
	${Boost_INCLUDE_DIRS}
)
target_link_libraries(crypt_disk
	OpenSSL::SSL
)

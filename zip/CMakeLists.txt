find_package(ZLIB)

add_executable(dtss_zip dtss_zip.cpp)
target_include_directories(dtss_zip PUBLIC
	${Boost_INCLUDE_DIRS}
)
target_link_libraries(dtss_zip
	${Boost_LIBRARIES}
	Threads::Threads
	ZLIB::ZLIB
)

add_executable(dtss_zip_disk dtss_zip_disk.cpp)
target_include_directories(dtss_zip PUBLIC
	${Boost_INCLUDE_DIRS}
)
target_link_libraries(dtss_zip_disk
	${Boost_LIBRARIES}
	Threads::Threads
	ZLIB::ZLIB
)

add_executable(zip zip.cpp)
target_include_directories(zip PUBLIC
	${Boost_INCLUDE_DIRS}
)
target_link_libraries(zip
	ZLIB::ZLIB
)

add_executable(zip_disk zip_disk.cpp)
target_include_directories(zip_disk PUBLIC
	${Boost_INCLUDE_DIRS}
)
target_link_libraries(zip_disk
	ZLIB::ZLIB
)

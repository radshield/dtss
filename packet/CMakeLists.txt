find_package(re2 REQUIRED)

add_executable(dtss_packet dtss_packet.cpp)
target_include_directories(dtss_packet PUBLIC
	${Boost_INCLUDE_DIRS}
)
target_link_libraries(dtss_packet
	${Boost_LIBRARIES}
	Threads::Threads
	re2::re2
)

add_executable(packet packet.cpp)
target_include_directories(packet PUBLIC
	${Boost_INCLUDE_DIRS}
)
target_link_libraries(packet
	re2::re2
)

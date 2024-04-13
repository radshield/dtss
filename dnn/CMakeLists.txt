add_executable(dtss_dnn dtss_dnn.cpp)
target_include_directories(dtss_dnn PUBLIC
	${Boost_INCLUDE_DIRS}
)
target_link_libraries(dtss_dnn
	${Boost_LIBRARIES}
	Threads::Threads
)

add_executable(dnn dnn.cpp)
target_include_directories(dnn PUBLIC
	${Boost_INCLUDE_DIRS}
)


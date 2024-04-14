add_executable(dtss_dnn dtss_dnn.cpp)
target_include_directories(dtss_dnn PUBLIC
	${Boost_INCLUDE_DIRS}
)
target_link_libraries(dtss_dnn
	${Boost_LIBRARIES}
	Threads::Threads
)
target_compile_options(dtss_dnn PUBLIC
	-ffast-math
	-march=native
)

add_executable(dnn dnn.cpp)
target_include_directories(dnn PUBLIC
	${Boost_INCLUDE_DIRS}
)
target_compile_options(dnn PUBLIC
	-ffast-math
	-march=native
)

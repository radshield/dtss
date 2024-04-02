find_package(OpenCV REQUIRED)

add_executable(dtss_img dtss_img.cpp)
target_include_directories(dtss_img PUBLIC
	${Boost_INCLUDE_DIRS}
	${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(dtss_img
	${Boost_LIBRARIES}
	${OpenCV_LIBS}
	Threads::Threads
)

add_executable(img img.cpp)
target_include_directories(img PUBLIC
	${Boost_INCLUDE_DIRS}
	${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(img
	${OpenCV_LIBS}
)

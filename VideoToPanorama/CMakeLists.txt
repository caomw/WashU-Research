cmake_minimum_required(VERSION 2.8)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Ofast -msse2")
project( VideoToPanorama )
find_package( OpenCV REQUIRED )
add_executable( VideoToPanorama VideoToPanorama.cpp )
target_link_libraries( VideoToPanorama ${OpenCV_LIBS} )
cmake_minimum_required(VERSION 3.1)
project(Eye_Tracking)
find_package(OpenCV REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
include_directories(${OpenCV_INCLUDE_DIRS})

set(SOURCE_FILES main.cpp)
add_executable(Eye_Tracking ${SOURCE_FILES} statistics.h statistics.cpp)

target_link_libraries(Eye_Tracking ${OpenCV_LIBS})
cmake_minimum_required(VERSION 3.1)
project(Stats)
find_package(OpenCV REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
include_directories(${OpenCV_INCLUDE_DIRS})

#set(SOURCE_FILES main.cpp)
add_executable(Stats statistics.h statistics.cpp)

target_link_libraries(Stats ${OpenCV_LIBS})
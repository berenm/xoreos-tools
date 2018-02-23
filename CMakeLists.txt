cmake_minimum_required(VERSION 2.11.0)

project(ungranny)

add_definitions(-std=c++1y)
add_executable(gr22grd src/decompress.cpp src/gr22grd.cpp)
add_executable(gr22grf src/decompress.cpp src/gr22grf.cpp)
add_executable(gr22yml src/decompress.cpp src/gr22yaml.cpp)

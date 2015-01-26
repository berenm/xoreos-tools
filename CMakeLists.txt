cmake_minimum_required(VERSION 2.11.0)

project(ungranny)

add_definitions(-std=c++1y)
add_executable(gr22grd src/decompress.cpp src/gr22grd.cpp)
add_executable(gr22grf src/decompress.cpp src/gr22grf.cpp)
add_executable(gr22yml src/decompress.cpp src/gr22yaml.cpp)

enable_testing()
add_executable(test-decompress-01 src/decompress.cpp test/test-decompress-01.cpp)
add_test(test-decompress-01 ./test-decompress-01)
add_executable(test-decompress-02 src/decompress.cpp test/test-decompress-02.cpp)
add_test(test-decompress-02 ./test-decompress-02)
add_executable(test-decompress-03 src/decompress.cpp test/test-decompress-03.cpp)
add_test(test-decompress-03 ./test-decompress-03)

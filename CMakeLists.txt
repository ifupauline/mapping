cmake_minimum_required(VERSION 3.5)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include_directories(include)

project(mapping)

add_subdirectory(src)
add_subdirectory(exp/pa)
add_subdirectory(exp/map)
add_subdirectory(exp/exp1)
add_subdirectory(exp/exp2)

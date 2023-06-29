cmake_minimum_required(VERSION 3.5)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include_directories(include)

project(bpp-ifc)

add_subdirectory(src)
add_subdirectory(exp/dse)
add_subdirectory(tests/wcrt)
add_subdirectory(tests/ex_case_study)
add_subdirectory(tests/ex_sigcomp)

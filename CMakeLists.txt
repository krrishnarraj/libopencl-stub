cmake_minimum_required(VERSION 2.6)

project(OpenCL)

add_library(${PROJECT_NAME} STATIC src/libopencl.c)
target_include_directories(${PROJECT_NAME} PRIVATE "include")

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
   CMAKE_CXX_COMPILER_ID STREQUAL "GNU"   OR
   CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  target_compile_options(${PROJECT_NAME} PRIVATE -O2 -fPIC -Wall)
endif()


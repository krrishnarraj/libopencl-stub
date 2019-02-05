cmake_minimum_required(VERSION 2.6)

add_library(OpenCL STATIC src/libopencl.c)
target_include_directories(OpenCL PRIVATE "include")

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
   CMAKE_CXX_COMPILER_ID STREQUAL "GNU"   OR
   CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  target_compile_options(OpenCL PRIVATE -O2 -fPIC -Wall)
endif()


cmake_minimum_required (VERSION 3.6)
enable_language (C)
project (libOpenCL-stub)

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -fPIC -Wall")
set (LIBRARY_NAME OpenCL)

set(LIBSRC src/libopencl.c)

include_directories (${CMAKE_SOURCE_DIR}/include)
add_library (${LIBRARY_NAME} STATIC ${LIBSRC})
if (WIN32)
target_link_libraries(${LIBRARY_NAME} ${OPENCL_LIBRARIES})
else()
target_link_libraries(${LIBRARY_NAME} dl ${OPENCL_LIBRARIES})
endif()
set_target_properties(${LIBRARY_NAME} PROPERTIES 
    LIBRARY_OUTPUT_DIRECTORY lib
    ARCHIVE_OUTPUT_DIRECTORY lib
    OUTPUT_NAME ${LIBRARY_NAME}
    )

install(TARGETS ${LIBRARY_NAME}
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	RUNTIME DESTINATION bin)

file(GLOB HEADERS ./include/*.h ./include/CL/*.h)
install(FILES ${HEADERS} DESTINATION include)

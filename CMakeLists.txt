cmake_minimum_required (VERSION 3.6)
enable_language (C)
project (libOpenCL-stub)

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -fPIC -Wall")
set (CL_LIBRARY_NAME OpenCL)

set(LIBSRC src/libopencl.c)

include_directories (./include)
add_library (${CL_LIBRARY_NAME} STATIC ${LIBSRC})
if (WIN32)
target_link_libraries(${CL_LIBRARY_NAME})
else()
target_link_libraries(${CL_LIBRARY_NAME} dl)
endif()
set_target_properties(${CL_LIBRARY_NAME} PROPERTIES 
    LIBRARY_OUTPUT_DIRECTORY lib
    ARCHIVE_OUTPUT_DIRECTORY lib
    OUTPUT_NAME ${CL_LIBRARY_NAME}
    )

install(TARGETS ${CL_LIBRARY_NAME}
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	RUNTIME DESTINATION bin)

file(GLOB HEADERS ./include/*.h ./include/CL/*.h)
install(FILES ${HEADERS} DESTINATION include)

cmake_minimum_required(VERSION 3.13)

# Small static library providing global operator new/delete overrides
# with Tracy memory-tracing instrumentation.
add_library(Profiler STATIC
    Profiler.cpp
)

target_include_directories(Profiler PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
)

# Link Tracy if it exists
if(TARGET tracy)
	target_link_libraries(Profiler PUBLIC tracy)
endif()

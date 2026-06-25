set(CORE_PROFILER_SOURCES
    Profiler.cpp
)

add_dll(CoreProfiler ${CORE_PROFILER_SOURCES})
core_apply_target_defaults(CoreProfiler)

target_link_libraries(CoreProfiler PUBLIC
    CoreBase
)

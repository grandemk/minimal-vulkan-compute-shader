add_library(tracy SHARED 
    public/TracyClient.cpp
)

target_compile_definitions(tracy PRIVATE TRACY_EXPORTS)

target_include_directories(tracy PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/public
)
    

target_compile_definitions(tracy PUBLIC
    TRACY_ENABLE
    TRACY_DELAYED_INIT
    TRACY_MANUAL_LIFETIME
)

set_property(TARGET tracy PROPERTY EXTERNAL_LIBRARY TRUE)
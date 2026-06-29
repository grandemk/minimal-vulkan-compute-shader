cmake_minimum_required(VERSION 3.13)

project(
    minimal-vulkan-compute-shader
    VERSION 0.0.1
	DESCRIPTION "A minimal application with Vulkan Compute Shaders using DXC and HLSL"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-Wall -g")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# Find Vulkan
find_package(Vulkan)

# Find Tracy (optional) - check local tracy directory first
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tracy/CMakeLists.txt")
	add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/tracy" tracy_build)
	set(Tracy_FOUND TRUE)
	message(STATUS "Tracy profiler found in local directory and will be enabled")
	add_compile_definitions(TRACY_ENABLE TRACY_VK_CONTEXT TRACY_MANUAL_LIFETIME TRACY_DELAYED_INIT)
else()
	set(Tracy_FOUND FALSE)
	message(STATUS "Tracy profiler not found in local tracy directory")
endif()

#######################################################################
#                               SHADERS                               #
#######################################################################

# Find DXC (DirectX Shader Compiler)
find_program(dxc_executable NAMES dxc)

set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)

file(GLOB HLSL_SHADERS
  ${SHADER_SOURCE_DIR}/*.hlsl)

add_custom_command(
  COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_BINARY_DIR}
  OUTPUT ${SHADER_BINARY_DIR}
  COMMENT "Creating ${SHADER_BINARY_DIR}"
)

foreach(source IN LISTS HLSL_SHADERS)
  get_filename_component(FILENAME ${source} NAME_WE)
  
  # Set compile flags based on shader type
  set(SHADER_TYPE "")
  if(EXISTS "${SHADER_SOURCE_DIR}/${FILENAME}.comp")
    set(SHADER_TYPE "cs")
  else()
    set(SHADER_TYPE "cs")
  endif()
  
  # For compute shaders, use cs_6_0 profile
  set(SHADER_PROFILE "${SHADER_TYPE}_6_0")
  
  add_custom_command(
    COMMAND
      ${dxc_executable}
      -spirv                        # Output SPIR-V instead of DXIL
      -T ${SHADER_PROFILE}          # Shader profile (cs_6_0 for compute)
      -E main                       # Entry point name
      -fspv-target-env=vulkan1.1
      ${source}
      -Fo ${SHADER_BINARY_DIR}/${FILENAME}.hlsl.spv
    OUTPUT ${SHADER_BINARY_DIR}/${FILENAME}.hlsl.spv
    DEPENDS ${SHADER_BINARY_DIR} ${source}
    COMMENT "Compiling HLSL shader: ${FILENAME}.hlsl -> ${FILENAME}.hlsl.spv"
  )
  list(APPEND SPV_SHADERS ${SHADER_BINARY_DIR}/${FILENAME}.hlsl.spv)
endforeach()

add_custom_target(compile-shaders ALL DEPENDS ${SPV_SHADERS})

# Add Profiler subdirectory
add_subdirectory(Profiler)

#######################################################################
#                           MAIN EXECUTABLE                           #
#######################################################################

# Add the executable
add_executable(main src/main.cpp)

# Add the binary tree to the search path for include files
target_include_directories(main PUBLIC
	"${PROJECT_BINARY_DIR}"
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_SOURCE_DIR}/include"
	"${PROJECT_SOURCE_DIR}/Profiler"
	${Vulkan_INCLUDE_DIRS}
	)

target_link_libraries(main PUBLIC
	${Vulkan_LIBRARIES}
	Profiler
	$<$<BOOL:${Tracy_FOUND}>:tracy>
	)

# Make sure shaders are compiled before the main target
add_dependencies(main compile-shaders)

# Copy compiled shaders to the source directory for runtime loading
add_custom_command(
  TARGET compile-shaders
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${SHADER_BINARY_DIR}
  ${SHADER_SOURCE_DIR}
  COMMENT "Copying compiled shaders to source directory"
)

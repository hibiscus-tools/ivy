cmake_minimum_required(VERSION 3.20)

project(ivy CXX CUDA)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} --expt-relaxed-constexpr")
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -Xcudafe \"--diag_suppress=20012 --diag_suppress=20011 --diag_suppress=3189 --diag_suppress=20014\"")

find_package(Vulkan REQUIRED)
find_package(glslang REQUIRED)
find_package(OpenMP REQUIRED)

include_directories(include
	dependencies
	dependencies/imgui
	dependencies/glm)

add_library(ivy-interface SHARED
	dependencies/imgui/imgui.cpp
	dependencies/imgui/imgui_draw.cpp
	dependencies/imgui/imgui_widgets.cpp
	dependencies/imgui/imgui_demo.cpp
	dependencies/imgui/imgui_tables.cpp
	dependencies/imgui/backends/imgui_impl_glfw.cpp
	dependencies/imgui/backends/imgui_impl_vulkan.cpp)

add_library(ivy-core SHARED
	source/biome.cpp source/cursor_dispatcher.cpp
	source/exec/globals.cpp source/exec/user_interface.cpp
	source/exec/viewport.cpp source/prebuilt.cpp source/sdf.cpp
	source/shlighting.cpp source/core/camera.cpp source/core/contexts.cpp
	source/core/caches.cpp source/core/mesh.cpp source/core/polygon.cpp
	source/core/texture.cpp source/core/transform.cpp)

# TODO: target object

add_executable(din source/targets/din.cpp)
add_executable(din_cuda source/targets/din_cuda.cu)
add_executable(features source/targets/features.cpp)
add_executable(sdf source/targets/sdf.cpp)

add_compile_options(-Wall)

add_definitions(-DIVY_ROOT=\"${CMAKE_SOURCE_DIR}\" -DVULKAN_HPP_NO_SPACESHIP_OPERATOR)

set(IVY_LIBRARIES ivy-core ivy-interface fmt assimp glfw SPIRV glslang::glslang
	glslang::glslang-default-resource-limits Vulkan::Vulkan)

target_link_libraries(din ${IVY_LIBRARIES})
target_link_libraries(din_cuda ${IVY_LIBRARIES})
target_link_libraries(features ${IVY_LIBRARIES})
target_link_libraries(sdf ${IVY_LIBRARIES})

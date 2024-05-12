cmake_minimum_required(VERSION 3.20)

project(ivy CXX)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)
find_package(glslang REQUIRED)

include_directories(include
	dependencies
	dependencies/imgui
	dependencies/glm)

set(IMGUI_SOURCES
	dependencies/imgui/imgui.cpp
	dependencies/imgui/imgui_draw.cpp
	dependencies/imgui/imgui_widgets.cpp
	dependencies/imgui/imgui_demo.cpp
	dependencies/imgui/imgui_tables.cpp
	dependencies/imgui/backends/imgui_impl_glfw.cpp
	dependencies/imgui/backends/imgui_impl_vulkan.cpp)

set(IVY_SOURCES source/biome.cpp source/cursor_dispatcher.cpp
	source/exec/globals.cpp source/exec/user_interface.cpp
	source/exec/viewport.cpp source/prebuilt.cpp source/sdf.cpp
	source/shlighting.cpp source/core/camera.cpp source/core/contexts.cpp
	source/core/caches.cpp source/core/mesh.cpp source/core/polygon.cpp
	source/core/texture.cpp source/core/transform.cpp)

# TODO: target object

add_executable(features ${IVY_SOURCES} ${IMGUI_SOURCES} source/targets/features.cpp)
add_executable(sdf ${IVY_SOURCES} ${IMGUI_SOURCES} source/targets/sdf.cpp)

add_compile_options(-Wall)

add_definitions("-DIVY_ROOT=\"${CMAKE_SOURCE_DIR}\"")

set(IVY_LIBRARIES fmt assimp glfw SPIRV glslang::glslang
	glslang::glslang-default-resource-limits Vulkan::Vulkan)

target_link_libraries(features ${IVY_LIBRARIES})
target_link_libraries(sdf ${IVY_LIBRARIES})

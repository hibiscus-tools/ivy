cmake_minimum_required(VERSION 3.20)

project(ivy CXX)

set(CMAKE_CXX_STANDARD 20)

# find_package(OpenMP REQUIRED)
find_package(Vulkan REQUIRED)
find_package(glslang REQUIRED)

include_directories(include
	dependencies
	dependencies/glm
	dependencies/microlog
	dependencies/imgui)

add_library(ivycore SHARED
	source/biome.cpp
	source/caches.cpp
	source/camera.cpp
	source/contexts.cpp
	source/texture.cpp
	source/mesh.cpp
	source/polygon.cpp
	source/transform.cpp)

add_library(uicore SHARED
	dependencies/imgui/imgui.cpp
	dependencies/imgui/imgui_draw.cpp
	dependencies/imgui/imgui_widgets.cpp
	dependencies/imgui/imgui_demo.cpp
	dependencies/imgui/imgui_tables.cpp
	dependencies/imgui/backends/imgui_impl_glfw.cpp
	dependencies/imgui/backends/imgui_impl_vulkan.cpp)

add_executable(ivy source/main.cpp)

add_definitions("-DIVY_ROOT=\"${CMAKE_SOURCE_DIR}\"")

set(LIBRARIES
	assimp
	glfw
	SPIRV
	glslang::glslang
	glslang::glslang-default-resource-limits
	Vulkan::Vulkan)

target_link_libraries(ivy ${LIBRARIES} ivycore uicore)

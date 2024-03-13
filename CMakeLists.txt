cmake_minimum_required(VERSION 3.20)

project(ivy CXX)

set(CMAKE_CXX_STANDARD 20)

# find_package(OpenMP REQUIRED)
find_package(Vulkan REQUIRED)
find_package(glslang REQUIRED)

add_subdirectory(dependencies/oak)

include_directories(include
	dependencies
	dependencies/oak/include
	dependencies/glm
	dependencies/microlog
	dependencies/imgui)

add_executable(ivy source/main.cpp source/biome.cpp)

add_definitions("-DIVY_ROOT=\"${CMAKE_SOURCE_DIR}\"")

target_link_libraries(ivy
    oak
	assimp
	glfw
	SPIRV
	glslang::glslang
	glslang::glslang-default-resource-limits
	Vulkan::Vulkan)

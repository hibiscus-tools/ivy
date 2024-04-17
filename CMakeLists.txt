cmake_minimum_required(VERSION 3.20)

project(ivy CXX)

set(CMAKE_CXX_STANDARD 20)

# find_package(OpenMP REQUIRED)
find_package(Vulkan REQUIRED)
find_package(glslang REQUIRED)

add_subdirectory(dependencies/oak)

include_directories(include
	dependencies
	dependencies/glm
	dependencies/microlog
	dependencies/oak/dependencies
	dependencies/oak/dependencies/imgui
	dependencies/oak/include)

add_executable(features
	source/targets/features.cpp
	source/biome.cpp
	source/prebuilt.cpp
	source/shlighting.cpp)

target_compile_options(features PRIVATE -Wall)

add_definitions("-DIVY_ROOT=\"${CMAKE_SOURCE_DIR}\"")

target_link_libraries(features
	oak
	assimp
	glfw
	SPIRV
	glslang::glslang
	glslang::glslang-default-resource-limits
	Vulkan::Vulkan)

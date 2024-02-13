cmake_minimum_required(VERSION 3.20)

project(ivy CXX)

set(CXX_STANDARD 20)

# find_package(OpenMP REQUIRED)
find_package(Vulkan REQUIRED)
find_package(glslang REQUIRED)

include_directories(include dependencies dependencies/glm dependencies/microlog)

add_library(ivycore SHARED
	source/camera.cpp
	source/contexts.cpp
	source/mesh.cpp
	source/transform.cpp)

add_executable(ivy source/main.cpp)

add_definitions("-DIVY_ROOT=\"${CMAKE_SOURCE_DIR}\"")

set(LIBRARIES
	assimp
	glfw
	SPIRV
	glslang::glslang
	glslang::glslang-default-resource-limits
	Vulkan::Vulkan)

target_link_libraries(ivy ${LIBRARIES} ivycore)

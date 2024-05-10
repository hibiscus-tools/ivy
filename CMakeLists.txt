cmake_minimum_required(VERSION 3.20)

project(ivy CXX)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)
find_package(glslang REQUIRED)

add_subdirectory(dependencies/oak)

include_directories(include dependencies dependencies/glm
	dependencies/oak/dependencies dependencies/oak/dependencies/imgui
	dependencies/oak/include)

set(IVY_SOURCES source/biome.cpp source/cursor_dispatcher.cpp
	source/exec/globals.cpp source/exec/user_interface.cpp
	source/exec/viewport.cpp source/prebuilt.cpp source/sdf.cpp
	source/shlighting.cpp)

# TODO: target object

add_executable(features ${IVY_SOURCES} source/targets/features.cpp)
add_executable(sdf ${IVY_SOURCES} source/targets/sdf.cpp)

add_compile_options(-Wall)

add_definitions("-DIVY_ROOT=\"${CMAKE_SOURCE_DIR}\"")

set(IVY_LIBRARIES oak fmt assimp glfw SPIRV glslang::glslang
	glslang::glslang-default-resource-limits Vulkan::Vulkan)

target_link_libraries(features ${IVY_LIBRARIES})
target_link_libraries(sdf ${IVY_LIBRARIES})

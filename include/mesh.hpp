#pragma once

#include <filesystem>
#include <vector>

#include <glm/glm.hpp>

struct Mesh {
	// Vertex attributes
	std::vector <glm::vec3> positions;
	std::vector <glm::vec3> normals;
	std::vector <glm::vec2> uvs;

	// Topology
	std::vector <glm::uvec3> triangles;

	// Loading meshes
	static std::vector <Mesh> load(const std::filesystem::path &);
};

std::vector <float> interleave_attributes(const Mesh &);
#pragma once

#include <vector>

#include <glm/glm.hpp>

struct Polygon {
	// Vertex attributes
	std::vector <glm::vec2> positions;
	std::vector <glm::vec2> uvs;

	// Topology
	std::vector <glm::uvec3> triangles;

	// Static polygon builders
	static Polygon screen();
};

std::vector <float> interleave_attributes(const Polygon &);
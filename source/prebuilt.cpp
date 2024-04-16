#include "prebuilt.hpp"

namespace ivy {

Mesh box(const glm::vec3 &center, const glm::vec3 &size)
{
	float x = size.x;
	float y = size.y;
	float z = size.z;

	// All 24 vertices, with correct normals
	std::vector <glm::vec3> vertices {
		{ center.x - x, center.y - y, center.z + z },
		{ center.x + x, center.y - y, center.z + z },
		{ center.x + x, center.y + y, center.z + z },
		{ center.x - x, center.y + y, center.z + z },
		{ center.x - x, center.y - y, center.z - z },
		{ center.x + x, center.y - y, center.z - z },
		{ center.x + x, center.y + y, center.z - z },
		{ center.x - x, center.y + y, center.z - z },
		{ center.x - x, center.y - y, center.z + z },
		{ center.x - x, center.y - y, center.z - z },
		{ center.x - x, center.y + y, center.z - z },
		{ center.x - x, center.y + y, center.z + z },
		{ center.x + x, center.y - y, center.z + z },
		{ center.x + x, center.y - y, center.z - z },
		{ center.x + x, center.y + y, center.z - z },
		{ center.x + x, center.y + y, center.z + z },
		{ center.x - x, center.y + y, center.z + z },
		{ center.x + x, center.y + y, center.z + z },
		{ center.x + x, center.y + y, center.z - z },
		{ center.x - x, center.y + y, center.z - z },
		{ center.x - x, center.y - y, center.z + z },
		{ center.x + x, center.y - y, center.z + z },
		{ center.x + x, center.y - y, center.z - z },
		{ center.x - x, center.y - y, center.z - z },
	};

	std::vector <glm::vec3> normals {
		{ 0.0f, 0.0f, 1.0f },
		{ 0.0f, 0.0f, 1.0f },
		{ 0.0f, 0.0f, 1.0f },
		{ 0.0f, 0.0f, 1.0f },
		{ 0.0f, 0.0f, -1.0f },
		{ 0.0f, 0.0f, -1.0f },
		{ 0.0f, 0.0f, -1.0f },
		{ 0.0f, 0.0f, -1.0f },
		{ -1.0f, 0.0f, 0.0f },
		{ -1.0f, 0.0f, 0.0f },
		{ -1.0f, 0.0f, 0.0f },
		{ -1.0f, 0.0f, 0.0f },
		{ 1.0f, 0.0f, 0.0f },
		{ 1.0f, 0.0f, 0.0f },
		{ 1.0f, 0.0f, 0.0f },
		{ 1.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ 0.0f, -1.0f, 0.0f },
		{ 0.0f, -1.0f, 0.0f },
		{ 0.0f, -1.0f, 0.0f },
		{ 0.0f, -1.0f, 0.0f },
	};

	std::vector <glm::vec2> uvs {
		{ 0.0f, 0.0f },
		{ 1.0f, 0.0f },
		{ 1.0f, 1.0f },
		{ 0.0f, 1.0f },
		{ 0.0f, 0.0f },
		{ 1.0f, 0.0f },
		{ 1.0f, 1.0f },
		{ 0.0f, 1.0f },
		{ 0.0f, 0.0f },
		{ 1.0f, 0.0f },
		{ 1.0f, 1.0f },
		{ 0.0f, 1.0f },
		{ 0.0f, 0.0f },
		{ 1.0f, 0.0f },
		{ 1.0f, 1.0f },
		{ 0.0f, 1.0f },
		{ 0.0f, 0.0f },
		{ 1.0f, 0.0f },
		{ 1.0f, 1.0f },
		{ 0.0f, 1.0f },
		{ 0.0f, 0.0f },
		{ 1.0f, 0.0f },
		{ 1.0f, 1.0f },
		{ 0.0f, 1.0f }
	};

	// All 36 indices
	std::vector <glm::uvec3> triangles {
		{ 0, 1, 2 },	{ 2, 3, 0 },	// Front
		{ 4, 6, 5 },	{ 6, 4, 7 },	// Back
		{ 8, 10, 9 },	{ 10, 8, 11 },	// Left
		{ 12, 13, 14 },	{ 14, 15, 12 },	// Right
		{ 16, 17, 18 },	{ 18, 19, 16 },	// Top
		{ 20, 22, 21 },	{ 22, 20, 23 }	// Bottom
	};

	return { vertices, normals, uvs, triangles };
}

Mesh sphere(const glm::vec3 &center, float radius)
{

}

}
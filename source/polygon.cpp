#include "polygon.hpp"

// Mesh functions
std::vector <float> interleave_attributes(const Polygon &p)
{
	// TODO: size check beforehand
	std::vector <float> attributes;
	for (size_t i = 0; i < p.positions.size(); i++) {
		attributes.push_back(p.positions[i].x);
		attributes.push_back(p.positions[i].y);

		attributes.push_back(p.uvs[i].x);
		attributes.push_back(p.uvs[i].y);
	}

	return attributes;
}

// Static mesh builders
Polygon Polygon::screen()
{
	return Polygon {
		.positions = {
			{ -1, -1 },
			{ -1,  1 },
			{  1,  1 },
			{  1, -1 }
		},

		.uvs = {
			{ 0, 0 },
			{ 0, 1 },
			{ 1, 1 },
			{ 1, 0 }
		},

		.triangles = {
			{ 0, 1, 2 },
			{ 0, 2, 3 },
		}
	};
}
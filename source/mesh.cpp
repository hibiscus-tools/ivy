#include "microlog.h"

#include "mesh.hpp"
#include "material.hpp"

// Mesh functions
std::vector <float> interleave_attributes(const Mesh &m)
{
	// TODO: size check beforehand
	std::vector <float> attributes;
	for (size_t i = 0; i < m.positions.size(); i++) {
		attributes.push_back(m.positions[i].x);
		attributes.push_back(m.positions[i].y);
		attributes.push_back(m.positions[i].z);

		attributes.push_back(m.normals[i].x);
		attributes.push_back(m.normals[i].y);
		attributes.push_back(m.normals[i].z);
		
		attributes.push_back(m.uvs[i].x);
		attributes.push_back(m.uvs[i].y);
	}

	return attributes;
}
#pragma once

#include <oak/transform.hpp>
#include <oak/mesh.hpp>
#include <oak/material.hpp>

// TODO: namesapace ivy

// Transform; already defined from oak

// Geometry; any surface to be rendered
struct Geometry {
	Mesh mesh;
	Material material;
	bool visible; // TODO: base struct for renderables
};


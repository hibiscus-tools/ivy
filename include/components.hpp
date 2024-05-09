#pragma once

#include <oak/transform.hpp>
#include <oak/mesh.hpp>
#include <oak/material.hpp>

#include "collider.hpp"

namespace ivy {

// Transform; already defined from oak
using Transform = Transform;

// Geometry; any surface to be rendered
struct Geometry {
	Mesh mesh;
	Material material;
	bool visible; // TODO: base struct for renderables
};

// Collider; collision shape for physics
using Collider = physics::Collider;

}

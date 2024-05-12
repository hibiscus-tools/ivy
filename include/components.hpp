#pragma once

#include "collider.hpp"
#include "core/transform.hpp"
#include "core/mesh.hpp"
#include "core/material.hpp"

namespace ivy {

// Transform; already defined from core
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

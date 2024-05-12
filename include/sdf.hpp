#pragma once

#include <variant>

#include <glm/glm.hpp>

#include "core/transform.hpp"

namespace ivy::sdf {

// Primitives
struct Sphere {
	glm::vec3 center;
	float radius;
};

struct Box {
	glm::vec3 min;
	glm::vec3 max;
};

// Compound shapes or scenes
using Shape = std::variant <Sphere, Box>;

// NOTE: serializing transform buffers is a separate task
struct Compound {
	std::vector <Shape> shapes;

	// TODO: alias
	std::vector <glm::vec4> serialize() const;
};

}

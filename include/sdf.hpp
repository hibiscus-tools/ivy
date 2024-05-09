#pragma once

#include <glm/glm.hpp>

namespace ivy {

struct Sphere {
	glm::vec3 center;
	float radius;
};

struct Box {
	glm::vec3 min;
	glm::vec3 max;
};

}

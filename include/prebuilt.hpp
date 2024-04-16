#pragma once

#include <glm/glm.hpp>

#include <oak/mesh.hpp>

namespace ivy {

// Mesh prebuilts
Mesh box(const glm::vec3 &, const glm::vec3 &);
Mesh sphere(const glm::vec3 &, float);

}
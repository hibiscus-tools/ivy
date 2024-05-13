#pragma once

#include <glm/glm.hpp>

#include "core/texture.hpp"

namespace ivy {

struct SHLighting {
	glm::mat4 red;
	glm::mat4 green;
	glm::mat4 blue;

	static SHLighting from(const Texture &);
};

}
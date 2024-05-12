#pragma once

#include <glm/glm.hpp>

// Uber material structure
struct Material {
	std::string identifier;

	glm::vec3 diffuse;
	glm::vec3 specular;
	float roughness;

	struct {
		std::string diffuse;
		std::string specular;
		std::string normal;
	} textures;

	static Material null() {
		return Material {
			"null",
			{ 1, 0, 1 }, // classic purple color
			{ 0, 0, 0 },
			1, { "", "", "" }
		};
	}
};
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
};
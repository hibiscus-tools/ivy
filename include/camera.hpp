#pragma once

#include "transform.hpp"

#define GLSL_ALIGN alignas(sizeof(glm::vec4))

struct RayFrame {
	GLSL_ALIGN glm::vec3 origin;
	GLSL_ALIGN glm::vec3 lower_left;
	GLSL_ALIGN glm::vec3 horizontal;
	GLSL_ALIGN glm::vec3 vertical;
};

struct Camera {
	float aspect = 1.0f;
	float fov = 45.0f;
	float near = 0.1f;
	float far = 10000.0f;

	// TODO: static
	void from(float, float = 45.0f, float = 0.1f, float = 1000.0f);

	glm::mat4 perspective_matrix() const;

	RayFrame rayframe(const Transform &) const;

	static glm::mat4 view_matrix(const Transform &);
};

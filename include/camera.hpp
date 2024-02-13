#pragma once

#include "transform.hpp"

struct Camera {
	float aspect = 1.0f;
	float fov = 45.0f;
	float near = 0.1f;
	float far = 1000.0f;

	void from(float, float = 45.0f, float = 0.1f, float = 1000.0f);
	glm::mat4 perspective_matrix() const;
	static glm::mat4 view_matrix(const Transform &);
};

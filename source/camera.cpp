#include <glm/gtc/matrix_transform.hpp>

#include "camera.hpp"

// Camera
void Camera::from(float aspect_, float fov_, float near_, float far_)
{
	aspect = aspect_;
	fov = fov_;
	near = near_;
	far = far_;
}

glm::mat4 Camera::perspective_matrix() const
{
	return glm::perspective(
		glm::radians(fov),
		aspect, near, far
	);
}

glm::mat4 Camera::view_matrix(const Transform &transform)
{
	auto [right, up, forward] = transform.axes();
	return glm::lookAt(
		transform.position,
		transform.position + forward,
		up
	);
}


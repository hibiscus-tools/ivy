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

RayFrame Camera::rayframe(const Transform &transform) const
{
	auto [right, up, forward] = transform.axes();

	// Convert FOV to radians
        float vfov = glm::radians(fov);

        float h = std::tan(vfov / 2);

        float vheight = 2 * h;
        float vwidth = vheight * aspect;

        glm::vec3 w = glm::normalize(-forward);
        glm::vec3 u = glm::normalize(cross(up, w));
        glm::vec3 v = glm::cross(w, u);
        
	glm::vec3 horizontal = u * vwidth;
        glm::vec3 vertical = v * vheight;

	return RayFrame {
		.origin = transform.position,
		.lower_left = transform.position - horizontal/2.0f - vertical/2.0f - w,
		.horizontal = horizontal,
		.vertical = vertical
	};
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
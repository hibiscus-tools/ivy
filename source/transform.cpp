#include <glm/gtc/quaternion.hpp>

#include "transform.hpp"

// Transform
void Transform::from(const glm::vec3 &position_, const glm::vec3 &rotation_, const glm::vec3 &scale_)
{
	position = position_;
	rotation = rotation_;
	scale = scale_;
}

glm::mat4 Transform::matrix() const
{
	glm::mat4 pmat = glm::translate(glm::mat4(1.0f), position);
	glm::mat4 rmat = glm::mat4_cast(glm::quat(glm::radians(rotation)));
	glm::mat4 smat = glm::scale(glm::mat4(1.0f), scale);
	return pmat * rmat * smat;
}

glm::vec3 Transform::right() const
{
	glm::quat q = glm::quat(rotation);
	return glm::normalize(glm::vec3(q * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)));
}

glm::vec3 Transform::up() const
{
	glm::quat q = glm::quat(rotation);
	return glm::normalize(glm::vec3(q * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f)));
}

glm::vec3 Transform::forward() const
{
	glm::quat q = glm::quat(rotation);
	return glm::normalize(glm::vec3(q * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)));
}

std::tuple <glm::vec3, glm::vec3, glm::vec3> Transform::axes() const
{
	glm::quat q = glm::quat(rotation);
	return std::make_tuple(
		glm::normalize(glm::vec3(q * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f))),
		glm::normalize(glm::vec3(q * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f))),
		glm::normalize(glm::vec3(q * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)))
	);
}

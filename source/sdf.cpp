#include "sdf.hpp"

namespace ivy::sdf {

std::vector <glm::vec4> Compound::serialize() const
{
	std::vector <glm::vec4> result;

	struct visitor {
		std::vector <glm::vec4> &buffer;

		void operator()(const Sphere &sphere) {
			buffer.emplace_back(sphere.center, 0.0f);
			buffer.emplace_back(sphere.radius);
		}

		void operator()(const Box &box) {
			buffer.emplace_back(box.min, 0.0f);
			buffer.emplace_back(box.max, 0.0f);
		}
	};

	result.emplace_back(shapes.size());
	for (const Shape &s : shapes)
		std::visit(visitor { result }, s);

	return result;
}

}

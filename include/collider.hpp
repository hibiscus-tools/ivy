#pragma once

#include <functional>
#include <variant>

#include <oak/transform.hpp>

#include "sdf.hpp"

namespace ivy::physics {

using ColliderShape = std::variant <Sphere, Box>;

struct Collider {
	std::reference_wrapper <Transform> transform;

	ColliderShape shape;

	bool gravity;
	bool enabled;

	// TODO: attach method for components which require it
	template <typename Shape>
	Collider(Transform &transform_, const Shape &shape_,
		bool gravity_ = true, bool enabled_ = true)
			: transform(transform_), shape(shape_), gravity(gravity_), enabled(enabled_) {}
};

}

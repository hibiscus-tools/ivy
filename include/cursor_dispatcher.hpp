#pragma once

#include <memory>
#include <unordered_map>

#include <littlevk/littlevk.hpp>

#include <glm/glm.hpp>

namespace ivy {

struct CursorDispatcher {
	struct MouseInfo {
		bool drag = false;
		bool voided = true;
		float last_x = 0.0f;
		float last_y = 0.0f;
		float drag_x = 0.0f;
		float drag_y = 0.0f;
		float delta_x = 0.0f;
		float delta_y = 0.0f;
	} mouse;

	std::unordered_map <const glm::vec4 *, std::function <void (const MouseInfo &)>> handlers;

	static std::unique_ptr <CursorDispatcher> from(GLFWwindow *);
};

}
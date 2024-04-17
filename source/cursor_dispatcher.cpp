#include <imgui/imgui.h>

#include "cursor_dispatcher.hpp"

namespace ivy {

static void cursor_callback(GLFWwindow *window, double x, double y) {
	static auto in_region =[](const glm::vec4 &r, double x, double y) {
		return (x >= r.x && x <= r.z) && (y >= r.y && y <= r.w);
	};

	// Perform the regular computations
	auto dispatcher = (CursorDispatcher *) glfwGetWindowUserPointer(window);

	CursorDispatcher::MouseInfo &mouse = dispatcher->mouse;

	if (mouse.voided) {
		mouse.last_x = x;
		mouse.last_y = y;
		mouse.voided = false;
	}

	mouse.delta_x = x - mouse.last_x;
	mouse.delta_y = y - mouse.last_y;
	mouse.last_x = x;
	mouse.last_y = y;

	// Go over all regional handlers
	bool taken = false;
	for (const auto &[region, handler] : dispatcher->handlers) {
		if (mouse.drag && !in_region(*region, mouse.drag_x, mouse.drag_y))
			continue;

		if (in_region(*region, x, y)) {
			handler(mouse);
			taken = true;
		}
	}

	// If no regional handlers are contesting the cursor, give it to ImGui
	if (!taken)
		ImGui::GetIO().MousePos = ImVec2(x, y);
}

static void button_callback(GLFWwindow *window, int button, int action, int mods) {
	auto dispatcher = (CursorDispatcher *) glfwGetWindowUserPointer(window);

	CursorDispatcher::MouseInfo &mouse = dispatcher->mouse;

	// Ignore if on ImGui window
	ImGuiIO &io = ImGui::GetIO();
	io.AddMouseButtonEvent(button, action);

	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		mouse.drag = (action == GLFW_PRESS);
		if (action == GLFW_RELEASE)
			mouse.voided = true;

		if (action == GLFW_PRESS) {
			double x;
			double y;
			glfwGetCursorPos(window, &x, &y);
			mouse.drag_x = x;
			mouse.drag_y = y;
		}
	}
}

std::unique_ptr <CursorDispatcher> CursorDispatcher::from(GLFWwindow *window)
{
	auto dispatcher = std::make_unique <CursorDispatcher> ();
	glfwSetWindowUserPointer(window, dispatcher.get());
	glfwSetMouseButtonCallback(window, button_callback);
	glfwSetCursorPosCallback(window, cursor_callback);
	return dispatcher;
}

}
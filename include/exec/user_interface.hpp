#pragma once

#include <imgui/imgui.h>

#include "cursor_dispatcher.hpp"
#include "globals.hpp"
#include "viewport.hpp"

namespace ivy::exec {

// User interface layer based relying on the engine state
struct UserInterface {
	// Must have a valid engine state
	Globals &engine;

	// Render pass, and the corresponding framebuffers
	struct {
		vk::RenderPass render_pass;
		std::vector <vk::Framebuffer> framebuffers;
		vk::Extent2D extent;
	} vk;

	// Fonts
	ImFont *primary_font;

	// GLFW input handlers
	std::unique_ptr <CursorDispatcher> cursor_dispatcher_ref;

	// If the viewport is active, it is here
	std::unique_ptr <Viewport> viewport_ref;

	// Methods
	void resize(const vk::Extent2D &);
	void draw(const vk::CommandBuffer &, const littlevk::SurfaceOperation &);

	// Construction
	static UserInterface from(Globals &engine);
};

}

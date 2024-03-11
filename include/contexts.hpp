#pragma once

#include <littlevk/littlevk.hpp>

// For one device/window
struct DeviceResourceContext : littlevk::Skeleton {
	vk::PhysicalDevice phdev;
	vk::PhysicalDeviceMemoryProperties memory_properties;

	littlevk::Deallocator *dal = nullptr;

	vk::CommandPool command_pool;
	vk::DescriptorPool descriptor_pool;

	std::vector <vk::CommandBuffer> command_buffers;

	littlevk::PresentSyncronization sync;

	std::optional <std::pair <vk::CommandBuffer, littlevk::SurfaceOperation>> new_frame(size_t);
	void end_frame(const vk::CommandBuffer &, size_t) const;
	void present_frame(const littlevk::SurfaceOperation &, size_t);
	bool valid_window() const;

	static DeviceResourceContext from(const vk::PhysicalDevice &, const std::vector <const char *> &, const vk::PhysicalDeviceFeatures2KHR &);
};

// For one render pass
struct RenderContext {
	vk::RenderPass render_pass;
	std::vector <vk::Framebuffer> framebuffers;

	static RenderContext from(const DeviceResourceContext &);
};

// Pair these together to begin a render pass
struct LiveRenderContext {
	const DeviceResourceContext &drc;
	const RenderContext &rc;

	LiveRenderContext(const DeviceResourceContext &drc, const RenderContext &rc)
			: drc(drc), rc(rc) {}

	void begin_render_pass(const vk::CommandBuffer &, const littlevk::SurfaceOperation &);
};

// For an ImGui context
void imgui_context_from(const DeviceResourceContext &, const RenderContext &);

void imgui_begin();
void imgui_end(const vk::CommandBuffer &);
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

	static DeviceResourceContext from(const vk::PhysicalDevice &, const std::vector <const char *> &, const vk::PhysicalDeviceFeatures2KHR &);
};

std::optional <std::pair <vk::CommandBuffer, littlevk::SurfaceOperation>> new_frame(DeviceResourceContext &, size_t);
void end_frame(const DeviceResourceContext &, const vk::CommandBuffer &, size_t);
void present_frame(DeviceResourceContext &, const littlevk::SurfaceOperation &, size_t);
bool valid_window(const DeviceResourceContext &);

// For one render pass
struct RenderContext {
	vk::RenderPass render_pass;
	std::vector <vk::Framebuffer> framebuffers;

	static RenderContext from(DeviceResourceContext &);
};

void begin_render_pass(const DeviceResourceContext &, const RenderContext &, const vk::CommandBuffer &, const littlevk::SurfaceOperation &);

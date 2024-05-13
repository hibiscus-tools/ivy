#pragma once

#include <littlevk/littlevk.hpp>

// For one device/window
struct VulkanResourceBase : littlevk::Skeleton {
	vk::PhysicalDevice phdev;
	vk::PhysicalDeviceMemoryProperties memory_properties;

	littlevk::Deallocator *dal = nullptr;

	vk::CommandPool command_pool;
	vk::DescriptorPool descriptor_pool;

	std::vector <vk::CommandBuffer> command_buffers;

	littlevk::PresentSyncronization sync;

	std::optional <std::pair <vk::CommandBuffer, littlevk::SurfaceOperation>> new_frame(size_t);
	void end_frame(const vk::CommandBuffer &, size_t) const;
	littlevk::SurfaceOperation present_frame(const littlevk::SurfaceOperation &, size_t);
	bool valid_window() const;

	bool destroy() override {
		delete dal;
		return littlevk::Skeleton::destroy();
	}

	static VulkanResourceBase from(const vk::PhysicalDevice &, const std::vector <const char *> &, const vk::PhysicalDeviceFeatures2KHR &);
};

// For an ImGui context
void imgui_context_from(const VulkanResourceBase &, const vk::RenderPass &);

void imgui_begin();
void imgui_end(const vk::CommandBuffer &);

// TODO: move prepare to here...

#pragma once

#include "texture.hpp"
#include "contexts.hpp"

namespace ivy {

struct DeviceTextureCache {
	vk::Device device;
	vk::CommandPool command_pool;
	vk::Queue queue;
	vk::PhysicalDeviceMemoryProperties memory_properties;
	
	littlevk::Deallocator *dal;

	std::unordered_map <std::string, Texture> host_textures;
	std::unordered_map <std::string, littlevk::Image> device_textures;

	void load(const std::filesystem::path &path);
	void upload(const std::filesystem::path &path);

	static DeviceTextureCache from(const VulkanResourceBase &drc) {
		DeviceTextureCache dtc {
			.device = drc.device,
			.command_pool = drc.command_pool,
			.queue = drc.graphics_queue,
			.memory_properties = drc.memory_properties,
			.dal = drc.dal
		};

		// Populate with a default blank texture
		// TODO: should this be a checkboard instead?
		dtc.host_textures["blank"] = Texture::blank();
		dtc.upload("blank");

		return dtc;
	}
};

}
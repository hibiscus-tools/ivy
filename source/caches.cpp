#include <littlevk/littlevk.hpp>
#include <microlog/microlog.h>

#include "caches.hpp"

void DeviceTextureCache::load(const std::filesystem::path &path)
{
	std::string tr = path.string();
	if (host_textures.count(tr))
		return;

	Texture tex = Texture::load(tr);
	host_textures[tr] = tex;
}

void DeviceTextureCache::upload(const std::filesystem::path &path)
{
	std::string tr = path.string();
	ulog_assert(host_textures.count(tr), "load_texture", "could not find path %s\n", tr.c_str());

	const Texture &tex = host_textures[tr];

	littlevk::Image image = littlevk::image(device, {
		(uint32_t) tex.width, (uint32_t) tex.height,
		vk::Format::eR8G8B8A8Unorm, // TODO: conditional
		vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
		vk::ImageAspectFlagBits::eColor
	}, memory_properties).unwrap(dal);

	// Upload the image data
	littlevk::Buffer staging = littlevk::buffer(
		device,
		4 * tex.width * tex.height,
		vk::BufferUsageFlagBits::eTransferSrc,
		memory_properties
	).unwrap(dal);

	littlevk::upload(device, staging, tex.pixels);

	// TODO: some state wise struct to simplify transitioning?
	littlevk::submit_now(device, command_pool, queue,
		[&](const vk::CommandBuffer &cmd) {
			littlevk::transition(cmd, image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
			littlevk::copy_buffer_to_image(cmd, image, staging, vk::ImageLayout::eTransferDstOptimal);
			littlevk::transition(cmd, image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
		}
	);

	// Free interim data
	littlevk::destroy_buffer(device, staging);

	device_textures[tr] = image;
}
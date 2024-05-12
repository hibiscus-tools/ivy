#include <littlevk/littlevk.hpp>

#include "core/caches.hpp"

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

	if (!host_textures.count(tr)) {
		fprintf(stderr, "DeviceTextureCache::upload: could not find path %s\n", tr.c_str());
		return;
	}

	// TODO: callback
	// ulog_assert(host_textures.count(tr), "load_texture", "could not find path %s\n", tr.c_str());

	const Texture &tex = host_textures[tr];

	littlevk::Image image;
	littlevk::Buffer staging;

	 std::tie(image, staging) = bind(device, memory_properties, dal)
		.image((uint32_t) tex.width, (uint32_t) tex.height,
			vk::Format::eR8G8B8A8Unorm, // TODO: conditional
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
			vk::ImageAspectFlagBits::eColor)
		.buffer(tex.pixels, vk::BufferUsageFlagBits::eTransferSrc);

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

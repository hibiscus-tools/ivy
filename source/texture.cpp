#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <microlog/microlog.h>

#include "texture.hpp"

Texture Texture::load(const std::filesystem::path &path)
{
	std::string tr = path.string();

	ulog_assert(std::filesystem::exists(path), "Texture::load", "could not find path %s\n", tr.c_str());

	// TODO: common image loading utility
	int width;
	int height;
	int channels;

	stbi_set_flip_vertically_on_load(true);

	uint8_t *pixels = stbi_load(tr.c_str(), &width, &height, &channels, 4);
	ulog_info("Texture::load", "loaded image (%s) with dimensions (%d, %d, %d)\n", tr.c_str(), width, height, channels);

	// TODO: use the right number of channels...
	std::vector <uint8_t> vector;
	// vector.resize(width * height * channels);
	vector.resize(width * height * 4);
	memcpy(vector.data(), pixels, vector.size() * sizeof(uint8_t));

	return Texture {
		.width = width,
		.height = height,
		.channels = channels,
		.pixels = vector
	};
}

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include "core/texture.hpp"

namespace ivy {

void Texture::save(const std::filesystem::path &path) const
{
	stbi_flip_vertically_on_write(true);
	stbi_write_png(path.c_str(), width, height, 4, (uint8_t *) pixels.data(), width * sizeof(uint32_t));
}

Texture Texture::load(const std::filesystem::path &path)
{
	std::string tr = path.string();
	if (!std::filesystem::exists(path)) {
		fprintf(stderr, "Texture::load: could not find path : %s\n", tr.c_str());
		return {};
	}

	int width;
	int height;
	int channels;

	stbi_set_flip_vertically_on_load(true);

	uint8_t *pixels = stbi_load(tr.c_str(), &width, &height, &channels, 4);

	// TODO: use the right number of channels...
	std::vector <uint8_t> vector;
	vector.resize(width * height * 4);
	memcpy(vector.data(), pixels, vector.size() * sizeof(uint8_t));
	free(pixels);

	return Texture {
		.width = width,
		.height = height,
		.channels = channels,
		.pixels = vector
	};
}

Texture Texture::blank()
{
	return Texture {
		.width = 1,
		.height = 1,
		.channels = 4,
		.pixels = { 0, 0, 0, 0 }
	};
}

}
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "core/texture.hpp"

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

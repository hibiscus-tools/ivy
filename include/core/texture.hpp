#pragma once

#include <vector>
#include <filesystem>

#include <glm/glm.hpp>

namespace ivy {

// Host texture
struct Texture {
	int width;
	int height;
	int channels;
	std::vector <uint8_t> pixels;

	std::vector <glm::vec3> as_rgb() const {
		std::vector <glm::vec3> rgb;
		rgb.resize(width * height);

		for (int i = 0; i < width; i++) {
			for (int j = 0; j < height; j++) {
				uint32_t index = (i * height + j);
				uint32_t pixel = *(uint32_t *) &pixels[index << 2];
				float r = ((pixel >> 16) & 0xff)/255.0f;
				float g = ((pixel >> 8) & 0xff)/255.0f;
				float b = (pixel & 0xff)/255.0f;
				rgb[index] = { r, g, b };
			}
		}

		return rgb;
	}

	void save(const std::filesystem::path &) const;

	static Texture load(const std::filesystem::path &);
	static Texture blank();
};

}
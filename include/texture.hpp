#pragma once

#include <vector>
#include <filesystem>

// Host texture
struct Texture {
	int width;
	int height;
	int channels;
	std::vector <uint8_t> pixels;

	static Texture load(const std::filesystem::path &);
};

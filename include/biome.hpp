#pragma once

#include <filesystem>

#include "mesh.hpp"
#include "material.hpp"

// A biome is a collection of geometry and materials
struct Biome {
	std::vector <Mesh> geometry;
	std::vector <Material> materials;

	static Biome load(const std::filesystem::path &);
};
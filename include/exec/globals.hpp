#pragma once

#include <oak/contexts.hpp>

#include <optional>

#include "biome.hpp"

namespace ivy::exec {

// Global state of the engine
struct Globals {
	// Fundamental Vulkan resources
	VulkanResourceBase vrb;

	// Currently active biome
	std::optional <std::reference_wrapper <Biome>> biome;

	// Methods
	Biome &active_biome();

	// Construction
	static Globals from();
};

VulkanResourceBase prepare_vulkan_resource_base();

}

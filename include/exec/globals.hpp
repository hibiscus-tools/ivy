#pragma once

#include <optional>

#include "biome.hpp"
#include "core/contexts.hpp"

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

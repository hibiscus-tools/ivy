#pragma once

#include <glm/glm.hpp>

#include <littlevk/littlevk.hpp>

#include "core/contexts.hpp"
#include "core/material.hpp"

// Vulkan ports of rendering structures
struct VulkanGeometry {
	littlevk::Buffer vertices;
	littlevk::Buffer triangles;
	size_t count = 0;

	template <typename G>
	static VulkanGeometry from(const VulkanResourceBase &, const G &);
};

template <typename G>
VulkanGeometry VulkanGeometry::from(const VulkanResourceBase &drc, const G &g)
{
	VulkanGeometry vm;
	vm.count = 3 * g.triangles.size();
	std::tie(vm.vertices, vm.triangles) = bind(drc.device, drc.memory_properties, drc.dal)
		.buffer(interleave_attributes(g), vk::BufferUsageFlagBits::eVertexBuffer)
		.buffer(g.triangles, vk::BufferUsageFlagBits::eIndexBuffer);
	return vm;
}

struct VulkanMaterial {
	alignas(16) glm::vec3 albedo;
	alignas(16) glm::vec3 specular;
	int has_albedo_texture;

	static VulkanMaterial from(const Material &material) {
		return VulkanMaterial {
			material.diffuse,
			material.specular,
			!material.textures.diffuse.empty()
		};
	}
};

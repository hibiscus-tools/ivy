#pragma once

#include <oak/transform.hpp>
#include <oak/camera.hpp>
#include <oak/caches.hpp>

#include "biome.hpp"
#include "vkport.hpp"
#include "cursor_dispatcher.hpp"

namespace ivy::exec {

// Viewport for the current biome
struct Viewport {
	// Only active while there is a valid Biome
	const Biome &biome;

	// Vulkan resource base
	const VulkanResourceBase &vrb;

	// Render pass and framebuffer data
	struct {
		vk::RenderPass render_pass;

		// TODO: one or multiple fbs?
		littlevk::Image depth;
		std::vector <littlevk::Image> images;
		std::vector <vk::Framebuffer> framebuffers;
		vk::Extent2D extent;
	} vk;

	// Pipelines
	struct {
		littlevk::Pipeline raster;
		littlevk::Pipeline sdf;
		littlevk::Pipeline environment;
	} pipelines;

	// Scrap data
	struct {
		littlevk::Buffer shl;
		VulkanGeometry screen;

		vk::DescriptorSet sdf_descriptor;
		vk::DescriptorSet environment_descriptor;
	} scrap;

	// ImGui handles
	std::vector <vk::DescriptorSet> imgui_descriptors;

	// Cache for textures for the biome
	DeviceTextureCache dtc;

	// Caches
	struct {
		std::unordered_map <uint32_t, VulkanGeometry> geometry;
		std::unordered_map <uint32_t, vk::DescriptorSet> descriptors;
	} caches;

	// Viewport camera configuration
	Camera camera;
	Transform camera_transform;

	// Region in user interface (min x, min y, max x, max y)
	glm::vec4 region;

	// Default sampler
	vk::Sampler sampler;

	struct AwaitResourceFree {
		int left;
		size_t frame;
		std::variant <littlevk::Image, vk::DescriptorSet> resource;
	};

	// TODO: display size as internal statistics
	std::list <AwaitResourceFree> await_free_queue;

	// Cursor handler
	void cursor_handler(const CursorDispatcher::MouseInfo &);

	// Resizing
	void resize(const vk::Extent2D &);
	void export_framebuffers_to_imgui();

	// Preparing resources
	void prepare();
	void prepare_render_pass();
	void prepare_raster_pipeline();
	void prepare_sdf_pipeline();
	void prepare_environment_pipeline();

	// Caching functions
	void cache_geometry_properties(ComponentRef <Geometry> &);

	// Rendering functions
	void render(const vk::CommandBuffer &, const littlevk::SurfaceOperation &);

	// Construction
	static std::unique_ptr <Viewport> from(Biome &, const VulkanResourceBase &,
			std::unique_ptr <CursorDispatcher> &, const vk::Extent2D &);
};

}

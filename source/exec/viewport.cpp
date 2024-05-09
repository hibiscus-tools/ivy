#define LITTLEVK_GLM_TRANSLATOR

#include <imgui/backends/imgui_impl_vulkan.h>

#include <glm/gtc/quaternion.hpp>

#include <oak/polygon.hpp>

#include <microlog/microlog.h>

#include "exec/viewport.hpp"
#include "paths.hpp"
#include "shlighting.hpp"

// Import names
using enum vk::DescriptorType;
using enum vk::ShaderStageFlagBits;
using enum vk::ImageLayout;

using standalone::readfile;

// Push constants
struct MVPConstants {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
	alignas(16) glm::vec3 camera;
};

struct RayFrameExtra : RayFrame {
	float near;
	float far;
};

// Pipeline configurations
static constexpr auto rendering_dslbs = std::array <vk::DescriptorSetLayoutBinding, 3> {{
	{ 0, eCombinedImageSampler, 1, eFragment },
	{ 1, eUniformBuffer, 1, eFragment },
	{ 2, eUniformBuffer, 1, eFragment }
}};

static constexpr auto sdf_dslbs = std::array <vk::DescriptorSetLayoutBinding, 1> {{
	{ 0, eInputAttachment, 1, eFragment },
}};

static constexpr auto environment_dslbs = std::array <vk::DescriptorSetLayoutBinding, 2> {{
	{ 0, eInputAttachment, 1, eFragment },
	{ 1, eCombinedImageSampler, 1, eFragment }
}};

namespace ivy::exec {

// TODO: mouse/key io structure
static void handle_key_input(GLFWwindow *const win, Transform &camera_transform)
{
	static float last_time = 0.0f;

	constexpr float speed = 500.0f;

	float delta = speed * float(glfwGetTime() - last_time);
	last_time = glfwGetTime();

	glm::vec3 velocity(0.0f);
	if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS)
		velocity.z -= delta;
	else if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS)
		velocity.z += delta;

	if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS)
		velocity.x -= delta;
	else if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS)
		velocity.x += delta;

	if (glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS)
		velocity.y += delta;
	else if (glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS)
		velocity.y -= delta;

	glm::quat q = glm::quat(camera_transform.rotation);
	velocity = q * glm::vec4(velocity, 0.0f);
	camera_transform.position += velocity;
}

// Cursor handler
void Viewport::cursor_handler(const CursorDispatcher::MouseInfo &mouse)
{
	float xoffset = 0.001f * mouse.delta_x;
	float yoffset = 0.001f * mouse.delta_y;

	if (mouse.drag) {
		camera_transform.rotation.x += yoffset;
		camera_transform.rotation.y -= xoffset;
		camera_transform.rotation.x = glm::clamp(camera_transform.rotation.x, -89.0f, 89.0f);
	}
}

// Export framebuffer images for ImGui
void Viewport::export_framebuffers_to_imgui()
{
	// Generate the descriptor sets
	for (size_t i = 0; i < imgui_descriptors.size(); i++)
		await_free_queue.push_back({ .left = 1, .frame = i, .resource = imgui_descriptors[i] });

	imgui_descriptors.clear();
	for (const littlevk::Image &image : vk.images) {
		vk::DescriptorSet dset = ImGui_ImplVulkan_AddTexture
			(static_cast <VkSampler> (sampler),
			static_cast <VkImageView> (image.view),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		imgui_descriptors.push_back(dset);
	}
}

// Resize the framebuffers (including from a null state)
void Viewport::resize(const vk::Extent2D &extent)
{
	// Skip if the extent is the same
	if (extent == vk.extent)
		return;

	// Translate the extent
	vk.extent = extent;

	// Free old resources
	for (size_t i = 0; i < vk.images.size(); i++)
		await_free_queue.push_back({ .left = 1, .frame = i, .resource = vk.images[i] });

	if (vk.depth.device_size())
		await_free_queue.push_back({ .left = 2, .frame = 0, .resource = vk.depth });

	// Allocate the images
	vk.images.clear();
	for (size_t i = 0; i < vrb.swapchain.images.size(); i++) {
		vk.images.push_back(littlevk::bind(vrb.device, vrb.memory_properties, vrb.dal)
			.image(extent.width, extent.height,
				vrb.swapchain.format,
				vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment,
				vk::ImageAspectFlagBits::eColor));
	}

	// Transition right away
	// TODO: bind
	littlevk::submit_now(vrb.device, vrb.command_pool, vrb.graphics_queue,
		[&](const vk::CommandBuffer &cmd) {
			for (const littlevk::Image &image : vk.images) {
				littlevk::transition(cmd, image,
					vk::ImageLayout::eUndefined,
					vk::ImageLayout::eShaderReadOnlyOptimal);
			}
		}
	);

	// Create the depth buffer
	vk.depth = bind(vrb.device, vrb.memory_properties, vrb.dal)
		.image(extent,
			vk::Format::eD32Sfloat,
			vk::ImageUsageFlagBits::eDepthStencilAttachment
				| vk::ImageUsageFlagBits::eInputAttachment,
			vk::ImageAspectFlagBits::eDepth);

	// Create the framebuffers
	littlevk::FramebufferGenerator generator(vrb.device, vk.render_pass, extent, vrb.dal);
	for (const littlevk::Image &image : vk.images)
		generator.add(image.view, vk.depth.view);

	vk.framebuffers = generator.unpack();

	// Bind the depth buffer wherever necessary
	littlevk::bind(vrb.device, scrap.sdf_descriptor, sdf_dslbs)
		.update(0, 0, sampler, vk.depth.view, eGeneral)
		.finalize();

	littlevk::bind(vrb.device, scrap.environment_descriptor, environment_dslbs)
		.update(0, 0, sampler, vk.depth.view, eDepthReadOnlyOptimal)
		.finalize();

	// Export to ImGui
	export_framebuffers_to_imgui();
}

// Prepare internal resources
void Viewport::prepare()
{
	sampler = littlevk::SamplerAssembler(vrb.device, vrb.dal);

	prepare_render_pass();
	prepare_raster_pipeline();
	prepare_sdf_pipeline();
	prepare_environment_pipeline();

	// Load the environment map
	// TODO: load a blue skybox
	const std::string environment = IVY_ROOT "/data/environments/crossroads.hdr";

	// Environment subpass resources
	// TODO: put into that function...
	vk::DescriptorSet environment_dset = littlevk::bind(vrb.device, vrb.descriptor_pool)
		.allocate_descriptor_sets(*pipelines.environment.dsl).front();

	dtc.load(environment);
	dtc.upload(environment);
	const littlevk::Image &environment_map = dtc.device_textures[environment];

	littlevk::bind(vrb.device, environment_dset, environment_dslbs)
		.update(1, 0, sampler, environment_map.view, eShaderReadOnlyOptimal)
		.finalize();

	scrap.environment_descriptor = environment_dset;
}

// Prepare the render pass
void Viewport::prepare_render_pass()
{
	vk.render_pass = littlevk::RenderPassAssembler(vrb.device, vrb.dal)
		.add_attachment(littlevk::default_color_attachment(vrb.swapchain.format))
		.add_attachment(littlevk::default_depth_attachment())
		// (A) Primary rasterization
		.add_subpass(vk::PipelineBindPoint::eGraphics)
			.color_attachment(0, eColorAttachmentOptimal)
			.depth_attachment(1, eDepthStencilAttachmentOptimal)
			.done()
		// (B) Raymarching signed distance fields
		.add_subpass(vk::PipelineBindPoint::eGraphics)
			.input_attachment(1, eGeneral) // TODO:: read and write
			.color_attachment(0, eColorAttachmentOptimal)
			.depth_attachment(1, eGeneral)
			.done()
		// (C) Environment mapping
		.add_subpass(vk::PipelineBindPoint::eGraphics)
			.input_attachment(1, eDepthReadOnlyOptimal)
			.color_attachment(0, eColorAttachmentOptimal)
			.done()
		// (C) -> (B) -> (A)
		.add_dependency(0, 1,
			vk::PipelineStageFlagBits::eFragmentShader,
			vk::PipelineStageFlagBits::eFragmentShader)
		.add_dependency(1, 2,
			vk::PipelineStageFlagBits::eFragmentShader,
			vk::PipelineStageFlagBits::eFragmentShader);
}

// Preparing the pipelines
void Viewport::prepare_raster_pipeline()
{
	// Upload the lighting information to the device
	SHLighting shl {
		.red = glm::mat4(0.9f),
		.green = glm::mat4(0.9f),
		.blue = glm::mat4(1.0f),
	};

	scrap.shl = bind(vrb.device, vrb.memory_properties, vrb.dal)
		.buffer(&shl, sizeof(shl), vk::BufferUsageFlagBits::eUniformBuffer);

	// Pipeline
	constexpr auto raster_layout = littlevk::VertexLayout <glm::vec3, glm::vec3, glm::vec2> ();

	auto raster_bundle = littlevk::ShaderStageBundle(vrb.device, vrb.dal)
		.attach(readfile(IVY_SHADERS "/mesh.vert"), eVertex)
		.attach(readfile(IVY_SHADERS "/environment.frag"), eFragment);

	pipelines.raster = littlevk::PipelineAssembler(vrb.device, vrb.window, vrb.dal)
		.with_render_pass(vk.render_pass, 0)
		.with_vertex_layout(raster_layout)
		.with_shader_bundle(raster_bundle)
		.with_dsl_bindings(rendering_dslbs)
		.with_push_constant <MVPConstants> (eVertex);
}

void Viewport::prepare_sdf_pipeline()
{
	// Pipeline
	constexpr auto vlayout = littlevk::VertexLayout <glm::vec2, glm::vec2> ();

	auto bundle = littlevk::ShaderStageBundle(vrb.device, vrb.dal)
		.attach(readfile(IVY_SHADERS "/screen.vert"), eVertex)
		.attach(readfile(IVY_SHADERS "/sdf.frag"), eFragment);

	pipelines.sdf = littlevk::PipelineAssembler(vrb.device, vrb.window, vrb.dal)
		.with_render_pass(vk.render_pass, 1)
		.with_vertex_layout(vlayout)
		.with_shader_bundle(bundle)
		.alpha_blending(true)
		.with_dsl_bindings(sdf_dslbs)
		.with_push_constant <RayFrameExtra> (eFragment);

	// Allocate the corresponding descriptor set
	scrap.sdf_descriptor = littlevk::bind(vrb.device, vrb.descriptor_pool)
		.allocate_descriptor_sets(*pipelines.sdf.dsl).front();
}

void Viewport::prepare_environment_pipeline()
{
	// Allocate the geometry for the screen now itself
	// TODO: generate inside the v shader itself
	auto screen = Polygon::screen();
	scrap.screen = VulkanGeometry::from(vrb, screen);

	// Pipeline
	constexpr auto vlayout = littlevk::VertexLayout <glm::vec2, glm::vec2> ();

	auto bundle = littlevk::ShaderStageBundle(vrb.device, vrb.dal)
		.attach(readfile(IVY_SHADERS "/screen.vert"), eVertex)
		.attach(readfile(IVY_SHADERS "/post.frag"), eFragment);

	pipelines.environment = littlevk::PipelineAssembler(vrb.device, vrb.window, vrb.dal)
		.with_render_pass(vk.render_pass, 2)
		.with_vertex_layout(vlayout)
		.with_shader_bundle(bundle)
		.with_dsl_bindings(environment_dslbs)
		.with_push_constant <RayFrameExtra> (eFragment);
}

void Viewport::cache_geometry_properties(ComponentRef <Geometry> &g)
{
	uint32_t i = g.hash();
	// ulog_info(__FUNCTION__, "Caching geometry with hash: %d\n", i);

	g->mesh = deduplicate(g->mesh);
	g->mesh.normals = smooth_normals(g->mesh);
	caches.geometry[i] = VulkanGeometry::from(vrb, g->mesh);

	vk::DescriptorSet dset = littlevk::bind(vrb.device, vrb.descriptor_pool)
		.allocate_descriptor_sets(*pipelines.raster.dsl).front();

	// TODO: stream/batchify
	const littlevk::Image &image = [&](const std::string &diffuse) {
		if (!diffuse.empty()) {
			dtc.load(diffuse);
			dtc.upload(diffuse);
			return dtc.device_textures[diffuse];
		}

		return dtc.device_textures["blank"];
	} (g->material.textures.diffuse);

	// Export the material as well
	auto vmat = VulkanMaterial::from(g->material);

	littlevk::Buffer material_buffer = littlevk::bind(vrb.device, vrb.memory_properties, vrb.dal)
		.buffer(&vmat, sizeof(vmat), vk::BufferUsageFlagBits::eUniformBuffer);

	littlevk::bind(vrb.device, dset, rendering_dslbs)
		.update(0, 0, sampler, image.view, eShaderReadOnlyOptimal)
		.update(1, 0, *scrap.shl, 0, sizeof(SHLighting))
		.update(2, 0, *material_buffer, 0, sizeof(VulkanMaterial))
		.finalize();

	caches.descriptors[i] = dset;
}

// TODO: keep an internal frame state?
void Viewport::render(const vk::CommandBuffer &cmd, const littlevk::SurfaceOperation &op)
{
	camera.aspect = float(vk.extent.width)/float(vk.extent.height);

	littlevk::viewport_and_scissor(cmd, vk.extent);

	// Key input
	handle_key_input(vrb.window->handle, camera_transform);

	// Begin the render pass
	const auto &rpbi = littlevk::default_rp_begin_info <2>
		(vk.render_pass, vk.framebuffers[op.index], vk.extent)
		.clear_color(0, std::array <float, 4> { 1.0f, 1.0f, 1.0f, 1.0f });

	cmd.beginRenderPass(rpbi, vk::SubpassContents::eInline);

	// Render all active geometry
	// TODO: methods
	{
		auto ppl = pipelines.raster;

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ppl.handle);

		MVPConstants mvp {};
		mvp.proj = camera.perspective_matrix();
		mvp.view = Camera::view_matrix(camera_transform);
		mvp.camera = camera_transform.position;

		for (auto [transform, g] : biome.grab_all <Transform, Geometry> ()) {
			if (caches.geometry.count(g.hash()) == 0)
				cache_geometry_properties(g);

			// TODO: check dirty flag
			uint32_t index = g.hash();
			const auto &vg = caches.geometry[index];
			const auto &dset = caches.descriptors[index];

			// TODO: if not in cache, skip for now and spawn a thread for it (requries a thread pool)

			mvp.model = transform->matrix();
//				mvp.model = glm::mat4(1.0f);

			cmd.pushConstants <MVPConstants> (ppl.layout, eVertex, 0, mvp);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, ppl.layout, 0, dset, {});
			cmd.bindVertexBuffers(0, { vg.vertices.buffer }, { 0 });
			cmd.bindIndexBuffer(vg.triangles.buffer, 0, vk::IndexType::eUint32);
			cmd.drawIndexed(vg.count, 1, 0, 0, 0);
		}
	}

	// Render the signed distance fields
	// TODO: separate rendering stages
	cmd.nextSubpass(vk::SubpassContents::eInline);

	{
		auto ppl = pipelines.sdf;

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ppl.handle);

		RayFrame rayframe = camera.rayframe(camera_transform);
		RayFrameExtra rayframe_extra;

		rayframe_extra.origin = rayframe.origin;
		rayframe_extra.lower_left = rayframe.lower_left;
		rayframe_extra.horizontal = rayframe.horizontal;
		rayframe_extra.vertical = rayframe.vertical;
		rayframe_extra.near = camera.near;
		rayframe_extra.far = camera.far;

		cmd.pushConstants <RayFrameExtra> (ppl.layout, eFragment, 0, rayframe_extra);

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, ppl.layout,
			0, scrap.sdf_descriptor, {});

		cmd.bindVertexBuffers(0, { scrap.screen.vertices.buffer }, { 0 });
		cmd.bindIndexBuffer(scrap.screen.triangles.buffer, 0, vk::IndexType::eUint32);
		cmd.drawIndexed(scrap.screen.count, 1, 0, 0, 0);
	}

	// Render the environment
	cmd.nextSubpass(vk::SubpassContents::eInline);

	{
		auto ppl = pipelines.environment;

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ppl.handle);

		// RayFrame rayframe = camera.rayframe(camera_transform);
		//
		// cmd.pushConstants <RayFrame> (ppl.layout, eFragment, 0, rayframe);

		RayFrame rayframe = camera.rayframe(camera_transform);
		RayFrameExtra rayframe_extra;

		rayframe_extra.origin = rayframe.origin;
		rayframe_extra.lower_left = rayframe.lower_left;
		rayframe_extra.horizontal = rayframe.horizontal;
		rayframe_extra.vertical = rayframe.vertical;
		rayframe_extra.near = camera.near;
		rayframe_extra.far = camera.far;

		cmd.pushConstants <RayFrameExtra> (ppl.layout, eFragment, 0, rayframe_extra);

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, ppl.layout,
			0, scrap.environment_descriptor, {});

		cmd.bindVertexBuffers(0, { scrap.screen.vertices.buffer }, { 0 });
		cmd.bindIndexBuffer(scrap.screen.triangles.buffer, 0, vk::IndexType::eUint32);
		cmd.drawIndexed(scrap.screen.count, 1, 0, 0, 0);
	}

	// End the current render pass
	cmd.endRenderPass();

	// Transition to a reasonable layout
	littlevk::transition(cmd, vk.images[op.index],
		vk::ImageLayout::ePresentSrcKHR,
		vk::ImageLayout::eShaderReadOnlyOptimal);

	// Handling the free queue
	await_free_queue.remove_if(
		[&](AwaitResourceFree &arf) -> bool {
			if (arf.frame == op.index) {
				arf.left--;
				if (arf.left < 0) {
					// TODO: visitor
					if (std::holds_alternative <littlevk::Image> (arf.resource)) {
						littlevk::destroy_image(vrb.device, std::get <littlevk::Image> (arf.resource));
						ulog_info("Viewport", "Destroyed leftover image\n");
					}

					if (std::holds_alternative <vk::DescriptorSet> (arf.resource)) {
						ImGui_ImplVulkan_RemoveTexture(std::get <vk::DescriptorSet> (arf.resource));
						ulog_info("Viewport", "Destroyed leftover descriptor set\n");
					}
				}
			}

			return arf.left < 0;
		}
	);
}

std::unique_ptr <Viewport> Viewport::from
(
	Biome &biome,
	const VulkanResourceBase &vrb,
	std::unique_ptr <CursorDispatcher> &cursor_dispatcher,
	const vk::Extent2D &extent
)
{
	// Allocate the viewport
	auto viewport = std::make_unique <Viewport> (Viewport {
		.biome = biome,
		.vrb = vrb,
		.dtc = DeviceTextureCache::from(vrb)
	});

	// Set it off to prepare itself
	viewport->prepare();
	viewport->resize(extent);

	// Register the viewport for cursor control
	Viewport *raw = viewport.get();
	cursor_dispatcher->handlers[&viewport->region] = [raw](const CursorDispatcher::MouseInfo &info) {
		return raw->cursor_handler(info);
	};

	// Return once completed
	return viewport;
}

}

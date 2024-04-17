#define LITTLEVK_GLM_TRANSLATOR

#include <vector>
#include <functional>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_vulkan.h>

#include <littlevk/littlevk.hpp>
#include <microlog/microlog.h>

#include <oak/caches.hpp>
#include <oak/camera.hpp>
#include <oak/contexts.hpp>
#include <oak/polygon.hpp>
#include <oak/transform.hpp>

#include "biome.hpp"
#include "prebuilt.hpp"
#include "shlighting.hpp"
#include "vkport.hpp"

#ifndef IVY_ROOT
#define IVY_ROOT ".."
#endif

#define IVY_SHADERS IVY_ROOT "/shaders"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

// Import names
using enum vk::DescriptorType;
using enum vk::ShaderStageFlagBits;
using enum vk::ImageLayout;

using standalone::readfile;

struct MVPConstants {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

void handle_key_input(GLFWwindow *const, Transform &);
void button_callback(GLFWwindow *, int, int, int);
void cursor_callback(GLFWwindow *, double, double);

struct CursorDispatcher {
	struct MouseInfo {
		bool drag = false;
		bool voided = true;
		float last_x = 0.0f;
		float last_y = 0.0f;
		float drag_x = 0.0f;
		float drag_y = 0.0f;
		float delta_x = 0.0f;
		float delta_y = 0.0f;
	} mouse;

	std::unordered_map <const glm::vec4 *, std::function <void (const MouseInfo &)>> handlers;

	static void cursor_callback(GLFWwindow *window, double x, double y) {
		static auto in_region =[](const glm::vec4 &r, double x, double y) {
			return (x >= r.x && x <= r.z) && (y >= r.y && y <= r.w);
		};

		// Perform the regular computations
		auto dispatcher = (CursorDispatcher *) glfwGetWindowUserPointer(window);

		MouseInfo &mouse = dispatcher->mouse;

		if (mouse.voided) {
			mouse.last_x = x;
			mouse.last_y = y;
			mouse.voided = false;
		}

		mouse.delta_x = x - mouse.last_x;
		mouse.delta_y = y - mouse.last_y;
		mouse.last_x = x;
		mouse.last_y = y;

		// Go over all regional handlers
		bool taken = false;
		for (const auto &[region, handler] : dispatcher->handlers) {
			if (mouse.drag && !in_region(*region, mouse.drag_x, mouse.drag_y))
				continue;

			if (in_region(*region, x, y)) {
				handler(mouse);
				taken = true;
			}
		}

		// If no regional handlers are contesting the cursor, give it to ImGui
		if (!taken)
			ImGui::GetIO().MousePos = ImVec2(x, y);
	}

	static void button_callback(GLFWwindow *window, int button, int action, int mods) {
		auto dispatcher = (CursorDispatcher *) glfwGetWindowUserPointer(window);

		MouseInfo &mouse = dispatcher->mouse;

		// Ignore if on ImGui window
		ImGuiIO &io = ImGui::GetIO();
		io.AddMouseButtonEvent(button, action);

		if (button == GLFW_MOUSE_BUTTON_LEFT) {
			mouse.drag = (action == GLFW_PRESS);
			if (action == GLFW_RELEASE)
				mouse.voided = true;

			if (action == GLFW_PRESS) {
				double x;
				double y;
				glfwGetCursorPos(window, &x, &y);
				mouse.drag_x = x;
				mouse.drag_y = y;
			}
		}
	}

	static std::unique_ptr <CursorDispatcher> from(GLFWwindow *window) {
		auto dispatcher = std::make_unique <CursorDispatcher> ();
		glfwSetWindowUserPointer(window, dispatcher.get());
		glfwSetMouseButtonCallback(window, CursorDispatcher::button_callback);
		glfwSetCursorPosCallback(window, CursorDispatcher::cursor_callback);
		return dispatcher;
	}
};

namespace ivy::exec {

// Global state of the engine
struct Globals {
	// Fundamental Vulkan resources
	VulkanResourceBase vrb;

	static VulkanResourceBase prepare_vulkan_resource_base() {
		// Device extensions
		static const std::vector <const char *> EXTENSIONS {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME
		};

		// Load physical device
		auto predicate = [](vk::PhysicalDevice phdev) {
			return littlevk::physical_device_able(phdev, EXTENSIONS);
		};

		vk::PhysicalDevice phdev = littlevk::pick_physical_device(predicate);

		// Enable features
		vk::PhysicalDeviceFeatures2KHR features {};

		vk::PhysicalDeviceFragmentShaderBarycentricFeaturesKHR barycentrics {};
		barycentrics.fragmentShaderBarycentric = vk::True;

		vk::PhysicalDeviceSeparateDepthStencilLayoutsFeaturesKHR separation {};
		separation.separateDepthStencilLayouts = vk::True;

		features.pNext = &barycentrics;
		barycentrics.pNext = &separation;

		phdev.getFeatures2(&features);

		// Create the resource base
		return VulkanResourceBase::from(phdev, EXTENSIONS, features);
	}

	// Currently active biome
	std::optional <std::reference_wrapper <Biome>> biome;

	Biome &active_biome() {
		return *biome;
	}

	static Globals from() {
		return { prepare_vulkan_resource_base(), std::nullopt };
	}
};

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
		littlevk::Pipeline environment;
	} pipelines;

	// Scrap data
	struct {
		littlevk::Buffer shl;
		VulkanGeometry screen;
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

	// Pipeline configurations
	static constexpr auto rendering_dslbs = std::array <vk::DescriptorSetLayoutBinding, 3> {{
		{ 0, eCombinedImageSampler, 1, eFragment },
		{ 1, eUniformBuffer, 1, eFragment },
		{ 2, eUniformBuffer, 1, eFragment }
	}};

	static constexpr auto environment_dslbs = std::array <vk::DescriptorSetLayoutBinding, 2> {{
		{ 0, eInputAttachment, 1, eFragment },
		{ 1, eCombinedImageSampler, 1, eFragment }
	}};

	static std::unique_ptr <Viewport> from(Biome &biome,
			const VulkanResourceBase &vrb,
			std::unique_ptr <CursorDispatcher> &cursor_dispatcher,
			const vk::Extent2D &extent) {
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

	// Cursor handler
	void cursor_handler(const CursorDispatcher::MouseInfo &mouse) {
		float xoffset = 0.001f * mouse.delta_x;
		float yoffset = 0.001f * mouse.delta_y;

		if (mouse.drag) {
			camera_transform.rotation.x += yoffset;
			camera_transform.rotation.y -= xoffset;
			camera_transform.rotation.x = glm::clamp(camera_transform.rotation.x, -89.0f, 89.0f);
		}
	}

	// Prepare the render pass
	void prepare_render_pass() {
		vk.render_pass = littlevk::RenderPassAssembler(vrb.device, vrb.dal)
			.add_attachment(littlevk::default_color_attachment(vrb.swapchain.format))
			.add_attachment(littlevk::default_depth_attachment())
			.add_subpass(vk::PipelineBindPoint::eGraphics)
				.color_attachment(0, eColorAttachmentOptimal)
				.depth_attachment(1, eDepthStencilAttachmentOptimal)
				.done()
			.add_subpass(vk::PipelineBindPoint::eGraphics)
				.input_attachment(1, eDepthReadOnlyOptimal)
				.color_attachment(0, eColorAttachmentOptimal)
				.done()
			.add_dependency(0, 1,
				vk::PipelineStageFlagBits::eFragmentShader,
				vk::PipelineStageFlagBits::eFragmentShader);
	}

	// Default sampler
	vk::Sampler sampler;

	struct AwaitResourceFree {
		int left;
		size_t frame;
		std::variant <littlevk::Image, vk::DescriptorSet> resource;
	};

	// TODO: display size as internal statistics
	std::list <AwaitResourceFree> await_free_queue;

	// Export framebuffer images for ImGui
	void export_framebuffers_to_imgui() {
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
	void resize(const vk::Extent2D &extent) {
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

		// Bind the depth buffer
		littlevk::bind(vrb.device, scrap.environment_descriptor, environment_dslbs)
			.update(0, 0, sampler, vk.depth.view, eDepthReadOnlyOptimal)
			.finalize();

		// Export to ImGui
		export_framebuffers_to_imgui();
	}

	// Preparing the pipelines
	void prepare_raster_pipeline() {
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

	void prepare_environment_pipeline() {
		// Allocate the geometry for the screen now itself
		auto screen = Polygon::screen();
		scrap.screen = VulkanGeometry::from(vrb, screen);

		// Pipeline
		constexpr auto environment_vlayout = littlevk::VertexLayout <glm::vec2, glm::vec2> ();

		auto environment_bundle = littlevk::ShaderStageBundle(vrb.device, vrb.dal)
			.attach(readfile(IVY_SHADERS "/screen.vert"), eVertex)
			.attach(readfile(IVY_SHADERS "/post.frag"), eFragment);

		pipelines.environment = littlevk::PipelineAssembler(vrb.device, vrb.window, vrb.dal)
			.with_render_pass(vk.render_pass, 1)
			.with_vertex_layout(environment_vlayout)
			.with_shader_bundle(environment_bundle)
			.with_dsl_bindings(environment_dslbs)
			.with_push_constant <RayFrame> (eFragment);
	}

	// Prepare internal resources
	void prepare() {
		sampler = littlevk::SamplerAssembler(vrb.device, vrb.dal);

		prepare_render_pass();
		prepare_raster_pipeline();
		prepare_environment_pipeline();

		// Load the environment map
		// TODO: load a blue skybox
		const std::string environment = IVY_ROOT "/data/environments/crossroads.hdr";

		// Environment subpass resources
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

	void cache_geometry_properties(ComponentRef <Geometry> &g) {
		uint32_t i = g.hash();
//		ulog_info(__FUNCTION__, "Caching geometry with hash: %d\n", i);

		g->mesh = deduplicate(g->mesh);
		g->mesh.normals = smooth_normals(g->mesh);
		caches.geometry[i] = VulkanGeometry::from(vrb, g->mesh);

		// TODO: gather all textures, then load them in parallel (thread pool)
		const auto &textures = g->material.textures;

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
	void render(const vk::CommandBuffer &cmd, const littlevk::SurfaceOperation &op) {
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

		// Render the environment
		cmd.nextSubpass(vk::SubpassContents::eInline);

		{
			auto ppl = pipelines.environment;

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ppl.handle);

			RayFrame rayframe = camera.rayframe(camera_transform);

			cmd.pushConstants <RayFrame> (ppl.layout, eFragment, 0, rayframe);

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
};

// User interface layer based relying on the engine state
struct UserInterface {
	// Must have a valid engine state
	Globals &engine;

	// Render pass, and the corresponding framebuffers
	struct {
		vk::RenderPass render_pass;
		std::vector <vk::Framebuffer> framebuffers;
		vk::Extent2D extent;
	} vk;

	// GLFW input handlers
	std::unique_ptr <CursorDispatcher> cursor_dispatcher_ref;

	// If the viewport is active, it is here
	std::unique_ptr <Viewport> viewport_ref;

	static UserInterface from(Globals &engine) {
		// Configure the render pass
		// TODO: prepare
		vk::RenderPass render_pass = littlevk::RenderPassAssembler(engine.vrb.device, engine.vrb.dal)
			.add_attachment(littlevk::default_color_attachment(engine.vrb.swapchain.format))
			.add_subpass(vk::PipelineBindPoint::eGraphics)
				.color_attachment(0, eColorAttachmentOptimal)
				.done();

		// Configure ImGui
		imgui_context_from(engine.vrb, render_pass);

		ImGuiIO &io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigWindowsMoveFromTitleBarOnly = true;

		// Handoff
		UserInterface ui {
			.engine = engine,
			.vk = {
				.render_pass = render_pass,
			},
			.cursor_dispatcher_ref = CursorDispatcher::from(engine.vrb.window->handle),
			.viewport_ref = nullptr,
		};

		ui.resize(engine.vrb.window->extent);
		return ui;
	}

	static void biome_tree(const Biome &biome) {
		// TODO: define method
		std::function <void (const Inhabitant &inh)> recursive_note = [&](const Inhabitant &inh) -> void {
			if (inh.children.empty())
				return ImGui::Text("%s", inh.identifier.c_str());

			if (ImGui::TreeNode(inh.identifier.c_str())) {
				for (const InhabitantRef &ic : inh.children)
					recursive_note(*ic);
				ImGui::TreePop();
			}
		};

		if (ImGui::Begin("Scene tree")) {
			for (const auto &inh : biome.inhabitants) {
				if (inh.parent)
					continue;
				recursive_note(inh);
			}

			ImGui::End();
		}
	}

	void resize(const vk::Extent2D &extent) {
		if (vk.extent == extent)
			return;

		// Transfer the extent
		vk.extent = extent;

		// Generate the framebuffers
		littlevk::FramebufferGenerator generator(engine.vrb.device, vk.render_pass, extent, engine.vrb.dal);
		for (const vk::ImageView &view : engine.vrb.swapchain.image_views)
			generator.add(view);

		vk.framebuffers = generator.unpack();
	}

	void draw(const vk::CommandBuffer &cmd, const littlevk::SurfaceOperation &op) {
		// TODO: manage own index?

		// Begin the render pass
		const auto &rpbi = littlevk::default_rp_begin_info <2>
			(vk.render_pass, vk.framebuffers[op.index], vk.extent)
			.clear_color(0, std::array <float, 4> { 1.0f, 1.0f, 1.0f, 1.0f });

		cmd.beginRenderPass(rpbi, vk::SubpassContents::eInline);

		// Render the user interface
		vk::Extent2D viewport_size;

		imgui_begin();

		ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

		// Top bar
		if (ImGui::BeginMainMenuBar()) {
			ImGui::MenuItem("File");
			ImGui::MenuItem("Project");
			ImGui::EndMainMenuBar();
		}

		if (engine.biome) {
			biome_tree(*engine.biome);

			if (!viewport_ref) {
				viewport_ref = Viewport::from(*engine.biome, engine.vrb,
					cursor_dispatcher_ref, vk::Extent2D { 1000, 1000 });
			}
		}

		// Add a view for the viewport and render it if available
		if (ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar)) {
			if (viewport_ref) {
				ImVec2 size = ImGui::GetContentRegionAvail();
				viewport_size.width = (uint32_t) size.x;
				viewport_size.height = (uint32_t) size.y;
				ImGui::Image(viewport_ref->imgui_descriptors[op.index], size);

				// Set bounds for correct cursor operation
				ImVec2 min = ImGui::GetItemRectMin();
				ImVec2 max = ImGui::GetItemRectMax();
				viewport_ref->region = { min.x, min.y, max.x, max.y };
			} // TODO: initialize the viewport ref here (check for biome as well)

			ImGui::End();
		}

		// TODO: properties panel
		if (ImGui::Begin("Inhabitant Properties")) {
			ImGui::End();
		}

		imgui_end(cmd);

		// End the current render pass
		cmd.endRenderPass();

		// Generate the viewport rendering
		if (viewport_ref) {
			viewport_ref->resize(viewport_size);
			viewport_ref->render(cmd, op);
		}
	}
};

}

// TODO: mouse/key io structure
void handle_key_input(GLFWwindow *const win, Transform &camera_transform)
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

struct MouseInfo {
	bool drag = false;
	bool voided = true;
	float last_x = 0.0f;
	float last_y = 0.0f;
} static mouse;

void button_callback(GLFWwindow *window, int button, int action, int mods)
{
	// Ignore if on ImGui window
	ImGuiIO &io = ImGui::GetIO();
	io.AddMouseButtonEvent(button, action);

	if (ImGui::GetIO().WantCaptureMouse)
		return;

	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		mouse.drag = (action == GLFW_PRESS);
		if (action == GLFW_RELEASE)
			mouse.voided = true;
	}
}

void cursor_callback(GLFWwindow *window, double xpos, double ypos)
{
	Transform *camera_transform = (Transform *) glfwGetWindowUserPointer(window);

	// Ignore if on ImGui window
	ImGuiIO &io = ImGui::GetIO();
	io.MousePos = ImVec2(xpos, ypos);

	if (io.WantCaptureMouse)
		return;

	if (mouse.voided) {
		mouse.last_x = xpos;
		mouse.last_y = ypos;
		mouse.voided = false;
	}

	float xoffset = xpos - mouse.last_x;
	float yoffset = ypos - mouse.last_y;

	mouse.last_x = xpos;
	mouse.last_y = ypos;

	constexpr float sensitivity = 0.001f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	if (mouse.drag) {
		camera_transform->rotation.x += yoffset;
		camera_transform->rotation.y -= xoffset;

		if (camera_transform->rotation.x > 89.0f)
			camera_transform->rotation.x = 89.0f;
		if (camera_transform->rotation.x < -89.0f)
			camera_transform->rotation.x = -89.0f;
	}
}

int main()
{
	auto engine = ivy::exec::Globals::from();
	auto user_interface = ivy::exec::UserInterface::from(engine);

//	engine.biome = Biome::blank();
	engine.biome = Biome::load(IVY_ROOT "/data/sponza/sponza.obj");

	Mesh box = ivy::box({ 0, 10, 0 }, { 100, 100, 100 });

	InhabitantRef inh = engine.active_biome().new_inhabitant();

	inh->identifier = "Box";
	inh->add_component <Transform> ();
	inh->add_component <Geometry> (box, Material::null(), true);

	// Rendering
	size_t frame = 0;
	while (engine.vrb.valid_window()) {
		// Get events
		glfwPollEvents();

		// Begin the new frame
		auto [cmd, op] = engine.vrb.new_frame(frame).value();
		if (op.status == littlevk::SurfaceOperation::eResize)
			user_interface.resize(engine.vrb.window->extent);

		float t = 10.0f * glfwGetTime();
		glm::vec3 position = { 50 * sin(t), 20 * cos(t), 100 * cos(t/2) };
		inh->transform->position = position;

		// Draw the user interface
		user_interface.draw(cmd, op);

		// Complete and present the frame
		engine.vrb.end_frame(cmd, frame);
		auto pop = engine.vrb.present_frame(op, frame);
		if (pop.status == littlevk::SurfaceOperation::eResize)
			user_interface.resize(engine.vrb.window->extent);

		frame = 1 - frame;
	}
}
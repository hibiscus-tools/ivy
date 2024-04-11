#define LITTLEVK_GLM_TRANSLATOR

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <imgui/imgui.h>

#include <littlevk/littlevk.hpp>
#include <microlog/microlog.h>

#include <oak/caches.hpp>
#include <oak/camera.hpp>
#include <oak/contexts.hpp>
#include <oak/polygon.hpp>
#include <oak/transform.hpp>

#include "biome.hpp"
#include "shlighting.hpp"

#ifndef IVY_ROOT
#define IVY_ROOT ".."
#endif

#define IVY_SHADERS IVY_ROOT "/shaders"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

// TODO: util
// Functions
std::string readfile(const std::string &path)
{
	std::ifstream file(path);
	ulog_assert(file.is_open(), "Could not open file: %s", path.c_str());
	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

struct MVPConstants {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

static littlevk::Pipeline mesh_normals_pipeline(const DeviceResourceContext &drc, const RenderContext &rc)
{
	using Layout = littlevk::VertexLayout <glm::vec3, glm::vec3>;

	auto bundle = littlevk::ShaderStageBundle (drc.device, drc.dal)
		.attach(readfile(IVY_SHADERS "/mesh.vert"), vk::ShaderStageFlagBits::eVertex)
		.attach(readfile(IVY_SHADERS "/normals.frag"), vk::ShaderStageFlagBits::eFragment);

	return littlevk::PipelineCompiler <Layout> (drc.device, drc.window, drc.dal)
		.with_render_pass(rc.render_pass)
		.with_shader_bundle(bundle)
		.with_push_constant <MVPConstants> (vk::ShaderStageFlagBits::eVertex);
}

struct VulkanGeometry {
	littlevk::Buffer vertices;
	littlevk::Buffer triangles;
	size_t count;

	template <typename G>
	static VulkanGeometry from(const DeviceResourceContext &, const G &);
};

template <typename G>
VulkanGeometry VulkanGeometry::from(const DeviceResourceContext &drc, const G &g)
{
	VulkanGeometry vm;

	vm.count = 3 * g.triangles.size();

	vm.vertices = littlevk::buffer
	(
		drc.device,
		interleave_attributes(g),
		vk::BufferUsageFlagBits::eVertexBuffer,
		drc.memory_properties
	).unwrap(drc.dal);

	vm.triangles = littlevk::buffer
	(
		drc.device,
		g.triangles,
		vk::BufferUsageFlagBits::eIndexBuffer,
		drc.memory_properties
	).unwrap(drc.dal);

	return vm;
}

void handle_key_input(GLFWwindow *const win, Transform &camera_transform)
{
	static float last_time = 0.0f;

	constexpr float speed = 250.0f;

	float delta = speed * float(glfwGetTime() - last_time);
	last_time = glfwGetTime();

	// TODO: littlevk io system
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
	static const std::vector <const char *> extensions {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME
	};

	// Load physical device
	auto predicate = [](vk::PhysicalDevice phdev) {
		return littlevk::physical_device_able(phdev, extensions);
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

	// Configure a device resource context
	DeviceResourceContext drc = DeviceResourceContext::from(phdev, extensions, features);

	// Texture cache
	DeviceTextureCache dtc = DeviceTextureCache::from(drc);

	// Specific rendering context
//	RenderContext rc = RenderContext::from(drc);
	RenderContext rc {};
	littlevk::Image depth_buffer;

	// Subpasses
	{
		std::array <vk::AttachmentDescription, 2> attachments {
			littlevk::default_color_attachment(drc.swapchain.format),
			littlevk::default_depth_attachment()
		};

		vk::AttachmentReference color_attachment {
			0, vk::ImageLayout::eColorAttachmentOptimal
		};

		vk::AttachmentReference depth_attachment {
			1, vk::ImageLayout::eDepthStencilAttachmentOptimal
		};

		vk::SubpassDescription rendering_subpass {
			{}, vk::PipelineBindPoint::eGraphics,
			{}, color_attachment,
			{}, &depth_attachment
		};

		vk::AttachmentReference depth_input_attachment {
			1, vk::ImageLayout::eDepthReadOnlyOptimalKHR
		};

		std::array <vk::AttachmentReference, 1> environment_pass_inputs {
			depth_input_attachment
		};

		vk::SubpassDescription environment_subpass {
			{}, vk::PipelineBindPoint::eGraphics,
			environment_pass_inputs, color_attachment,
			{}, {}
		};

		std::array <vk::SubpassDescription, 2> subpasses {
			rendering_subpass,
			environment_subpass
		};

		vk::SubpassDependency dependency {
			0, 1,
			vk::PipelineStageFlagBits::eFragmentShader,
			vk::PipelineStageFlagBits::eFragmentShader,
		};

		rc.render_pass = littlevk::render_pass(drc.device,
			vk::RenderPassCreateInfo {
				{}, attachments,  subpasses, dependency
			}
		).unwrap(drc.dal);

		// Create the depth buffer
		littlevk::ImageCreateInfo depth_info {
			drc.window->extent.width,
			drc.window->extent.height,
			vk::Format::eD32Sfloat,
			vk::ImageUsageFlagBits::eDepthStencilAttachment
				| vk::ImageUsageFlagBits::eInputAttachment,
			vk::ImageAspectFlagBits::eDepth,
		};

		depth_buffer = littlevk::image(
			drc.device,
			depth_info, drc.memory_properties
		).unwrap(drc.dal);

		// Create framebuffers from the swapchain
		littlevk::FramebufferSetInfo fb_info {
			.swapchain = drc.swapchain,
			.render_pass = rc.render_pass,
			.extent = drc.window->extent,
			.depth_buffer = depth_buffer.view
		};

		rc.framebuffers = littlevk::framebuffers(drc.device, fb_info).unwrap(drc.dal);
	}

	// ImGui context
	imgui_context_from(drc, rc);

	Camera camera;
	Transform camera_transform;
	Transform mesh_transform;

	camera.aspect = drc.aspect_ratio();

	glfwSetWindowUserPointer(drc.window->handle, &camera_transform);
	glfwSetMouseButtonCallback(drc.window->handle, button_callback);
	glfwSetCursorPosCallback(drc.window->handle, cursor_callback);

	// Load the scene
	Biome biome = Biome::load(IVY_ROOT "/data/sponza/sponza.obj");

	const auto &geometries = biome.grab_all <Geometry> ();

	// TODO: shared mesh resource manager, used by rendering layer
	std::vector <VulkanGeometry> vgs;
	for (const Geometry &g : geometries)
		vgs.emplace_back(VulkanGeometry::from(drc, g.mesh));

	// TODO: gather all textures, then load them in parallel (thread pool)
	for (const Geometry &g : geometries) {
		const auto &textures = g.material.textures;
		if (textures.diffuse.size())
			dtc.load(textures.diffuse);
	}

	// Load the environment map and compute the spherical harmonics coefficients
	const std::string environment = IVY_ROOT "/data/environments/crossroads.hdr";

	dtc.load(environment);

	const Texture &tex = dtc.host_textures[environment];

	SHLighting shl = SHLighting::from(tex);

	// Upload the lighting information to the device
	littlevk::Buffer uniform_shl= littlevk::buffer
	(
		drc.device,
		std::array <SHLighting, 1> { shl },
		vk::BufferUsageFlagBits::eUniformBuffer,
		drc.memory_properties
	).unwrap(drc.dal);

	// Build the pipeline
	using RenderLayout = littlevk::VertexLayout <glm::vec3, glm::vec3, glm::vec2>;
	using EnvironmentLayout = littlevk::VertexLayout <glm::vec2, glm::vec2>;

	auto render_bundle = littlevk::ShaderStageBundle(drc.device, drc.dal)
		.attach(readfile(IVY_SHADERS "/mesh.vert"), vk::ShaderStageFlagBits::eVertex)
		.attach(readfile(IVY_SHADERS "/environment.frag"), vk::ShaderStageFlagBits::eFragment);

	auto environment_bundle = littlevk::ShaderStageBundle(drc.device, drc.dal)
		.attach(readfile(IVY_SHADERS "/screen.vert"), vk::ShaderStageFlagBits::eVertex)
		.attach(readfile(IVY_SHADERS "/post.frag"), vk::ShaderStageFlagBits::eFragment);

	littlevk::Pipeline render_ppl = littlevk::PipelineCompiler <RenderLayout> (drc.device, drc.window, drc.dal)
		.with_render_pass(rc.render_pass, 0)
		.with_shader_bundle(render_bundle)
		.with_dsl_binding(0, 1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment)
		.with_dsl_binding(1, 1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment)
		.with_push_constant <MVPConstants> (vk::ShaderStageFlagBits::eVertex);

	littlevk::Pipeline environment_ppl = littlevk::PipelineCompiler <EnvironmentLayout> (drc.device, drc.window, drc.dal)
		.with_render_pass(rc.render_pass, 1)
		.with_shader_bundle(environment_bundle)
		.with_dsl_binding(0, 1, vk::DescriptorType::eInputAttachment, vk::ShaderStageFlagBits::eFragment)
		.with_dsl_binding(1, 1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment)
		.with_push_constant <RayFrame> (vk::ShaderStageFlagBits::eFragment);

	auto screen = Polygon::screen();
	auto screen_vk = VulkanGeometry::from(drc, screen);

	// Default sampler
	// TODO: sampler builder...
	static constexpr vk::SamplerCreateInfo default_sampler_info {
		vk::SamplerCreateFlags {},
		vk::Filter::eLinear,
		vk::Filter::eLinear,
		vk::SamplerMipmapMode::eLinear,
		vk::SamplerAddressMode::eRepeat,
		vk::SamplerAddressMode::eRepeat,
		vk::SamplerAddressMode::eRepeat,
		0.0f,
		vk::False,
		1.0f,
		vk::False,
		vk::CompareOp::eAlways,
		0.0f,
		0.0f,
		vk::BorderColor::eIntOpaqueBlack,
		vk::False
	};

	vk::Sampler sampler = littlevk::sampler(drc.device, default_sampler_info).unwrap(drc.dal);

	// Construct descriptor sets for each mesh
	std::vector <vk::DescriptorSet> dsets;
	for (const Geometry &g : geometries) {
		// TODO: allocate all in a batch outside...
		vk::DescriptorSet dset = drc.device.allocateDescriptorSets
		(
			vk::DescriptorSetAllocateInfo {
				drc.descriptor_pool, render_ppl.dsl.value()
			}
		).front();

		// TODO: littlevk generator
		vk::DescriptorImageInfo image_info {};

		const auto &textures = g.material.textures;
		if (textures.diffuse.size()) {
			dtc.upload(textures.diffuse);

			const littlevk::Image &image = dtc.device_textures[textures.diffuse];

			image_info  = vk::DescriptorImageInfo {
				sampler, image.view,
				vk::ImageLayout::eShaderReadOnlyOptimal
			};
		} else {
			const littlevk::Image &image = dtc.device_textures.begin()->second;

			image_info  = vk::DescriptorImageInfo {
				sampler, image.view,
				vk::ImageLayout::eShaderReadOnlyOptimal
			};
		}

		vk::DescriptorBufferInfo buffer_info {
			uniform_shl.buffer,
			0, uniform_shl.device_size()
		};

		// TODO: arrays...
		// TODO: littlevk version
		vk::WriteDescriptorSet image_write {
			dset,
			0, 0, 1,
			vk::DescriptorType::eCombinedImageSampler,
			&image_info, nullptr, nullptr
		};

		vk::WriteDescriptorSet buffer_write {
			dset,
			1, 0, 1,
			vk::DescriptorType::eUniformBuffer,
			nullptr, &buffer_info, nullptr
		};

		drc.device.updateDescriptorSets(image_write, nullptr);
		drc.device.updateDescriptorSets(buffer_write, nullptr);

		dsets.push_back(dset);
	}

	// Another descriptor set for the environment subpass
	vk::DescriptorSet environment_dset = drc.device.allocateDescriptorSets
	(
		vk::DescriptorSetAllocateInfo {
			drc.descriptor_pool, environment_ppl.dsl.value()
		}
	).front();

	vk::DescriptorImageInfo depth_attachment_info {
		sampler, depth_buffer.view,
		vk::ImageLayout::eDepthReadOnlyOptimal
	};

	vk::WriteDescriptorSet input_attachment_write {
		environment_dset,
		0, 0, 1,
		vk::DescriptorType::eInputAttachment,
		&depth_attachment_info, nullptr, nullptr
	};

	drc.device.updateDescriptorSets(input_attachment_write, nullptr);

	dtc.upload(environment);

	// TODO: metaprogramming to avoid errors
	const littlevk::Image &environment_map = dtc.device_textures[environment];

	vk::DescriptorImageInfo environment_map_info {
		sampler, environment_map.view,
		vk::ImageLayout::eShaderReadOnlyOptimal
	};

	vk::WriteDescriptorSet environment_map_write {
		environment_dset,
		1, 0, 1,
		vk::DescriptorType::eCombinedImageSampler,
		&environment_map_info, nullptr, nullptr
	};

	drc.device.updateDescriptorSets(environment_map_write, nullptr);

	// Rendering
	size_t frame = 0;
	while (drc.valid_window()) {
		// Get events
		glfwPollEvents();

		// Moving the camera
		handle_key_input(drc.window->handle, camera_transform);

		auto [cmd, op] = drc.new_frame(frame).value();

		// Render things
		{
			LiveRenderContext(drc, rc).begin_render_pass(cmd, op);

			auto ppl = render_ppl;

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ppl.handle);

			MVPConstants mvp {};
			mvp.model = glm::mat4(1.0f);
			mvp.proj = camera.perspective_matrix();
			mvp.view = Camera::view_matrix(camera_transform);

			cmd.pushConstants <MVPConstants> (ppl.layout, vk::ShaderStageFlagBits::eVertex, 0, mvp);

			for (uint32_t i = 0; i < vgs.size(); i++) {
				const auto &vg = vgs[i];
				const auto &dset = dsets[i];

				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, ppl.layout, 0, dset, {});
				cmd.bindVertexBuffers(0, { vg.vertices.buffer }, { 0 });
				cmd.bindIndexBuffer(vg.triangles.buffer, 0, vk::IndexType::eUint32);
				cmd.drawIndexed(vg.count, 1, 0, 0, 0);
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ppl.handle);

			imgui_begin();

			std::function <void (Inhabitant)> recursive_note = [&](Inhabitant i) -> void {
				if (i.children.empty())
					return ImGui::Text("%s", i.identifier.c_str());

				if (ImGui::TreeNode(i.identifier.c_str())) {
					for (const Inhabitant &ic : i.children)
						recursive_note(ic);
					ImGui::TreePop();
				}
			};

			if (ImGui::Begin("Scene tree")) {
				for (const Inhabitant &i : biome.inhabitants)
					recursive_note(i);

				ImGui::End();
			}

			imgui_end(cmd);

			// Second subpass, for the envirnonment map
			cmd.nextSubpass(vk::SubpassContents::eInline);

			{
				auto ppl = environment_ppl;

				cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ppl.handle);

				RayFrame frame = camera.rayframe(camera_transform);

				cmd.pushConstants <RayFrame> (ppl.layout, vk::ShaderStageFlagBits::eFragment, 0, frame);

				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, ppl.layout, 0, environment_dset, {});
				cmd.bindVertexBuffers(0, { screen_vk.vertices.buffer }, { 0 });
				cmd.bindIndexBuffer(screen_vk.triangles.buffer, 0, vk::IndexType::eUint32);
				cmd.drawIndexed(screen_vk.count, 1, 0, 0, 0);
			}

			cmd.endRenderPass();
		}

		drc.end_frame(cmd, frame);
		drc.present_frame(op, frame);
		frame = 1 - frame;
	}
}

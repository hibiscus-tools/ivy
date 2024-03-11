#define LITTLEVK_GLM_TRANSLATOR

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <littlevk/littlevk.hpp>

// TODO: move to dependencies
#include "microlog.h"

#include "camera.hpp"
#include "contexts.hpp"
#include "biome.hpp"
#include "transform.hpp"

#ifndef IVY_ROOT
#define IVY_ROOT ".."
#endif

#define IVY_SHADERS IVY_ROOT "/shaders"

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

struct Pipeline {
	vk::Pipeline pipeline;
	vk::PipelineLayout layout;
};

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

void handle_key_input(const DeviceResourceContext &drc, Transform &camera_transform)
{
	static float last_time = 0.0f;

	constexpr float speed = 250.0f;

	float delta = speed * float(glfwGetTime() - last_time);
	last_time = glfwGetTime();

	// TODO: littlevk io system
	GLFWwindow *win = drc.window->handle;

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
	// ImGuiIO &io = ImGui::GetIO();
	// io.AddMouseButtonEvent(button, action);
	//
	// if (ImGui::GetIO().WantCaptureMouse)
	// 	return;

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
	// ImGuiIO &io = ImGui::GetIO();
	// io.MousePos = ImVec2(xpos, ypos);
	//
	// if (io.WantCaptureMouse)
	// 	return;

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

struct DeviceTextureCache {
	vk::Device device;
	vk::CommandPool command_pool;
	vk::Queue queue;
	vk::PhysicalDeviceMemoryProperties memory_properties;
	
	littlevk::Deallocator *dal;

	std::unordered_map <std::string, littlevk::Image> textures;

	bool contains(const std::filesystem::path &path) {
		return textures.count(path.string()) > 0;
	}

	const littlevk::Image &operator[](const std::filesystem::path &path) const {
		return textures.at(path.string());
	}

	const littlevk::Image &scapegoat() const {
		// TODO: populate with a blank texture if needed (or load a checkerboard, etc)
		return textures.begin()->second;
	}

	static DeviceTextureCache from(const DeviceResourceContext &drc) {
		DeviceTextureCache dtc {
			.device = drc.device,
			.command_pool = drc.command_pool,
			.queue = drc.graphics_queue,
			.memory_properties = drc.memory_properties,
			.dal = drc.dal
		};

		return dtc;
	}
};

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

// TODO: 2 separate functionalities for texture cache
// 1. Load texture into host memory
// 2. Upload texture into device memory (not everything needs to be uploaded)
static void load_texture(DeviceTextureCache &dtc, const std::filesystem::path &path)
{
	std::string tr = path.string();
	if (dtc.contains(tr))
		return;

	ulog_assert(std::filesystem::exists(path), "load_texture", "could not find path %s\n", tr.c_str());

	// TODO: common image loading utility
	int width;
	int height;
	int channels;

	stbi_set_flip_vertically_on_load(true);

	uint8_t *pixels = stbi_load(tr.c_str(), &width, &height, &channels, 4);

	littlevk::Image image = littlevk::image(dtc.device, {
		(uint32_t) width, (uint32_t) height,
		vk::Format::eR8G8B8A8Unorm,
		vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
		vk::ImageAspectFlagBits::eColor
	}, dtc.memory_properties).unwrap(dtc.dal);

	// Upload the image data
	littlevk::Buffer staging_buffer = littlevk::buffer(
		dtc.device,
		4 * width * height,
		vk::BufferUsageFlagBits::eTransferSrc,
		dtc.memory_properties
	).value;

	littlevk::upload(dtc.device, staging_buffer, pixels);

	// TODO: some state wise struct to simplify transitioning?
	littlevk::submit_now(dtc.device, dtc.command_pool, dtc.queue,
		[&](const vk::CommandBuffer &cmd) {
			littlevk::transition(cmd, image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
			littlevk::copy_buffer_to_image(cmd, image, staging_buffer, vk::ImageLayout::eTransferDstOptimal);
			littlevk::transition(cmd, image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
		}
	);

	// Free interim data
	littlevk::destroy_buffer(dtc.device, staging_buffer);
	stbi_image_free(pixels);

	ulog_info("load_texture", "loaded image (%s) with dimensions (%d, %d)\n", tr.c_str(), width, height);

	// app.image_cache[path.string()] = image;
	dtc.textures[tr] = image;
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

	VkPhysicalDeviceFragmentShaderBarycentricFeaturesNV barycentrics {};
	barycentrics.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_NV;
	barycentrics.fragmentShaderBarycentric = VK_TRUE;

	features.pNext = &barycentrics;

	phdev.getFeatures2(&features);

	// Configure a device resource context
	DeviceResourceContext drc = DeviceResourceContext::from(phdev, extensions, features);

	// Texture cache
	DeviceTextureCache dtc = DeviceTextureCache::from(drc);

	// Specific rendering context
	RenderContext rc = RenderContext::from(drc);

	Camera camera;
	Transform camera_transform;
	Transform mesh_transform;

	camera.aspect = drc.aspect_ratio();

	glfwSetWindowUserPointer(drc.window->handle, &camera_transform);
	glfwSetMouseButtonCallback(drc.window->handle, button_callback);
	glfwSetCursorPosCallback(drc.window->handle, cursor_callback);

	// Load the scene
	Biome biome = Biome::load(IVY_ROOT "/data/sponza/sponza.obj");
	
	std::vector <VulkanGeometry> vgs;
	for (const Mesh &mesh : biome.geometry)
		vgs.emplace_back(VulkanGeometry::from(drc, mesh));

	// TODO: gather all textures, then load them in parallel (thread pool)
	for (const Material &material : biome.materials) {
		const auto &textures = material.textures;
		if (textures.diffuse.size())
			load_texture(dtc, textures.diffuse);
	}

	// Build the pipeline	
	using Layout = littlevk::VertexLayout <glm::vec3, glm::vec3, glm::vec2>;

	auto bundle = littlevk::ShaderStageBundle(drc.device, drc.dal)
		.attach(readfile(IVY_SHADERS "/mesh.vert"), vk::ShaderStageFlagBits::eVertex)
		.attach(readfile(IVY_SHADERS "/albedo.frag"), vk::ShaderStageFlagBits::eFragment);

	littlevk::Pipeline ppl = littlevk::PipelineCompiler <Layout> (drc.device, drc.window, drc.dal)
		.with_render_pass(rc.render_pass)
		.with_shader_bundle(bundle)
		.with_dsl_binding(0, 1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment)
		.with_push_constant <MVPConstants> (vk::ShaderStageFlagBits::eVertex);

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
	for (const Material &material : biome.materials) {
		// TODO: allocate all in a batch outside...
		vk::DescriptorSet dset = drc.device.allocateDescriptorSets
		(
			vk::DescriptorSetAllocateInfo {
				drc.descriptor_pool, ppl.dsl.value()
			}
		).front();
			
		vk::DescriptorImageInfo image_info {};

		const auto &textures = material.textures;
		if (textures.diffuse.size()) {
			const littlevk::Image &image = dtc[textures.diffuse];

			image_info  = vk::DescriptorImageInfo {
				sampler, image.view,
				vk::ImageLayout::eShaderReadOnlyOptimal
			};
		} else {
			const littlevk::Image &image = dtc.scapegoat();

			image_info  = vk::DescriptorImageInfo {
				sampler, image.view,
				vk::ImageLayout::eShaderReadOnlyOptimal
			};
		}
			
		vk::WriteDescriptorSet write {
			dset,
			0, 0, 1,
			vk::DescriptorType::eCombinedImageSampler,
			&image_info,
			nullptr, nullptr
		};

		drc.device.updateDescriptorSets(write, nullptr);
		dsets.push_back(dset);
	}

	// Rendering
	size_t frame = 0;
	while (valid_window(drc)) {
		// Get events
		glfwPollEvents();

		// Moving the camera
		handle_key_input(drc, camera_transform);

		auto [cmd, op] = *new_frame(drc, frame);

		// Render things
		{
			begin_render_pass(drc, rc, cmd, op);

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ppl.handle);

			MVPConstants mvp;
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
			cmd.endRenderPass();
		}

		end_frame(drc, cmd, frame);

		// NOTE: this part is optional for a differentiable renderer
		present_frame(drc, op, frame);

		frame = 1 - frame;
	}

	// TODO: silhouette based gradient
	// Also image based sdf of the silhouettee; sobel and then flood fill
	// (test in python or something); should bring denser gradients for points outside the silhouette
}

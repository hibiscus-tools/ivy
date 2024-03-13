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
#include <oak/transform.hpp>

#include "biome.hpp"

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

	std::vector <glm::vec3> rgb = tex.as_rgb();

	float dt_dp = glm::pi <float> ();
	dt_dp = (2 * dt_dp * dt_dp)/rgb.size();

	glm::vec3 vY0;
	glm::vec3 vY1[3];
	glm::vec3 vY2[5];

	vY0 = glm::vec3(0.0f);
	vY1[0] = vY1[1] = vY1[2] = glm::vec3(0.0f);
	vY2[0] = vY2[1] = vY2[2] = vY2[3] = vY2[4] = glm::vec3(0.0f);

	for (uint32_t i = 0; i < tex.width; i++) {
		for (uint32_t j = 0; j < tex.height; j++) {
			uint32_t index = i * tex.height + j;

			// Convert to unit direction
			float u = i/float(tex.width);
			float v = j/float(tex.height);

			float theta = u * glm::pi <float> ();
			float phi = v * glm::two_pi <float> ();

			float sin_theta = sin(theta);

			glm::vec3 n {
				sin_theta * cos(phi),
				sin_theta * sin(phi),
				cos(theta)
			};

			// Convolve with the spherical harmonics
			float Y0 = 0.282095;
			
			float Y1[] = {
				0.488603f * n.z,
				0.488603f * n.x,
				0.499603f * n.y
			};

			float Y2[] = {
				0.315392f * (3 * n.z * n.z - 1),
				1.092548f * n.x * n.z,
				0.546274f * (n.x * n.x - n.y * n.y),
				1.092548f * n.x * n.y,
				1.092548f * n.y * n.z
			};

			const glm::vec3 &radiance = rgb[index];

			vY0 += radiance * Y0 * sin_theta * dt_dp;
			
			vY1[0] += radiance * Y1[0] * sin_theta * dt_dp;
			vY1[1] += radiance * Y1[1] * sin_theta * dt_dp;
			vY1[2] += radiance * Y1[2] * sin_theta * dt_dp;
			
			vY2[0] += radiance * Y2[0] * sin_theta * dt_dp;
			vY2[1] += radiance * Y2[1] * sin_theta * dt_dp;
			vY2[2] += radiance * Y2[2] * sin_theta * dt_dp;
			vY2[3] += radiance * Y2[3] * sin_theta * dt_dp;
			vY2[4] += radiance * Y2[4] * sin_theta * dt_dp;
		}
	}

	ulog_info("sh lighting", "vY0: %f, %f, %f\n", vY0.x, vY0.y, vY0.z);

	constexpr float c1 = 0.429043f;
	constexpr float c2 = 0.511664f;
	constexpr float c3 = 0.743125f;
	constexpr float c4 = 0.886227f;
	constexpr float c5 = 0.247708f;

	glm::mat4 M_red {
		c1 * vY2[2].x, c1 * vY2[2].x, c1 * vY2[1].x, c2 * vY1[1].x,
		c1 * vY2[2].x, -c1 * vY2[2].x, c1 * vY2[4].x, c2 * vY1[2].x,
		c2 * vY2[1].x, c1 * vY2[4].x, c3 * vY2[0].x, c2 * vY1[0].x,
		c2 * vY1[1].x, c2 * vY1[2].x, c2 * vY1[0].x, c4 * vY0.x - c5 * vY2[0].x
	};

	glm::mat4 M_green {
		c1 * vY2[2].y, c1 * vY2[2].y, c1 * vY2[1].y, c2 * vY1[1].y,
		c1 * vY2[2].y, -c1 * vY2[2].y, c1 * vY2[4].y, c2 * vY1[2].y,
		c2 * vY2[1].y, c1 * vY2[4].y, c3 * vY2[0].y, c2 * vY1[0].y,
		c2 * vY1[1].y, c2 * vY1[2].y, c2 * vY1[0].y, c4 * vY0.y - c5 * vY2[0].y
	};

	glm::mat4 M_blue {
		c1 * vY2[2].z, c1 * vY2[2].z, c1 * vY2[1].z, c2 * vY1[1].z,
		c1 * vY2[2].z, -c1 * vY2[2].z, c1 * vY2[4].z, c2 * vY1[2].z,
		c2 * vY2[1].z, c1 * vY2[4].z, c3 * vY2[0].z, c2 * vY1[0].z,
		c2 * vY1[1].z, c2 * vY1[2].z, c2 * vY1[0].z, c4 * vY0.z - c5 * vY2[0].z
	};

	ulog_info("sh lighting", "Mred: %s\n", glm::to_string(M_red).c_str());
	ulog_info("sh lighting", "Mred: %s\n", glm::to_string(M_green).c_str());
	ulog_info("sh lighting", "Mred: %s\n", glm::to_string(M_blue).c_str());

	struct SHLighting {
	    glm::mat4 red;
	    glm::mat4 green;
	    glm::mat4 blue;
	};

	SHLighting shl { .red = M_red, .green = M_green, .blue = M_blue };

	std::vector <uint32_t> original;
	std::vector <uint32_t> integrated;

	original.resize(rgb.size());
	integrated.resize(rgb.size());

	auto rgb_to_hex = [](const glm::vec3 &color) -> uint32_t {
		uint32_t r = color.x * 255.0f;
		uint32_t g = color.y * 255.0f;
		uint32_t b = color.z * 255.0f;
		return ((r & 0xff) << 16)
			| ((g & 0xff) << 8)
			| ((b & 0xff))
			| 0xff000000;
	};

	for (uint32_t i = 0; i < tex.width; i++) {
		for (uint32_t j = 0; j < tex.height; j++) {
			uint32_t index = i * tex.height + j;

			const glm::vec3 &color = rgb[index];
			original[index] = rgb_to_hex(color);

			// Convert to unit direction
			float u = i/float(tex.width);
			float v = j/float(tex.height);

			float theta = u * glm::pi <float> ();
			float phi = v * glm::two_pi <float> ();

			float sin_theta = sin(theta);

			glm::vec3 n {
				sin_theta * cos(phi),
				sin_theta * sin(phi),
				cos(theta)
			};

			glm::vec4 pn { n, 1.0f };
			glm::vec3 ic {
				glm::dot(M_red * pn, pn),
				glm::dot(M_green * pn, pn),
				glm::dot(M_blue * pn, pn),
			};

			ic = glm::clamp(ic, 0.0f, 1.0f);

			integrated[index] = rgb_to_hex(ic);
		}
	}

	stbi_flip_vertically_on_write(true);
	stbi_write_png("original.png", tex.width, tex.height, 4, original.data(), sizeof(uint32_t) * tex.width);
	stbi_write_png("integrated.png", tex.width, tex.height, 4, integrated.data(), sizeof(uint32_t) * tex.width);

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
	for (const Geometry &g : geometries) {
		// TODO: allocate all in a batch outside...
		vk::DescriptorSet dset = drc.device.allocateDescriptorSets
		(
			vk::DescriptorSetAllocateInfo {
				drc.descriptor_pool, ppl.dsl.value()
			}
		).front();
			
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
	while (drc.valid_window()) {
		// Get events
		glfwPollEvents();

		// Moving the camera
		handle_key_input(drc.window->handle, camera_transform);

		auto [cmd, op] = drc.new_frame(frame).value();

		// Render things
		{
			LiveRenderContext(drc, rc).begin_render_pass(cmd, op);

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

			cmd.endRenderPass();
		}

		drc.end_frame(cmd, frame);
		drc.present_frame(op, frame);
		frame = 1 - frame;
	}
}

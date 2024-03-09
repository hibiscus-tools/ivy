#define LITTLEVK_GLM_TRANSLATOR

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <littlevk/littlevk.hpp>

// TODO: move to dependencies
#include "microlog.h"

#include "camera.hpp"
#include "contexts.hpp"
#include "mesh.hpp"
#include "polygon.hpp"
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

	constexpr float speed = 100.0f;

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

	// Specific rendering context
	RenderContext rc = RenderContext::from(drc);

	Camera camera;
	Transform camera_transform;
	Transform mesh_transform;

	glfwSetWindowUserPointer(drc.window->handle, &camera_transform);
	glfwSetMouseButtonCallback(drc.window->handle, button_callback);
	glfwSetCursorPosCallback(drc.window->handle, cursor_callback);

	littlevk::Pipeline normals = mesh_normals_pipeline(drc, rc);

	// Mesh mesh = Mesh::load(IVY_ROOT "/data/planck.obj");
	Polygon mesh = Polygon::screen();
	VulkanGeometry vm_screen_quad = VulkanGeometry::from(drc, mesh);

	// Build the pipeline	
	using Layout = littlevk::VertexLayout <glm::vec2, glm::vec2>;

	auto bundle = littlevk::ShaderStageBundle (drc.device, drc.dal)
		.attach(readfile(IVY_SHADERS "/screen.vert"), vk::ShaderStageFlagBits::eVertex)
		.attach(readfile(IVY_SHADERS "/sdf.frag"), vk::ShaderStageFlagBits::eFragment);

	littlevk::Pipeline ppl = littlevk::PipelineCompiler <Layout> (drc.device, drc.window, drc.dal)
		.with_render_pass(rc.render_pass)
		.with_shader_bundle(bundle)
		.with_push_constant <RayFrame> (vk::ShaderStageFlagBits::eFragment);

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
	
			RayFrame rayframe = camera.rayframe(camera_transform);
			cmd.pushConstants <RayFrame> (ppl.layout, vk::ShaderStageFlagBits::eFragment, 0, rayframe);
			cmd.bindVertexBuffers(0, { vm_screen_quad.vertices.buffer }, { 0 });
			cmd.bindIndexBuffer(vm_screen_quad.triangles.buffer, 0, vk::IndexType::eUint32);
			cmd.drawIndexed(vm_screen_quad.count, 1, 0, 0, 0);

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

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
#include "prebuilt.hpp"

#ifndef IVY_ROOT
#define IVY_ROOT ".."
#endif

#define IVY_SHADERS IVY_ROOT "/shaders"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

struct MVPConstants {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

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

// Deduplicating vertices based on vertex position
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

int main()
{
	using enum vk::DescriptorType;
	using enum vk::ShaderStageFlagBits;
	using enum vk::ImageLayout;

	using standalone::readfile;

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

	// Configure a device resource context
	DeviceResourceContext drc = DeviceResourceContext::from(phdev, EXTENSIONS, features);

	// Texture cache
	DeviceTextureCache dtc = DeviceTextureCache::from(drc);

	// Configuring the render pass
	vk::RenderPass render_pass = littlevk::RenderPassAssembler(drc.device, drc.dal)
		.add_attachment(littlevk::default_color_attachment(drc.swapchain.format))
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

	// Create the depth buffer
	littlevk::Image depth_buffer = bind(drc.device, drc.memory_properties, drc.dal)
		.image(drc.window->extent,
			vk::Format::eD32Sfloat,
			vk::ImageUsageFlagBits::eDepthStencilAttachment
				| vk::ImageUsageFlagBits::eInputAttachment,
			vk::ImageAspectFlagBits::eDepth);

	// Create the framebuffers
	littlevk::FramebufferGenerator generator(drc.device, render_pass, drc.window->extent, drc.dal);
	for (const vk::ImageView &view : drc.swapchain.image_views)
		generator.add(view, depth_buffer.view);

	std::vector <vk::Framebuffer> framebuffers = generator.unpack();

	// ImGui context
	imgui_context_from(drc, render_pass);

	Camera camera;
	Transform camera_transform;
	Transform mesh_transform;

	camera.aspect = drc.aspect_ratio();

	glfwSetWindowUserPointer(drc.window->handle, &camera_transform);
	glfwSetMouseButtonCallback(drc.window->handle, button_callback);
	glfwSetCursorPosCallback(drc.window->handle, cursor_callback);

	// Load the scene
	Biome &biome = Biome::load(IVY_ROOT "/data/sponza/sponza.obj");

	Mesh box = ivy::box({ 0, 10, 0 }, { 10, 10, 10 });
	InhabitantRef inh = biome.new_inhabitant();
	inh->add_component <Transform> ();
	inh->add_component <Geometry> (box, Material::null(), true);

	auto geometries = biome.geometries;

	// TODO: shared mesh resource manager, used by rendering layer
	// TODO: dynamically loading, start the renderer and populate the scene...
	std::unordered_map <uint32_t, VulkanGeometry> vg_map;

	std::vector <VulkanGeometry> vgs;
	for (size_t i = 0; i < geometries.size(); i++) {
		auto &g = geometries[i];
		g.mesh = deduplicate(g.mesh);
		g.mesh.normals = smooth_normals(g.mesh);
		vg_map[i] = VulkanGeometry::from(drc, g.mesh);

		// TODO: gather all textures, then load them in parallel (thread pool)
		const auto &textures = g.material.textures;
		if (!textures.diffuse.empty())
			dtc.load(textures.diffuse);
	}

	// Load the environment map and compute the spherical harmonics coefficients
	const std::string environment = IVY_ROOT "/data/environments/crossroads.hdr";

	dtc.load(environment);
	const Texture &tex = dtc.host_textures[environment];

	SHLighting shl = SHLighting::from(tex);

	// Upload the lighting information to the device
	littlevk::Buffer uniform_shl = bind(drc.device, drc.memory_properties, drc.dal)
		.buffer(&shl, sizeof(shl), vk::BufferUsageFlagBits::eUniformBuffer);

	// TODO: mesh interleave with vertex layout specializations....

	// Build the pipeline
	constexpr auto rendering_vlayout = littlevk::VertexLayout <glm::vec3, glm::vec3, glm::vec2> ();
	constexpr auto environment_vlayout = littlevk::VertexLayout <glm::vec2, glm::vec2> ();

	constexpr auto rendering_dslbs = std::array <vk::DescriptorSetLayoutBinding, 3> {{
		{ 0, eCombinedImageSampler, 1, eFragment },
		{ 1, eUniformBuffer, 1, eFragment },
		{ 2, eUniformBuffer, 1, eFragment }
	}};

	constexpr auto environment_dslbs = std::array <vk::DescriptorSetLayoutBinding, 2> {{
		{ 0, eInputAttachment, 1, eFragment },
		{ 1, eCombinedImageSampler, 1, eFragment }
	}};

	auto render_bundle = littlevk::ShaderStageBundle(drc.device, drc.dal)
		.attach(readfile(IVY_SHADERS "/mesh.vert"), eVertex)
		.attach(readfile(IVY_SHADERS "/environment.frag"), eFragment);

	auto environment_bundle = littlevk::ShaderStageBundle(drc.device, drc.dal)
		.attach(readfile(IVY_SHADERS "/screen.vert"), eVertex)
		.attach(readfile(IVY_SHADERS "/post.frag"), eFragment);

	littlevk::Pipeline render_ppl = littlevk::PipelineAssembler(drc.device, drc.window, drc.dal)
		.with_render_pass(render_pass, 0)
		.with_vertex_layout(rendering_vlayout)
		.with_shader_bundle(render_bundle)
		.with_dsl_bindings(rendering_dslbs)
		.with_push_constant <MVPConstants> (eVertex);

	littlevk::Pipeline environment_ppl = littlevk::PipelineAssembler(drc.device, drc.window, drc.dal)
		.with_render_pass(render_pass, 1)
		.with_vertex_layout(environment_vlayout)
		.with_shader_bundle(environment_bundle)
		.with_dsl_bindings(environment_dslbs)
		.with_push_constant <RayFrame> (eFragment);

	auto screen = Polygon::screen();
	auto screen_vk = VulkanGeometry::from(drc, screen);

	// Default sampler
	vk::Sampler sampler = littlevk::SamplerAssembler(drc.device, drc.dal);

	// Construct descriptor sets for each mesh
	std::unordered_map <uint32_t, vk::DescriptorSet> dset_map;
	for (size_t i = 0; i < geometries.size(); i++) {
		const auto &g = geometries[i];
		// TODO: allocate all in a batch outside...
		vk::DescriptorSet dset = littlevk::bind(drc.device, drc.descriptor_pool)
			.allocate_descriptor_sets(*render_ppl.dsl).front();

		// TODO: stream/batchify
		const littlevk::Image &image = [&](const std::string &diffuse) {
			if (!diffuse.empty()) {
				dtc.upload(diffuse);
				return dtc.device_textures[diffuse];
			}

			return dtc.device_textures.begin()->second;
		} (g.material.textures.diffuse);

		// Export the material as well
		auto vmat = VulkanMaterial::from(g.material);

		littlevk::Buffer material_buffer = littlevk::bind(drc.device, drc.memory_properties, drc.dal)
			.buffer(&vmat, sizeof(vmat), vk::BufferUsageFlagBits::eUniformBuffer);

		littlevk::bind(drc.device, dset, rendering_dslbs)
			.update(0, 0, sampler, image.view, eShaderReadOnlyOptimal)
			.update(1, 0, *uniform_shl, 0, sizeof(SHLighting))
			.update(2, 0, *material_buffer, 0, sizeof(VulkanMaterial))
			.finalize();

		dset_map[i] = dset;
	}

	// Another descriptor set for the environment subpass
	vk::DescriptorSet environment_dset = littlevk::bind(drc.device, drc.descriptor_pool)
		.allocate_descriptor_sets(*environment_ppl.dsl).front();

	dtc.upload(environment);
	const littlevk::Image &environment_map = dtc.device_textures[environment];

	littlevk::bind(drc.device, environment_dset, environment_dslbs)
		.update(0, 0, sampler, depth_buffer.view, eDepthReadOnlyOptimal)
		.update(1, 0, sampler, environment_map.view, eShaderReadOnlyOptimal)
		.finalize();

	// Rendering
	size_t frame = 0;
	while (drc.valid_window()) {
		// Get events
		glfwPollEvents();

		// Moving the camera
		handle_key_input(drc.window->handle, camera_transform);

		// Begin the new frame
		auto [cmd, op] = drc.new_frame(frame).value();

		// Render things
		const auto &rpbi = littlevk::default_rp_begin_info <2>
			(render_pass, framebuffers[op.index], drc.window)
			.clear_color(0, std::array <float, 4> { 1.0f, 1.0f, 1.0f, 1.0f });

		cmd.beginRenderPass(rpbi, vk::SubpassContents::eInline);

		float t = glfwGetTime();
		glm::vec3 position = { 20 * sin(t), 10 * cos(t), 20 * cos(t/2) };
		inh->transform->position = position;

		{
			auto ppl = render_ppl;

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ppl.handle);

			MVPConstants mvp {};
			mvp.proj = camera.perspective_matrix();
			mvp.view = Camera::view_matrix(camera_transform);

			for (auto [transform, g] : biome.grab_all <Transform, Geometry> ()) {
				uint32_t index = g.hash();
				const auto &vg = vg_map[index];
				const auto &dset = dset_map[index];

				mvp.model = transform->matrix();

				cmd.pushConstants <MVPConstants> (ppl.layout, eVertex, 0, mvp);
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, ppl.layout, 0, dset, {});
				cmd.bindVertexBuffers(0, { vg.vertices.buffer }, { 0 });
				cmd.bindIndexBuffer(vg.triangles.buffer, 0, vk::IndexType::eUint32);
				cmd.drawIndexed(vg.count, 1, 0, 0, 0);
			}
		}

		{
			imgui_begin();

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

			imgui_end(cmd);
		}

		// Second subpass, for the envirnonment map
		cmd.nextSubpass(vk::SubpassContents::eInline);

		{
			auto ppl = environment_ppl;

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ppl.handle);

			RayFrame rayframe = camera.rayframe(camera_transform);

			cmd.pushConstants <RayFrame> (ppl.layout, eFragment, 0, rayframe);

			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, ppl.layout, 0, environment_dset, {});
			cmd.bindVertexBuffers(0, { screen_vk.vertices.buffer }, { 0 });
			cmd.bindIndexBuffer(screen_vk.triangles.buffer, 0, vk::IndexType::eUint32);
			cmd.drawIndexed(screen_vk.count, 1, 0, 0, 0);
		}

		// Finish rendering
		cmd.endRenderPass();

		// Complete and presen the frame
		drc.end_frame(cmd, frame);
		drc.present_frame(op, frame);
		frame = 1 - frame;
	}
}
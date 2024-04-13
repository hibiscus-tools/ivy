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

struct MVPConstants {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

void handle_key_input(GLFWwindow *const win, Transform &camera_transform)
{
	static float last_time = 0.0f;

	constexpr float speed = 250.0f;

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

int main()
{
	using enum vk::DescriptorType;
	using enum vk::ShaderStageFlagBits;
	using enum vk::ImageLayout;

	using standalone::readfile;

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
	vk::RenderPass render_pass;
	std::vector <vk::Framebuffer> framebuffers;

	littlevk::Image depth_buffer;

	// Configuring the render pass
	render_pass = littlevk::RenderPassAssembler(drc.device, drc.dal)
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
	depth_buffer = bind(drc.device, drc.memory_properties, drc.dal)
		.image(drc.window->extent,
			vk::Format::eD32Sfloat,
			vk::ImageUsageFlagBits::eDepthStencilAttachment
				| vk::ImageUsageFlagBits::eInputAttachment,
			vk::ImageAspectFlagBits::eDepth);

	littlevk::FramebufferGenerator generator(drc.device, render_pass, drc.window->extent, drc.dal);
	for (const vk::ImageView &view : drc.swapchain.image_views)
		generator.add(view, depth_buffer.view);
	framebuffers = generator.unpack();

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
	Biome biome = Biome::load(IVY_ROOT "/data/sponza/sponza.obj");

	auto geometries = biome.grab_all <Geometry> ();

	// TODO: shared mesh resource manager, used by rendering layer
	// TODO: dynamically loading, start the renderer and populate the scene...
	std::vector <VulkanGeometry> vgs;
	for (Geometry &g : geometries) {
		g.mesh = deduplicate(g.mesh);
		g.mesh.normals = smooth_normals(g.mesh);
		vgs.emplace_back(VulkanGeometry::from(drc, g.mesh));
	}

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

	// TODO: mesh interleave with vertexlayout specializations....

	// Build the pipeline
	constexpr auto rendering_vlayout = littlevk::VertexLayout <glm::vec3, glm::vec3, glm::vec2> ();
	constexpr auto environment_vlayout = littlevk::VertexLayout <glm::vec2, glm::vec2> ();

	constexpr auto rendering_dslbs = std::array <vk::DescriptorSetLayoutBinding, 2> {{
		{ 0, eCombinedImageSampler, 1, eFragment },
		{ 1, eUniformBuffer, 1, eFragment }
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
	std::vector <vk::DescriptorSet> dsets;
	for (const Geometry &g : geometries) {
		// TODO: allocate all in a batch outside...
		vk::DescriptorSet dset = littlevk::bind(drc.device, drc.descriptor_pool)
			.allocateDescriptorSets(*render_ppl.dsl).front();

		// TODO: stream/batchify
		const littlevk::Image &image = [&](const std::string &diffuse) {
			if (!diffuse.empty()) {
				dtc.upload(diffuse);
				return dtc.device_textures[diffuse];
			}

			return dtc.device_textures.begin()->second;
		} (g.material.textures.diffuse);

		// TODO: lazy? do it upon destruction or manually...
		littlevk::descriptor_set_update(drc.device, dset, rendering_dslbs, {
			littlevk::DescriptorImageElementInfo(sampler, image.view, eShaderReadOnlyOptimal),
			littlevk::DescriptorBufferElementInfo(uniform_shl.buffer, 0, uniform_shl.device_size())
		});

		dsets.push_back(dset);
	}

	// Another descriptor set for the environment subpass
	vk::DescriptorSet environment_dset = littlevk::bind(drc.device, drc.descriptor_pool)
		.allocateDescriptorSets(*environment_ppl.dsl).front();

	dtc.upload(environment);
	const littlevk::Image &environment_map = dtc.device_textures[environment];

	littlevk::descriptor_set_update(drc.device, environment_dset, environment_dslbs, {
		littlevk::DescriptorImageElementInfo(sampler, depth_buffer.view, eDepthReadOnlyOptimal),
		littlevk::DescriptorImageElementInfo(sampler, environment_map.view, eShaderReadOnlyOptimal)
	});

	// TODO: .buffer(...), .image(...), ...updater(...)

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
			const auto &rpbi = littlevk::default_rp_begin_info <2>
			        (render_pass, framebuffers[op.index], drc.window)
				.clear_color(0, std::array <float, 4> { 1.0f, 1.0f, 1.0f, 1.0f });

			cmd.beginRenderPass(rpbi, vk::SubpassContents::eInline);

			auto ppl = render_ppl;

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ppl.handle);

			MVPConstants mvp {};
			mvp.model = glm::mat4(1.0f);
			mvp.proj = camera.perspective_matrix();
			mvp.view = Camera::view_matrix(camera_transform);

			cmd.pushConstants <MVPConstants> (ppl.layout, eVertex, 0, mvp);

			for (uint32_t i = 0; i < vgs.size(); i++) {
				const auto &vg = vgs[i];
				const auto &dset = dsets[i];

				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, ppl.layout, 0, dset, {});
				// TODO: bind(cmd, vg).draw(instance, fireindex, offset...)
				cmd.bindVertexBuffers(0, { vg.vertices.buffer }, { 0 });
				cmd.bindIndexBuffer(vg.triangles.buffer, 0, vk::IndexType::eUint32);
				cmd.drawIndexed(vg.count, 1, 0, 0, 0);
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ppl.handle);

			{
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
			}

			// Second subpass, for the envirnonment map
			cmd.nextSubpass(vk::SubpassContents::eInline);

			{
				auto ppl = environment_ppl;

				cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ppl.handle);

				RayFrame frame = camera.rayframe(camera_transform);

				cmd.pushConstants <RayFrame> (ppl.layout, eFragment, 0, frame);

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
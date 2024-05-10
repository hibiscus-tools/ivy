#include <memory>

#include <fmt/printf.h>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_vulkan.h>

#include <glm/gtc/quaternion.hpp>

#include <littlevk/littlevk.hpp>

#include <microlog/microlog.h>

#include <oak/contexts.hpp>
#include <oak/polygon.hpp>
#include <oak/camera.hpp>
#include <oak/transform.hpp>
#include <vulkan/vulkan_enums.hpp>

#include "paths.hpp"
#include "vkport.hpp"
#include "exec/globals.hpp"

using standalone::readfile;

static void imgui_configure(const VulkanResourceBase &vrb, const vk::RenderPass &render_pass)
{
	// Configure ImGui
	imgui_context_from(vrb, render_pass);

	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigWindowsMoveFromTitleBarOnly = true;

	// Configure the font
	// ui.primary_font = io.Fonts->AddFontFromFileTTF(IVY_ROOT "/data/fonts/Tajawal-Medium.ttf", 16);
}

struct UserInterfaceModule {
	virtual void draw(const vk::CommandBuffer &) = 0;
};

struct Viewport : UserInterfaceModule {
	using resizer = std::function <void (const vk::Extent2D &)>;
	using viewer = std::function <vk::ImageView ()>;

	vk::DescriptorSet fb_descriptor;
	vk::ImageView fb_view_cached;
	vk::Sampler fb_sampler;

	viewer ftn_view;
	resizer ftn_resize;

	Viewport(const VulkanResourceBase &vrb, const viewer &viewer_, const resizer &resizer_)
			: fb_descriptor(nullptr),
			fb_view_cached(nullptr),
			ftn_view(viewer_),
			ftn_resize(resizer_) {
		fb_sampler = littlevk::SamplerAssembler(vrb.device, vrb.dal);
	}

	void draw(const vk::CommandBuffer &cmd) override {
		auto view = ftn_view();
		if (view != fb_view_cached && view) {
			fb_view_cached = view;
			fb_descriptor = ImGui_ImplVulkan_AddTexture
				(static_cast <VkSampler> (fb_sampler),
				static_cast <VkImageView> (view),
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			// TODO: delete the old
		}

		if (ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar)) {
			ImVec2 size = ImGui::GetContentRegionAvail();

			if (view) {
				ftn_resize({ uint32_t(size.x), uint32_t(size.y) });
				ImGui::Image(fb_descriptor, size);
			}

			ImGui::End();
		}
	}

	// TODO: capture utility
};

struct UserInterface {
	const VulkanResourceBase &vrb;

	vk::RenderPass render_pass;
	std::vector <vk::Framebuffer> framebuffers;

	std::vector <std::unique_ptr <UserInterfaceModule>> modules;

	void draw(const vk::CommandBuffer &cmd, const littlevk::SurfaceOperation &op) {
		// Begin the render pass
		const auto &rpbi = littlevk::default_rp_begin_info <2>
			(render_pass, framebuffers[op.index], vrb.window->extent)
			.clear_color(0, std::array <float, 4> { 1.0f, 1.0f, 1.0f, 1.0f });

		// Begin the render pass
		cmd.beginRenderPass(rpbi, vk::SubpassContents::eInline);

		imgui_begin();

		ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
		for (auto &ptr : modules)
			ptr->draw(cmd);

		imgui_end(cmd);

		// End the current render pass
		cmd.endRenderPass();
	}

	static UserInterface from(const VulkanResourceBase &vrb) {
		auto ui = UserInterface { vrb };

		ui.render_pass = littlevk::RenderPassAssembler(vrb.device, vrb.dal)
			.add_attachment(littlevk::default_color_attachment(vrb.swapchain.format))
			.add_subpass(vk::PipelineBindPoint::eGraphics)
				.color_attachment(0, vk::ImageLayout::eColorAttachmentOptimal)
				.done();

		// Generate the framebuffers
		littlevk::FramebufferGenerator generator(vrb.device, ui.render_pass, vrb.window->extent, vrb.dal);
		for (const vk::ImageView &view : vrb.swapchain.image_views)
			generator.add(view);

		ui.framebuffers = generator.unpack();

		// Configure the ImGui context
		imgui_configure(vrb, ui.render_pass);

		return ui;
	}
};

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

int main()
{
	auto vrb = ivy::exec::prepare_vulkan_resource_base();
	auto ui = UserInterface::from(vrb);

	// Render pass
	vk::RenderPass render_pass = littlevk::RenderPassAssembler(vrb.device, vrb.dal)
		.add_attachment(littlevk::default_color_attachment(vrb.swapchain.format))
		.add_subpass(vk::PipelineBindPoint::eGraphics)
			.color_attachment(0, vk::ImageLayout::eColorAttachmentOptimal)
			.done();

	// Allocate the images and framebuffer
	auto extent = vk::Extent2D { 1024, 1024 };

	littlevk::Image image = littlevk::bind(vrb.device, vrb.memory_properties, vrb.dal)
			.image(extent.width, extent.height,
				vrb.swapchain.format,
				vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment,
				vk::ImageAspectFlagBits::eColor);

	littlevk::FramebufferGenerator generator(vrb.device, render_pass, extent, vrb.dal);
	generator.add(image.view);

	auto framebuffers = generator.unpack();

	// Pipeline
	auto bundle = littlevk::ShaderStageBundle(vrb.device, vrb.dal)
		.attach(readfile(IVY_SHADERS "/splat.vert"), vk::ShaderStageFlagBits::eVertex)
		.attach(readfile(IVY_SHADERS "/sdf.frag"), vk::ShaderStageFlagBits::eFragment);

	littlevk::Pipeline ppl = littlevk::PipelineAssembler(vrb.device, vrb.window, vrb.dal)
		.with_render_pass(render_pass, 0)
		.with_shader_bundle(bundle)
		.alpha_blending(true)
		.with_push_constant <RayFrame> (vk::ShaderStageFlagBits::eFragment);

	// Load data into the pipeline

	Camera camera;
	Transform camera_transform;

	std::list <std::pair <littlevk::Image, int>> freeq;

	auto ftn = [&](const vk::CommandBuffer &cmd) {
		extent = image.extent;

		camera.aspect = float(extent.width)/float(extent.height);
		littlevk::viewport_and_scissor(cmd, extent);

		// Begin the render pass
		const auto &rpbi = littlevk::default_rp_begin_info <2>
			(render_pass, framebuffers[0], extent)
			.clear_color(0, std::array <float, 4> { 1.0f, 1.0f, 1.0f, 1.0f });

		cmd.beginRenderPass(rpbi, vk::SubpassContents::eInline);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ppl.handle);

		RayFrame rayframe = camera.rayframe(camera_transform);

		cmd.pushConstants <RayFrame> (ppl.layout, vk::ShaderStageFlagBits::eFragment, 0, rayframe);
		cmd.draw(6, 1, 0, 0);

		// End the current render pass
		cmd.endRenderPass();

		// Transition to a reasonable layout
		littlevk::transition(cmd, image,
			vk::ImageLayout::ePresentSrcKHR,
			vk::ImageLayout::eShaderReadOnlyOptimal);

		freeq.remove_if([&](auto &pr) -> bool {
			auto &[image, countdown] = pr;
			if (--countdown <= 0) {
				littlevk::destroy_image(vrb.device, image);
				return true;
			}

			return false;
		});
	};

	auto resize = [&](const vk::Extent2D &extent) {
		if (image.extent == extent)
			return;

		// Free old
		freeq.push_back({ image, 2 });

		// Allocate new
		image = littlevk::bind(vrb.device, vrb.memory_properties, vrb.dal)
			.image(extent.width, extent.height,
				vrb.swapchain.format,
				vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment,
				vk::ImageAspectFlagBits::eColor);

		littlevk::FramebufferGenerator generator(vrb.device, render_pass, extent, vrb.dal);
		generator.add(image.view);

		framebuffers = generator.unpack();
	};

	auto view = [&]() -> vk::ImageView {
		return image.view;
	};

	ui.modules.emplace_back(std::make_unique <Viewport> (vrb, view, resize));

	// Rendering
	size_t frame = 0;
	while (vrb.valid_window()) {
		// Get events
		glfwPollEvents();

		// Begin the new frame
		auto [cmd, op] = vrb.new_frame(frame).value();

		// TODO: multithreaded queues?
		handle_key_input(vrb.window->handle, camera_transform);

		ftn(cmd);

		ui.draw(cmd, op);

		// Complete and present the frame
		vrb.end_frame(cmd, frame);
		vrb.present_frame(op, frame);

		frame = 1 - frame;
	}
}

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>

#include "contexts.hpp"

DeviceResourceContext DeviceResourceContext::from(const vk::PhysicalDevice &phdev, const std::vector <const char *> &extensions, const vk::PhysicalDeviceFeatures2KHR &features)
{
	DeviceResourceContext drc;

	// TODO: option for headless, null swapchain etc
	drc.skeletonize(phdev, { 1920, 1080 }, "IVY", extensions, features);

	drc.phdev = phdev;
	drc.memory_properties = phdev.getMemoryProperties();
	drc.dal = new littlevk::Deallocator(drc.device);
	drc.sync = littlevk::present_syncronization(drc.device, 2).unwrap(drc.dal);

	// Allocate command buffers
	drc.command_pool = littlevk::command_pool
	(
		drc.device,
		vk::CommandPoolCreateInfo {
			vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			littlevk::find_graphics_queue_family(phdev)
		}
	).unwrap(drc.dal);

	drc.command_buffers = drc.device.allocateCommandBuffers({
		drc.command_pool,
		vk::CommandBufferLevel::ePrimary, 2
	});

	// Allocate descriptor pool
	vk::DescriptorPoolSize pool_size {
		vk::DescriptorType::eCombinedImageSampler, 1 << 10
	};

	drc.descriptor_pool = littlevk::descriptor_pool(
		drc.device, vk::DescriptorPoolCreateInfo {
			{}, 1 << 10, pool_size
		}
	).unwrap(drc.dal);

	return drc;
}

std::optional <std::pair <vk::CommandBuffer, littlevk::SurfaceOperation>> DeviceResourceContext::new_frame(size_t frame)
{
	// Get next image
	littlevk::SurfaceOperation op;
	op = littlevk::acquire_image(device, swapchain.swapchain, sync[frame]);
	if (op.status == littlevk::SurfaceOperation::eResize) {
		resize();
		return std::nullopt;
	}

	vk::CommandBuffer cmd = command_buffers[frame];
	cmd.begin(vk::CommandBufferBeginInfo {});

	littlevk::viewport_and_scissor(cmd, littlevk::RenderArea(window));

	// Record command buffer
	return std::make_pair(cmd, op);
}

void DeviceResourceContext::end_frame(const vk::CommandBuffer &cmd, size_t frame) const
{
	cmd.end();

	// Submit command buffer while signaling the semaphore
	// TODO: littlevk shortcut for this...
	vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

	vk::SubmitInfo submit_info {
		1, &sync.image_available[frame],
		&wait_stage,
		1, &cmd,
		1, &sync.render_finished[frame]
	};

	graphics_queue.submit(submit_info, sync.in_flight[frame]);
}

void DeviceResourceContext::present_frame(const littlevk::SurfaceOperation &op, size_t frame)
{
	// Send image to the screen
	littlevk::SurfaceOperation pop = littlevk::present_image(present_queue, swapchain.swapchain, sync[frame], op.index);
	if (pop.status == littlevk::SurfaceOperation::eResize)
		resize();
}

bool DeviceResourceContext::valid_window() const
{
	return glfwWindowShouldClose(window->handle) == 0;
}

RenderContext RenderContext::from(const DeviceResourceContext &drc)
{
	RenderContext rc;

	// Create the render pass
	rc.render_pass = littlevk::default_color_depth_render_pass(drc.device, drc.swapchain.format).unwrap(drc.dal);

	// Create the depth buffer
	littlevk::ImageCreateInfo depth_info {
		drc.window->extent.width,
		drc.window->extent.height,
		vk::Format::eD32Sfloat,
		vk::ImageUsageFlagBits::eDepthStencilAttachment,
		vk::ImageAspectFlagBits::eDepth,
	};

	littlevk::Image depth_buffer = littlevk::image(
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

	return rc;
}

// TODO: bind(...) to return this?
void LiveRenderContext::begin_render_pass(const vk::CommandBuffer &cmd, const littlevk::SurfaceOperation &op)
{
	const auto &rpbi = littlevk::default_rp_begin_info <2>
		(rc.render_pass, rc.framebuffers[op.index], drc.window)
		.clear_value(0, vk::ClearColorValue {
			std::array <float, 4> { 1.0f, 1.0f, 1.0f, 1.0f }
		});

	return cmd.beginRenderPass(rpbi, vk::SubpassContents::eInline);
}

// ImGui configureation
void imgui_context_from(const DeviceResourceContext &drc, const RenderContext &rc)
{
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForVulkan(drc.window->handle, true);

	// Allocate descriptor pool
	vk::DescriptorPoolSize pool_size {
		vk::DescriptorType::eCombinedImageSampler, 1 << 10
	};

	vk::DescriptorPool descriptor_pool = littlevk::descriptor_pool(
		drc.device, vk::DescriptorPoolCreateInfo {
			{}, 1 << 10, pool_size
		}
	).unwrap(drc.dal);

	// Initialize ImGui
	ImGui_ImplVulkan_InitInfo init_info = {};

	init_info.Instance = littlevk::detail::get_vulkan_instance();
	init_info.DescriptorPool = descriptor_pool;
	init_info.Device = drc.device;
	init_info.ImageCount = 3;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.MinImageCount = 3;
	init_info.PhysicalDevice = drc.phdev;
	init_info.Queue = drc.graphics_queue;
	init_info.RenderPass = rc.render_pass;
	
	ImGui_ImplVulkan_Init(&init_info);
}

void imgui_begin()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}


void imgui_end(const vk::CommandBuffer &cmd)
{
	ImGui::Render();

	// Write to the command buffer
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}
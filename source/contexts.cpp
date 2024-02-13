#include "contexts.hpp"

DeviceResourceContext DeviceResourceContext::from(const vk::PhysicalDevice &phdev, const std::vector <const char *> &extensions, const vk::PhysicalDeviceFeatures2KHR &features)
{
		DeviceResourceContext drc;

		// TODO: option for headless, null swapchain etc
		drc.skeletonize(phdev, { 1000, 1000 }, "Vyne", extensions, features);

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

		return drc;
	}

std::optional <std::pair <vk::CommandBuffer, littlevk::SurfaceOperation>> new_frame(DeviceResourceContext &drc, size_t frame)
{
	// Get next image
	littlevk::SurfaceOperation op;
	op = littlevk::acquire_image(drc.device, drc.swapchain.swapchain, drc.sync[frame]);
	if (op.status == littlevk::SurfaceOperation::eResize) {
		drc.resize();
		return std::nullopt;
	}

	vk::CommandBuffer cmd = drc.command_buffers[frame];
	cmd.begin(vk::CommandBufferBeginInfo {});

	littlevk::viewport_and_scissor(cmd, littlevk::RenderArea(drc.window));

	// Record command buffer
	return std::make_pair(cmd, op);
}

void end_frame(const DeviceResourceContext &drc, const vk::CommandBuffer &cmd, size_t frame)
{
	cmd.end();

	// Submit command buffer while signaling the semaphore
	// TODO: littlevk shortcut for this...
	vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

	vk::SubmitInfo submit_info {
		1, &drc.sync.image_available[frame],
		&wait_stage,
		1, &cmd,
		1, &drc.sync.render_finished[frame]
	};

	drc.graphics_queue.submit(submit_info, drc.sync.in_flight[frame]);
}

void present_frame(DeviceResourceContext &drc, const littlevk::SurfaceOperation &op, size_t frame)
{
	// Send image to the screen
	littlevk::SurfaceOperation pop = littlevk::present_image(drc.present_queue, drc.swapchain.swapchain, drc.sync[frame], op.index);
	if (pop.status == littlevk::SurfaceOperation::eResize)
		drc.resize();
}

bool valid_window(const DeviceResourceContext &drc)
{
	return glfwWindowShouldClose(drc.window->handle) == 0;
}

RenderContext RenderContext::from(DeviceResourceContext &drc)
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
	littlevk::FramebufferSetInfo fb_info;
	fb_info.swapchain = &drc.swapchain;
	fb_info.render_pass = rc.render_pass;
	fb_info.extent = drc.window->extent;
	fb_info.depth_buffer = &depth_buffer.view;

	rc.framebuffers = littlevk::framebuffers(drc.device, fb_info).unwrap(drc.dal);

	return rc;
}

void begin_render_pass(const DeviceResourceContext &drc, const RenderContext &rc, const vk::CommandBuffer &cmd, const littlevk::SurfaceOperation &op)
{
	const auto &rpbi = littlevk::default_rp_begin_info <2>
		(rc.render_pass, rc.framebuffers[op.index], drc.window)
		.clear_value(0, vk::ClearColorValue {
			std::array <float, 4> { 1.0f, 1.0f, 1.0f, 1.0f }
		});

	return cmd.beginRenderPass(rpbi, vk::SubpassContents::eInline);
}


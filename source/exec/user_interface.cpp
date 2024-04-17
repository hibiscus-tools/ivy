#include <imgui/imgui.h>

#include "exec/user_interface.hpp"

namespace ivy::exec {

static void biome_tree(const Biome &biome)
{
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

void UserInterface::resize(const vk::Extent2D &extent)
{
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

void UserInterface::draw(const vk::CommandBuffer &cmd, const littlevk::SurfaceOperation &op)
{
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

UserInterface UserInterface::from(Globals &engine)
{
	// Configure the render pass
	// TODO: prepare
	vk::RenderPass render_pass = littlevk::RenderPassAssembler(engine.vrb.device, engine.vrb.dal)
		.add_attachment(littlevk::default_color_attachment(engine.vrb.swapchain.format))
		.add_subpass(vk::PipelineBindPoint::eGraphics)
			.color_attachment(0, vk::ImageLayout::eColorAttachmentOptimal)
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

}
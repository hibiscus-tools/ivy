#include <variant>

#include <imgui/imgui.h>

#include "exec/user_interface.hpp"

namespace ivy::exec {

std::optional <uint32_t> biome_tree(Biome &biome)
{
	static std::optional <uint32_t> selected_id;

	// TODO: detect escape?

	// TODO: viepwort method
	// TODO: define method
	const std::function <void (const ComponentRef <Inhabitant> &)> recursive_note = [&](const ComponentRef <Inhabitant> &inh) -> void {
		if (inh->children.empty()) {
			if (ImGui::Selectable(inh->identifier.c_str(), selected_id == inh.hash())) {
				printf("Selected inhabitant: %s\n", inh->identifier.c_str());
				selected_id = inh.hash();
			}

			return;
		}

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (selected_id == inh.hash())
			flags |= ImGuiTreeNodeFlags_Selected;

		bool open = ImGui::TreeNodeEx(inh->identifier.c_str(), flags);
		if (ImGui::IsItemClicked()) {
			printf("Selected inhabitant: %s\n", inh->identifier.c_str());
			selected_id = inh.hash();
		}

		if (open) {
			for (const auto &ic : inh->children)
				recursive_note(ic);
			ImGui::TreePop();
		}
	};

	if (ImGui::Begin("Scene tree")) {
		for (size_t i = 0; i < biome.inhabitants.size(); i++) {
			if (biome.inhabitants[i].parent.has_value())
				continue;
			recursive_note(ComponentRef <Inhabitant> (biome.inhabitants, i));
		}

		ImGui::End();
	}

	return selected_id;
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

	ImGui::PushFont(primary_font);

	// Top bar
	if (ImGui::BeginMainMenuBar()) {
		ImGui::MenuItem("File");
		ImGui::MenuItem("Project");
		ImGui::EndMainMenuBar();
	}

	// TODO: check from the viewport as well, which caches every time a click happens
	std::optional <uint32_t> selected_id;
	if (engine.biome) {
		selected_id = biome_tree(*engine.biome);

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
		if (selected_id) {
			Biome &biome = (*engine.biome);
			auto &inh = biome.inhabitants[*selected_id];
			ImGui::Text("%s", inh.identifier.c_str());

			// Go through each component
			if (inh.transform.has_value()) {
				// auto t = inh.transform;
				// auto p = t->position;
				// auto r = t->rotation;
				// auto s = t->scale;

				ImGui::Separator();
//				if (ImGui::TreeNodeEx("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::Text("Transform");
//					ImGui::Text("Position");
//					ImGui::SameLine();
//					ImGui::Text("%.2f %.2f %.2f", p.x, p.y, p.z);
//
//					ImGui::Text("Rotation");
//					ImGui::SameLine();
//					ImGui::Text("%.2f %.2f %.2f", r.x, r.y, r.z);
//
//					ImGui::Text("Scale");
//					ImGui::SameLine();
//					ImGui::Text("%.2f %.2f %.2f", s.x, s.y, s.z);
				// if (ImGui::BeginChild("f")) {
				// 	ImGui::Text("Scale");
				// 	ImGui::Text("Scale");
				// 	ImGui::Text("Scale");
				// 	ImGui::EndChild();
				// }

				// ImGui::SameLine();
				// if (ImGui::BeginChild("g")) {
				// 	ImGui::Text("Scale");
				// 	ImGui::EndChild();
				// }

//				ImGui::TreePop();
			}

			if (inh.geometry.has_value()) {
				ImGui::Separator();
				ImGui::Text("Geometry");

				Mesh mesh = inh.geometry->mesh;
				Material material = inh.geometry->material;
				ImGui::Text("Mesh: %lu vertices and %lu triangles", mesh.positions.size(), mesh.triangles.size());

				ImGui::Text("Materials");
			}

			if (inh.collider.has_value()) {
				ImGui::Separator();
				ImGui::Text("Collider");

				auto shape = inh.collider->shape;

				if (std::holds_alternative <sdf::Box> (shape))
					ImGui::Text("Shape: Box");

				if (std::holds_alternative <sdf::Sphere> (shape))
					ImGui::Text("Shape: Sphere");
			}
		}

		ImGui::End();
	}

	ImGui::PopFont();

	imgui_end(cmd);

	// End the current render pass
	cmd.endRenderPass();

	// Generate the viewport rendering
	if (viewport_ref && viewport_size.width > 0 && viewport_size.height > 0) {
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

	// Configure the font
	ui.primary_font = io.Fonts->AddFontFromFileTTF(IVY_ROOT "/data/fonts/Tajawal-Medium.ttf", 16);

	return ui;
}

}

#include <littlevk/littlevk.hpp>
#include <microlog/microlog.h>

#include <oak/transform.hpp>

#include "biome.hpp"
#include "components.hpp"
#include "exec/globals.hpp"
#include "exec/user_interface.hpp"
#include "prebuilt.hpp"

int main()
{
	auto engine = ivy::exec::Globals::from();
	auto user_interface = ivy::exec::UserInterface::from(engine);

//	engine.biome = Biome::blank();
	engine.biome = ivy::Biome::load(IVY_ROOT "/data/sponza/sponza.obj");

	Mesh box = ivy::box({ 0, 10, 0 }, { 10, 10, 10 });

	ivy::Sphere sphere = ivy::Sphere({ 0, 10, 0 }, 100);

	ivy::ComponentRef <ivy::Inhabitant> inh = engine.active_biome().new_inhabitant();

	inh->identifier = "Box";
	inh->add_component <ivy::Transform> ();
	inh->add_component <ivy::Geometry> (box, Material::null(), true);

	auto [transform] = *inh->grab <ivy::Transform> ();
	inh->add_component <ivy::Collider> (*transform, sphere, true, true);

	// Rendering
	size_t frame = 0;
	while (engine.vrb.valid_window()) {
		// Get events
		glfwPollEvents();

		// Begin the new frame
		auto [cmd, op] = engine.vrb.new_frame(frame).value();
		if (op.status == littlevk::SurfaceOperation::eResize)
			user_interface.resize(engine.vrb.window->extent);

		// float t = 10.0f * glfwGetTime();
		// glm::vec3 position = { 50 * sin(t), 20 * cos(t), 100 * cos(t/2) };
		// inh->transform->position = position;

		// Draw the user interface
		user_interface.draw(cmd, op);

		// Complete and present the frame
		engine.vrb.end_frame(cmd, frame);
		auto pop = engine.vrb.present_frame(op, frame);
		if (pop.status == littlevk::SurfaceOperation::eResize)
			user_interface.resize(engine.vrb.window->extent);

		frame = 1 - frame;
	}
}

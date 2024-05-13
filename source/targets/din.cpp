#include <fmt/printf.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtx/string_cast.hpp>

#include <littlevk/littlevk.hpp>

#include "core/texture.hpp"
#include "exec/globals.hpp"
#include "paths.hpp"

// TODO: specialization of typed texture is uint32_t for saving and loading...
template <typename T>
struct Texture {
	size_t width;
	size_t height;
	std::vector <T> pixels;

	T *operator[](size_t i) {
		return &pixels[i * width];
	}

	const T *operator[](size_t i) const {
		return &pixels[i * width];
	}

	T sample(glm::vec2 uv) const {
		uv = glm::clamp(uv, 0.0f, 1.0f);
		float i = (height - 1) * uv.x;
		float j = (width - 1) * uv.y;

		size_t i0 = floor(i);
		size_t i1 = ceil(i);

		size_t j0 = floor(j);
		size_t j1 = ceil(j);

		i -= i0;
		j -= j0;

		const T &v00 = pixels[i0 * width + j0];
		const T &v01 = pixels[i0 * width + j1];
		const T &v10 = pixels[i1 * width + j0];
		const T &v11 = pixels[i1 * width + j1];

		return v00 * (1 - i) * (1 - j)
			+ v01 * (1 - i) * j
			+ v10 * i * (1 - j)
			+ v11 * i * j;
	}

	ivy::Texture as_texture() const;

	static Texture from(size_t w, size_t h) {
		return { w, h, std::vector <T> (w * h) };
	}

	static Texture from(const ivy::Texture &texture) {
		return {
			(size_t) texture.width,
			(size_t) texture.height,
			texture.as_rgb()
		};
	}
};

template <typename T, typename U, typename F>
Texture <T> transform(const Texture <U> &texture, const F &ftn)
{
	auto result = Texture <T> ::from(texture.width, texture.height);
	for (size_t i = 0; i < texture.width * texture.height; i++)
		result.pixels[i] = ftn(texture.pixels[i]);

	return result;
}

template <>
ivy::Texture Texture <glm::vec4> ::as_texture() const
{
	std::vector <uint8_t> uint_pixels(width * height * sizeof(uint32_t));

	for (int i = 0; i < width; i++) {
		for (int j = 0; j < height; j++) {
			uint32_t index = (i * height + j);

			const glm::vec4 &c = pixels[index];

			uint8_t *dst = &uint_pixels[index << 2];
			dst[0] = 255.0f * c.r;
			dst[1] = 255.0f * c.g;
			dst[2] = 255.0f * c.b;
			dst[3] = 255.0f * c.a;
		}
	}

	return { (int) width, (int) height, 4, uint_pixels };
}

template <>
ivy::Texture Texture <glm::vec3> ::as_texture() const
{
	std::vector <uint8_t> uint_pixels(width * height * sizeof(uint32_t));

	for (int i = 0; i < width; i++) {
		for (int j = 0; j < height; j++) {
			uint32_t index = (i * height + j);

			const glm::vec3 &c = pixels[index];

			uint8_t *dst = &uint_pixels[index << 2];
			dst[0] = 255.0f * c.r;
			dst[1] = 255.0f * c.g;
			dst[2] = 255.0f * c.b;
			dst[3] = 0xff;
		}
	}

	return { (int) width, (int) height, 4, uint_pixels };
}

template <>
ivy::Texture Texture <glm::vec2> ::as_texture() const
{
	std::vector <uint8_t> uint_pixels(width * height * sizeof(uint32_t));

	for (int i = 0; i < width; i++) {
		for (int j = 0; j < height; j++) {
			uint32_t index = (i * height + j);

			const glm::vec2 &c = pixels[index];

			uint8_t *dst = &uint_pixels[index << 2];
			dst[0] = 255.0f * c.r;
			dst[1] = 255.0f * c.g;
			dst[2] = 0;
			dst[3] = 0xff;
		}
	}

	return { (int) width, (int) height, 4, uint_pixels };
}

struct DIn {
	Texture <glm::vec2> uvs;
	Texture <glm::vec3> colors;

	Texture <glm::vec3> render(size_t w, size_t h) const {
		auto tex = Texture <glm::vec3> ::from(w, h);

		for (size_t i = 0; i < h; i++) {
			for (size_t j = 0; j < w; j++) {
				glm::vec2 uv = { float(i)/h, float(j)/w };

				uv = uvs.sample(uv);
				tex[i][j] = colors.sample(uv);
			}
		}

		return tex;
	}

	static DIn from(size_t iw, size_t ih, size_t cw, size_t ch) {
		auto uvs = Texture <glm::vec2> ::from(iw, ih);
		auto colors = Texture <glm::vec3> ::from(cw, ch);

		// Normal UV initialization
		for (size_t i = 0; i < ih; i++) {
			for (size_t j = 0; j < iw; j++) {
				uvs[i][j] = {
					float(i)/ih,
					float(j)/iw
				};
			}
		}

		// Random color initialization
		for (size_t i = 0; i < ch; i++) {
			for (size_t j = 0; j < cw; j++)
				colors[i][j] = glm::linearRand(glm::vec3(0.0f), glm::vec3(1.0f));
		}

		return DIn { uvs, colors };
	}
};

void optimize(DIn &din, const Texture <glm::vec3> &reference)
{
	auto loss = [&](const Texture <glm::vec3> &image) {
		size_t pixels = image.width * image.height;

		float loss = 0;
		for (size_t i = 0; i < pixels; i++) {
			float l = glm::length(reference.pixels[i] - image.pixels[i]);
			loss += (l * l)/float(pixels);
		}

		return loss;
	};

	// TODO: Texture::like
	auto dcolors = Texture <glm::vec3> ::from(din.colors.width, din.colors.height);

	float beta1 = 0.9f;
	float beta2 = 0.999f;

	auto dcolors_M = Texture <glm::vec3> ::from(din.colors.width, din.colors.height);
	auto dcolors_S = Texture <glm::vec3> ::from(din.colors.width, din.colors.height);
	auto dcolors_Mh = Texture <glm::vec3> ::from(din.colors.width, din.colors.height);
	auto dcolors_Sh = Texture <glm::vec3> ::from(din.colors.width, din.colors.height);

	for (size_t i = 0; i < 100; i++) {
		auto proxy = din.render(reference.width, reference.height);
		fmt::println("{:>3}: {}", i + 1, loss(proxy));

		for (size_t i = 0; i < reference.height; i++) {
			for (size_t j = 0; j < reference.width; j++) {
				glm::vec2 uv = { float(i)/reference.height, float(j)/reference.width };

				uv = din.uvs.sample(uv);
				uv = glm::clamp(uv, 0.0f, 1.0f);

				glm::vec3 c = din.colors.sample(uv);
				glm::vec3 dc = reference[i][j] - c;

				float si = (din.colors.height - 1) * uv.x;
				float sj = (din.colors.width - 1) * uv.y;

				size_t i0 = floor(si);
				size_t i1 = ceil(si);

				size_t j0 = floor(sj);
				size_t j1 = ceil(sj);

				si -= i0;
				sj -= j0;

				float k = 2.0f/float(reference.width * reference.height);
				// dcolors[i0][j0] -= k * (1 - si) * (1 - sj) * dc;
				// dcolors[i0][j1] -= k * (1 - si) * sj * dc;
				// dcolors[i1][j0] -= k * si * (1 - sj) * dc;
				// dcolors[i1][j1] -= k * si * sj * dc;

				dcolors[i0][j0] -= k * dc;
				dcolors[i0][j1] -= k * dc;
				dcolors[i1][j0] -= k * dc;
				dcolors[i1][j1] -= k * dc;
			}
		}

		for (size_t i = 0; i < din.colors.width; i++) {
			for (size_t j = 0; j < din.colors.height; j++) {
				constexpr float epsilon = 1e-6f;

				dcolors_M[i][j] = beta1 *  dcolors_M[i][j] + (1 - beta1) * dcolors[i][j];
				dcolors_M[i][j] = beta2 *  dcolors_S[i][j] + (1 - beta2) * dcolors[i][j] * dcolors[i][j];

				dcolors_Mh[i][j] = dcolors_M[i][j]/(1 - powf(beta1, i + 1));
				dcolors_Sh[i][j] = dcolors_S[i][j]/(1 - powf(beta2, i + 1));

				din.colors[i][j] += 0.05f * dcolors_Mh[i][j]/glm::sqrt(dcolors_Sh[i][j] + epsilon);
			}
		}
	}
}

// Rendering with full pipeline
constexpr char renderer[] = R"(
#version 450

layout (location = 0) in vec2 uv;

layout (binding = 0) uniform sampler2D uvs;
layout (binding = 1) uniform sampler2D colors;

layout (location = 0) out vec4 fragment;

void main()
{
	vec2 iuv = texture(uvs, uv).xy;

	vec3 color = texture(colors, iuv).xyz;
	fragment = vec4(color, 1.0f);
}
)";

// Evaluating derivatives
constexpr char renderer_backwards[] = R"(
#version 450

layout (local_size_x = 16, local_size_y = 16) in;

layout (push_constant) uniform Extents {
	vec2 ref_extent;
	vec2 uvs_extent;
	vec2 colors_extent;
};

layout (binding = 0) uniform sampler2D reference;
layout (binding = 1) uniform sampler2D uvs;
layout (binding = 2) uniform sampler2D colors;

layout (binding = 3, rgba32f) uniform writeonly image2D dcolors;

void main()
{
	vec2 uv = gl_GlobalInvocationID.xy/ref_extent;

	vec2 din_uv = texture(uvs, uv).xy;
	vec3 color = texture(colors, din_uv).xyz;
	vec3 target = texture(reference, uv).xyz;

	vec3 dcolor = 2 * (target - color)/(colors_extent.x * colors_extent.y);
}
)";

int main()
{
	auto source = ivy::Texture::load(IVY_ROOT "/data/textures/cornell_box.png");
	fmt::println("source: {} x {}", source.width, source.height);

	auto reference = Texture <glm::vec3> ::from(source);

	auto din = DIn::from(128, 128, 32, 32);

	reference.as_texture().save("reference.png");

	din.uvs.as_texture().save("uvs.png");
	din.colors.as_texture().save("initial.png");

	{
		auto tex = din.render(reference.width, reference.height);
		tex.as_texture().save("renderer.png");
	}

	// Vulkan porting
	auto vrb = ivy::exec::prepare_vulkan_resource_base();

	vk::RenderPass render_pass = littlevk::RenderPassAssembler(vrb.device, vrb.dal)
		.add_attachment(littlevk::default_color_attachment(vk::Format::eR32G32B32A32Sfloat))
		.add_subpass(vk::PipelineBindPoint::eGraphics)
			.color_attachment(0, vk::ImageLayout::eColorAttachmentOptimal)
			.done();

	littlevk::Image render_target = littlevk::bind(vrb.device, vrb.memory_properties, vrb.dal)
			.image(reference.width, reference.height,
				vk::Format::eR32G32B32A32Sfloat,
				// vrb.swapchain.format,
				vk::ImageUsageFlagBits::eTransferSrc
				| vk::ImageUsageFlagBits::eColorAttachment
				| vk::ImageUsageFlagBits::eSampled,
				vk::ImageAspectFlagBits::eColor);

	littlevk::Buffer staging_render_target = littlevk::bind(vrb.device, vrb.memory_properties, vrb.dal)
			.buffer(render_target.device_size(), vk::BufferUsageFlagBits::eTransferDst);

	littlevk::FramebufferGenerator generator(vrb.device, render_pass, render_target.extent, vrb.dal);
	generator.add(render_target.view);

	auto framebuffers = generator.unpack();

	auto bundle = littlevk::ShaderStageBundle(vrb.device, vrb.dal)
		.attach(standalone::readfile(IVY_ROOT "/shaders/splat.vert"), vk::ShaderStageFlagBits::eVertex)
		.attach(renderer, vk::ShaderStageFlagBits::eFragment);

	// TODO: infer from argument to a render_once function
	static constexpr auto rendering_dslbs = std::array <vk::DescriptorSetLayoutBinding, 2> {{
		{ 0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },
		{ 1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },
	}};

	littlevk::Pipeline ppl = littlevk::PipelineAssembler <littlevk::eGraphics> (vrb.device, vrb.window, vrb.dal)
		.with_render_pass(render_pass, 0)
		.with_shader_bundle(bundle)
		.with_dsl_bindings(rendering_dslbs);

	vk::DescriptorSet dset = littlevk::bind(vrb.device, vrb.descriptor_pool)
		.allocate_descriptor_sets(*ppl.dsl).front();

	auto render_function = [&](const vk::CommandBuffer &cmd) {
		littlevk::viewport_and_scissor(cmd, render_target.extent);

		// Begin the render pass
		const auto &rpbi = littlevk::default_rp_begin_info <2>
			(render_pass, framebuffers[0], render_target.extent)
			.clear_color(0, std::array <float, 4> { 1.0f, 1.0f, 1.0f, 1.0f });

		cmd.beginRenderPass(rpbi, vk::SubpassContents::eInline);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ppl.handle);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, ppl.layout, 0, dset, {});
		cmd.draw(6, 1, 0, 0);

		// End the current render pass
		cmd.endRenderPass();

		littlevk::transition(cmd, render_target, vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eTransferSrcOptimal);
		littlevk::copy_image_to_buffer(cmd, render_target, staging_render_target, vk::ImageLayout::eTransferSrcOptimal);
	};

	// TODO: direct typed texture -> image...
	littlevk::Image din_uvs;
	littlevk::Image din_colors;

	std::tie(din_uvs, din_colors) = littlevk::bind(vrb.device, vrb.memory_properties, vrb.dal)
			.image(din.uvs.width, din.uvs.height,
				vk::Format::eR32G32B32A32Sfloat,
				vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
				vk::ImageAspectFlagBits::eColor)
			.image(din.colors.width, din.colors.height,
				vk::Format::eR32G32B32A32Sfloat,
				vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
				vk::ImageAspectFlagBits::eColor);

	littlevk::Buffer staging_uvs;
	littlevk::Buffer staging_colors;

	// TODO: lift the buffers...
	auto lift_v2 = [](const glm::vec2 &v) { return glm::vec4(v, 0, 0); };
	auto lift_v3 = [](const glm::vec3 &v) { return glm::vec4(v, 0); };
	auto trim_v4 = [](const glm::vec4 &v) { return glm::vec3(v); };

	std::tie(staging_uvs, staging_colors) = littlevk::bind(vrb.device, vrb.memory_properties, vrb.dal)
			.buffer(transform <glm::vec4> (din.uvs, lift_v2).pixels, vk::BufferUsageFlagBits::eTransferSrc)
			.buffer(transform <glm::vec4> (din.colors, lift_v3).pixels, vk::BufferUsageFlagBits::eTransferSrc);

	vk::Sampler sampler = littlevk::SamplerAssembler(vrb.device, vrb.dal);

	littlevk::bind(vrb.device, dset, rendering_dslbs)
		.update(0, 0, sampler, din_uvs.view, vk::ImageLayout::eShaderReadOnlyOptimal)
		.update(1, 0, sampler, din_colors.view, vk::ImageLayout::eShaderReadOnlyOptimal)
		.finalize();

	// TODO: render graph node system? via submit instead?
	littlevk::submit_now(vrb.device, vrb.command_pool, vrb.graphics_queue,
		[&](const vk::CommandBuffer &cmd) {
			littlevk::transition(cmd, din_uvs, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
			littlevk::transition(cmd, din_colors, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

			littlevk::copy_buffer_to_image(cmd, din_uvs, staging_uvs, vk::ImageLayout::eTransferDstOptimal);
			littlevk::copy_buffer_to_image(cmd, din_colors, staging_colors, vk::ImageLayout::eTransferDstOptimal);

			littlevk::transition(cmd, din_uvs, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
			littlevk::transition(cmd, din_colors, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
		}
	);

	littlevk::submit_now(vrb.device, vrb.command_pool, vrb.graphics_queue, render_function);

	auto render_target_texture = Texture <glm::vec4> ::from(reference.width, reference.height);
	littlevk::download(vrb.device, staging_render_target, render_target_texture.pixels);
	// transform <glm::vec3> (render_target_texture, trim_v4).as_texture().save("gpu-render.png");
	render_target_texture.as_texture().save("gpu-render.png");

	fmt::println("pixel 0: {}", glm::to_string(render_target_texture.pixels[0]));

	// optimize(din, reference);
	//
	// {
	// 	auto tex = din.render(reference.width, reference.height);
	// 	tex.as_texture().save("after.png");
	// }

	littlevk::submit_now(vrb.device, vrb.command_pool, vrb.graphics_queue,
		[&](const vk::CommandBuffer &cmd) {
			for (const auto &image : vrb.swapchain.images)
				littlevk::transition(cmd, image, vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);
		}
	);

	// size_t frame = 0;
	// while (vrb.valid_window()) {
	// 	glfwPollEvents();
	//
	// 	auto [cmd, op] = vrb.new_frame(frame).value();
	// 	render_function(cmd);
	// 	vrb.end_frame(cmd, frame);
	// 	vrb.present_frame(op, frame);
	// 	frame = 1 - frame;
	// }

	struct backward_push_constants {
		glm::vec2 ref_extent;
		glm::vec2 uvs_extent;
		glm::vec2 colors_extent;
	};

	static constexpr auto render_backwards_dslbs = std::array <vk::DescriptorSetLayoutBinding, 3> {{
		{ 0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eCompute },
		{ 1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eCompute },
		{ 2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eCompute },
	}};

	littlevk::Pipeline backward_ppl = [vrb]()
	{
		auto bundle = littlevk::ShaderStageBundle(vrb.device, vrb.dal)
			.attach(renderer_backwards, vk::ShaderStageFlagBits::eCompute);

		return littlevk::PipelineAssembler <littlevk::eCompute> (vrb.device, vrb.dal)
			.with_push_constant <backward_push_constants> (vk::ShaderStageFlagBits::eCompute)
			.with_dsl_bindings(render_backwards_dslbs)
			.with_shader_bundle(bundle);
	} ();

	// TODO: replace with textureSize and imageSize
	backward_push_constants bpc;
	bpc.ref_extent = { reference.width, reference.height };
	bpc.uvs_extent = { din.uvs.width, din.uvs.height };
	bpc.colors_extent = { din.colors.width, din.colors.height };

	vk::DescriptorSet backward_dset = littlevk::bind(vrb.device, vrb.descriptor_pool)
		.allocate_descriptor_sets(*backward_ppl.dsl).front();

	littlevk::bind(vrb.device, backward_dset, render_backwards_dslbs)
		.update(0, 0, sampler, render_target.view, vk::ImageLayout::eShaderReadOnlyOptimal)
		.update(1, 0, sampler, din_uvs.view, vk::ImageLayout::eShaderReadOnlyOptimal)
		.update(2, 0, sampler, din_colors.view, vk::ImageLayout::eShaderReadOnlyOptimal)
		.finalize();

	littlevk::submit_now(vrb.device, vrb.command_pool, vrb.graphics_queue,
		[&](const vk::CommandBuffer &cmd) {
			littlevk::transition(cmd, render_target, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, backward_ppl.handle);
			cmd.pushConstants <backward_push_constants> (backward_ppl.layout, vk::ShaderStageFlagBits::eCompute, 0, bpc);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, backward_ppl.layout, 0, backward_dset, {});
			cmd.dispatch(reference.width, reference.height, 1);
		}
	);

	vrb.device.waitIdle();
	vrb.destroy();
}

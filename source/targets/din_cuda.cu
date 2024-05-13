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

int main()
{

}

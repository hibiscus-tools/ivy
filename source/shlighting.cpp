#include <glm/gtc/constants.hpp>

#include <microlog/microlog.h>

#include "shlighting.hpp"

SHLighting SHLighting::from(const Texture &tex)
{
	std::vector <glm::vec3> rgb = tex.as_rgb();

	float dt_dp = glm::pi <float> ();
	dt_dp = (2 * dt_dp * dt_dp)/rgb.size();

	glm::vec3 vY0;
	glm::vec3 vY1[3];
	glm::vec3 vY2[5];

	vY0 = glm::vec3(0.0f);
	vY1[0] = vY1[1] = vY1[2] = glm::vec3(0.0f);
	vY2[0] = vY2[1] = vY2[2] = vY2[3] = vY2[4] = glm::vec3(0.0f);

	for (int i = 0; i < tex.width; i++) {
		for (int j = 0; j < tex.height; j++) {
			uint32_t index = i * tex.height + j;

			// Convert to unit direction
			float u = i/float(tex.width);
			float v = j/float(tex.height);

			float theta = u * glm::pi <float> ();
			float phi = v * glm::two_pi <float> ();

			float sin_theta = sin(theta);

			glm::vec3 n {
				sin_theta * cos(phi),
				sin_theta * sin(phi),
				cos(theta)
			};

			// Convolve with the spherical harmonics
			float Y0 = 0.282095f;

			constexpr float cc0 = 0.488603f;
			float Y1[] = {
				cc0 * n.x,
				cc0 * n.z,
				cc0 * n.y
			};

			constexpr float cc1 = 1.092548f;
			constexpr float cc2 = 0.315392f;
			constexpr float cc3 = 0.546274f;
			float Y2[] = {
				cc3 * (n.x * n.x - n.y * n.y),
				cc1 * n.x * n.z,
				cc2 * (3 * n.z * n.z - 1),
				cc1 * n.y * n.z,
				cc1 * n.x * n.y
			};

			const glm::vec3 &radiance = rgb[index];

			vY0 += radiance * Y0 * sin_theta * dt_dp;

			vY1[0] += radiance * Y1[0] * sin_theta * dt_dp;
			vY1[1] += radiance * Y1[1] * sin_theta * dt_dp;
			vY1[2] += radiance * Y1[2] * sin_theta * dt_dp;

			vY2[0] += radiance * Y2[0] * sin_theta * dt_dp;
			vY2[1] += radiance * Y2[1] * sin_theta * dt_dp;
			vY2[2] += radiance * Y2[2] * sin_theta * dt_dp;
			vY2[3] += radiance * Y2[3] * sin_theta * dt_dp;
			vY2[4] += radiance * Y2[4] * sin_theta * dt_dp;
		}
	}

	ulog_info("sh lighting", "vY0: %f, %f, %f\n", vY0.x, vY0.y, vY0.z);

	constexpr float c1 = 0.429043f;
	constexpr float c2 = 0.511664f;
	constexpr float c3 = 0.743125f;
	constexpr float c4 = 0.886227f;
	constexpr float c5 = 0.247708f;

	glm::mat4 Ms[3];

	for (uint32_t i = 0; i < 3; i++) {
		float L00 = vY0[i];

		float L1p1 = vY1[0][i];
		float L1e0 = vY1[1][i];
		float L1m1 = vY1[2][i];

		float L2p2 = vY2[0][i];
		float L2p1 = vY2[1][i];
		float L2e0 = vY2[2][i];
		float L2m1 = vY2[3][i];
		float L2m2 = vY2[4][i];

		Ms[i] = glm::mat4 {
			c1 * L2p2, c1 * L2m2, c1 * L2p1, c2 * L1p1,
			c1 * L2m2, -c1 * L2p2, c1 * L2m1, c2 * L1m1,
			c1 * L2p1, c1 * L2m1, c3 * L2e0, c2 * L1e0,
			c2 * L1p1, c2 * L1m1, c2 * L1e0, c4 * L00 - c5 * L2e0
		};
	}

	return { Ms[0], Ms[1], Ms[2] };
}
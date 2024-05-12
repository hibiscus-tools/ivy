#include <unordered_map>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include "core/mesh.hpp"

// Mesh functions
std::vector <float> interleave_attributes(const Mesh &m)
{
	// TODO: size check beforehand
	std::vector <float> attributes;
	for (size_t i = 0; i < m.positions.size(); i++) {
		attributes.push_back(m.positions[i].x);
		attributes.push_back(m.positions[i].y);
		attributes.push_back(m.positions[i].z);

		attributes.push_back(m.normals[i].x);
		attributes.push_back(m.normals[i].y);
		attributes.push_back(m.normals[i].z);

		attributes.push_back(m.uvs[i].x);
		attributes.push_back(m.uvs[i].y);
	}

	return attributes;
}

std::vector <glm::vec3> smooth_normals(const Mesh &mesh)
{
	std::vector <glm::vec3> normals(mesh.positions.size(), glm::vec3(0.0f));
	for (size_t i = 0; i < mesh.triangles.size(); i++) {
		const glm::uvec3 &t = mesh.triangles[i];

		const glm::vec3 &v0 = mesh.positions[t.x];
		const glm::vec3 &v1 = mesh.positions[t.y];
		const glm::vec3 &v2 = mesh.positions[t.z];

		glm::vec3 n = glm::cross(v1 - v0, v2 - v0);

		normals[t.x] += n;
		normals[t.y] += n;
		normals[t.z] += n;
	}

	for (glm::vec3 &n : normals) {
		float l = glm::length(n);
		if (l > 0)
			n /= l;
	}

	return normals;
}

Mesh deduplicate(const Mesh &mesh)
{
	std::vector <glm::vec3> positions;
	std::vector <glm::vec3> normals;
	std::vector <glm::vec2> uvs;

	std::vector <glm::uvec3> triangles;

	std::unordered_map <glm::vec3, uint32_t> vmap;

	auto push = [&](const glm::vec3 &v, const glm::vec3 &n, const glm::vec2 &p) -> uint32_t {
		if (vmap.count(v))
			return vmap[v];

		uint32_t size = positions.size();
		positions.push_back(v);
		normals.push_back(n);
		uvs.push_back(p);
		vmap[v] = size;

		return size;
	};

	for (const auto &t : mesh.triangles) {
		uint32_t tx = push(mesh.positions[t.x], mesh.normals[t.x], mesh.uvs[t.x]);
		uint32_t ty = push(mesh.positions[t.y], mesh.normals[t.y], mesh.uvs[t.y]);
		uint32_t tz = push(mesh.positions[t.z], mesh.normals[t.z], mesh.uvs[t.z]);
		triangles.emplace_back(tx, ty, tz);
	}

	return { positions, normals, uvs, triangles };
}

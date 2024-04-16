#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;

layout (binding = 0) uniform sampler2D diffuse_texture;

// Spherical harmonics lighting
layout (binding = 1) uniform SHLihgting {
	mat4 Mred;
	mat4 Mgreen;
	mat4 Mblue;
} shl;

layout (binding = 2) uniform Materials {
	vec3 albedo;
	vec3 specular;

	int has_albedo_texture;
} material;

layout (location = 0) out vec4 fragment;

// Tone mapping
vec3 aces(vec3 x)
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
	vec3 albedo = material.albedo;
	if (material.has_albedo_texture != 0) {
		vec4 f = texture(diffuse_texture, uv);
		if (f.a < 0.5)
			discard;

		albedo = f.xyz;
	}

	// TODO: a mix between interpolated and numerical normals? for auto sharpness
	vec4 N = vec4(normal, 1);
	float r = dot(N, shl.Mred * N);
	float g = dot(N, shl.Mgreen * N);
	float b = dot(N, shl.Mblue * N);
	vec3 C = vec3(r, g, b) * albedo;

	fragment = vec4(aces(C), 1.0);
}
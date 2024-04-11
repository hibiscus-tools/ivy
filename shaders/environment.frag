#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;

layout (binding = 0) uniform sampler2D diffuse_texture;

// Spherical harmonics lighting
layout (binding = 1) uniform UniformBufferObject {
	mat4 Mred;
	mat4 Mgreen;
	mat4 Mblue;
};

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
	vec4 albedo = texture(diffuse_texture, uv);
	if (albedo.a < 0.5)
		discard;

	// TODO: transform the normals
	vec3 dU = dFdx(position);
	vec3 dV = dFdyFine(position);
	vec3 N = normalize(cross(dU, dV));

	vec4 N1 = vec4(N, 1);
	float r = dot(N1, Mred * N1);
	float g = dot(N1, Mgreen * N1);
	float b = dot(N1, Mblue * N1);
	vec3 C = vec3(r, g, b) * albedo.xyz;

	fragment = vec4(aces(C), 1.0);
}
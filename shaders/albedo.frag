#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;

layout (location = 0) out vec4 fragment;

layout (binding = 0) uniform sampler2D diffuse_texture;

void main()
{
	vec4 albedo = texture(diffuse_texture, uv);
	if (albedo.a < 0.5)
		discard;

	fragment = vec4(albedo.xyz, 1);
}
#version 450

layout (input_attachment_index = 0, binding = 0) uniform subpassInput depth;

layout (location = 0) in vec2 uv;

layout (push_constant) uniform PushConstants {
	vec3 origin;
	vec3 lower_left;
	vec3 horizontal;
	vec3 vertical;
	float near;
	float far;
};

layout (location = 0) out vec4 fragment;
layout (location = 1) out float depth_again;

struct Sphere {
	vec3 origin;
	float radius;
};

struct Box {
	vec3 origin;
	vec3 hsize;
};

float sdf(Sphere sphere, vec3 p)
{
	return distance(p, sphere.origin) - sphere.radius;
}

float sdf(Box box, vec3 p)
{
	vec3 q = abs(p - box.origin) - box.hsize;
	return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

#define PI 3.1415926535897932384626433832795

void main()
{
	// Sphere sphere = Sphere(vec3(0, 200, 0), 100.0);
	Box box = Box(vec3(0, 200, 0), vec3(100));

	float s = 1e10f;

	vec3 ray = normalize(lower_left + horizontal * uv.x + vertical * (1.0 - uv.y) - origin);

	vec3 p = origin;
	for (uint i = 0; i < 256; i++) {
		s = sdf(box, p);
		if (abs(s) < 1e-3f) {
			vec3 off = 2 * PI * (p - box.origin)/box.hsize;
			float x = float(sin(off.x * 8) > 0.9);
			float y = float(sin(off.y * 8) > 0.9);
			float z = float(sin(off.z * 8) > 0.9);
			if (x > 0 || y > 0 || z > 0)
				break;

			/* vec3 n = normalize(p - vec3(0, 200, 0));
			float phi = mod(PI + atan(n.z, n.x), 2 * PI);
			float theta = PI - acos(n.y);

			float h = float(sin(phi * 25) > 0.9);
			float v = float(sin(theta * 35) > 0.9);
			if (h > 0 || v > 0)
				break; */

			p += 0.01 * ray;
			continue;
		}

		p += abs(s) * ray;
	}

	float d = subpassLoad(depth).x;
	float linearized = near * far / (far + d * (near - far));
	if (abs(s) > 1e-3f)
		discard;

	if (length(origin - p) > linearized)
		discard;

	vec3 albedo = vec3(0.5, 0.8, 0.5);
	fragment = vec4(albedo, 1);

	depth_again = length(origin - p);
}

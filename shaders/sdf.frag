#version 450

layout (location = 0) in vec2 uv;

layout (push_constant) uniform PushConstants {
	vec3 origin;
	vec3 lower_left;
	vec3 horizontal;
	vec3 vertical;
};

layout (location = 0) out vec4 fragment;

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

void main()
{
	Sphere sphere = Sphere(vec3(0, 2, 0), 10.0);
	// Box box = Box(vec3(0, 2, 0), vec3(5, 10, 2));

	float s = 1e10f;

	vec3 ray = normalize(lower_left + horizontal * uv.x + vertical * (1.0 - uv.y) - origin);

	vec3 p = origin;
	for (uint i = 0; i < 256; i++) {
		s = sdf(sphere, p);
		if (abs(s) < 1e-3f)
			break;

		p += abs(s) * ray;
	}

	if (abs(s) > 1e-3f)
		discard;

	vec3 albedo = vec3(0.5, 0.8, 0.5);
	fragment = vec4(albedo, 1);

	// TODO: depth buffer to write to as well
}

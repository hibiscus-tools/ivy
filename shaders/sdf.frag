#version 450

layout (push_constant) uniform PushConstants {
	vec3 origin;
	vec3 lower_left;
	vec3 horizontal;
	vec3 vertical;
};

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 fragment;

struct Sphere {
	vec3 origin;
	float radius;
};

struct Box {
	vec3 origin;
	vec3 hextent;
};

float sdf(Sphere sphere, vec3 p)
{
	return distance(p, sphere.origin) - sphere.radius;
}

float sdf(Box box, vec3 p)
{
	vec3 q = abs(p - box.origin) - box.hextent;
	return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float smoothmin(float a, float b, float k)
{
	float h = max(k - abs(a - b), 0)/k;
	return min(a, b) - k * h * h * h/6.0;
}

float sdf(vec3 p)
{
	Sphere sphere1 = Sphere(vec3(0, 11, 0), 10.0);
	Sphere sphere2 = Sphere(vec3(0, -11, 0), 10.0);
	Box box1 = Box(vec3(0, -7, 5), vec3(5));

	return smoothmin(sdf(sphere1, p), sdf(box1, p), 10.0);
}

void main()
{
	vec3 ray = normalize(lower_left + horizontal * uv.x + vertical * (1.0 - uv.y) - origin);
	
	vec3 p = origin;
	for (uint i = 0; i < 256; i++) {
		float s = sdf(p);
		if (abs(s) < 1e-3)
			break;
		p += s * ray;
	}

	float epsilon = 1e-3f;
	
	vec3 p100 = p + epsilon * vec3(1, 0, 0);
	vec3 m100 = p - epsilon * vec3(1, 0, 0);
	float dsx = (sdf(p100) - sdf(m100))/(2 * epsilon);
	
	vec3 p010 = p + epsilon * vec3(0, 1, 0);
	vec3 m010 = p - epsilon * vec3(0, 1, 0);
	float dsy = (sdf(p010) - sdf(m010))/(2 * epsilon);
	
	vec3 p001 = p + epsilon * vec3(0, 0, 1);
	vec3 m001 = p - epsilon * vec3(0, 0, 1);
	float dsz = (sdf(p001) - sdf(m001))/(2 * epsilon);

	float s = sdf(p);

	vec3 n = vec3(dsx, dsy, dsz);
	vec3 l = normalize(vec3(1, 1, 1));

	if (abs(s) < 1e-3f) {
		vec3 albedo = vec3(0.6, 0.5, 0.9);
		// fragment = vec4(0.5 + 0.5 * n, 0);
		fragment = vec4(albedo * (0.1 + 0.9 * max(0, dot(n, l))), 0);
	} else {
		discard;
	}
}
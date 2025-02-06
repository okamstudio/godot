layout(push_constant, std430) uniform Params {
	ivec2 size;
	float blur_size;
	float blur_scale;

	mat2 proj_zw; // Bottom-right 2x2 corner of the projection matrix with reverse-z and z-remap applied

	int blur_steps;
	bool blur_near_active;
	float blur_near_begin;
	float blur_near_end;

	bool blur_far_active;
	float blur_far_begin;
	float blur_far_end;
	bool second_pass;

	bool half_size;
	bool use_jitter;
	float jitter_seed;
	bool use_physical_near;

	bool use_physical_far;
	float blur_size_near;
	float blur_size_far;
	uint pad[1];
}
params;

//used to work around downsampling filter
#define DEPTH_GAP 0.0

const float GOLDEN_ANGLE = 2.39996323;

//note: uniform pdf rand [0;1[
float hash12n(vec2 p) {
	p = fract(p * vec2(5.3987, 5.4421));
	p += dot(p.yx, p.xy + vec2(21.5351, 14.3137));
	return fract(p.x * p.y * 95.4307);
}

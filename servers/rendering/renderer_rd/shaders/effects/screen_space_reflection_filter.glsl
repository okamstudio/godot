#[compute]

#version 450

#VERSION_DEFINES

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(rgba16f, set = 0, binding = 0) uniform restrict readonly image2D source_ssr;
layout(r8, set = 0, binding = 1) uniform restrict readonly image2D source_radius;
layout(rgba8, set = 1, binding = 0) uniform restrict readonly image2D source_normal;

layout(rgba16f, set = 2, binding = 0) uniform restrict writeonly image2D dest_ssr;
#ifndef VERTICAL_PASS
layout(r8, set = 2, binding = 1) uniform restrict writeonly image2D dest_radius;
#endif
layout(r32f, set = 3, binding = 0) uniform restrict readonly image2D source_depth;

layout(push_constant, std430) uniform Params {
	vec4 proj_info;

	bool orthogonal;
	float edge_tolerance;
	int increment;
	uint view_index;

	ivec2 screen_size;
	bool vertical;
	uint steps;

	// Used to add break up samples over multiple frames. Value is an integer from 0 to taa_phase_count -1.
	float taa_frame_count;
	uint pad0;
	uint pad1;
	uint pad2;
}
params;

#include "screen_space_reflection_inc.glsl"

#define GAUSS_TABLE_SIZE 15

const float gauss_table[GAUSS_TABLE_SIZE + 1] = float[](
		0.1847392078702266,
		0.16595854345772326,
		0.12031364177766891,
		0.07038755277896766,
		0.03322925565155569,
		0.012657819729901945,
		0.0038903040680094217,
		0.0009646503390864025,
		0.00019297087402915717,
		0.000031139936308099136,
		0.000004053309048174758,
		4.255228059965837e-7,
		3.602517634249573e-8,
		2.4592560765896795e-9,
		1.3534945386863618e-10,
		0.0 //one more for interpolation
);

float gauss_weight(float p_val) {
	float idxf;
	float c = modf(max(0.0, p_val * float(GAUSS_TABLE_SIZE)), idxf);
	int idx = int(idxf);
	if (idx >= GAUSS_TABLE_SIZE + 1) {
		return 0.0;
	}

	return mix(gauss_table[idx], gauss_table[idx + 1], c);
}

#define M_PI 3.14159265359

// Interleaved Gradient Noise
// https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
float quick_hash(vec2 pos) {
	const vec3 magic = vec3(0.06711056f, 0.00583715f, 52.9829189f);
	return fract(magic.z * fract(dot(pos, magic.xy)));
}

void do_filter(inout vec4 accum, inout float accum_radius, inout float divisor, ivec2 texcoord, ivec2 increment, vec3 p_pos, vec3 normal, float p_limit_radius) {
	for (int i = 1; i < params.steps; i++) {
		float d = float(i * params.increment);
		ivec2 tc = texcoord + increment * i;
		float depth = imageLoad(source_depth, tc).r;
		vec3 view_pos = reconstructCSPosition(vec2(tc) + 0.5, depth);
		vec3 view_normal = normalize(imageLoad(source_normal, tc).rgb * 2.0 - 1.0);
		view_normal.y = -view_normal.y;

		float r = imageLoad(source_radius, tc).r;
		float radius = round(r * 255.0);

		float angle_n = 1.0 - abs(dot(normal, view_normal));
		if (angle_n > params.edge_tolerance) {
			break;
		}

		float angle = abs(dot(normal, normalize(view_pos - p_pos)));

		if (angle > params.edge_tolerance) {
			break;
		}

		if (d < radius) {
			float w = gauss_weight(d / radius);
			accum += imageLoad(source_ssr, tc) * w;
#ifndef VERTICAL_PASS
			accum_radius += r * w;
#endif
			divisor += w;
		}
	}
}

void main() {
	// Pixel being shaded
	ivec2 ssC = ivec2(gl_GlobalInvocationID.xy);

	if (any(greaterThanEqual(ssC.xy, params.screen_size))) { //too large, do nothing
		return;
	}

	float base_contrib = gauss_table[0];

	vec4 accum = imageLoad(source_ssr, ssC);

	float accum_radius = imageLoad(source_radius, ssC).r;
	float radius = accum_radius * 255.0;

	float divisor = gauss_table[0];
	accum *= divisor;
	accum_radius *= divisor;

	// The intent is to spread the increment on Low quality (3) to be between 1 and 5.
	// On Medium quality (2), it would be between 1 and 3.
	//
	// See https://github.com/godotengine/godot/blob/ad2722f48180c21147a31ee59e15722a237a7445/servers/rendering/renderer_rd/effects/ss_effects.cpp#L1535-L1544
	// for values of `params.increment` depending on quality.
	int increment_offset = params.increment - 1;
	int taa_jitter = int(quick_hash(vec2(ssC.xy) + vec2(params.taa_frame_count * 5.0, params.taa_frame_count * 2.0)) * (params.increment + increment_offset) - increment_offset);

#ifdef VERTICAL_PASS
	ivec2 direction = ivec2(0, params.increment + taa_jitter);
#else
	ivec2 direction = ivec2(params.increment + taa_jitter, 0);
#endif
	float depth = imageLoad(source_depth, ssC).r;
	vec3 pos = reconstructCSPosition(vec2(ssC.xy) + 0.5, depth);
	vec3 normal = imageLoad(source_normal, ssC).xyz * 2.0 - 1.0;
	normal = normalize(normal);
	normal.y = -normal.y;

	do_filter(accum, accum_radius, divisor, ssC.xy, direction, pos, normal, radius);
	do_filter(accum, accum_radius, divisor, ssC.xy, -direction, pos, normal, radius);

	if (divisor > 0.0) {
		accum /= divisor;
		accum_radius /= divisor;
	} else {
		accum = vec4(0.0);
		accum_radius = 0.0;
	}

	imageStore(dest_ssr, ssC, accum);

#ifndef VERTICAL_PASS
	imageStore(dest_radius, ssC, vec4(accum_radius));
#endif
}

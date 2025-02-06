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

	uint pad;
	float edge_tolerance;
	int increment;
	uint view_index;

	ivec2 screen_size;
	bool vertical;
	uint steps;
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

vec3 reconstructCSPosition(vec2 screen_pos, float view_z) {
	if (sc_multiview) {
		screen_pos = screen_pos / vec2(params.screen_size) * 2.0 - 1.0;
		vec4 ndc = vec4(screen_pos, z_ndc_from_view(screen_pos, view_z), 1.0);
		vec4 view = scene_data.inv_projection[params.view_index] * ndc;
		return view.xyz / view.w;
	} else {
		return vec3(-(screen_pos.xy * params.proj_info.xy + params.proj_info.zw) * (scene_data.projection[params.view_index][2][3] * view_z + scene_data.projection[params.view_index][3][3]), view_z);
	}
}

#define M_PI 3.14159265359

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
#ifdef VERTICAL_PASS
	ivec2 direction = ivec2(0, params.increment);
#else
	ivec2 direction = ivec2(params.increment, 0);
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

/* clang-format off */
#[vertex]

#version 450

#VERSION_DEFINES

#include "bokeh_dof_inc.glsl"

layout(location = 0) out vec2 uv_interp;
/* clang-format on */

void main() {
	// old code, ARM driver bug on Mali-GXXx GPUs and Vulkan API 1.3.xxx
	// https://github.com/godotengine/godot/pull/92817#issuecomment-2168625982
	//vec2 base_arr[3] = vec2[](vec2(-1.0, -1.0), vec2(-1.0, 3.0), vec2(3.0, -1.0));
	//gl_Position = vec4(base_arr[gl_VertexIndex], 0.0, 1.0);
	//uv_interp = clamp(gl_Position.xy, vec2(0.0, 0.0), vec2(1.0, 1.0)) * 2.0; // saturate(x) * 2.0

	vec2 vertex_base;
	if (gl_VertexIndex == 0) {
		vertex_base = vec2(-1.0, -1.0);
	} else if (gl_VertexIndex == 1) {
		vertex_base = vec2(-1.0, 3.0);
	} else {
		vertex_base = vec2(3.0, -1.0);
	}
	gl_Position = vec4(vertex_base, 0.0, 1.0);
	uv_interp = clamp(vertex_base, vec2(0.0, 0.0), vec2(1.0, 1.0)) * 2.0; // saturate(x) * 2.0
}

/* clang-format off */
#[fragment]

#version 450

#VERSION_DEFINES

#include "bokeh_dof_inc.glsl"

layout(location = 0) in vec2 uv_interp;
/* clang-format on */

#ifdef MODE_GEN_BLUR_SIZE
layout(location = 0) out float weight;

layout(set = 0, binding = 0) uniform sampler2D source_depth;
#else
layout(location = 0) out vec4 frag_color;
#ifdef OUTPUT_WEIGHT
layout(location = 1) out float weight;
#endif

layout(set = 0, binding = 0) uniform sampler2D source_color;
layout(set = 1, binding = 0) uniform sampler2D source_weight;
#ifdef MODE_COMPOSITE_BOKEH
layout(set = 2, binding = 0) uniform sampler2D original_weight;
#endif
#endif

//DOF
// Bokeh single pass implementation based on https://tuxedolabs.blogspot.com/2018/05/bokeh-depth-of-field-in-single-pass.html

#ifdef MODE_GEN_BLUR_SIZE

float get_depth_at_pos(vec2 uv) {
	float depth = textureLod(source_depth, uv, 0.0).x * 2.0 - 1.0;
	if (params.orthogonal) {
		depth = -(depth * (params.z_far - params.z_near) - (params.z_far + params.z_near)) / 2.0;
	} else {
		depth = 2.0 * params.z_near * params.z_far / (params.z_far + params.z_near + depth * (params.z_far - params.z_near));
	}
	return depth;
}

float get_blur_size(float depth) {
	if (params.blur_near_active && depth < params.blur_near_begin) {
		if (params.use_physical_near) {
			// Physically-based.
			float d = abs(params.blur_near_begin - depth);
			return -(d / (params.blur_near_begin - d)) * params.blur_size_near - DEPTH_GAP; // Near blur is negative.
		} else {
			// Non-physically-based.
			return -(1.0 - smoothstep(params.blur_near_end, params.blur_near_begin, depth)) * params.blur_size - DEPTH_GAP; // Near blur is negative.
		}
	}

	if (params.blur_far_active && depth > params.blur_far_begin) {
		if (params.use_physical_far) {
			// Physically-based.
			float d = abs(params.blur_far_begin - depth);
			return (d / (params.blur_far_begin + d)) * params.blur_size_far + DEPTH_GAP;
		} else {
			// Non-physically-based.
			return smoothstep(params.blur_far_begin, params.blur_far_end, depth) * params.blur_size + DEPTH_GAP;
		}
	}

	return 0.0;
}

#endif

#if defined(MODE_BOKEH_BOX) || defined(MODE_BOKEH_HEXAGONAL)

vec4 weighted_filter_dir(vec2 dir, vec2 uv, vec2 pixel_size) {
	dir *= pixel_size;
	vec4 color = texture(source_color, uv);
	color.a = texture(source_weight, uv).r;

	vec4 accum = color;
	float total = 1.0;

	float blur_scale = params.blur_size / float(params.blur_steps);

	if (params.use_jitter) {
		uv += dir * (hash12n(uv + params.jitter_seed) - 0.5);
	}

	for (int i = -params.blur_steps; i <= params.blur_steps; i++) {
		if (i == 0) {
			continue;
		}
		float radius = float(i) * blur_scale;
		vec2 suv = uv + dir * radius;
		radius = abs(radius);

		vec4 sample_color = texture(source_color, suv);
		sample_color.a = texture(source_weight, suv).r;
		float limit;

		if (sample_color.a < color.a) {
			limit = abs(sample_color.a);
		} else {
			limit = abs(color.a);
		}

		limit -= DEPTH_GAP;

		float m = smoothstep(radius - 0.5, radius + 0.5, limit);

		accum += mix(color, sample_color, m);

		total += 1.0;
	}

	return accum / total;
}

#endif

void main() {
	vec2 pixel_size = 1.0 / vec2(params.size);
	vec2 uv = uv_interp;

#ifdef MODE_GEN_BLUR_SIZE
	uv += pixel_size * 0.5;
	float center_depth = get_depth_at_pos(uv);
	weight = get_blur_size(center_depth);
#endif

#ifdef MODE_BOKEH_BOX
	//pixel_size*=0.5; //resolution is doubled
	if (params.second_pass || !params.half_size) {
		uv += pixel_size * 0.5; //half pixel to read centers
	} else {
		uv += pixel_size * 0.25; //half pixel to read centers from full res
	}

	float alpha = texture(source_color, uv).a; // retain this
	vec2 dir = (params.second_pass ? vec2(0.0, 1.0) : vec2(1.0, 0.0));

	vec4 color = weighted_filter_dir(dir, uv, pixel_size);

	frag_color = color;
	frag_color.a = alpha; // attempt to retain this in case we have a transparent background, ignored if half_size
#ifdef OUTPUT_WEIGHT
	weight = color.a;
#endif

#endif

#ifdef MODE_BOKEH_HEXAGONAL

	//pixel_size*=0.5; //resolution is doubled
	if (params.second_pass || !params.half_size) {
		uv += pixel_size * 0.5; //half pixel to read centers
	} else {
		uv += pixel_size * 0.25; //half pixel to read centers from full res
	}

	float alpha = texture(source_color, uv).a; // retain this

	vec2 dir = (params.second_pass ? normalize(vec2(1.0, 0.577350269189626)) : vec2(0.0, 1.0));

	vec4 color = weighted_filter_dir(dir, uv, pixel_size);

	if (params.second_pass) {
		dir = normalize(vec2(-1.0, 0.577350269189626));

		vec4 color2 = weighted_filter_dir(dir, uv, pixel_size);

		color.rgb = min(color.rgb, color2.rgb);
		color.a = (color.a + color2.a) * 0.5;
	}

	frag_color = color;
	frag_color.a = alpha; // attempt to retain this in case we have a transparent background, ignored if half_size
#ifdef OUTPUT_WEIGHT
	weight = color.a;
#endif

#endif

#ifdef MODE_BOKEH_CIRCULAR
	if (params.half_size) {
		pixel_size *= 0.5; //resolution is doubled
	}

	uv += pixel_size * 0.5; //half pixel to read centers

	vec4 color = texture(source_color, uv);
	float alpha = color.a; // retain this
	color.a = texture(source_weight, uv).r;

	vec4 color_accum = color;
	float accum = 1.0;

	float radius = params.blur_scale;
	for (float ang = 0.0; radius < params.blur_size; ang += GOLDEN_ANGLE) {
		vec2 uv_adj = uv + vec2(cos(ang), sin(ang)) * pixel_size * radius;

		vec4 sample_color = texture(source_color, uv_adj);
		sample_color.a = texture(source_weight, uv_adj).r;

		float limit = abs(sample_color.a);
		if (sample_color.a > color.a) {
			limit = clamp(limit, 0.0, abs(color.a) * 2.0);
		}

		limit -= DEPTH_GAP;

		float m = smoothstep(radius - 0.5, radius + 0.5, limit);
		color_accum += mix(color_accum / accum, sample_color, m);
		accum += 1.0;

		radius += params.blur_scale / radius;
	}

	color_accum = color_accum / accum;

	frag_color.rgb = color_accum.rgb;
	frag_color.a = alpha; // attempt to retain this in case we have a transparent background, ignored if half_size
#ifdef OUTPUT_WEIGHT
	weight = color_accum.a;
#endif

#endif

#ifdef MODE_COMPOSITE_BOKEH
	frag_color.rgb = texture(source_color, uv).rgb;

	float center_weight = texture(source_weight, uv).r;
	float sample_weight = texture(original_weight, uv).r;

	float mix_amount;
	if (sample_weight < center_weight) {
		mix_amount = min(1.0, max(0.0, max(abs(center_weight), abs(sample_weight)) - DEPTH_GAP));
	} else {
		mix_amount = min(1.0, max(0.0, abs(center_weight) - DEPTH_GAP));
	}

	// let alpha blending take care of mixing
	frag_color.a = mix_amount;
#endif
}

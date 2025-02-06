///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016, Intel Corporation
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of
// the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// File changes (yyyy-mm-dd)
// 2016-09-07: filip.strugar@intel.com: first commit
// 2020-12-05: clayjohn: convert to Vulkan and Godot
// 2021-05-27: clayjohn: convert SSAO to SSIL
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#[compute]

#version 450

#VERSION_DEFINES

#define SSIL_MAIN_DISK_SAMPLE_COUNT (32)
const vec4 sample_pattern[SSIL_MAIN_DISK_SAMPLE_COUNT] = {
	vec4(0.78488064, 0.56661671, 1.500000, -0.126083), vec4(0.26022232, -0.29575172, 1.500000, -1.064030), vec4(0.10459357, 0.08372527, 1.110000, -2.730563), vec4(-0.68286800, 0.04963045, 1.090000, -0.498827),
	vec4(-0.13570161, -0.64190155, 1.250000, -0.532765), vec4(-0.26193795, -0.08205118, 0.670000, -1.783245), vec4(-0.61177456, 0.66664219, 0.710000, -0.044234), vec4(0.43675563, 0.25119025, 0.610000, -1.167283),
	vec4(0.07884444, 0.86618668, 0.640000, -0.459002), vec4(-0.12790935, -0.29869005, 0.600000, -1.729424), vec4(-0.04031125, 0.02413622, 0.600000, -4.792042), vec4(0.16201244, -0.52851415, 0.790000, -1.067055),
	vec4(-0.70991218, 0.47301072, 0.640000, -0.335236), vec4(0.03277707, -0.22349690, 0.600000, -1.982384), vec4(0.68921727, 0.36800742, 0.630000, -0.266718), vec4(0.29251814, 0.37775412, 0.610000, -1.422520),
	vec4(-0.12224089, 0.96582592, 0.600000, -0.426142), vec4(0.11071457, -0.16131058, 0.600000, -2.165947), vec4(0.46562141, -0.59747696, 0.600000, -0.189760), vec4(-0.51548797, 0.11804193, 0.600000, -1.246800),
	vec4(0.89141309, -0.42090443, 0.600000, 0.028192), vec4(-0.32402530, -0.01591529, 0.600000, -1.543018), vec4(0.60771245, 0.41635221, 0.600000, -0.605411), vec4(0.02379565, -0.08239821, 0.600000, -3.809046),
	vec4(0.48951152, -0.23657045, 0.600000, -1.189011), vec4(-0.17611565, -0.81696892, 0.600000, -0.513724), vec4(-0.33930185, -0.20732205, 0.600000, -1.698047), vec4(-0.91974425, 0.05403209, 0.600000, 0.062246),
	vec4(-0.15064627, -0.14949332, 0.600000, -1.896062), vec4(0.53180975, -0.35210401, 0.600000, -0.758838), vec4(0.41487166, 0.81442589, 0.600000, -0.505648), vec4(-0.24106961, -0.32721516, 0.600000, -1.665244)
};

// these values can be changed (up to SSIL_MAX_TAPS) with no changes required elsewhere; values for 4th and 5th preset are ignored but array needed to avoid compilation errors
// the actual number of texture samples is two times this value (each "tap" has two symmetrical depth texture samples)
const int num_taps[5] = { 3, 5, 12, 0, 0 };

#define SSIL_TILT_SAMPLES_ENABLE_AT_QUALITY_PRESET (99) // to disable simply set to 99 or similar
#define SSIL_TILT_SAMPLES_AMOUNT (0.4)
//
#define SSIL_HALOING_REDUCTION_ENABLE_AT_QUALITY_PRESET (1) // to disable simply set to 99 or similar
#define SSIL_HALOING_REDUCTION_AMOUNT (0.8) // values from 0.0 - 1.0, 1.0 means max weighting (will cause artifacts, 0.8 is more reasonable)
//
#define SSIL_DEPTH_MIPS_ENABLE_AT_QUALITY_PRESET (2)
#define SSIL_DEPTH_MIPS_GLOBAL_OFFSET (-4.3) // best noise/quality/performance tradeoff, found empirically
//
// WARNING: The edge handling is hard-coded to 'disabled' on quality level 0, and enabled above,
// on the C++ side; while toggling it here will work for testing purposes, it will not yield
// performance gains (or correct results).
#define SSIL_DEPTH_BASED_EDGES_ENABLE_AT_QUALITY_PRESET (1)
//
#define SSIL_REDUCE_RADIUS_NEAR_SCREEN_BORDER_ENABLE_AT_QUALITY_PRESET (1)

#define SSIL_MAX_TAPS 32
#define SSIL_ADAPTIVE_TAP_BASE_COUNT 5
#define SSIL_ADAPTIVE_TAP_FLEXIBLE_COUNT (SSIL_MAX_TAPS - SSIL_ADAPTIVE_TAP_BASE_COUNT)
#define SSIL_DEPTH_MIP_LEVELS 4

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2DArray source_depth_mipmaps;
layout(rgba8, set = 0, binding = 1) uniform restrict readonly image2D source_normal;
layout(set = 0, binding = 2) uniform Constants { //get into a lower set
	vec4 rotation_matrices[20];
}
constants;

#ifdef ADAPTIVE
layout(rgba16, set = 1, binding = 0) uniform restrict readonly image2DArray source_ssil;
layout(set = 1, binding = 1) uniform sampler2D source_importance;
layout(set = 1, binding = 2, std430) buffer Counter {
	uint sum;
}
counter;
#endif

layout(rgba16, set = 2, binding = 0) uniform restrict writeonly image2D dest_image;
layout(r8, set = 2, binding = 1) uniform image2D edges_weights_image;

layout(set = 3, binding = 0) uniform sampler2D last_frame;
layout(set = 3, binding = 1) uniform ProjectionConstants {
	mat4 reprojection;
}
projection_constants;

layout(push_constant, std430) uniform Params {
	ivec2 screen_size;
	int pass;
	int quality;

	vec2 half_screen_pixel_size;
	vec2 half_screen_pixel_size_x025;

	vec2 NDC_to_view_mul;
	vec2 NDC_to_view_add;

	vec2 pad2;
	float z_near;
	float z_far;

	float radius;
	float intensity;
	int size_multiplier;
	int pad;

	float fade_out_mul;
	float fade_out_add;
	float normal_rejection_amount;
	float inv_radius_near_limit;

	bool is_orthogonal;
	float neg_inv_radius;
	float load_counter_avg_div;
	float adaptive_sample_limit;

	ivec2 pass_coord_offset;
	vec2 pass_uv_offset;
}
params;

float pack_edges(vec4 p_edgesLRTB) {
	p_edgesLRTB = round(clamp(p_edgesLRTB, 0.0, 1.0) * 3.05);
	return dot(p_edgesLRTB, vec4(64.0 / 255.0, 16.0 / 255.0, 4.0 / 255.0, 1.0 / 255.0));
}

vec3 NDC_to_view_space(vec2 p_pos, float p_viewspace_depth) {
	if (params.is_orthogonal) {
		return vec3((params.NDC_to_view_mul * p_pos.xy + params.NDC_to_view_add), p_viewspace_depth);
	} else {
		return vec3((params.NDC_to_view_mul * p_pos.xy + params.NDC_to_view_add) * p_viewspace_depth, p_viewspace_depth);
	}
}

// calculate effect radius and fit our screen sampling pattern inside it
void calculate_radius_parameters(const float p_pix_center_length, const vec2 p_pixel_size_at_center, out float r_lookup_radius, out float r_radius, out float r_fallof_sq) {
	r_radius = params.radius;

	// when too close, on-screen sampling disk will grow beyond screen size; limit this to avoid closeup temporal artifacts
	const float too_close_limit = clamp(p_pix_center_length * params.inv_radius_near_limit, 0.0, 1.0) * 0.8 + 0.2;

	r_radius *= too_close_limit;

	// 0.85 is to reduce the radius to allow for more samples on a slope to still stay within influence
	r_lookup_radius = (0.85 * r_radius) / p_pixel_size_at_center.x;

	// used to calculate falloff (both for AO samples and per-sample weights)
	r_fallof_sq = -1.0 / (r_radius * r_radius);
}

vec4 calculate_edges(const float p_center_z, const float p_left_z, const float p_right_z, const float p_top_z, const float p_bottom_z) {
	// slope-sensitive depth-based edge detection
	vec4 edgesLRTB = vec4(p_left_z, p_right_z, p_top_z, p_bottom_z) - p_center_z;
	vec4 edgesLRTB_slope_adjusted = edgesLRTB + edgesLRTB.yxwz;
	edgesLRTB = min(abs(edgesLRTB), abs(edgesLRTB_slope_adjusted));
	return clamp((1.3 - edgesLRTB / (p_center_z * 0.040)), 0.0, 1.0);
}

vec3 load_normal(ivec2 p_pos) {
	vec3 encoded_normal = normalize(imageLoad(source_normal, p_pos).xyz * 2.0 - 1.0);
	encoded_normal.z = -encoded_normal.z;
	return encoded_normal;
}

vec3 load_normal(ivec2 p_pos, ivec2 p_offset) {
	vec3 encoded_normal = normalize(imageLoad(source_normal, p_pos + p_offset).xyz * 2.0 - 1.0);
	encoded_normal.z = -encoded_normal.z;
	return encoded_normal;
}

// all vectors in viewspace
float calculate_pixel_obscurance(vec3 p_pixel_normal, vec3 p_hit_delta, float p_fallof_sq) {
	float length_sq = dot(p_hit_delta, p_hit_delta);
	float NdotD = dot(p_pixel_normal, p_hit_delta) / sqrt(length_sq);

	float falloff_mult = max(0.0, length_sq * p_fallof_sq + 1.0);

	return max(0, NdotD - 0.05) * falloff_mult;
}

void SSIL_tap_inner(const int p_quality_level, inout vec3 r_color_sum, inout float r_obscurance_sum, inout float r_weight_sum, const vec2 p_sampling_uv, const float p_mip_level, const vec3 p_pix_center_pos, vec3 p_pixel_normal, const float p_fallof_sq, const float p_weight_mod) {
	// get depth at sample
	float viewspace_sample_z = textureLod(source_depth_mipmaps, vec3(p_sampling_uv, params.pass), p_mip_level).x;
	vec3 sample_normal = load_normal(ivec2(p_sampling_uv * vec2(params.screen_size)));

	// convert to viewspace
	vec3 hit_pos = NDC_to_view_space(p_sampling_uv.xy, viewspace_sample_z);
	vec3 hit_delta = hit_pos - p_pix_center_pos;

	float obscurance = calculate_pixel_obscurance(p_pixel_normal, hit_delta, p_fallof_sq);
	float weight = 1.0;

	if (p_quality_level >= SSIL_HALOING_REDUCTION_ENABLE_AT_QUALITY_PRESET) {
		float reduct = max(0, -hit_delta.z);
		reduct = clamp(reduct * params.neg_inv_radius + 2.0, 0.0, 1.0);
		weight = SSIL_HALOING_REDUCTION_AMOUNT * reduct + (1.0 - SSIL_HALOING_REDUCTION_AMOUNT);
	}

	// Translate sampling_uv to last screen's coordinates
	const vec4 sample_pos = projection_constants.reprojection * vec4(p_sampling_uv * 2.0 - 1.0, (viewspace_sample_z - params.z_near) / (params.z_far - params.z_near) * 2.0 - 1.0, 1.0);
	vec2 reprojected_sampling_uv = (sample_pos.xy / sample_pos.w) * 0.5 + 0.5;

	weight *= p_weight_mod;

	r_obscurance_sum += obscurance * weight;

	vec3 sample_color = textureLod(last_frame, reprojected_sampling_uv, 5.0).rgb;
	// Reduce impact of fireflies by tonemapping before averaging: http://graphicrants.blogspot.com/2013/12/tone-mapping.html
	sample_color /= (1.0 + dot(sample_color, vec3(0.2126, 0.7152, 0.0722)));
	r_color_sum += sample_color * obscurance * weight * mix(1.0, smoothstep(0.0, 0.1, -dot(sample_normal, normalize(hit_delta))), params.normal_rejection_amount);
	r_weight_sum += weight;
}

void SSILTap(const int p_quality_level, inout vec3 r_color_sum, inout float r_obscurance_sum, inout float r_weight_sum, const int p_tap_index, const mat2 p_rot_scale, const vec3 p_pix_center_pos, vec3 p_pixel_normal, const vec2 p_normalized_screen_pos, const float p_mip_offset, const float p_fallof_sq, float p_weight_mod, vec2 p_norm_xy, float p_norm_xy_length) {
	vec2 sample_offset;
	float sample_pow_2_len;

	// patterns
	{
		vec4 new_sample = sample_pattern[p_tap_index];
		sample_offset = new_sample.xy * p_rot_scale;
		sample_pow_2_len = new_sample.w; // precalculated, same as: sample_pow_2_len = log2( length( new_sample.xy ) );
		p_weight_mod *= new_sample.z;
	}

	// snap to pixel center (more correct obscurance math, avoids artifacts)
	sample_offset = round(sample_offset);

	// calculate MIP based on the sample distance from the center, similar to as described
	// in http://graphics.cs.williams.edu/papers/SAOHPG12/.
	float mip_level = (p_quality_level < SSIL_DEPTH_MIPS_ENABLE_AT_QUALITY_PRESET) ? (0) : (sample_pow_2_len + p_mip_offset);

	vec2 sampling_uv = sample_offset * params.half_screen_pixel_size + p_normalized_screen_pos;

	SSIL_tap_inner(p_quality_level, r_color_sum, r_obscurance_sum, r_weight_sum, sampling_uv, mip_level, p_pix_center_pos, p_pixel_normal, p_fallof_sq, p_weight_mod);

	// for the second tap, just use the mirrored offset
	vec2 sample_offset_mirrored_uv = -sample_offset;

	// tilt the second set of samples so that the disk is effectively rotated by the normal
	// effective at removing one set of artifacts, but too expensive for lower quality settings
	if (p_quality_level >= SSIL_TILT_SAMPLES_ENABLE_AT_QUALITY_PRESET) {
		float dot_norm = dot(sample_offset_mirrored_uv, p_norm_xy);
		sample_offset_mirrored_uv -= dot_norm * p_norm_xy_length * p_norm_xy;
		sample_offset_mirrored_uv = round(sample_offset_mirrored_uv);
	}

	// snap to pixel center (more correct obscurance math, avoids artifacts)
	vec2 sampling_mirrored_uv = sample_offset_mirrored_uv * params.half_screen_pixel_size + p_normalized_screen_pos;

	SSIL_tap_inner(p_quality_level, r_color_sum, r_obscurance_sum, r_weight_sum, sampling_mirrored_uv, mip_level, p_pix_center_pos, p_pixel_normal, p_fallof_sq, p_weight_mod);
}

void generate_SSIL(out vec3 r_color, out vec4 r_edges, out float r_obscurance, out float r_weight, const vec2 p_pos, int p_quality_level, bool p_adaptive_base) {
	vec2 pos_rounded = trunc(p_pos);
	uvec2 upos = uvec2(pos_rounded);

	const int number_of_taps = (p_adaptive_base) ? (SSIL_ADAPTIVE_TAP_BASE_COUNT) : (num_taps[p_quality_level]);
	float pix_z, pix_left_z, pix_top_z, pix_right_z, pix_bottom_z;

	vec4 valuesUL = textureGather(source_depth_mipmaps, vec3(pos_rounded * params.half_screen_pixel_size, params.pass));
	vec4 valuesBR = textureGather(source_depth_mipmaps, vec3((pos_rounded + vec2(1.0)) * params.half_screen_pixel_size, params.pass));

	// get this pixel's viewspace depth
	pix_z = valuesUL.y;

	// get left right top bottom neighboring pixels for edge detection (gets compiled out on quality_level == 0)
	pix_left_z = valuesUL.x;
	pix_top_z = valuesUL.z;
	pix_right_z = valuesBR.z;
	pix_bottom_z = valuesBR.x;

	vec2 normalized_screen_pos = pos_rounded * params.half_screen_pixel_size + params.half_screen_pixel_size_x025;
	vec3 pix_center_pos = NDC_to_view_space(normalized_screen_pos, pix_z);

	// Load this pixel's viewspace normal
	uvec2 full_res_coord = upos * 2 * params.size_multiplier + params.pass_coord_offset.xy;
	vec3 pixel_normal = load_normal(ivec2(full_res_coord));

	const vec2 pixel_size_at_center = NDC_to_view_space(normalized_screen_pos.xy + params.half_screen_pixel_size, pix_center_pos.z).xy - pix_center_pos.xy;

	float pixel_lookup_radius;
	float fallof_sq;

	// calculate effect radius and fit our screen sampling pattern inside it
	float viewspace_radius;
	calculate_radius_parameters(length(pix_center_pos), pixel_size_at_center, pixel_lookup_radius, viewspace_radius, fallof_sq);

	// calculate samples rotation/scaling
	mat2 rot_scale_matrix;
	uint pseudo_random_index;

	{
		vec4 rotation_scale;
		// reduce effect radius near the screen edges slightly; ideally, one would render a larger depth buffer (5% on each side) instead
		if (!p_adaptive_base && (p_quality_level >= SSIL_REDUCE_RADIUS_NEAR_SCREEN_BORDER_ENABLE_AT_QUALITY_PRESET)) {
			float near_screen_border = min(min(normalized_screen_pos.x, 1.0 - normalized_screen_pos.x), min(normalized_screen_pos.y, 1.0 - normalized_screen_pos.y));
			near_screen_border = clamp(10.0 * near_screen_border + 0.6, 0.0, 1.0);
			pixel_lookup_radius *= near_screen_border;
		}

		// load & update pseudo-random rotation matrix
		pseudo_random_index = uint(pos_rounded.y * 2 + pos_rounded.x) % 5;
		rotation_scale = constants.rotation_matrices[params.pass * 5 + pseudo_random_index];
		rot_scale_matrix = mat2(rotation_scale.x * pixel_lookup_radius, rotation_scale.y * pixel_lookup_radius, rotation_scale.z * pixel_lookup_radius, rotation_scale.w * pixel_lookup_radius);
	}

	// the main obscurance & sample weight storage
	vec3 color_sum = vec3(0.0);
	float obscurance_sum = 0.0;
	float weight_sum = 0.0;

	// edge mask for between this and left/right/top/bottom neighbor pixels - not used in quality level 0 so initialize to "no edge" (1 is no edge, 0 is edge)
	vec4 edgesLRTB = vec4(1.0, 1.0, 1.0, 1.0);

	// Move center pixel slightly towards camera to avoid imprecision artifacts due to using of 16bit depth buffer.
	pix_center_pos *= 0.99;

	if (!p_adaptive_base && (p_quality_level >= SSIL_DEPTH_BASED_EDGES_ENABLE_AT_QUALITY_PRESET)) {
		edgesLRTB = calculate_edges(pix_z, pix_left_z, pix_right_z, pix_top_z, pix_bottom_z);
	}

	const float global_mip_offset = SSIL_DEPTH_MIPS_GLOBAL_OFFSET;
	float mip_offset = (p_quality_level < SSIL_DEPTH_MIPS_ENABLE_AT_QUALITY_PRESET) ? (0) : (log2(pixel_lookup_radius) + global_mip_offset);

	// Used to tilt the second set of samples so that the disk is effectively rotated by the normal
	// effective at removing one set of artifacts, but too expensive for lower quality settings
	vec2 norm_xy = vec2(pixel_normal.x, pixel_normal.y);
	float norm_xy_length = length(norm_xy);
	norm_xy /= vec2(norm_xy_length, -norm_xy_length);
	norm_xy_length *= SSIL_TILT_SAMPLES_AMOUNT;

	// standard, non-adaptive approach
	if ((p_quality_level != 3) || p_adaptive_base) {
		for (int i = 0; i < number_of_taps; i++) {
			SSILTap(p_quality_level, color_sum, obscurance_sum, weight_sum, i, rot_scale_matrix, pix_center_pos, pixel_normal, normalized_screen_pos, mip_offset, fallof_sq, 1.0, norm_xy, norm_xy_length);
		}
	}
#ifdef ADAPTIVE
	else {
		// add new ones if needed
		vec2 full_res_uv = normalized_screen_pos + params.pass_uv_offset.xy;
		float importance = textureLod(source_importance, full_res_uv, 0.0).x;

		//Need to store obscurance from base pass
		// load existing base values
		vec4 base_values = imageLoad(source_ssil, ivec3(upos, params.pass));
		weight_sum += imageLoad(edges_weights_image, ivec2(upos)).r * float(SSIL_ADAPTIVE_TAP_BASE_COUNT * 4.0);
		color_sum += (base_values.rgb) * weight_sum;
		obscurance_sum += (base_values.a) * weight_sum;

		// increase importance around edges
		float edge_count = dot(1.0 - edgesLRTB, vec4(1.0, 1.0, 1.0, 1.0));

		float avg_total_importance = float(counter.sum) * params.load_counter_avg_div;

		float importance_limiter = clamp(params.adaptive_sample_limit / avg_total_importance, 0.0, 1.0);
		importance *= importance_limiter;

		float additional_sample_count = SSIL_ADAPTIVE_TAP_FLEXIBLE_COUNT * importance;

		const float blend_range = 3.0;
		const float blend_range_inv = 1.0 / blend_range;

		additional_sample_count += 0.5;
		uint additional_samples = uint(additional_sample_count);
		uint additional_samples_to = min(SSIL_MAX_TAPS, additional_samples + SSIL_ADAPTIVE_TAP_BASE_COUNT);

		for (uint i = SSIL_ADAPTIVE_TAP_BASE_COUNT; i < additional_samples_to; i++) {
			additional_sample_count -= 1.0f;
			float weight_mod = clamp(additional_sample_count * blend_range_inv, 0.0, 1.0);
			SSILTap(p_quality_level, color_sum, obscurance_sum, weight_sum, int(i), rot_scale_matrix, pix_center_pos, pixel_normal, normalized_screen_pos, mip_offset, fallof_sq, weight_mod, norm_xy, norm_xy_length);
		}
	}
#endif

	// Early out for adaptive base
	if (p_adaptive_base) {
		vec3 color = color_sum / weight_sum;

		r_color = color;
		r_edges = vec4(0.0);
		r_obscurance = obscurance_sum / weight_sum;
		r_weight = weight_sum;
		return;
	}

	// Calculate weighted average
	vec3 color = color_sum / weight_sum;
	color /= 1.0 - dot(color, vec3(0.2126, 0.7152, 0.0722));

	// Calculate fadeout (1 close, gradient, 0 far)
	float fade_out = clamp(pix_center_pos.z * params.fade_out_mul + params.fade_out_add, 0.0, 1.0);

	// Reduce the SSIL if we're on the edge to remove artifacts on edges (we don't care for the lower quality one)
	if (!p_adaptive_base && (p_quality_level >= SSIL_DEPTH_BASED_EDGES_ENABLE_AT_QUALITY_PRESET)) {
		// when there's more than 2 opposite edges, start fading out the occlusion to reduce aliasing artifacts
		float edge_fadeout_factor = clamp((1.0 - edgesLRTB.x - edgesLRTB.y) * 0.35, 0.0, 1.0) + clamp((1.0 - edgesLRTB.z - edgesLRTB.w) * 0.35, 0.0, 1.0);

		fade_out *= clamp(1.0 - edge_fadeout_factor, 0.0, 1.0);
	}

	color = params.intensity * color;

	color *= fade_out;

	// outputs!
	r_color = color;
	r_edges = edgesLRTB; // These are used to prevent blurring across edges, 1 means no edge, 0 means edge, 0.5 means half way there, etc.
	r_obscurance = clamp((obscurance_sum / weight_sum) * params.intensity, 0.0, 1.0);
	r_weight = weight_sum;
}

void main() {
	vec3 out_color;
	float out_obscurance;
	float out_weight;
	vec4 out_edges;
	ivec2 ssC = ivec2(gl_GlobalInvocationID.xy);
	if (any(greaterThanEqual(ssC, params.screen_size))) { //too large, do nothing
		return;
	}

	vec2 uv = vec2(gl_GlobalInvocationID) + vec2(0.5);
#ifdef SSIL_BASE
	generate_SSIL(out_color, out_edges, out_obscurance, out_weight, uv, params.quality, true);

	imageStore(dest_image, ssC, vec4(out_color, out_obscurance));
	imageStore(edges_weights_image, ssC, vec4(out_weight / (float(SSIL_ADAPTIVE_TAP_BASE_COUNT) * 4.0)));
#else
	generate_SSIL(out_color, out_edges, out_obscurance, out_weight, uv, params.quality, false); // pass in quality levels

	imageStore(dest_image, ssC, vec4(out_color, out_obscurance));
	imageStore(edges_weights_image, ssC, vec4(pack_edges(out_edges)));
#endif
}

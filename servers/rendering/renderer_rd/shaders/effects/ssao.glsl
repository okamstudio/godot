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
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#[compute]

#version 450

#VERSION_DEFINES

#define INTELSSAO_MAIN_DISK_SAMPLE_COUNT (32)
const vec4 sample_pattern[INTELSSAO_MAIN_DISK_SAMPLE_COUNT] = {
	vec4(0.78488064, 0.56661671, 1.500000, -0.126083), vec4(0.26022232, -0.29575172, 1.500000, -1.064030), vec4(0.10459357, 0.08372527, 1.110000, -2.730563), vec4(-0.68286800, 0.04963045, 1.090000, -0.498827),
	vec4(-0.13570161, -0.64190155, 1.250000, -0.532765), vec4(-0.26193795, -0.08205118, 0.670000, -1.783245), vec4(-0.61177456, 0.66664219, 0.710000, -0.044234), vec4(0.43675563, 0.25119025, 0.610000, -1.167283),
	vec4(0.07884444, 0.86618668, 0.640000, -0.459002), vec4(-0.12790935, -0.29869005, 0.600000, -1.729424), vec4(-0.04031125, 0.02413622, 0.600000, -4.792042), vec4(0.16201244, -0.52851415, 0.790000, -1.067055),
	vec4(-0.70991218, 0.47301072, 0.640000, -0.335236), vec4(0.03277707, -0.22349690, 0.600000, -1.982384), vec4(0.68921727, 0.36800742, 0.630000, -0.266718), vec4(0.29251814, 0.37775412, 0.610000, -1.422520),
	vec4(-0.12224089, 0.96582592, 0.600000, -0.426142), vec4(0.11071457, -0.16131058, 0.600000, -2.165947), vec4(0.46562141, -0.59747696, 0.600000, -0.189760), vec4(-0.51548797, 0.11804193, 0.600000, -1.246800),
	vec4(0.89141309, -0.42090443, 0.600000, 0.028192), vec4(-0.32402530, -0.01591529, 0.600000, -1.543018), vec4(0.60771245, 0.41635221, 0.600000, -0.605411), vec4(0.02379565, -0.08239821, 0.600000, -3.809046),
	vec4(0.48951152, -0.23657045, 0.600000, -1.189011), vec4(-0.17611565, -0.81696892, 0.600000, -0.513724), vec4(-0.33930185, -0.20732205, 0.600000, -1.698047), vec4(-0.91974425, 0.05403209, 0.600000, 0.062246),
	vec4(-0.15064627, -0.14949332, 0.600000, -1.896062), vec4(0.53180975, -0.35210401, 0.600000, -0.758838), vec4(0.41487166, 0.81442589, 0.600000, -0.505648), vec4(-0.24106961, -0.32721516, 0.600000, -1.665244)
};

// these values can be changed (up to SSAO_MAX_TAPS) with no changes required elsewhere; values for 4th and 5th preset are ignored but array needed to avoid compilation errors
// the actual number of texture samples is two times this value (each "tap" has two symmetrical depth texture samples)
const int num_taps[5] = { 3, 5, 12, 0, 0 };

#define SSAO_TILT_SAMPLES_ENABLE_AT_QUALITY_PRESET (99) // to disable simply set to 99 or similar
#define SSAO_TILT_SAMPLES_AMOUNT (0.4)
//
#define SSAO_HALOING_REDUCTION_ENABLE_AT_QUALITY_PRESET (1) // to disable simply set to 99 or similar
#define SSAO_HALOING_REDUCTION_AMOUNT (0.6) // values from 0.0 - 1.0, 1.0 means max weighting (will cause artifacts, 0.8 is more reasonable)
//
#define SSAO_NORMAL_BASED_EDGES_ENABLE_AT_QUALITY_PRESET (2) // to disable simply set to 99 or similar
#define SSAO_NORMAL_BASED_EDGES_DOT_THRESHOLD (0.5) // use 0-0.1 for super-sharp normal-based edges
//
#define SSAO_DETAIL_AO_ENABLE_AT_QUALITY_PRESET (1) // whether to use detail; to disable simply set to 99 or similar
//
// WARNING: The MIP generation on the C++ side will be enabled on quality preset 2 regardless of
// this value, so if changing here, change the C++ side too.
#define SSAO_DEPTH_MIPS_ENABLE_AT_QUALITY_PRESET (2)
#define SSAO_DEPTH_MIPS_GLOBAL_OFFSET (-4.3) // best noise/quality/performance tradeoff, found empirically
//
// WARNING: The edge handling is hard-coded to 'disabled' on quality level 0, and enabled above,
// on the C++ side; while toggling it here will work for testing purposes, it will not yield
// performance gains (or correct results).
#define SSAO_DEPTH_BASED_EDGES_ENABLE_AT_QUALITY_PRESET (1)
//
#define SSAO_REDUCE_RADIUS_NEAR_SCREEN_BORDER_ENABLE_AT_QUALITY_PRESET (1)

#define SSAO_MAX_TAPS 32
#define SSAO_ADAPTIVE_TAP_BASE_COUNT 5
#define SSAO_ADAPTIVE_TAP_FLEXIBLE_COUNT (SSAO_MAX_TAPS - SSAO_ADAPTIVE_TAP_BASE_COUNT)
#define SSAO_DEPTH_MIP_LEVELS 4

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2DArray source_depth_mipmaps;
layout(rgba8, set = 0, binding = 1) uniform restrict readonly image2D source_normal;

#ifdef ADAPTIVE
layout(rg8, set = 1, binding = 0) uniform restrict readonly image2DArray source_ssao;
layout(set = 1, binding = 1) uniform sampler2D source_importance;
layout(set = 1, binding = 2, std430) buffer Counter {
	uint sum;
}
counter;
#endif

layout(rg8, set = 2, binding = 0) uniform restrict writeonly image2D dest_image;

// This push_constant is full - 128 bytes - if you need to add more data, consider adding to the uniform buffer instead
layout(push_constant, std430) uniform Params {
	ivec2 screen_size;
	int pass;
	int quality;

	vec2 half_screen_pixel_size;
	int size_multiplier;
	float detail_intensity;

	vec2 NDC_to_view_mul;
	vec2 NDC_to_view_add;

	vec2 pad2;
	vec2 half_screen_pixel_size_x025;

	float radius;
	float intensity;
	float shadow_power;
	float shadow_clamp;

	float fade_out_mul;
	float fade_out_add;
	float horizon_angle_threshold;
	float inv_radius_near_limit;

	bool is_orthogonal;
	float neg_inv_radius;
	float load_counter_avg_div;
	float adaptive_sample_limit;

	ivec2 pass_coord_offset;
	vec2 pass_uv_offset;
}
params;

// Interleaved Gradient Noise
// https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
float quick_hash(vec2 pos) {
	const vec3 magic = vec3(0.06711056f, 0.00583715f, 52.9829189f);
	return fract(magic.z * fract(dot(pos, magic.xy)));
}

// packing/unpacking for edges; 2 bits per edge mean 4 gradient values (0, 0.33, 0.66, 1) for smoother transitions!
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

	return max(0, NdotD - params.horizon_angle_threshold) * falloff_mult;
}

void SSAO_tap_inner(const int p_quality_level, inout float r_obscurance_sum, inout float r_weight_sum, const vec2 p_sampling_uv, const float p_mip_level, const vec3 p_pix_center_pos, vec3 p_pixel_normal, const float p_fallof_sq, const float p_weight_mod) {
	// get depth at sample
	float viewspace_sample_z = textureLod(source_depth_mipmaps, vec3(p_sampling_uv, params.pass), p_mip_level).x;

	// convert to viewspace
	vec3 hit_pos = NDC_to_view_space(p_sampling_uv.xy, viewspace_sample_z).xyz;
	vec3 hit_delta = hit_pos - p_pix_center_pos;

	float obscurance = calculate_pixel_obscurance(p_pixel_normal, hit_delta, p_fallof_sq);
	float weight = 1.0;

	if (p_quality_level >= SSAO_HALOING_REDUCTION_ENABLE_AT_QUALITY_PRESET) {
		float reduct = max(0, -hit_delta.z);
		reduct = clamp(reduct * params.neg_inv_radius + 2.0, 0.0, 1.0);
		weight = SSAO_HALOING_REDUCTION_AMOUNT * reduct + (1.0 - SSAO_HALOING_REDUCTION_AMOUNT);
	}
	weight *= p_weight_mod;
	r_obscurance_sum += obscurance * weight;
	r_weight_sum += weight;
}

void SSAOTap(const int p_quality_level, inout float r_obscurance_sum, inout float r_weight_sum, const int p_tap_index, const mat2 p_rot_scale, const vec3 p_pix_center_pos, vec3 p_pixel_normal, const vec2 p_normalized_screen_pos, const float p_mip_offset, const float p_fallof_sq, float p_weight_mod, vec2 p_norm_xy, float p_norm_xy_length) {
	vec2 sample_offset;
	float sample_pow_2_len;

	// patterns
	{
		// Choose different sample pattern starting point based on pass
		vec4 new_sample = sample_pattern[(p_tap_index + 8 * params.pass) % INTELSSAO_MAIN_DISK_SAMPLE_COUNT];
		sample_offset = new_sample.xy * p_rot_scale;
		sample_pow_2_len = new_sample.w; // precalculated, same as: sample_pow_2_len = log2( length( new_sample.xy ) );
		// Commented out for now since it only makes sense when starting from the first index every time (I think?)
		// p_weight_mod *= new_sample.z;
	}

	// snap to pixel center (more correct obscurance math, avoids artifacts)
	sample_offset = round(sample_offset);

	// calculate MIP based on the sample distance from the center, similar to as described
	// in http://graphics.cs.williams.edu/papers/SAOHPG12/.
	float mip_level = (p_quality_level < SSAO_DEPTH_MIPS_ENABLE_AT_QUALITY_PRESET) ? (0) : (sample_pow_2_len + p_mip_offset);

	vec2 sampling_uv = sample_offset * params.half_screen_pixel_size + p_normalized_screen_pos;

	SSAO_tap_inner(p_quality_level, r_obscurance_sum, r_weight_sum, sampling_uv, mip_level, p_pix_center_pos, p_pixel_normal, p_fallof_sq, p_weight_mod);

	// for the second tap, just use the mirrored offset
	vec2 sample_offset_mirrored_uv = -sample_offset;

	// tilt the second set of samples so that the disk is effectively rotated by the normal
	// effective at removing one set of artifacts, but too expensive for lower quality settings
	if (p_quality_level >= SSAO_TILT_SAMPLES_ENABLE_AT_QUALITY_PRESET) {
		float dot_norm = dot(sample_offset_mirrored_uv, p_norm_xy);
		sample_offset_mirrored_uv -= dot_norm * p_norm_xy_length * p_norm_xy;
		sample_offset_mirrored_uv = round(sample_offset_mirrored_uv);
	}

	// snap to pixel center (more correct obscurance math, avoids artifacts)
	vec2 sampling_mirrored_uv = sample_offset_mirrored_uv * params.half_screen_pixel_size + p_normalized_screen_pos;

	SSAO_tap_inner(p_quality_level, r_obscurance_sum, r_weight_sum, sampling_mirrored_uv, mip_level, p_pix_center_pos, p_pixel_normal, p_fallof_sq, p_weight_mod);
}

void generate_SSAO_shadows_internal(out float r_shadow_term, out vec4 r_edges, out float r_weight, const vec2 p_pos, int p_quality_level, bool p_adaptive_base) {
	vec2 pos_rounded = trunc(p_pos);
	uvec2 upos = uvec2(pos_rounded);

	const int number_of_taps = (p_adaptive_base) ? (SSAO_ADAPTIVE_TAP_BASE_COUNT) : (num_taps[p_quality_level]);
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

	const vec2 pixel_size_at_center = NDC_to_view_space(normalized_screen_pos.xy + params.half_screen_pixel_size * 2.0, pix_center_pos.z).xy - pix_center_pos.xy;

	float pixel_lookup_radius;
	float fallof_sq;

	// calculate effect radius and fit our screen sampling pattern inside it
	float viewspace_radius;
	calculate_radius_parameters(length(pix_center_pos), pixel_size_at_center, pixel_lookup_radius, viewspace_radius, fallof_sq);

	// calculate samples rotation/scaling
	mat2 rot_scale_matrix;
	{
		// reduce effect radius near the screen edges slightly; ideally, one would render a larger depth buffer (5% on each side) instead
		if (!p_adaptive_base && (p_quality_level >= SSAO_REDUCE_RADIUS_NEAR_SCREEN_BORDER_ENABLE_AT_QUALITY_PRESET)) {
			float near_screen_border = min(min(normalized_screen_pos.x, 1.0 - normalized_screen_pos.x), min(normalized_screen_pos.y, 1.0 - normalized_screen_pos.y));
			near_screen_border = clamp(10.0 * near_screen_border + 0.6, 0.0, 1.0);
			pixel_lookup_radius *= near_screen_border;
		}

		// Update pseudo-random rotation matrix
		vec2 seed = full_res_coord * 0.333333333; // Multiplier chosen empirically for noise that doesn't exhibit repeating patterns
		float angle = quick_hash(seed) * 3.14159;
		float ca = cos(angle);
		float sa = sin(angle);
		float r = quick_hash(seed.yx);
		r = 0.8 + 0.4 * r * r; // Roughly mimicking the scale range generated on the CPU side currently, and prefer closer samples
		rot_scale_matrix = mat2(ca, sa, -sa, ca) * r * pixel_lookup_radius;
	}

	// the main obscurance & sample weight storage
	float obscurance_sum = 0.0;
	float weight_sum = 0.0;

	// edge mask for between this and left/right/top/bottom neighbor pixels - not used in quality level 0 so initialize to "no edge" (1 is no edge, 0 is edge)
	vec4 edgesLRTB = vec4(1.0, 1.0, 1.0, 1.0);

	// Move center pixel slightly towards camera to avoid imprecision artifacts due to using of 16bit depth buffer.
	pix_center_pos *= 0.99;

	if (!p_adaptive_base && (p_quality_level >= SSAO_DEPTH_BASED_EDGES_ENABLE_AT_QUALITY_PRESET)) {
		edgesLRTB = calculate_edges(pix_z, pix_left_z, pix_right_z, pix_top_z, pix_bottom_z);
	}

	// adds a more high definition sharp effect, which gets blurred out (reuses left/right/top/bottom samples that we used for edge detection)
	if (!p_adaptive_base && (p_quality_level >= SSAO_DETAIL_AO_ENABLE_AT_QUALITY_PRESET)) {
		// disable in case of quality level 4 (reference)
		if (p_quality_level != 4) {
			//approximate neighboring pixels positions (actually just deltas or "positions - pix_center_pos" )
			vec3 normalized_viewspace_dir = vec3(pix_center_pos.xy / pix_center_pos.zz, 1.0);
			vec3 pixel_left_delta = vec3(-pixel_size_at_center.x, 0.0, 0.0) + normalized_viewspace_dir * (pix_left_z - pix_center_pos.z);
			vec3 pixel_right_delta = vec3(+pixel_size_at_center.x, 0.0, 0.0) + normalized_viewspace_dir * (pix_right_z - pix_center_pos.z);
			vec3 pixel_top_delta = vec3(0.0, -pixel_size_at_center.y, 0.0) + normalized_viewspace_dir * (pix_top_z - pix_center_pos.z);
			vec3 pixel_bottom_delta = vec3(0.0, +pixel_size_at_center.y, 0.0) + normalized_viewspace_dir * (pix_bottom_z - pix_center_pos.z);

			const float range_reduction = 4.0f; // this is to avoid various artifacts
			const float modified_fallof_sq = range_reduction * fallof_sq;

			vec4 additional_obscurance;
			additional_obscurance.x = calculate_pixel_obscurance(pixel_normal, pixel_left_delta, modified_fallof_sq);
			additional_obscurance.y = calculate_pixel_obscurance(pixel_normal, pixel_right_delta, modified_fallof_sq);
			additional_obscurance.z = calculate_pixel_obscurance(pixel_normal, pixel_top_delta, modified_fallof_sq);
			additional_obscurance.w = calculate_pixel_obscurance(pixel_normal, pixel_bottom_delta, modified_fallof_sq);

			obscurance_sum += params.detail_intensity * dot(additional_obscurance, edgesLRTB);
		}
	}

	// Sharp normals also create edges - but this adds to the cost as well
	if (!p_adaptive_base && (p_quality_level >= SSAO_NORMAL_BASED_EDGES_ENABLE_AT_QUALITY_PRESET)) {
		vec3 neighbour_normal_left = load_normal(ivec2(full_res_coord), ivec2(-2, 0));
		vec3 neighbour_normal_right = load_normal(ivec2(full_res_coord), ivec2(2, 0));
		vec3 neighbour_normal_top = load_normal(ivec2(full_res_coord), ivec2(0, -2));
		vec3 neighbour_normal_bottom = load_normal(ivec2(full_res_coord), ivec2(0, 2));

		const float dot_threshold = SSAO_NORMAL_BASED_EDGES_DOT_THRESHOLD;

		vec4 normal_edgesLRTB;
		normal_edgesLRTB.x = clamp((dot(pixel_normal, neighbour_normal_left) + dot_threshold), 0.0, 1.0);
		normal_edgesLRTB.y = clamp((dot(pixel_normal, neighbour_normal_right) + dot_threshold), 0.0, 1.0);
		normal_edgesLRTB.z = clamp((dot(pixel_normal, neighbour_normal_top) + dot_threshold), 0.0, 1.0);
		normal_edgesLRTB.w = clamp((dot(pixel_normal, neighbour_normal_bottom) + dot_threshold), 0.0, 1.0);

		edgesLRTB *= normal_edgesLRTB;
	}

	const float global_mip_offset = SSAO_DEPTH_MIPS_GLOBAL_OFFSET;
	float mip_offset = (p_quality_level < SSAO_DEPTH_MIPS_ENABLE_AT_QUALITY_PRESET) ? (0) : (log2(pixel_lookup_radius) + global_mip_offset);

	// Used to tilt the second set of samples so that the disk is effectively rotated by the normal
	// effective at removing one set of artifacts, but too expensive for lower quality settings
	vec2 norm_xy = vec2(pixel_normal.x, pixel_normal.y);
	float norm_xy_length = length(norm_xy);
	norm_xy /= vec2(norm_xy_length, -norm_xy_length);
	norm_xy_length *= SSAO_TILT_SAMPLES_AMOUNT;

	// standard, non-adaptive approach
	if ((p_quality_level != 3) || p_adaptive_base) {
		for (int i = 0; i < number_of_taps; i++) {
			SSAOTap(p_quality_level, obscurance_sum, weight_sum, i, rot_scale_matrix, pix_center_pos, pixel_normal, normalized_screen_pos, mip_offset, fallof_sq, 1.0, norm_xy, norm_xy_length);
		}
	}
#ifdef ADAPTIVE
	else {
		// add new ones if needed
		vec2 full_res_uv = normalized_screen_pos + params.pass_uv_offset.xy;
		float importance = textureLod(source_importance, full_res_uv, 0.0).x;

		// this is to normalize SSAO_DETAIL_AO_AMOUNT across all pixel regardless of importance
		obscurance_sum *= (SSAO_ADAPTIVE_TAP_BASE_COUNT / float(SSAO_MAX_TAPS)) + (importance * SSAO_ADAPTIVE_TAP_FLEXIBLE_COUNT / float(SSAO_MAX_TAPS));

		// load existing base values
		vec2 base_values = imageLoad(source_ssao, ivec3(upos, params.pass)).xy;
		weight_sum += base_values.y * float(SSAO_ADAPTIVE_TAP_BASE_COUNT * 4.0);
		obscurance_sum += (base_values.x) * weight_sum;

		// increase importance around edges
		float edge_count = dot(1.0 - edgesLRTB, vec4(1.0, 1.0, 1.0, 1.0));

		float avg_total_importance = float(counter.sum) * params.load_counter_avg_div;

		float importance_limiter = clamp(params.adaptive_sample_limit / avg_total_importance, 0.0, 1.0);
		importance *= importance_limiter;

		float additional_sample_count = SSAO_ADAPTIVE_TAP_FLEXIBLE_COUNT * importance;

		const float blend_range = 3.0;
		const float blend_range_inv = 1.0 / blend_range;

		additional_sample_count += 0.5;
		uint additional_samples = uint(additional_sample_count);
		uint additional_samples_to = min(SSAO_MAX_TAPS, additional_samples + SSAO_ADAPTIVE_TAP_BASE_COUNT);

		for (uint i = SSAO_ADAPTIVE_TAP_BASE_COUNT; i < additional_samples_to; i++) {
			additional_sample_count -= 1.0f;
			float weight_mod = clamp(additional_sample_count * blend_range_inv, 0.0, 1.0);
			SSAOTap(p_quality_level, obscurance_sum, weight_sum, int(i), rot_scale_matrix, pix_center_pos, pixel_normal, normalized_screen_pos, mip_offset, fallof_sq, weight_mod, norm_xy, norm_xy_length);
		}
	}
#endif

	// early out for adaptive base - just output weight (used for the next pass)
	if (p_adaptive_base) {
		float obscurance = obscurance_sum / weight_sum;

		r_shadow_term = obscurance;
		r_edges = vec4(0.0);
		r_weight = weight_sum;
		return;
	}

	// calculate weighted average
	float obscurance = obscurance_sum / weight_sum;

	// calculate fadeout (1 close, gradient, 0 far)
	float fade_out = clamp(pix_center_pos.z * params.fade_out_mul + params.fade_out_add, 0.0, 1.0);

	// Reduce the SSAO shadowing if we're on the edge to remove artifacts on edges (we don't care for the lower quality one)
	if (!p_adaptive_base && (p_quality_level >= SSAO_DEPTH_BASED_EDGES_ENABLE_AT_QUALITY_PRESET)) {
		// when there's more than 2 opposite edges, start fading out the occlusion to reduce aliasing artifacts
		float edge_fadeout_factor = clamp((1.0 - edgesLRTB.x - edgesLRTB.y) * 0.35, 0.0, 1.0) + clamp((1.0 - edgesLRTB.z - edgesLRTB.w) * 0.35, 0.0, 1.0);

		fade_out *= clamp(1.0 - edge_fadeout_factor, 0.0, 1.0);
	}

	// strength
	obscurance = params.intensity * obscurance;

	// clamp
	obscurance = min(obscurance, params.shadow_clamp);

	// fadeout
	obscurance *= fade_out;

	// conceptually switch to occlusion with the meaning being visibility (grows with visibility, occlusion == 1 implies full visibility),
	// to be in line with what is more commonly used.
	float occlusion = 1.0 - obscurance;

	// modify the gradient
	// note: this cannot be moved to a later pass because of loss of precision after storing in the render target
	occlusion = pow(clamp(occlusion, 0.0, 1.0), params.shadow_power);

	// outputs!
	r_shadow_term = occlusion; // Our final 'occlusion' term (0 means fully occluded, 1 means fully lit)
	r_edges = edgesLRTB; // These are used to prevent blurring across edges, 1 means no edge, 0 means edge, 0.5 means half way there, etc.
	r_weight = weight_sum;
}

void main() {
	float out_shadow_term;
	float out_weight;
	vec4 out_edges;
	ivec2 ssC = ivec2(gl_GlobalInvocationID.xy);
	if (any(greaterThanEqual(ssC, params.screen_size))) { //too large, do nothing
		return;
	}

	vec2 uv = vec2(gl_GlobalInvocationID) + vec2(0.5);
#ifdef SSAO_BASE
	generate_SSAO_shadows_internal(out_shadow_term, out_edges, out_weight, uv, params.quality, true);

	imageStore(dest_image, ivec2(gl_GlobalInvocationID.xy), vec4(out_shadow_term, out_weight / (float(SSAO_ADAPTIVE_TAP_BASE_COUNT) * 4.0), 0.0, 0.0));
#else
	generate_SSAO_shadows_internal(out_shadow_term, out_edges, out_weight, uv, params.quality, false); // pass in quality levels
	if (params.quality == 0) {
		out_edges = vec4(1.0);
	}

	imageStore(dest_image, ivec2(gl_GlobalInvocationID.xy), vec4(out_shadow_term, pack_edges(out_edges), 0.0, 0.0));
#endif
}

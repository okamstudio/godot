///////////////////////////////////////////////////////////////////////////////////
// Copyright(c) 2016-2022 Panos Karabelas
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is furnished
// to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
///////////////////////////////////////////////////////////////////////////////////
// File changes (yyyy-mm-dd)
// 2022-05-06: Panos Karabelas: first commit
// 2020-12-05: Joan Fons: convert to Vulkan and Godot
///////////////////////////////////////////////////////////////////////////////////

#[compute]

#version 450

#VERSION_DEFINES

// Based on Spartan Engine's TAA implementation (without TAA upscale).
// <https://github.com/PanosK92/SpartanEngine/blob/a8338d0609b85dc32f3732a5c27fb4463816a3b9/Data/shaders/temporal_antialiasing.hlsl>

#ifndef MOLTENVK_USED
#define USE_SUBGROUPS
#endif // MOLTENVK_USED

#define GROUP_SIZE 8
#define FLT_MIN 0.00000001
#define FLT_MAX 32767.0
#define RPC_9 0.11111111111
#define RPC_16 0.0625

#ifdef USE_SUBGROUPS
layout(local_size_x = GROUP_SIZE, local_size_y = GROUP_SIZE, local_size_z = 1) in;
#endif

layout(rgba16f, set = 0, binding = 0) uniform restrict readonly image2D color_buffer;
layout(set = 0, binding = 1) uniform sampler2D depth_buffer;
layout(rg16f, set = 0, binding = 2) uniform restrict readonly image2D velocity_buffer;
layout(rg16f, set = 0, binding = 3) uniform restrict readonly image2D last_velocity_buffer;
layout(set = 0, binding = 4) uniform sampler2D history_buffer;
layout(rgba16f, set = 0, binding = 5) uniform restrict writeonly image2D output_buffer;

layout(push_constant, std430) uniform Params {
	vec2 resolution;
	float disocclusion_threshold; // 0.1 / max(params.resolution.x, params.resolution.y
	float disocclusion_scale;
}
params;

const ivec2 kOffsets3x3[9] = {
	ivec2(-1, -1),
	ivec2(0, -1),
	ivec2(1, -1),
	ivec2(-1, 0),
	ivec2(0, 0),
	ivec2(1, 0),
	ivec2(-1, 1),
	ivec2(0, 1),
	ivec2(1, 1),
};

/*------------------------------------------------------------------------------
						THREAD GROUP SHARED MEMORY (LDS)
------------------------------------------------------------------------------*/

const int kBorderSize = 1;
const int kGroupSize = GROUP_SIZE;
const int kTileDimension = kGroupSize + kBorderSize * 2;
const int kTileDimension2 = kTileDimension * kTileDimension;

vec3 reinhard(vec3 hdr) {
	return hdr / (hdr + 1.0);
}
vec3 reinhard_inverse(vec3 sdr) {
	return sdr / (1.0 - sdr);
}

float get_depth(ivec2 thread_id) {
	return texelFetch(depth_buffer, thread_id, 0).r;
}

#ifdef USE_SUBGROUPS
shared vec3 tile_color[kTileDimension][kTileDimension];
shared float tile_depth[kTileDimension][kTileDimension];

vec3 load_color(uvec2 group_thread_id) {
	group_thread_id += kBorderSize;
	return tile_color[group_thread_id.x][group_thread_id.y];
}

void store_color(uvec2 group_thread_id, vec3 color) {
	tile_color[group_thread_id.x][group_thread_id.y] = color;
}

float load_depth(uvec2 group_thread_id) {
	group_thread_id += kBorderSize;
	return tile_depth[group_thread_id.x][group_thread_id.y];
}

void store_depth(uvec2 group_thread_id, float depth) {
	tile_depth[group_thread_id.x][group_thread_id.y] = depth;
}

void store_color_depth(uvec2 group_thread_id, ivec2 thread_id) {
	// out of bounds clamp
	thread_id = clamp(thread_id, ivec2(0, 0), ivec2(params.resolution) - ivec2(1, 1));

	store_color(group_thread_id, imageLoad(color_buffer, thread_id).rgb);
	store_depth(group_thread_id, get_depth(thread_id));
}

void populate_group_shared_memory(uvec2 group_id, uint group_index) {
	// Populate group shared memory
	ivec2 group_top_left = ivec2(group_id) * kGroupSize - kBorderSize;
	if (group_index < (kTileDimension2 >> 2)) {
		ivec2 group_thread_id_1 = ivec2(group_index % kTileDimension, group_index / kTileDimension);
		ivec2 group_thread_id_2 = ivec2((group_index + (kTileDimension2 >> 2)) % kTileDimension, (group_index + (kTileDimension2 >> 2)) / kTileDimension);
		ivec2 group_thread_id_3 = ivec2((group_index + (kTileDimension2 >> 1)) % kTileDimension, (group_index + (kTileDimension2 >> 1)) / kTileDimension);
		ivec2 group_thread_id_4 = ivec2((group_index + kTileDimension2 * 3 / 4) % kTileDimension, (group_index + kTileDimension2 * 3 / 4) / kTileDimension);

		store_color_depth(group_thread_id_1, group_top_left + group_thread_id_1);
		store_color_depth(group_thread_id_2, group_top_left + group_thread_id_2);
		store_color_depth(group_thread_id_3, group_top_left + group_thread_id_3);
		store_color_depth(group_thread_id_4, group_top_left + group_thread_id_4);
	}

	// Wait for group threads to load store data.
	groupMemoryBarrier();
	barrier();
}
#else
vec3 load_color(uvec2 screen_pos) {
	return imageLoad(color_buffer, ivec2(screen_pos)).rgb;
}

float load_depth(uvec2 screen_pos) {
	return get_depth(ivec2(screen_pos));
}
#endif

/*------------------------------------------------------------------------------
								VELOCITY
------------------------------------------------------------------------------*/

void depth_test_min(uvec2 pos, inout float min_depth, inout uvec2 min_pos) {
	float depth = load_depth(pos);

	if (depth < min_depth) {
		min_depth = depth;
		min_pos = pos;
	}
}

// Returns velocity with closest depth (3x3 neighborhood)
void get_closest_pixel_velocity_3x3(in uvec2 group_pos, uvec2 group_top_left, out vec2 velocity) {
	float min_depth = 1.0;
	uvec2 min_pos = group_pos;

	depth_test_min(group_pos + kOffsets3x3[0], min_depth, min_pos);
	depth_test_min(group_pos + kOffsets3x3[1], min_depth, min_pos);
	depth_test_min(group_pos + kOffsets3x3[2], min_depth, min_pos);
	depth_test_min(group_pos + kOffsets3x3[3], min_depth, min_pos);
	depth_test_min(group_pos + kOffsets3x3[4], min_depth, min_pos);
	depth_test_min(group_pos + kOffsets3x3[5], min_depth, min_pos);
	depth_test_min(group_pos + kOffsets3x3[6], min_depth, min_pos);
	depth_test_min(group_pos + kOffsets3x3[7], min_depth, min_pos);
	depth_test_min(group_pos + kOffsets3x3[8], min_depth, min_pos);

	// Velocity out
	velocity = imageLoad(velocity_buffer, ivec2(group_top_left + min_pos)).xy;
}

/*------------------------------------------------------------------------------
							  HISTORY SAMPLING
------------------------------------------------------------------------------*/

vec3 sample_catmull_rom_9(sampler2D stex, vec2 uv, vec2 resolution) {
	// Source: https://gist.github.com/TheRealMJP/c83b8c0f46b63f3a88a5986f4fa982b1
	// License: https://gist.github.com/TheRealMJP/bc503b0b87b643d3505d41eab8b332ae

	// We're going to sample a 4x4 grid of texels surrounding the target UV coordinate. We'll do this by rounding
	// down the sample location to get the exact center of our "starting" texel. The starting texel will be at
	// location [1, 1] in the grid, where [0, 0] is the top left corner.
	vec2 sample_pos = uv * resolution;
	vec2 texPos1 = floor(sample_pos - 0.5f) + 0.5f;

	// Compute the fractional offset from our starting texel to our original sample location, which we'll
	// feed into the Catmull-Rom spline function to get our filter weights.
	vec2 f = sample_pos - texPos1;

	// Compute the Catmull-Rom weights using the fractional offset that we calculated earlier.
	// These equations are pre-expanded based on our knowledge of where the texels will be located,
	// which lets us avoid having to evaluate a piece-wise function.
	vec2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
	vec2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
	vec2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
	vec2 w3 = f * f * (-0.5f + 0.5f * f);

	// Work out weighting factors and sampling offsets that will let us use bilinear filtering to
	// simultaneously evaluate the middle 2 samples from the 4x4 grid.
	vec2 w12 = w1 + w2;
	vec2 offset12 = w2 / (w1 + w2);

	// Compute the final UV coordinates we'll use for sampling the texture
	vec2 texPos0 = texPos1 - 1.0f;
	vec2 texPos3 = texPos1 + 2.0f;
	vec2 texPos12 = texPos1 + offset12;

	texPos0 /= resolution;
	texPos3 /= resolution;
	texPos12 /= resolution;

	vec3 result = vec3(0.0f, 0.0f, 0.0f);

	result += textureLod(stex, vec2(texPos0.x, texPos0.y), 0.0).xyz * w0.x * w0.y;
	result += textureLod(stex, vec2(texPos12.x, texPos0.y), 0.0).xyz * w12.x * w0.y;
	result += textureLod(stex, vec2(texPos3.x, texPos0.y), 0.0).xyz * w3.x * w0.y;

	result += textureLod(stex, vec2(texPos0.x, texPos12.y), 0.0).xyz * w0.x * w12.y;
	result += textureLod(stex, vec2(texPos12.x, texPos12.y), 0.0).xyz * w12.x * w12.y;
	result += textureLod(stex, vec2(texPos3.x, texPos12.y), 0.0).xyz * w3.x * w12.y;

	result += textureLod(stex, vec2(texPos0.x, texPos3.y), 0.0).xyz * w0.x * w3.y;
	result += textureLod(stex, vec2(texPos12.x, texPos3.y), 0.0).xyz * w12.x * w3.y;
	result += textureLod(stex, vec2(texPos3.x, texPos3.y), 0.0).xyz * w3.x * w3.y;

	return max(result, 0.0f);
}

/*------------------------------------------------------------------------------
							  HISTORY CLIPPING
------------------------------------------------------------------------------*/

// Based on "Temporal Reprojection Anti-Aliasing" - https://github.com/playdeadgames/temporal
vec3 clip_aabb(vec3 aabb_min, vec3 aabb_max, vec3 p, vec3 q) {
	vec3 r = q - p;
	vec3 rmax = (aabb_max - p.xyz);
	vec3 rmin = (aabb_min - p.xyz);

	if (r.x > rmax.x + FLT_MIN) {
		r *= (rmax.x / r.x);
	}
	if (r.y > rmax.y + FLT_MIN) {
		r *= (rmax.y / r.y);
	}
	if (r.z > rmax.z + FLT_MIN) {
		r *= (rmax.z / r.z);
	}

	if (r.x < rmin.x - FLT_MIN) {
		r *= (rmin.x / r.x);
	}
	if (r.y < rmin.y - FLT_MIN) {
		r *= (rmin.y / r.y);
	}
	if (r.z < rmin.z - FLT_MIN) {
		r *= (rmin.z / r.z);
	}

	return p + r;
}

// Clip history to the neighbourhood of the current sample
vec3 clip_history_3x3(uvec2 group_pos, vec3 color_history, vec2 velocity_closest) {
	// Sample a 3x3 neighbourhood
	vec3 s1 = load_color(group_pos + kOffsets3x3[0]);
	vec3 s2 = load_color(group_pos + kOffsets3x3[1]);
	vec3 s3 = load_color(group_pos + kOffsets3x3[2]);
	vec3 s4 = load_color(group_pos + kOffsets3x3[3]);
	vec3 s5 = load_color(group_pos + kOffsets3x3[4]);
	vec3 s6 = load_color(group_pos + kOffsets3x3[5]);
	vec3 s7 = load_color(group_pos + kOffsets3x3[6]);
	vec3 s8 = load_color(group_pos + kOffsets3x3[7]);
	vec3 s9 = load_color(group_pos + kOffsets3x3[8]);

	// Compute min and max (with an adaptive box size, which greatly reduces ghosting)
	vec3 color_avg = (s1 + s2 + s3 + s4 + s5 + s6 + s7 + s8 + s9) * RPC_9;
	vec3 color_avg2 = ((s1 * s1) + (s2 * s2) + (s3 * s3) + (s4 * s4) + (s5 * s5) + (s6 * s6) + (s7 * s7) + (s8 * s8) + (s9 * s9)) * RPC_9;
	float box_size = mix(0.0f, 2.5f, smoothstep(0.02f, 0.0f, length(velocity_closest)));
	vec3 dev = sqrt(abs(color_avg2 - (color_avg * color_avg))) * box_size;
	vec3 color_min = color_avg - dev;
	vec3 color_max = color_avg + dev;

	// Variance clipping
	vec3 color = clip_aabb(color_min, color_max, clamp(color_avg, color_min, color_max), color_history);

	// Clamp to prevent NaNs
	color = clamp(color, FLT_MIN, FLT_MAX);

	return color;
}

/*------------------------------------------------------------------------------
									TAA
------------------------------------------------------------------------------*/

const vec3 lumCoeff = vec3(0.299f, 0.587f, 0.114f);

float luminance(vec3 color) {
	return max(dot(color, lumCoeff), 0.0001f);
}

// This is "velocity disocclusion" as described by https://www.elopezr.com/temporal-aa-and-the-quest-for-the-holy-trail/.
// We use texel space, so our scale and threshold differ.
float get_factor_disocclusion(vec2 uv_reprojected, vec2 velocity) {
	vec2 velocity_previous = imageLoad(last_velocity_buffer, ivec2(uv_reprojected * params.resolution)).xy;
	vec2 velocity_texels = velocity * params.resolution;
	vec2 prev_velocity_texels = velocity_previous * params.resolution;
	float disocclusion = length(prev_velocity_texels - velocity_texels) - params.disocclusion_threshold;
	return clamp(disocclusion * params.disocclusion_scale, 0.0, 1.0);
}

vec3 temporal_antialiasing(uvec2 pos_group_top_left, uvec2 pos_group, uvec2 pos_screen, vec2 uv, sampler2D tex_history) {
	// Get the velocity of the current pixel
	vec2 velocity = imageLoad(velocity_buffer, ivec2(pos_screen)).xy;

	// Get reprojected uv
	vec2 uv_reprojected = uv + velocity;

	// Get input color
	vec3 color_input = load_color(pos_group);

	// Get history color (catmull-rom reduces a lot of the blurring that you get under motion)
	vec3 color_history = sample_catmull_rom_9(tex_history, uv_reprojected, params.resolution).rgb;

	// Clip history to the neighbourhood of the current sample (fixes a lot of the ghosting).
	vec2 velocity_closest = vec2(0.0); // This is best done by using the velocity with the closest depth.
	get_closest_pixel_velocity_3x3(pos_group, pos_group_top_left, velocity_closest);
	color_history = clip_history_3x3(pos_group, color_history, velocity_closest);

	// Compute blend factor
	float blend_factor = RPC_16; // We want to be able to accumulate as many jitter samples as we generated, that is, 16.
	{
		// If re-projected UV is out of screen, converge to current color immediately.
		float factor_screen = any(lessThan(uv_reprojected, vec2(0.0))) || any(greaterThan(uv_reprojected, vec2(1.0))) ? 1.0 : 0.0;

		// Increase blend factor when there is disocclusion (fixes a lot of the remaining ghosting).
		float factor_disocclusion = get_factor_disocclusion(uv_reprojected, velocity);

		// Add to the blend factor
		blend_factor = clamp(blend_factor + factor_screen + factor_disocclusion, 0.0, 1.0);
	}

	// Resolve
	vec3 color_resolved = vec3(0.0);
	{
		// Tonemap
		color_history = reinhard(color_history);
		color_input = reinhard(color_input);

		// Reduce flickering
		float lum_color = luminance(color_input);
		float lum_history = luminance(color_history);
		float diff = abs(lum_color - lum_history) / max(lum_color, max(lum_history, 1.001));
		diff = 1.0 - diff;
		diff = diff * diff;
		blend_factor = mix(0.0, blend_factor, diff);

		// Lerp/blend
		color_resolved = mix(color_history, color_input, blend_factor);

		// Inverse tonemap
		color_resolved = reinhard_inverse(color_resolved);
	}

	return color_resolved;
}

void main() {
#ifdef USE_SUBGROUPS
	populate_group_shared_memory(gl_WorkGroupID.xy, gl_LocalInvocationIndex);
#endif

	// Out of bounds check
	if (any(greaterThanEqual(vec2(gl_GlobalInvocationID.xy), params.resolution))) {
		return;
	}

#ifdef USE_SUBGROUPS
	const uvec2 pos_group = gl_LocalInvocationID.xy;
	const uvec2 pos_group_top_left = gl_WorkGroupID.xy * kGroupSize - kBorderSize;
#else
	const uvec2 pos_group = gl_GlobalInvocationID.xy;
	const uvec2 pos_group_top_left = uvec2(0, 0);
#endif
	const uvec2 pos_screen = gl_GlobalInvocationID.xy;
	const vec2 uv = (gl_GlobalInvocationID.xy + 0.5f) / params.resolution;

	vec3 result = temporal_antialiasing(pos_group_top_left, pos_group, pos_screen, uv, history_buffer);
	imageStore(output_buffer, ivec2(gl_GlobalInvocationID.xy), vec4(result, 1.0));
}

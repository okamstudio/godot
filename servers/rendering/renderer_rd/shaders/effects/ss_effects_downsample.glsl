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

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(push_constant, std430) uniform Params {
	vec2 pixel_size;
	float radius_sq;
	uint pad;
	mat2 proj_zw; // Bottom-right 2x2 corner of the projection matrix with reverse-z and z-remap applied
}
params;

layout(set = 0, binding = 0) uniform sampler2D source_depth;

layout(r16f, set = 1, binding = 0) uniform restrict writeonly image2DArray dest_image0; //rename
#ifdef GENERATE_MIPS
layout(r16f, set = 2, binding = 0) uniform restrict writeonly image2DArray dest_image1;
layout(r16f, set = 2, binding = 1) uniform restrict writeonly image2DArray dest_image2;
layout(r16f, set = 2, binding = 2) uniform restrict writeonly image2DArray dest_image3;
#ifdef GENERATE_FULL_MIPS
layout(r16f, set = 2, binding = 3) uniform restrict writeonly image2DArray dest_image4;
#endif
#endif

vec4 screen_space_to_view_space_depth(vec4 p_depth) {
	return -(params.proj_zw[1][0] - params.proj_zw[1][1] * p_depth) / (params.proj_zw[0][1] * p_depth - params.proj_zw[0][0]);
}

float screen_space_to_view_space_depth(float p_depth) {
	return -(params.proj_zw[1][0] - params.proj_zw[1][1] * p_depth) / (params.proj_zw[0][1] * p_depth - params.proj_zw[0][0]);
}

#ifdef GENERATE_MIPS

shared float depth_buffer[4][8][8];

float mip_smart_average(vec4 p_depths) {
	float closest = min(min(p_depths.x, p_depths.y), min(p_depths.z, p_depths.w));
	float fallof_sq = -1.0f / params.radius_sq;
	vec4 dists = p_depths - closest.xxxx;
	vec4 weights = clamp(dists * dists * fallof_sq + 1.0, 0.0, 1.0);
	return dot(weights, p_depths) / dot(weights, vec4(1.0, 1.0, 1.0, 1.0));
}

void prepare_depths_and_mips(vec4 p_samples, uvec2 p_output_coord, uvec2 p_gtid) {
	p_samples = screen_space_to_view_space_depth(p_samples);

	depth_buffer[0][p_gtid.x][p_gtid.y] = p_samples.w;
	depth_buffer[1][p_gtid.x][p_gtid.y] = p_samples.z;
	depth_buffer[2][p_gtid.x][p_gtid.y] = p_samples.x;
	depth_buffer[3][p_gtid.x][p_gtid.y] = p_samples.y;

	imageStore(dest_image0, ivec3(p_output_coord.x, p_output_coord.y, 0), vec4(p_samples.w));
	imageStore(dest_image0, ivec3(p_output_coord.x, p_output_coord.y, 1), vec4(p_samples.z));
	imageStore(dest_image0, ivec3(p_output_coord.x, p_output_coord.y, 2), vec4(p_samples.x));
	imageStore(dest_image0, ivec3(p_output_coord.x, p_output_coord.y, 3), vec4(p_samples.y));

	uint depth_array_index = 2 * (p_gtid.y % 2) + (p_gtid.x % 2);
	uvec2 depth_array_offset = ivec2(p_gtid.x % 2, p_gtid.y % 2);
	ivec2 buffer_coord = ivec2(p_gtid) - ivec2(depth_array_offset);

	p_output_coord /= 2;
	groupMemoryBarrier();
	barrier();

	// if (still_alive) <-- all threads alive here
	{
		float sample_00 = depth_buffer[depth_array_index][buffer_coord.x + 0][buffer_coord.y + 0];
		float sample_01 = depth_buffer[depth_array_index][buffer_coord.x + 0][buffer_coord.y + 1];
		float sample_10 = depth_buffer[depth_array_index][buffer_coord.x + 1][buffer_coord.y + 0];
		float sample_11 = depth_buffer[depth_array_index][buffer_coord.x + 1][buffer_coord.y + 1];

		float avg = mip_smart_average(vec4(sample_00, sample_01, sample_10, sample_11));
		imageStore(dest_image1, ivec3(p_output_coord.x, p_output_coord.y, depth_array_index), vec4(avg));
		depth_buffer[depth_array_index][buffer_coord.x][buffer_coord.y] = avg;
	}

	bool still_alive = p_gtid.x % 4 == depth_array_offset.x && p_gtid.y % 4 == depth_array_offset.y;

	p_output_coord /= 2;
	groupMemoryBarrier();
	barrier();

	if (still_alive) {
		float sample_00 = depth_buffer[depth_array_index][buffer_coord.x + 0][buffer_coord.y + 0];
		float sample_01 = depth_buffer[depth_array_index][buffer_coord.x + 0][buffer_coord.y + 2];
		float sample_10 = depth_buffer[depth_array_index][buffer_coord.x + 2][buffer_coord.y + 0];
		float sample_11 = depth_buffer[depth_array_index][buffer_coord.x + 2][buffer_coord.y + 2];

		float avg = mip_smart_average(vec4(sample_00, sample_01, sample_10, sample_11));
		imageStore(dest_image2, ivec3(p_output_coord.x, p_output_coord.y, depth_array_index), vec4(avg));
		depth_buffer[depth_array_index][buffer_coord.x][buffer_coord.y] = avg;
	}

	still_alive = p_gtid.x % 8 == depth_array_offset.x && depth_array_offset.y % 8 == depth_array_offset.y;

	p_output_coord /= 2;
	groupMemoryBarrier();
	barrier();

	if (still_alive) {
		float sample_00 = depth_buffer[depth_array_index][buffer_coord.x + 0][buffer_coord.y + 0];
		float sample_01 = depth_buffer[depth_array_index][buffer_coord.x + 0][buffer_coord.y + 4];
		float sample_10 = depth_buffer[depth_array_index][buffer_coord.x + 4][buffer_coord.y + 0];
		float sample_11 = depth_buffer[depth_array_index][buffer_coord.x + 4][buffer_coord.y + 4];

		float avg = mip_smart_average(vec4(sample_00, sample_01, sample_10, sample_11));
		imageStore(dest_image3, ivec3(p_output_coord.x, p_output_coord.y, depth_array_index), vec4(avg));
#ifndef GENERATE_FULL_MIPS
	}
#else
		depth_buffer[depth_array_index][buffer_coord.x][buffer_coord.y] = avg;
	}
	still_alive = p_gtid.x % 16 == depth_array_offset.x && depth_array_offset.y % 16 == depth_array_offset.y;

	p_output_coord /= 2;

	if (still_alive) {
		// Use the previous average, not ideal, but still not bad.
		float sample_00 = depth_buffer[depth_array_index][buffer_coord.x + 0][buffer_coord.y + 0];
		imageStore(dest_image4, ivec3(p_output_coord.x, p_output_coord.y, depth_array_index), vec4(sample_00));
	}
#endif
}
#else
#ifndef USE_HALF_BUFFERS
void prepare_depths(vec4 p_samples, uvec2 p_tid) {
	p_samples = screen_space_to_view_space_depth(p_samples);

	imageStore(dest_image0, ivec3(p_tid, 0), vec4(p_samples.w));
	imageStore(dest_image0, ivec3(p_tid, 1), vec4(p_samples.z));
	imageStore(dest_image0, ivec3(p_tid, 2), vec4(p_samples.x));
	imageStore(dest_image0, ivec3(p_tid, 3), vec4(p_samples.y));
}
#endif
#endif

void main() {
#ifdef USE_HALF_BUFFERS
// Half buffers means that we divide depth into two half res buffers (we only capture 1/4 of pixels).
#ifdef USE_HALF_SIZE
	float sample_00 = texelFetch(source_depth, ivec2(4 * gl_GlobalInvocationID.x + 0, 4 * gl_GlobalInvocationID.y + 0), 0).x;
	float sample_11 = texelFetch(source_depth, ivec2(4 * gl_GlobalInvocationID.x + 2, 4 * gl_GlobalInvocationID.y + 2), 0).x;
#else
	float sample_00 = texelFetch(source_depth, ivec2(2 * gl_GlobalInvocationID.x + 0, 2 * gl_GlobalInvocationID.y + 0), 0).x;
	float sample_11 = texelFetch(source_depth, ivec2(2 * gl_GlobalInvocationID.x + 1, 2 * gl_GlobalInvocationID.y + 1), 0).x;
#endif
	sample_00 = screen_space_to_view_space_depth(sample_00);
	sample_11 = screen_space_to_view_space_depth(sample_11);

	imageStore(dest_image0, ivec3(gl_GlobalInvocationID.xy, 0), vec4(sample_00));
	imageStore(dest_image0, ivec3(gl_GlobalInvocationID.xy, 3), vec4(sample_11));
#else //!USE_HALF_BUFFERS
#ifdef USE_HALF_SIZE
	ivec2 depth_buffer_coord = 4 * ivec2(gl_GlobalInvocationID.xy);
	ivec2 output_coord = ivec2(gl_GlobalInvocationID);

	vec2 uv = (vec2(depth_buffer_coord) + 0.5f) * params.pixel_size;
	vec4 samples;
	samples.x = textureLodOffset(source_depth, uv, 0, ivec2(0, 2)).x;
	samples.y = textureLodOffset(source_depth, uv, 0, ivec2(2, 2)).x;
	samples.z = textureLodOffset(source_depth, uv, 0, ivec2(2, 0)).x;
	samples.w = textureLodOffset(source_depth, uv, 0, ivec2(0, 0)).x;
#else
	ivec2 depth_buffer_coord = 2 * ivec2(gl_GlobalInvocationID.xy);
	ivec2 output_coord = ivec2(gl_GlobalInvocationID);

	vec2 uv = (vec2(depth_buffer_coord) + 0.5f) * params.pixel_size;
	vec4 samples = textureGather(source_depth, uv);
#endif //USE_HALF_SIZE
#ifdef GENERATE_MIPS
	prepare_depths_and_mips(samples, output_coord, gl_LocalInvocationID.xy);
#else
	prepare_depths(samples, gl_GlobalInvocationID.xy);
#endif
#endif //USE_HALF_BUFFERS
}

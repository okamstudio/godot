#[compute]

#version 450

#VERSION_DEFINES

#include "motion_vector_inc.glsl"

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(set = 0, binding = 0) uniform sampler2D depth_buffer;
layout(rg16f, set = 0, binding = 1) uniform restrict writeonly image2D velocity_buffer;

layout(push_constant, std430) uniform Params {
	highp mat4 reprojection_matrix;
	vec2 resolution;
	uint pad[2];
}
params;

void main() {
	// Out of bounds check.
	if (any(greaterThanEqual(vec2(gl_GlobalInvocationID.xy), params.resolution))) {
		return;
	}

	ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

	float depth = texelFetch(depth_buffer, pos, 0).x;
	vec2 uv = (vec2(pos) + 0.5f) / params.resolution;
	vec2 velocity = derive_motion_vector(uv, depth, params.reprojection_matrix);
	imageStore(velocity_buffer, pos, vec4(velocity, 0.0f, 0.0f));
}

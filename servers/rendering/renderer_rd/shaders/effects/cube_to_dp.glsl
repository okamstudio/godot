#[vertex]

#version 450

#VERSION_DEFINES

layout(push_constant, std430) uniform Params {
	float[2][2] proj_zw; // Bottom-right 2x2 corner of the projection matrix in OpenGL format (no reverse-z, no Y-flip, no z-remap)
	float z_far;
	uint pad;
	vec2 texel_size;
}
params;

layout(location = 0) out vec2 uv_interp;

void main() {
	vec2 base_arr[4] = vec2[](vec2(0.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 1.0), vec2(1.0, 0.0));
	uv_interp = base_arr[gl_VertexIndex];
	gl_Position = vec4(uv_interp * 2.0 - 1.0, 0.0, 1.0);
}

#[fragment]

#version 450

#VERSION_DEFINES

layout(location = 0) in vec2 uv_interp;

layout(set = 0, binding = 0) uniform samplerCube source_cube;

layout(push_constant, std430) uniform Params {
	float[2][2] proj_zw; // Bottom-right 2x2 corner of the projection matrix in OpenGL format (no reverse-z, no Y-flip, no z-remap)
	float z_far;
	uint pad;
	vec2 texel_size;
}
params;

void main() {
	vec2 uv = uv_interp;
	vec2 texel_size = abs(params.texel_size);

	uv = clamp(uv * (1.0 + 2.0 * texel_size) - texel_size, vec2(0.0), vec2(1.0));

	vec3 normal = vec3(uv * 2.0 - 1.0, 0.0);
	normal.z = 0.5 * (1.0 - dot(normal.xy, normal.xy)); // z = 1/2 - 1/2 * (x^2 + y^2)
	normal = normalize(normal);

	normal.y = -normal.y; //needs to be flipped to match projection matrix
	if (params.texel_size.x >= 0.0) { // Sign is used to encode Z flip
		normal.z = -normal.z;
	}

	float depth = texture(source_cube, normal).r;

	// absolute values for direction cosines, bigger value equals closer to basis axis
	vec3 unorm = abs(normal);

	if ((unorm.x >= unorm.y) && (unorm.x >= unorm.z)) {
		// x code
		unorm = normal.x > 0.0 ? vec3(1.0, 0.0, 0.0) : vec3(-1.0, 0.0, 0.0);
	} else if ((unorm.y > unorm.x) && (unorm.y >= unorm.z)) {
		// y code
		unorm = normal.y > 0.0 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, -1.0, 0.0);
	} else if ((unorm.z > unorm.x) && (unorm.z > unorm.y)) {
		// z code
		unorm = normal.z > 0.0 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 0.0, -1.0);
	} else {
		// oh-no we messed up code
		// has to be
		unorm = vec3(1.0, 0.0, 0.0);
	}

	float depth_fix = 1.0 / dot(normal, unorm);

	depth = 2.0 * depth - 1.0;
	float linear_depth = (params.proj_zw[1][1] * depth + params.proj_zw[1][0]) / (params.proj_zw[0][1] * depth + params.proj_zw[0][0]);
	// linear_depth equal to view space depth
	depth = (params.z_far - linear_depth * depth_fix) / params.z_far;
	gl_FragDepth = depth;
}

#[vertex]

#version 450

#VERSION_DEFINES

struct CellData {
	uint position; // xyz 10 bits
	uint albedo; //rgb albedo
	uint emission; //rgb normalized with e as multiplier
	uint normal; //RGB normal encoded
};

layout(set = 0, binding = 1, std140) buffer CellDataBuffer {
	CellData data[];
}
cell_data;

layout(set = 0, binding = 2) uniform texture3D color_tex;

layout(set = 0, binding = 3) uniform sampler tex_sampler;

layout(push_constant, std430) uniform Params {
	mat4 projection;
	uint cell_offset;
	float dynamic_range;
	float alpha;
	uint level;
	ivec3 bounds;
	uint pad;
}
params;

layout(location = 0) out vec4 color_interp;

void main() {
	const vec3 cube_triangles[36] = vec3[](
			vec3(-1.0f, -1.0f, -1.0f),
			vec3(-1.0f, -1.0f, 1.0f),
			vec3(-1.0f, 1.0f, 1.0f),
			vec3(1.0f, 1.0f, -1.0f),
			vec3(-1.0f, -1.0f, -1.0f),
			vec3(-1.0f, 1.0f, -1.0f),
			vec3(1.0f, -1.0f, 1.0f),
			vec3(-1.0f, -1.0f, -1.0f),
			vec3(1.0f, -1.0f, -1.0f),
			vec3(1.0f, 1.0f, -1.0f),
			vec3(1.0f, -1.0f, -1.0f),
			vec3(-1.0f, -1.0f, -1.0f),
			vec3(-1.0f, -1.0f, -1.0f),
			vec3(-1.0f, 1.0f, 1.0f),
			vec3(-1.0f, 1.0f, -1.0f),
			vec3(1.0f, -1.0f, 1.0f),
			vec3(-1.0f, -1.0f, 1.0f),
			vec3(-1.0f, -1.0f, -1.0f),
			vec3(-1.0f, 1.0f, 1.0f),
			vec3(-1.0f, -1.0f, 1.0f),
			vec3(1.0f, -1.0f, 1.0f),
			vec3(1.0f, 1.0f, 1.0f),
			vec3(1.0f, -1.0f, -1.0f),
			vec3(1.0f, 1.0f, -1.0f),
			vec3(1.0f, -1.0f, -1.0f),
			vec3(1.0f, 1.0f, 1.0f),
			vec3(1.0f, -1.0f, 1.0f),
			vec3(1.0f, 1.0f, 1.0f),
			vec3(1.0f, 1.0f, -1.0f),
			vec3(-1.0f, 1.0f, -1.0f),
			vec3(1.0f, 1.0f, 1.0f),
			vec3(-1.0f, 1.0f, -1.0f),
			vec3(-1.0f, 1.0f, 1.0f),
			vec3(1.0f, 1.0f, 1.0f),
			vec3(-1.0f, 1.0f, 1.0f),
			vec3(1.0f, -1.0f, 1.0f));

	vec3 vertex = cube_triangles[gl_VertexIndex] * 0.5 + 0.5;
#ifdef MODE_DEBUG_LIGHT_FULL
	uvec3 posu = uvec3(gl_InstanceIndex % params.bounds.x, (gl_InstanceIndex / params.bounds.x) % params.bounds.y, gl_InstanceIndex / (params.bounds.y * params.bounds.x));
#else
	uint cell_index = gl_InstanceIndex + params.cell_offset;

	uvec3 posu = uvec3(cell_data.data[cell_index].position & 0x7FF, (cell_data.data[cell_index].position >> 11) & 0x3FF, cell_data.data[cell_index].position >> 21);
#endif

#ifdef MODE_DEBUG_EMISSION
	color_interp.xyz = vec3(uvec3(cell_data.data[cell_index].emission & 0x1ff, (cell_data.data[cell_index].emission >> 9) & 0x1ff, (cell_data.data[cell_index].emission >> 18) & 0x1ff)) * pow(2.0, float(cell_data.data[cell_index].emission >> 27) - 15.0 - 9.0);
#endif

#ifdef MODE_DEBUG_COLOR
	color_interp.xyz = unpackUnorm4x8(cell_data.data[cell_index].albedo).xyz;
#endif

#ifdef MODE_DEBUG_LIGHT
	color_interp = texelFetch(sampler3D(color_tex, tex_sampler), ivec3(posu), int(params.level));
	color_interp.xyz *params.dynamic_range;
#endif

	float scale = (1 << params.level);

	gl_Position = params.projection * vec4((vec3(posu) + vertex) * scale, 1.0);

#ifdef MODE_DEBUG_LIGHT_FULL
	if (color_interp.a == 0.0) {
		gl_Position = vec4(0.0); //force clip and not draw
	}
#else
	color_interp.a = params.alpha;
#endif
}

#[fragment]

#version 450

#VERSION_DEFINES

layout(location = 0) in vec4 color_interp;
layout(location = 0) out vec4 frag_color;

void main() {
	frag_color = color_interp;

#ifdef MODE_DEBUG_LIGHT_FULL

	//there really is no alpha, so use dither

	int x = int(gl_FragCoord.x) % 4;
	int y = int(gl_FragCoord.y) % 4;
	int index = x + y * 4;
	float limit = 0.0;
	if (x < 8) {
		if (index == 0) {
			limit = 0.0625;
		}
		if (index == 1) {
			limit = 0.5625;
		}
		if (index == 2) {
			limit = 0.1875;
		}
		if (index == 3) {
			limit = 0.6875;
		}
		if (index == 4) {
			limit = 0.8125;
		}
		if (index == 5) {
			limit = 0.3125;
		}
		if (index == 6) {
			limit = 0.9375;
		}
		if (index == 7) {
			limit = 0.4375;
		}
		if (index == 8) {
			limit = 0.25;
		}
		if (index == 9) {
			limit = 0.75;
		}
		if (index == 10) {
			limit = 0.125;
		}
		if (index == 11) {
			limit = 0.625;
		}
		if (index == 12) {
			limit = 1.0;
		}
		if (index == 13) {
			limit = 0.5;
		}
		if (index == 14) {
			limit = 0.875;
		}
		if (index == 15) {
			limit = 0.375;
		}
	}
	if (frag_color.a < limit) {
		discard;
	}
#endif
}

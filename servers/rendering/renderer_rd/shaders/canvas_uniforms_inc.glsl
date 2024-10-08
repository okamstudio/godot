
#define MAX_LIGHTS_PER_ITEM 16

#define M_PI 3.14159265359

#define SDF_MAX_LENGTH 16384.0

//1 means enabled, 2+ means trails in use
#define FLAGS_INSTANCING_MASK 0x7F
#define FLAGS_INSTANCING_HAS_COLORS_SHIFT 7
#define FLAGS_INSTANCING_HAS_COLORS (1 << FLAGS_INSTANCING_HAS_COLORS_SHIFT)
#define FLAGS_INSTANCING_HAS_CUSTOM_DATA_SHIFT 8
#define FLAGS_INSTANCING_HAS_CUSTOM_DATA (1 << FLAGS_INSTANCING_HAS_CUSTOM_DATA_SHIFT)

#define FLAGS_CLIP_RECT_UV (1 << 9)
#define FLAGS_TRANSPOSE_RECT (1 << 10)
#define FLAGS_CONVERT_ATTRIBUTES_TO_LINEAR (1 << 11)
#define FLAGS_NINEPACH_DRAW_CENTER_SHIFT 12
#define FLAGS_NINEPACH_DRAW_CENTER (1 << FLAGS_NINEPACH_DRAW_CENTER_SHIFT)

#define FLAGS_NINEPATCH_H_MODE_SHIFT 16
#define FLAGS_NINEPATCH_V_MODE_SHIFT 18

#define FLAGS_LIGHT_COUNT_SHIFT 20

#define FLAGS_DEFAULT_NORMAL_MAP_USED (1 << 24)
#define FLAGS_DEFAULT_SPECULAR_MAP_USED (1 << 25)

#define FLAGS_USE_MSDF (1 << 26)
#define FLAGS_USE_LCD (1 << 27)

#define FLAGS_FLIP_H (1 << 28)
#define FLAGS_FLIP_V (1 << 29)

struct InstanceData {
	vec2 world_x;
	vec2 world_y;
	vec2 world_ofs;
	uint flags;
	uint specular_shininess;
#ifdef USE_PRIMITIVE
	vec2 points[3];
	vec2 uvs[3];
	uint colors[6];
#else
	vec4 modulation;
	vec4 ninepatch_margins;
	vec4 dst_rect; //for built-in rect and UV
	vec4 src_rect;
	vec2 pad;

#endif
	vec2 color_texture_pixel_size;
	uint lights[4];
};

layout(push_constant, std430) uniform Params {
	uint base_instance_index; // base index to instance data
	uint sc_packed_0;
	uint pad2;
	uint pad3;
}
params;

// Specialization constants.

#ifdef UBERSHADER

// Pull the constants from the draw call's push constants.
uint sc_packed_0() {
	return draw_call.sc_packed_0;
}

#else

// Pull the constants from the pipeline's specialization constants.
layout(constant_id = 0) const uint pso_sc_packed_0 = 0;

uint sc_packed_0() {
	return pso_sc_packed_0;
}

#endif

bool sc_use_lighting() {
	return ((sc_packed_0() >> 0) & 1U) != 0;
}

// In vulkan, sets should always be ordered using the following logic:
// Lower Sets: Sets that change format and layout less often
// Higher sets: Sets that change format and layout very often
// This is because changing a set for another with a different layout or format,
// invalidates all the upper ones (as likely internal base offset changes)

/* SET0: Globals */

// The values passed per draw primitives are cached within it

layout(set = 0, binding = 1, std140) uniform CanvasData {
	mat4 canvas_transform;
	mat4 screen_transform;
	mat4 canvas_normal_transform;
	vec4 canvas_modulation;
	vec2 screen_pixel_size;
	float time;
	bool use_pixel_snap;

	vec4 sdf_to_tex;
	vec2 screen_to_sdf;
	vec2 sdf_to_screen;

	uint directional_light_count;
	float tex_to_sdf;
	uint pad1;
	uint pad2;
}
canvas_data;

#define LIGHT_FLAGS_BLEND_MASK (3 << 16)
#define LIGHT_FLAGS_BLEND_MODE_ADD (0 << 16)
#define LIGHT_FLAGS_BLEND_MODE_SUB (1 << 16)
#define LIGHT_FLAGS_BLEND_MODE_MIX (2 << 16)
#define LIGHT_FLAGS_BLEND_MODE_MASK (3 << 16)
#define LIGHT_FLAGS_HAS_SHADOW (1 << 20)
#define LIGHT_FLAGS_FILTER_SHIFT 22
#define LIGHT_FLAGS_FILTER_MASK (3 << 22)
#define LIGHT_FLAGS_SHADOW_NEAREST (0 << 22)
#define LIGHT_FLAGS_SHADOW_PCF5 (1 << 22)
#define LIGHT_FLAGS_SHADOW_PCF13 (2 << 22)

struct Light {
	mat2x4 texture_matrix; //light to texture coordinate matrix (transposed)
	mat2x4 shadow_matrix; //light to shadow coordinate matrix (transposed)
	vec4 color;

	uint shadow_color; // packed
	uint flags; //index to light texture
	float shadow_pixel_size;
	float height;

	vec2 position;
	float shadow_zfar_inv;
	float shadow_y_ofs;

	vec4 atlas_rect;
};

layout(set = 0, binding = 2, std140) uniform LightData {
	Light data[MAX_LIGHTS];
}
light_array;

layout(set = 0, binding = 3) uniform texture2D atlas_texture;
layout(set = 0, binding = 4) uniform texture2D shadow_atlas_texture;

layout(set = 0, binding = 5) uniform sampler shadow_sampler;

layout(set = 0, binding = 6) uniform texture2D color_buffer;
layout(set = 0, binding = 7) uniform texture2D sdf_texture;

#include "samplers_inc.glsl"

layout(set = 0, binding = 9, std430) restrict readonly buffer GlobalShaderUniformData {
	vec4 data[];
}
global_shader_uniforms;

/* SET1: Is reserved for the material */

//

/* SET2: Instancing and Skeleton */

layout(set = 2, binding = 0, std430) restrict readonly buffer Transforms {
	vec4 data[];
}
transforms;

/* SET3: Texture */

layout(set = 3, binding = 0) uniform texture2D color_texture;
layout(set = 3, binding = 1) uniform texture2D normal_texture;
layout(set = 3, binding = 2) uniform texture2D specular_texture;
layout(set = 3, binding = 3) uniform sampler texture_sampler;

layout(set = 3, binding = 4, std430) restrict readonly buffer DrawData {
	InstanceData data[];
}
instances;

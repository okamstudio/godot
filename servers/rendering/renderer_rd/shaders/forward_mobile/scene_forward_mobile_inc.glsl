#define M_PI 3.14159265359
#define MAX_VIEWS 2

#if defined(USE_MULTIVIEW) && defined(has_VK_KHR_multiview)
#extension GL_EXT_multiview : enable
#endif

#include "../decal_data_inc.glsl"
#include "../scene_data_inc.glsl"

#if !defined(MODE_RENDER_DEPTH) || defined(MODE_RENDER_MATERIAL) || defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)
#ifndef NORMAL_USED
#define NORMAL_USED
#endif
#endif

#define USING_MOBILE_RENDERER

layout(push_constant, std430) uniform DrawCall {
	vec2 uv_offset;
	uint instance_index;
	uint pad;
#ifdef UBERSHADER
	uint sc_packed_0;
	uint sc_packed_1;
	uint sc_packed_2;
	float sc_packed_3;
	uint uc_packed_0;
	uint uc_padding_1;
	uint uc_padding_2;
	uint uc_padding_3;
#endif
}
draw_call;

/* Specialization Constants */

#ifdef UBERSHADER

#define POLYGON_CULL_DISABLED 0
#define POLYGON_CULL_FRONT 1
#define POLYGON_CULL_BACK 2

// Pull the constants from the draw call's push constants.
uint sc_packed_0() {
	return draw_call.sc_packed_0;
}

uint sc_packed_1() {
	return draw_call.sc_packed_1;
}

uint sc_packed_2() {
	return draw_call.sc_packed_2;
}

float sc_packed_3() {
	return draw_call.sc_packed_3;
}

uint uc_cull_mode() {
	return (draw_call.uc_packed_0 >> 0) & 3U;
}

#else

// Pull the constants from the pipeline's specialization constants.
layout(constant_id = 0) const uint pso_sc_packed_0 = 0;
layout(constant_id = 1) const uint pso_sc_packed_1 = 0;
layout(constant_id = 2) const uint pso_sc_packed_2 = 0;
layout(constant_id = 3) const float pso_sc_packed_3 = 2.0;

uint sc_packed_0() {
	return pso_sc_packed_0;
}

uint sc_packed_1() {
	return pso_sc_packed_1;
}

uint sc_packed_2() {
	return pso_sc_packed_2;
}

float sc_packed_3() {
	return pso_sc_packed_3;
}

#endif

bool sc_use_light_projector() {
	return ((sc_packed_0() >> 0) & 1U) != 0;
}

bool sc_use_light_soft_shadows() {
	return ((sc_packed_0() >> 1) & 1U) != 0;
}

bool sc_use_directional_soft_shadows() {
	return ((sc_packed_0() >> 2) & 1U) != 0;
}

bool sc_decal_use_mipmaps() {
	return ((sc_packed_0() >> 3) & 1U) != 0;
}

bool sc_projector_use_mipmaps() {
	return ((sc_packed_0() >> 4) & 1U) != 0;
}

bool sc_disable_fog() {
	return ((sc_packed_0() >> 5) & 1U) != 0;
}

bool sc_use_depth_fog() {
	return ((sc_packed_0() >> 6) & 1U) != 0;
}

bool sc_use_fog_aerial_perspective() {
	return ((sc_packed_0() >> 7) & 1U) != 0;
}

bool sc_use_fog_sun_scatter() {
	return ((sc_packed_0() >> 8) & 1U) != 0;
}

bool sc_use_fog_height_density() {
	return ((sc_packed_0() >> 9) & 1U) != 0;
}

bool sc_use_lightmap_bicubic_filter() {
	return ((sc_packed_0() >> 10) & 1U) != 0;
}

bool sc_multimesh() {
	return ((sc_packed_0() >> 11) & 1U) != 0;
}

bool sc_multimesh_format_2d() {
	return ((sc_packed_0() >> 12) & 1U) != 0;
}

bool sc_multimesh_has_color() {
	return ((sc_packed_0() >> 13) & 1U) != 0;
}

bool sc_multimesh_has_custom_data() {
	return ((sc_packed_0() >> 14) & 1U) != 0;
}

bool sc_scene_use_ambient_cubemap() {
	return ((sc_packed_0() >> 15) & 1U) != 0;
}

bool sc_scene_use_reflection_cubemap() {
	return ((sc_packed_0() >> 16) & 1U) != 0;
}

bool sc_scene_roughness_limiter_enabled() {
	return ((sc_packed_0() >> 17) & 1U) != 0;
}

uint sc_soft_shadow_samples() {
	return (sc_packed_0() >> 20) & 63U;
}

uint sc_penumbra_shadow_samples() {
	return (sc_packed_0() >> 26) & 63U;
}

uint sc_directional_soft_shadow_samples() {
	return (sc_packed_1() >> 0) & 63U;
}

uint sc_directional_penumbra_shadow_samples() {
	return (sc_packed_1() >> 6) & 63U;
}

uint sc_omni_lights() {
	return (sc_packed_1() >> 12) & 15U;
}

uint sc_spot_lights() {
	return (sc_packed_1() >> 16) & 15U;
}

uint sc_reflection_probes() {
	return (sc_packed_1() >> 20) & 15U;
}

uint sc_directional_lights() {
	return (sc_packed_1() >> 24) & 15U;
}

uint sc_decals() {
	return (sc_packed_1() >> 28) & 15U;
}

bool sc_directional_light_blend_split(uint i) {
	return ((sc_packed_2() >> i) & 1U) != 0;
}

float sc_luminance_multiplier() {
	return sc_packed_3();
}

/* Set 0: Base Pass (never changes) */

#include "../light_data_inc.glsl"

layout(set = 0, binding = 2) uniform sampler shadow_sampler;

#define INSTANCE_FLAGS_DYNAMIC (1 << 3)
#define INSTANCE_FLAGS_NON_UNIFORM_SCALE (1 << 4)
#define INSTANCE_FLAGS_USE_GI_BUFFERS (1 << 5)
#define INSTANCE_FLAGS_USE_SDFGI (1 << 6)
#define INSTANCE_FLAGS_USE_LIGHTMAP_CAPTURE (1 << 7)
#define INSTANCE_FLAGS_USE_LIGHTMAP (1 << 8)
#define INSTANCE_FLAGS_USE_SH_LIGHTMAP (1 << 9)
#define INSTANCE_FLAGS_USE_VOXEL_GI (1 << 10)
#define INSTANCE_FLAGS_PARTICLES (1 << 11)
#define INSTANCE_FLAGS_PARTICLE_TRAIL_SHIFT 16
//3 bits of stride
#define INSTANCE_FLAGS_PARTICLE_TRAIL_MASK 0xFF

layout(set = 0, binding = 3, std430) restrict readonly buffer OmniLights {
	LightData data[];
}
omni_lights;

layout(set = 0, binding = 4, std430) restrict readonly buffer SpotLights {
	LightData data[];
}
spot_lights;

layout(set = 0, binding = 5, std430) restrict readonly buffer ReflectionProbeData {
	ReflectionData data[];
}
reflections;

layout(set = 0, binding = 6, std140) uniform DirectionalLights {
	DirectionalLightData data[MAX_DIRECTIONAL_LIGHT_DATA_STRUCTS];
}
directional_lights;

#define LIGHTMAP_FLAG_USE_DIRECTION 1
#define LIGHTMAP_FLAG_USE_SPECULAR_DIRECTION 2

#define LIGHTMAP_SHADOWMASK_MODE_NONE 0
#define LIGHTMAP_SHADOWMASK_MODE_REPLACE 1
#define LIGHTMAP_SHADOWMASK_MODE_OVERLAY 2
#define LIGHTMAP_SHADOWMASK_MODE_ONLY 3

struct Lightmap {
	mediump mat3 normal_xform;
	vec2 light_texture_size;
	float exposure_normalization;
	uint flags;
};

layout(set = 0, binding = 7, std140) restrict readonly buffer Lightmaps {
	Lightmap data[];
}
lightmaps;

struct LightmapCapture {
	mediump vec4 sh[9];
};

layout(set = 0, binding = 8, std140) restrict readonly buffer LightmapCaptures {
	LightmapCapture data[];
}
lightmap_captures;

layout(set = 0, binding = 9) uniform mediump texture2D decal_atlas;
layout(set = 0, binding = 10) uniform mediump texture2D decal_atlas_srgb;

layout(set = 0, binding = 11, std430) restrict readonly buffer Decals {
	DecalData data[];
}
decals;

layout(set = 0, binding = 12, std430) restrict readonly buffer GlobalShaderUniformData {
	highp vec4 data[];
}
global_shader_uniforms;

layout(set = 0, binding = 13) uniform sampler DEFAULT_SAMPLER_LINEAR_WITH_MIPMAPS_CLAMP;

/* Set 1: Render Pass (changes per render pass) */

layout(set = 1, binding = 0, std140) uniform SceneDataBlock {
	SceneData data;
	SceneData prev_data;
}
scene_data_block;

struct InstanceData {
	highp mat4 transform; // 64 - 64
	uint flags; // 04 - 68
	uint instance_uniforms_ofs; // Base offset in global buffer for instance variables.	// 04 - 72
	uint gi_offset; // GI information when using lightmapping (VCT or lightmap index).    // 04 - 76
	uint layer_mask; // 04 - 80
	highp vec4 lightmap_uv_scale; // 16 - 96 Doubles as uv_offset when needed.

	uvec2 reflection_probes; // 08 - 104
	uvec2 omni_lights; // 08 - 112
	uvec2 spot_lights; // 08 - 120
	uvec2 decals; // 08 - 128

	vec4 compressed_aabb_position_pad; // 16 - 144 // Only .xyz is used. .w is padding.
	vec4 compressed_aabb_size_pad; // 16 - 160 // Only .xyz is used. .w is padding.
	vec4 uv_scale; // 16 - 176
};

layout(set = 1, binding = 1, std430) buffer restrict readonly InstanceDataBuffer {
	InstanceData data[];
}
instances;

#ifdef USE_RADIANCE_CUBEMAP_ARRAY

layout(set = 1, binding = 2) uniform mediump textureCubeArray radiance_cubemap;

#else

layout(set = 1, binding = 2) uniform mediump textureCube radiance_cubemap;

#endif

layout(set = 1, binding = 3) uniform mediump textureCubeArray reflection_atlas;

layout(set = 1, binding = 4) uniform highp texture2D shadow_atlas;

layout(set = 1, binding = 5) uniform highp texture2D directional_shadow_atlas;

// this needs to change to providing just the lightmap we're using..
layout(set = 1, binding = 6) uniform texture2DArray lightmap_textures[MAX_LIGHTMAP_TEXTURES * 2];

#ifdef USE_MULTIVIEW
layout(set = 1, binding = 9) uniform highp texture2DArray depth_buffer;
layout(set = 1, binding = 10) uniform mediump texture2DArray color_buffer;
#define multiviewSampler sampler2DArray
#else
layout(set = 1, binding = 9) uniform highp texture2D depth_buffer;
layout(set = 1, binding = 10) uniform mediump texture2D color_buffer;
#define multiviewSampler sampler2D
#endif // USE_MULTIVIEW

layout(set = 1, binding = 11) uniform sampler decal_sampler;

layout(set = 1, binding = 12) uniform sampler light_projector_sampler;

layout(set = 1, binding = 13 + 0) uniform sampler SAMPLER_NEAREST_CLAMP;
layout(set = 1, binding = 13 + 1) uniform sampler SAMPLER_LINEAR_CLAMP;
layout(set = 1, binding = 13 + 2) uniform sampler SAMPLER_NEAREST_WITH_MIPMAPS_CLAMP;
layout(set = 1, binding = 13 + 3) uniform sampler SAMPLER_LINEAR_WITH_MIPMAPS_CLAMP;
layout(set = 1, binding = 13 + 4) uniform sampler SAMPLER_NEAREST_WITH_MIPMAPS_ANISOTROPIC_CLAMP;
layout(set = 1, binding = 13 + 5) uniform sampler SAMPLER_LINEAR_WITH_MIPMAPS_ANISOTROPIC_CLAMP;
layout(set = 1, binding = 13 + 6) uniform sampler SAMPLER_NEAREST_REPEAT;
layout(set = 1, binding = 13 + 7) uniform sampler SAMPLER_LINEAR_REPEAT;
layout(set = 1, binding = 13 + 8) uniform sampler SAMPLER_NEAREST_WITH_MIPMAPS_REPEAT;
layout(set = 1, binding = 13 + 9) uniform sampler SAMPLER_LINEAR_WITH_MIPMAPS_REPEAT;
layout(set = 1, binding = 13 + 10) uniform sampler SAMPLER_NEAREST_WITH_MIPMAPS_ANISOTROPIC_REPEAT;
layout(set = 1, binding = 13 + 11) uniform sampler SAMPLER_LINEAR_WITH_MIPMAPS_ANISOTROPIC_REPEAT;

/* Set 2 Skeleton & Instancing (can change per item) */

layout(set = 2, binding = 0, std430) restrict readonly buffer Transforms {
	highp vec4 data[];
}
transforms;

/* Set 3 User Material */

#define LIGHT_BAKE_DISABLED 0
#define LIGHT_BAKE_STATIC 1
#define LIGHT_BAKE_DYNAMIC 2

struct LightData { //this structure needs to be as packed as possible
	highp vec3 position;
	highp float inv_radius;

	mediump vec3 direction;
	highp float size;

	mediump vec3 color;
	mediump float attenuation;

	mediump float cone_attenuation;
	mediump float cone_angle;
	mediump float specular_amount;
	mediump float shadow_opacity;

	highp vec4 atlas_rect; // rect in the shadow atlas
	highp mat4 shadow_matrix;
	highp float shadow_bias;
	highp float shadow_normal_bias;
	highp float transmittance_bias;
	highp float soft_shadow_size; // for spot, it's the size in uv coordinates of the light, for omni it's the span angle
	highp float soft_shadow_scale; // scales the shadow kernel for blurrier shadows
	uint mask;
	mediump float volumetric_fog_energy;
	uint bake_mode;
	highp vec4 projector_rect; //projector rect in srgb decal atlas
};

#define REFLECTION_AMBIENT_DISABLED 0
#define REFLECTION_AMBIENT_ENVIRONMENT 1
#define REFLECTION_AMBIENT_COLOR 2

struct ReflectionData {
	highp vec3 box_extents;
	mediump float index;
	highp vec3 box_offset;
	uint mask;
	mediump vec3 ambient; // ambient color
	mediump float intensity;
	mediump float blend_distance;
	bool exterior;
	bool box_project;
	uint ambient_mode;
	float exposure_normalization;
	float pad0;
	float pad1;
	float pad2;
	//0-8 is intensity,8-9 is ambient, mode
	highp mat4 local_matrix; // up to here for spot and omni, rest is for directional
	// notes: for ambientblend, use distance to edge to blend between already existing global environment
};

struct DirectionalLightData {
	mediump vec3 direction;
	highp float energy; // needs to be highp to avoid NaNs being created with high energy values (i.e. when using physical light units and over-exposing the image)
	mediump vec3 color;
	mediump float size;
	mediump float specular;
	uint mask;
	highp float softshadow_angle;
	highp float soft_shadow_scale;
	bool blend_splits;
	mediump float shadow_opacity;
	highp float fade_from;
	highp float fade_to;
	uvec2 pad;
	uint bake_mode;
	mediump float volumetric_fog_energy;
	highp vec4 shadow_bias;
	highp vec4 shadow_normal_bias;
	highp vec4 shadow_transmittance_bias;
	highp vec4 shadow_z_range;
	highp vec4 shadow_range_begin;
	highp vec4 shadow_split_offsets;
	highp mat4 shadow_matrix1;
	highp mat4 shadow_matrix2;
	highp mat4 shadow_matrix3;
	highp mat4 shadow_matrix4;
	highp vec2 uv_scale1;
	highp vec2 uv_scale2;
	highp vec2 uv_scale3;
	highp vec2 uv_scale4;
};

struct DecalData {
	highp mat4 xform; //to decal transform
	highp vec3 inv_extents;
	mediump float albedo_mix;
	highp vec4 albedo_rect;
	highp vec4 normal_rect;
	highp vec4 orm_rect;
	highp vec4 emission_rect;
	highp vec4 modulate;
	mediump float emission_energy;
	uint mask;
	mediump float upper_fade;
	mediump float lower_fade;
	mediump mat3x4 normal_xform;
	mediump vec3 normal;
	mediump float normal_fade;
};

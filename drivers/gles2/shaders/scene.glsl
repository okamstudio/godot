/* clang-format off */
[vertex]

#ifdef USE_GLES_OVER_GL
#define lowp
#define mediump
#define highp
#else
// Default to high precision variables for the vertex shader.
// Note that the fragment shader however may default to mediump on mobile for performance,
// and thus shared uniforms should use a specifier to be consistent in both shaders.
precision highp float;
precision highp int;
#endif

#if defined(ENSURE_CORRECT_NORMALS)
#define INVERSE_USED
#endif

/* clang-format on */
#include "stdlib.glsl"
/* clang-format off */

#define SHADER_IS_SRGB true

#define M_PI 3.14159265359

//
// attributes
//

attribute highp vec4 vertex_attrib; // attrib:0
/* clang-format on */
#ifdef ENABLE_OCTAHEDRAL_COMPRESSION
attribute vec4 normal_tangent_attrib; // attrib:1
#else
attribute vec3 normal_attrib; // attrib:1
#endif

#if defined(ENABLE_TANGENT_INTERP) || defined(ENABLE_NORMALMAP)
#ifdef ENABLE_OCTAHEDRAL_COMPRESSION
// packed into normal_attrib zw component
#else
attribute vec4 tangent_attrib; // attrib:2
#endif
#endif

#if defined(ENABLE_COLOR_INTERP)
attribute vec4 color_attrib; // attrib:3
#endif

#if defined(ENABLE_UV_INTERP)
attribute vec2 uv_attrib; // attrib:4
#endif

#if defined(ENABLE_UV2_INTERP) || defined(USE_LIGHTMAP)
attribute vec2 uv2_attrib; // attrib:5
#endif

#ifdef USE_SKELETON

#ifdef USE_SKELETON_SOFTWARE

attribute highp vec4 bone_transform_row_0; // attrib:13
attribute highp vec4 bone_transform_row_1; // attrib:14
attribute highp vec4 bone_transform_row_2; // attrib:15

#else

attribute vec4 bone_ids; // attrib:6
attribute highp vec4 bone_weights; // attrib:7

uniform highp sampler2D bone_transforms; // texunit:-1
uniform ivec2 skeleton_texture_size;

#endif

#endif

#ifdef USE_INSTANCING

attribute highp vec4 instance_xform_row_0; // attrib:8
attribute highp vec4 instance_xform_row_1; // attrib:9
attribute highp vec4 instance_xform_row_2; // attrib:10

attribute highp vec4 instance_color; // attrib:11
attribute highp vec4 instance_custom_data; // attrib:12

#endif

//
// uniforms
//

uniform highp mat4 camera_matrix;
uniform highp mat4 camera_inverse_matrix;
uniform highp mat4 projection_matrix;
uniform highp mat4 projection_inverse_matrix;

uniform highp mat4 world_transform;

uniform highp float time;

uniform highp vec2 viewport_size;

#ifdef RENDER_DEPTH
uniform float light_bias;
uniform float light_normal_bias;
#endif

uniform highp int view_index;

#ifdef ENABLE_OCTAHEDRAL_COMPRESSION
vec3 oct_to_vec3(vec2 e) {
	vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
	float t = max(-v.z, 0.0);
	v.xy += t * -sign(v.xy);
	return normalize(v);
}
#endif

//
// varyings
//

#if defined(RENDER_DEPTH) && defined(USE_RGBA_SHADOWS)
varying highp vec4 position_interp;
#endif

varying highp vec3 vertex_interp;
varying vec3 normal_interp;

#if defined(ENABLE_TANGENT_INTERP) || defined(ENABLE_NORMALMAP)
varying vec3 tangent_interp;
varying vec3 binormal_interp;
#endif

#if defined(ENABLE_COLOR_INTERP)
varying vec4 color_interp;
#endif

#if defined(ENABLE_UV_INTERP)
varying vec2 uv_interp;
#endif

#if defined(ENABLE_UV2_INTERP) || defined(USE_LIGHTMAP)
varying vec2 uv2_interp;
#endif

/* clang-format off */

VERTEX_SHADER_GLOBALS

/* clang-format on */

#ifdef RENDER_DEPTH_DUAL_PARABOLOID

varying highp float dp_clip;
uniform highp float shadow_dual_paraboloid_render_zfar;
uniform highp float shadow_dual_paraboloid_render_side;

#endif

#if defined(USE_SHADOW) && defined(USE_LIGHTING)

uniform highp mat4 light_shadow_matrix;
varying highp vec4 shadow_coord;

#if defined(LIGHT_USE_PSSM2) || defined(LIGHT_USE_PSSM3) || defined(LIGHT_USE_PSSM4)
uniform highp mat4 light_shadow_matrix2;
varying highp vec4 shadow_coord2;
#endif

#if defined(LIGHT_USE_PSSM3) || defined(LIGHT_USE_PSSM4)
uniform highp mat4 light_shadow_matrix3;
varying highp vec4 shadow_coord3;
#endif

#if defined(LIGHT_USE_PSSM4)
uniform highp mat4 light_shadow_matrix4;
varying highp vec4 shadow_coord4;
#endif

#endif

#if defined(USE_VERTEX_LIGHTING) && defined(USE_LIGHTING)

varying highp vec3 diffuse_interp;
varying highp vec3 specular_interp;

// general for all lights
uniform highp vec4 light_color;
uniform highp vec4 shadow_color;
uniform highp float light_specular;

// directional
uniform highp vec3 light_direction;

// omni
uniform highp vec3 light_position;

uniform highp float light_range;
uniform highp float light_attenuation;

// spot
uniform highp float light_spot_attenuation;
uniform highp float light_spot_range;
uniform highp float light_spot_angle;

float get_omni_attenuation(float distance, float inv_range, float decay) {
	float nd = distance * inv_range;
	nd *= nd;
	nd *= nd; // nd^4
	nd = max(1.0 - nd, 0.0);
	nd *= nd; // nd^2
	return nd * pow(max(distance, 0.0001), -decay);
}

void light_compute(
		vec3 N,
		vec3 L,
		vec3 V,
		vec3 light_color,
		vec3 attenuation,
		float roughness) {
//this makes lights behave closer to linear, but then addition of lights looks bad
//better left disabled

//#define SRGB_APPROX(m_var) m_var = pow(m_var,0.4545454545);
/*
#define SRGB_APPROX(m_var) {\
	float S1 = sqrt(m_var);\
	float S2 = sqrt(S1);\
	float S3 = sqrt(S2);\
	m_var = 0.662002687 * S1 + 0.684122060 * S2 - 0.323583601 * S3 - 0.0225411470 * m_var;\
	}
*/
#define SRGB_APPROX(m_var)

	float NdotL = dot(N, L);
	float cNdotL = max(NdotL, 0.0); // clamped NdotL
	float NdotV = dot(N, V);
	float cNdotV = max(NdotV, 0.0);

#if defined(DIFFUSE_OREN_NAYAR)
	vec3 diffuse_brdf_NL;
#else
	float diffuse_brdf_NL; // BRDF times N.L for calculating diffuse radiance
#endif

#if defined(DIFFUSE_LAMBERT_WRAP)
	// energy conserving lambert wrap shader
	diffuse_brdf_NL = max(0.0, (NdotL + roughness) / ((1.0 + roughness) * (1.0 + roughness)));

#elif defined(DIFFUSE_OREN_NAYAR)

	{
		// see http://mimosa-pudica.net/improved-oren-nayar.html
		float LdotV = dot(L, V);

		float s = LdotV - NdotL * NdotV;
		float t = mix(1.0, max(NdotL, NdotV), step(0.0, s));

		float sigma2 = roughness * roughness; // TODO: this needs checking
		vec3 A = 1.0 + sigma2 * (-0.5 / (sigma2 + 0.33) + 0.17 * diffuse_color / (sigma2 + 0.13));
		float B = 0.45 * sigma2 / (sigma2 + 0.09);

		diffuse_brdf_NL = cNdotL * (A + vec3(B) * s / t) * (1.0 / M_PI);
	}
#else
	// lambert by default for everything else
	diffuse_brdf_NL = cNdotL * (1.0 / M_PI);
#endif

	SRGB_APPROX(diffuse_brdf_NL)

	diffuse_interp += light_color * diffuse_brdf_NL * attenuation;

	if (roughness > 0.0) {
		// D
		float specular_brdf_NL = 0.0;

#if !defined(SPECULAR_DISABLED)
		//normalized blinn always unless disabled
		vec3 H = normalize(V + L);
		float cNdotH = max(dot(N, H), 0.0);
		float shininess = exp2(15.0 * (1.0 - roughness) + 1.0) * 0.25;
		float blinn = pow(cNdotH, shininess);
		blinn *= (shininess + 2.0) * (1.0 / (8.0 * M_PI));
		specular_brdf_NL = blinn;
#endif

		SRGB_APPROX(specular_brdf_NL)
		specular_interp += specular_brdf_NL * light_color * attenuation;
	}
}

#endif

#ifdef USE_VERTEX_LIGHTING

#ifdef USE_REFLECTION_PROBE1

uniform highp mat4 refprobe1_local_matrix;
varying mediump vec4 refprobe1_reflection_normal_blend;
uniform highp vec3 refprobe1_box_extents;

#ifndef USE_LIGHTMAP
varying mediump vec3 refprobe1_ambient_normal;
#endif

#endif //reflection probe1

#ifdef USE_REFLECTION_PROBE2

uniform highp mat4 refprobe2_local_matrix;
varying mediump vec4 refprobe2_reflection_normal_blend;
uniform highp vec3 refprobe2_box_extents;

#ifndef USE_LIGHTMAP
varying mediump vec3 refprobe2_ambient_normal;
#endif

#endif //reflection probe2

#endif //vertex lighting for refprobes

#if defined(FOG_DEPTH_ENABLED) || defined(FOG_HEIGHT_ENABLED)

varying vec4 fog_interp;

uniform mediump vec4 fog_color_base;
#ifdef LIGHT_MODE_DIRECTIONAL
uniform mediump vec4 fog_sun_color_amount;
#endif

uniform bool fog_transmit_enabled;
uniform mediump float fog_transmit_curve;

#ifdef FOG_DEPTH_ENABLED
uniform highp float fog_depth_begin;
uniform mediump float fog_depth_curve;
uniform mediump float fog_max_distance;
#endif

#ifdef FOG_HEIGHT_ENABLED
uniform highp float fog_height_min;
uniform highp float fog_height_max;
uniform mediump float fog_height_curve;
#endif

#endif //fog

void main() {
	highp vec4 vertex = vertex_attrib;

	mat4 world_matrix = world_transform;

#ifdef USE_INSTANCING
	{
		highp mat4 m = mat4(
				instance_xform_row_0,
				instance_xform_row_1,
				instance_xform_row_2,
				vec4(0.0, 0.0, 0.0, 1.0));
		world_matrix = world_matrix * transpose(m);
	}

#endif

#ifdef ENABLE_OCTAHEDRAL_COMPRESSION
	vec3 normal = oct_to_vec3(normal_tangent_attrib.xy);
#else
	vec3 normal = normal_attrib;
#endif

#if defined(ENABLE_TANGENT_INTERP) || defined(ENABLE_NORMALMAP)
#ifdef ENABLE_OCTAHEDRAL_COMPRESSION
	vec3 tangent = oct_to_vec3(vec2(normal_tangent_attrib.z, abs(normal_tangent_attrib.w) * 2.0 - 1.0));
	float binormalf = sign(normal_tangent_attrib.w);
#else
	vec3 tangent = tangent_attrib.xyz;
	float binormalf = tangent_attrib.a;
#endif
	vec3 binormal = normalize(cross(normal, tangent) * binormalf);
#endif

#if defined(ENABLE_COLOR_INTERP)
	color_interp = color_attrib;
#ifdef USE_INSTANCING
	color_interp *= instance_color;
#endif
#endif

#if defined(ENABLE_UV_INTERP)
	uv_interp = uv_attrib;
#endif

#if defined(ENABLE_UV2_INTERP) || defined(USE_LIGHTMAP)
	uv2_interp = uv2_attrib;
#endif

#if defined(OVERRIDE_POSITION)
	highp vec4 position;
#endif

#if !defined(SKIP_TRANSFORM_USED) && defined(VERTEX_WORLD_COORDS_USED)
	vertex = world_matrix * vertex;
#if defined(ENSURE_CORRECT_NORMALS)
	mat3 normal_matrix = mat3(transpose(inverse(world_matrix)));
	normal = normal_matrix * normal;
#else
	normal = normalize((world_matrix * vec4(normal, 0.0)).xyz);
#endif
#if defined(ENABLE_TANGENT_INTERP) || defined(ENABLE_NORMALMAP)

	tangent = normalize((world_matrix * vec4(tangent, 0.0)).xyz);
	binormal = normalize((world_matrix * vec4(binormal, 0.0)).xyz);
#endif
#endif

#ifdef USE_SKELETON

	highp mat4 bone_transform = mat4(0.0);

#ifdef USE_SKELETON_SOFTWARE
	// passing the transform as attributes

	bone_transform[0] = vec4(bone_transform_row_0.x, bone_transform_row_1.x, bone_transform_row_2.x, 0.0);
	bone_transform[1] = vec4(bone_transform_row_0.y, bone_transform_row_1.y, bone_transform_row_2.y, 0.0);
	bone_transform[2] = vec4(bone_transform_row_0.z, bone_transform_row_1.z, bone_transform_row_2.z, 0.0);
	bone_transform[3] = vec4(bone_transform_row_0.w, bone_transform_row_1.w, bone_transform_row_2.w, 1.0);

#else
	// look up transform from the "pose texture"
	{
		ivec4 bone_indicesi = ivec4(bone_ids); // cast to signed int

		ivec2 tex_ofs = ivec2(bone_indicesi.x * 3, 0);
		bone_transform = mat4(
								 texel2DFetch(bone_transforms, skeleton_texture_size, tex_ofs),
								 texel2DFetch(bone_transforms, skeleton_texture_size, tex_ofs + ivec2(1, 0)),
								 texel2DFetch(bone_transforms, skeleton_texture_size, tex_ofs + ivec2(2, 0)),
								 vec4(0.0, 0.0, 0.0, 1.0)) *
				bone_weights.x;

		tex_ofs = ivec2(bone_indicesi.y * 3, 0);

		bone_transform += mat4(
								  texel2DFetch(bone_transforms, skeleton_texture_size, tex_ofs),
								  texel2DFetch(bone_transforms, skeleton_texture_size, tex_ofs + ivec2(1, 0)),
								  texel2DFetch(bone_transforms, skeleton_texture_size, tex_ofs + ivec2(2, 0)),
								  vec4(0.0, 0.0, 0.0, 1.0)) *
				bone_weights.y;

		tex_ofs = ivec2(bone_indicesi.z * 3, 0);

		bone_transform += mat4(
								  texel2DFetch(bone_transforms, skeleton_texture_size, tex_ofs),
								  texel2DFetch(bone_transforms, skeleton_texture_size, tex_ofs + ivec2(1, 0)),
								  texel2DFetch(bone_transforms, skeleton_texture_size, tex_ofs + ivec2(2, 0)),
								  vec4(0.0, 0.0, 0.0, 1.0)) *
				bone_weights.z;

		tex_ofs = ivec2(bone_indicesi.w * 3, 0);

		bone_transform += mat4(
								  texel2DFetch(bone_transforms, skeleton_texture_size, tex_ofs),
								  texel2DFetch(bone_transforms, skeleton_texture_size, tex_ofs + ivec2(1, 0)),
								  texel2DFetch(bone_transforms, skeleton_texture_size, tex_ofs + ivec2(2, 0)),
								  vec4(0.0, 0.0, 0.0, 1.0)) *
				bone_weights.w;

		bone_transform = transpose(bone_transform);
	}

#endif

	world_matrix = world_matrix * bone_transform;

#endif

#ifdef USE_INSTANCING
	vec4 instance_custom = instance_custom_data;
#else
	vec4 instance_custom = vec4(0.0);

#endif

	mat4 local_projection_matrix = projection_matrix;

	mat4 modelview = camera_inverse_matrix * world_matrix;
	float roughness = 1.0;

#define projection_matrix local_projection_matrix
#define world_transform world_matrix

	float point_size = 1.0;

	{
		/* clang-format off */

VERTEX_SHADER_CODE

		/* clang-format on */
	}

	gl_PointSize = point_size;
	vec4 outvec = vertex;

	// use local coordinates
#if !defined(SKIP_TRANSFORM_USED) && !defined(VERTEX_WORLD_COORDS_USED)
	vertex = modelview * vertex;
#if defined(ENSURE_CORRECT_NORMALS)
	mat3 normal_matrix = mat3(transpose(inverse(modelview)));
	normal = normal_matrix * normal;
#else
	normal = normalize((modelview * vec4(normal, 0.0)).xyz);
#endif

#if defined(ENABLE_TANGENT_INTERP) || defined(ENABLE_NORMALMAP)
	tangent = normalize((modelview * vec4(tangent, 0.0)).xyz);
	binormal = normalize((modelview * vec4(binormal, 0.0)).xyz);
#endif
#endif

#if !defined(SKIP_TRANSFORM_USED) && defined(VERTEX_WORLD_COORDS_USED)
	vertex = camera_inverse_matrix * vertex;
	normal = normalize((camera_inverse_matrix * vec4(normal, 0.0)).xyz);
#if defined(ENABLE_TANGENT_INTERP) || defined(ENABLE_NORMALMAP)
	tangent = normalize((camera_inverse_matrix * vec4(tangent, 0.0)).xyz);
	binormal = normalize((camera_inverse_matrix * vec4(binormal, 0.0)).xyz);
#endif
#endif

	vertex_interp = vertex.xyz;
	normal_interp = normal;

#if defined(ENABLE_TANGENT_INTERP) || defined(ENABLE_NORMALMAP)
	tangent_interp = tangent;
	binormal_interp = binormal;
#endif

#ifdef RENDER_DEPTH

#ifdef RENDER_DEPTH_DUAL_PARABOLOID

	vertex_interp.z *= shadow_dual_paraboloid_render_side;
	normal_interp.z *= shadow_dual_paraboloid_render_side;

	dp_clip = vertex_interp.z; //this attempts to avoid noise caused by objects sent to the other parabolloid side due to bias

	//for dual paraboloid shadow mapping, this is the fastest but least correct way, as it curves straight edges

	highp vec3 vtx = vertex_interp + normalize(vertex_interp) * light_bias;
	highp float distance = length(vtx);
	vtx = normalize(vtx);
	vtx.xy /= 1.0 - vtx.z;
	vtx.z = (distance / shadow_dual_paraboloid_render_zfar);
	vtx.z = vtx.z * 2.0 - 1.0;

	vertex_interp = vtx;

#else
	float z_ofs = light_bias;
	z_ofs += (1.0 - abs(normal_interp.z)) * light_normal_bias;

	vertex_interp.z -= z_ofs;
#endif //dual parabolloid

#endif //depth

//vertex lighting
#if defined(USE_VERTEX_LIGHTING) && defined(USE_LIGHTING)
	//vertex shaded version of lighting (more limited)
	vec3 L;
	vec3 light_att;

#ifdef LIGHT_MODE_OMNI
	vec3 light_vec = light_position - vertex_interp;
	float light_length = length(light_vec);

	float normalized_distance = light_length / light_range;

	if (normalized_distance < 1.0) {
#ifdef USE_PHYSICAL_LIGHT_ATTENUATION
		float omni_attenuation = get_omni_attenuation(light_length, 1.0 / light_range, light_attenuation);
#else
		float omni_attenuation = pow(1.0 - normalized_distance, light_attenuation);
#endif

		light_att = vec3(omni_attenuation);
	} else {
		light_att = vec3(0.0);
	}

	L = normalize(light_vec);

#endif

#ifdef LIGHT_MODE_SPOT

	vec3 light_rel_vec = light_position - vertex_interp;
	float light_length = length(light_rel_vec);
	float normalized_distance = light_length / light_range;

	if (normalized_distance < 1.0) {
#ifdef USE_PHYSICAL_LIGHT_ATTENUATION
		float spot_attenuation = get_omni_attenuation(light_length, 1.0 / light_range, light_attenuation);
#else
		float spot_attenuation = pow(1.0 - normalized_distance, light_attenuation);
#endif

		vec3 spot_dir = light_direction;

		float spot_cutoff = light_spot_angle;

		float angle = dot(-normalize(light_rel_vec), spot_dir);

		if (angle > spot_cutoff) {
			float scos = max(angle, spot_cutoff);
			float spot_rim = max(0.0001, (1.0 - scos) / (1.0 - spot_cutoff));

			spot_attenuation *= 1.0 - pow(spot_rim, light_spot_attenuation);

			light_att = vec3(spot_attenuation);
		} else {
			light_att = vec3(0.0);
		}
	} else {
		light_att = vec3(0.0);
	}

	L = normalize(light_rel_vec);

#endif

#ifdef LIGHT_MODE_DIRECTIONAL
	vec3 light_vec = -light_direction;
	light_att = vec3(1.0); //no base attenuation
	L = normalize(light_vec);
#endif

	diffuse_interp = vec3(0.0);
	specular_interp = vec3(0.0);
	light_compute(normal_interp, L, -normalize(vertex_interp), light_color.rgb, light_att, roughness);

#endif

//shadows (for both vertex and fragment)
#if defined(USE_SHADOW) && defined(USE_LIGHTING)

	vec4 vi4 = vec4(vertex_interp, 1.0);
	shadow_coord = light_shadow_matrix * vi4;

#if defined(LIGHT_USE_PSSM2) || defined(LIGHT_USE_PSSM3) || defined(LIGHT_USE_PSSM4)
	shadow_coord2 = light_shadow_matrix2 * vi4;
#endif

#if defined(LIGHT_USE_PSSM3) || defined(LIGHT_USE_PSSM4)
	shadow_coord3 = light_shadow_matrix3 * vi4;
#endif

#if defined(LIGHT_USE_PSSM4)
	shadow_coord4 = light_shadow_matrix4 * vi4;
#endif

#endif //use shadow and use lighting

#ifdef USE_VERTEX_LIGHTING

#ifdef USE_REFLECTION_PROBE1
	{
		vec3 ref_normal = normalize(reflect(vertex_interp, normal_interp));
		vec3 local_pos = (refprobe1_local_matrix * vec4(vertex_interp, 1.0)).xyz;
		vec3 inner_pos = abs(local_pos / refprobe1_box_extents);
		float blend = max(inner_pos.x, max(inner_pos.y, inner_pos.z));

		{
			vec3 local_ref_vec = (refprobe1_local_matrix * vec4(ref_normal, 0.0)).xyz;
			refprobe1_reflection_normal_blend.xyz = local_ref_vec;
			refprobe1_reflection_normal_blend.a = blend;
		}
#ifndef USE_LIGHTMAP

		refprobe1_ambient_normal = (refprobe1_local_matrix * vec4(normal_interp, 0.0)).xyz;
#endif
	}

#endif //USE_REFLECTION_PROBE1

#ifdef USE_REFLECTION_PROBE2
	{
		vec3 ref_normal = normalize(reflect(vertex_interp, normal_interp));
		vec3 local_pos = (refprobe2_local_matrix * vec4(vertex_interp, 1.0)).xyz;
		vec3 inner_pos = abs(local_pos / refprobe2_box_extents);
		float blend = max(inner_pos.x, max(inner_pos.y, inner_pos.z));

		{
			vec3 local_ref_vec = (refprobe2_local_matrix * vec4(ref_normal, 0.0)).xyz;
			refprobe2_reflection_normal_blend.xyz = local_ref_vec;
			refprobe2_reflection_normal_blend.a = blend;
		}
#ifndef USE_LIGHTMAP

		refprobe2_ambient_normal = (refprobe2_local_matrix * vec4(normal_interp, 0.0)).xyz;
#endif
	}

#endif //USE_REFLECTION_PROBE2

#if defined(FOG_DEPTH_ENABLED) || defined(FOG_HEIGHT_ENABLED)

	float fog_amount = 0.0;

#ifdef LIGHT_MODE_DIRECTIONAL

	vec3 fog_color = mix(fog_color_base.rgb, fog_sun_color_amount.rgb, fog_sun_color_amount.a * pow(max(dot(normalize(vertex_interp), light_direction), 0.0), 8.0));
#else
	vec3 fog_color = fog_color_base.rgb;
#endif

#ifdef FOG_DEPTH_ENABLED

	{
		float fog_z = smoothstep(fog_depth_begin, fog_max_distance, length(vertex));

		fog_amount = pow(fog_z, fog_depth_curve) * fog_color_base.a;
	}
#endif

#ifdef FOG_HEIGHT_ENABLED
	{
		float y = (camera_matrix * vec4(vertex_interp, 1.0)).y;
		fog_amount = max(fog_amount, pow(smoothstep(fog_height_min, fog_height_max, y), fog_height_curve));
	}
#endif
	fog_interp = vec4(fog_color, fog_amount);

#endif //fog

#endif //use vertex lighting

#if defined(OVERRIDE_POSITION)
	gl_Position = position;
#else
	gl_Position = projection_matrix * vec4(vertex_interp, 1.0);
#endif

#if defined(RENDER_DEPTH) && defined(USE_RGBA_SHADOWS)
	position_interp = gl_Position;
#endif
}

/* clang-format off */
[fragment]

// texture2DLodEXT and textureCubeLodEXT are fragment shader specific.
// Do not copy these defines in the vertex section.
#ifndef USE_GLES_OVER_GL
#ifdef GL_EXT_shader_texture_lod
#extension GL_EXT_shader_texture_lod : enable
#define texture2DLod(img, coord, lod) texture2DLodEXT(img, coord, lod)
#define textureCubeLod(img, coord, lod) textureCubeLodEXT(img, coord, lod)
#endif
#endif // !USE_GLES_OVER_GL

#ifdef GL_ARB_shader_texture_lod
#extension GL_ARB_shader_texture_lod : enable
#endif

#if !defined(GL_EXT_shader_texture_lod) && !defined(GL_ARB_shader_texture_lod)
#define texture2DLod(img, coord, lod) texture2D(img, coord, lod)
#define textureCubeLod(img, coord, lod) textureCube(img, coord, lod)
#endif

#ifdef USE_GLES_OVER_GL
#define lowp
#define mediump
#define highp
#else
// On mobile devices we want to default to medium precision to increase performance in the fragment shader.
#if defined(USE_HIGHP_PRECISION)
precision highp float;
precision highp int;
#else
precision mediump float;
precision mediump int;
#endif
#endif

#include "stdlib.glsl"

#define M_PI 3.14159265359
#define SHADER_IS_SRGB true

//
// uniforms
//

uniform highp mat4 camera_matrix;
/* clang-format on */
uniform highp mat4 camera_inverse_matrix;
uniform highp mat4 projection_matrix;
uniform highp mat4 projection_inverse_matrix;

uniform highp mat4 world_transform;

uniform highp float time;
uniform highp int view_index;

uniform highp vec2 viewport_size;

#if defined(SCREEN_UV_USED)
uniform vec2 screen_pixel_size;
#endif

#if defined(SCREEN_TEXTURE_USED)
uniform highp sampler2D screen_texture; //texunit:-4
#endif
#if defined(DEPTH_TEXTURE_USED)
uniform highp sampler2D depth_texture; //texunit:-4
#endif

#ifdef USE_REFLECTION_PROBE1

#ifdef USE_VERTEX_LIGHTING

varying mediump vec4 refprobe1_reflection_normal_blend;
#ifndef USE_LIGHTMAP
varying mediump vec3 refprobe1_ambient_normal;
#endif

#else

uniform bool refprobe1_use_box_project;
uniform highp vec3 refprobe1_box_extents;
uniform vec3 refprobe1_box_offset;
uniform highp mat4 refprobe1_local_matrix;

#endif //use vertex lighting

uniform bool refprobe1_exterior;

uniform highp samplerCube reflection_probe1; //texunit:-5

uniform float refprobe1_intensity;
uniform vec4 refprobe1_ambient;

#endif //USE_REFLECTION_PROBE1

#ifdef USE_REFLECTION_PROBE2

#ifdef USE_VERTEX_LIGHTING

varying mediump vec4 refprobe2_reflection_normal_blend;
#ifndef USE_LIGHTMAP
varying mediump vec3 refprobe2_ambient_normal;
#endif

#else

uniform bool refprobe2_use_box_project;
uniform highp vec3 refprobe2_box_extents;
uniform vec3 refprobe2_box_offset;
uniform highp mat4 refprobe2_local_matrix;

#endif //use vertex lighting

uniform bool refprobe2_exterior;

uniform highp samplerCube reflection_probe2; //texunit:-6

uniform float refprobe2_intensity;
uniform vec4 refprobe2_ambient;

#endif //USE_REFLECTION_PROBE2

#define RADIANCE_MAX_LOD 6.0

#if defined(USE_REFLECTION_PROBE1) || defined(USE_REFLECTION_PROBE2)

void reflection_process(samplerCube reflection_map,
#ifdef USE_VERTEX_LIGHTING
		vec3 ref_normal,
#ifndef USE_LIGHTMAP
		vec3 amb_normal,
#endif
		float ref_blend,

#else //no vertex lighting
		vec3 normal, vec3 vertex,
		mat4 local_matrix,
		bool use_box_project, vec3 box_extents, vec3 box_offset,
#endif //vertex lighting
		bool exterior, float intensity, vec4 ref_ambient, float roughness, vec3 ambient, vec3 skybox, inout highp vec4 reflection_accum, inout highp vec4 ambient_accum) {

	vec4 reflection;

#ifdef USE_VERTEX_LIGHTING

	reflection.rgb = textureCubeLod(reflection_map, ref_normal, roughness * RADIANCE_MAX_LOD).rgb;

	float blend = ref_blend; //crappier blend formula for vertex
	blend *= blend;
	blend = max(0.0, 1.0 - blend);

#else //fragment lighting

	vec3 local_pos = (local_matrix * vec4(vertex, 1.0)).xyz;

	if (any(greaterThan(abs(local_pos), box_extents))) { //out of the reflection box
		return;
	}

	vec3 inner_pos = abs(local_pos / box_extents);
	float blend = max(inner_pos.x, max(inner_pos.y, inner_pos.z));
	blend = mix(length(inner_pos), blend, blend);
	blend *= blend;
	blend = max(0.0, 1.0 - blend);

	//reflect and make local
	vec3 ref_normal = normalize(reflect(vertex, normal));
	ref_normal = (local_matrix * vec4(ref_normal, 0.0)).xyz;

	if (use_box_project) { //box project

		vec3 nrdir = normalize(ref_normal);
		vec3 rbmax = (box_extents - local_pos) / nrdir;
		vec3 rbmin = (-box_extents - local_pos) / nrdir;

		vec3 rbminmax = mix(rbmin, rbmax, vec3(greaterThan(nrdir, vec3(0.0, 0.0, 0.0))));

		float fa = min(min(rbminmax.x, rbminmax.y), rbminmax.z);
		vec3 posonbox = local_pos + nrdir * fa;
		ref_normal = posonbox - box_offset.xyz;
	}

	reflection.rgb = textureCubeLod(reflection_map, ref_normal, roughness * RADIANCE_MAX_LOD).rgb;
#endif

	if (exterior) {
		reflection.rgb = mix(skybox, reflection.rgb, blend);
	}
	reflection.rgb *= intensity;
	reflection.a = blend;
	reflection.rgb *= blend;

	reflection_accum += reflection;

#ifndef USE_LIGHTMAP

	vec4 ambient_out;
#ifndef USE_VERTEX_LIGHTING

	vec3 amb_normal = (local_matrix * vec4(normal, 0.0)).xyz;
#endif

	ambient_out.rgb = textureCubeLod(reflection_map, amb_normal, RADIANCE_MAX_LOD).rgb;
	ambient_out.rgb = mix(ref_ambient.rgb, ambient_out.rgb, ref_ambient.a);
	if (exterior) {
		ambient_out.rgb = mix(ambient, ambient_out.rgb, blend);
	}

	ambient_out.a = blend;
	ambient_out.rgb *= blend;
	ambient_accum += ambient_out;

#endif
}

#endif //use refprobe 1 or 2

#ifdef USE_LIGHTMAP
uniform mediump sampler2D lightmap; //texunit:-4
uniform mediump float lightmap_energy;

#if defined(USE_LIGHTMAP_FILTER_BICUBIC)
uniform mediump vec2 lightmap_texture_size;

// w0, w1, w2, and w3 are the four cubic B-spline basis functions
float w0(float a) {
	return (1.0 / 6.0) * (a * (a * (-a + 3.0) - 3.0) + 1.0);
}

float w1(float a) {
	return (1.0 / 6.0) * (a * a * (3.0 * a - 6.0) + 4.0);
}

float w2(float a) {
	return (1.0 / 6.0) * (a * (a * (-3.0 * a + 3.0) + 3.0) + 1.0);
}

float w3(float a) {
	return (1.0 / 6.0) * (a * a * a);
}

// g0 and g1 are the two amplitude functions
float g0(float a) {
	return w0(a) + w1(a);
}

float g1(float a) {
	return w2(a) + w3(a);
}

// h0 and h1 are the two offset functions
float h0(float a) {
	return -1.0 + w1(a) / (w0(a) + w1(a));
}

float h1(float a) {
	return 1.0 + w3(a) / (w2(a) + w3(a));
}

vec4 texture2D_bicubic(sampler2D tex, vec2 uv) {
	vec2 texel_size = vec2(1.0) / lightmap_texture_size;

	uv = uv * lightmap_texture_size + vec2(0.5);

	vec2 iuv = floor(uv);
	vec2 fuv = fract(uv);

	float g0x = g0(fuv.x);
	float g1x = g1(fuv.x);
	float h0x = h0(fuv.x);
	float h1x = h1(fuv.x);
	float h0y = h0(fuv.y);
	float h1y = h1(fuv.y);

	vec2 p0 = (vec2(iuv.x + h0x, iuv.y + h0y) - vec2(0.5)) * texel_size;
	vec2 p1 = (vec2(iuv.x + h1x, iuv.y + h0y) - vec2(0.5)) * texel_size;
	vec2 p2 = (vec2(iuv.x + h0x, iuv.y + h1y) - vec2(0.5)) * texel_size;
	vec2 p3 = (vec2(iuv.x + h1x, iuv.y + h1y) - vec2(0.5)) * texel_size;

	return (g0(fuv.y) * (g0x * texture2D(tex, p0) + g1x * texture2D(tex, p1))) +
			(g1(fuv.y) * (g0x * texture2D(tex, p2) + g1x * texture2D(tex, p3)));
}
#endif //USE_LIGHTMAP_FILTER_BICUBIC
#endif

#ifdef USE_LIGHTMAP_CAPTURE
uniform mediump vec4 lightmap_captures[12];
#endif

#ifdef USE_RADIANCE_MAP

uniform samplerCube radiance_map; // texunit:-2

uniform mat4 radiance_inverse_xform;

#endif

uniform vec4 bg_color;
uniform float bg_energy;

uniform float ambient_sky_contribution;
uniform vec4 ambient_color;
uniform float ambient_energy;

#ifdef USE_LIGHTING

uniform highp vec4 shadow_color;

#ifdef USE_VERTEX_LIGHTING

//get from vertex
varying highp vec3 diffuse_interp;
varying highp vec3 specular_interp;

uniform highp vec3 light_direction; //may be used by fog, so leave here

#else
//done in fragment
// general for all lights
uniform highp vec4 light_color;

uniform highp float light_specular;

// directional
uniform highp vec3 light_direction;
// omni
uniform highp vec3 light_position;

uniform highp float light_attenuation;

// spot
uniform highp float light_spot_attenuation;
uniform highp float light_spot_range;
uniform highp float light_spot_angle;
#endif

//this is needed outside above if because dual paraboloid wants it
uniform highp float light_range;

#ifdef USE_SHADOW

uniform highp vec2 shadow_pixel_size;

#if defined(LIGHT_MODE_OMNI) || defined(LIGHT_MODE_SPOT)
uniform highp sampler2D light_shadow_atlas; //texunit:-3
#endif

#ifdef LIGHT_MODE_DIRECTIONAL
uniform highp sampler2D light_directional_shadow; // texunit:-3
uniform highp vec4 light_split_offsets;
uniform mediump float fade_from;
uniform mediump float fade_to;
#endif

varying highp vec4 shadow_coord;

#if defined(LIGHT_USE_PSSM2) || defined(LIGHT_USE_PSSM3) || defined(LIGHT_USE_PSSM4)
varying highp vec4 shadow_coord2;
#endif

#if defined(LIGHT_USE_PSSM3) || defined(LIGHT_USE_PSSM4)

varying highp vec4 shadow_coord3;

#if defined(LIGHT_USE_PSSM4)

varying highp vec4 shadow_coord4;

#endif
#endif

uniform vec4 light_clamp;

#endif // light shadow

// directional shadow

#endif

//
// varyings
//

#if defined(RENDER_DEPTH) && defined(USE_RGBA_SHADOWS)
varying highp vec4 position_interp;
#endif

varying highp vec3 vertex_interp;
varying vec3 normal_interp;

#if defined(ENABLE_TANGENT_INTERP) || defined(ENABLE_NORMALMAP)
varying vec3 tangent_interp;
varying vec3 binormal_interp;
#endif

#if defined(ENABLE_COLOR_INTERP)
varying vec4 color_interp;
#endif

#if defined(ENABLE_UV_INTERP)
varying vec2 uv_interp;
#endif

#if defined(ENABLE_UV2_INTERP) || defined(USE_LIGHTMAP)
varying vec2 uv2_interp;
#endif

varying vec3 view_interp;

vec3 F0(float metallic, float specular, vec3 albedo) {
	float dielectric = 0.16 * specular * specular;
	// use albedo * metallic as colored specular reflectance at 0 angle for metallic materials;
	// see https://google.github.io/filament/Filament.md.html
	return mix(vec3(dielectric), albedo, vec3(metallic));
}

/* clang-format off */

FRAGMENT_SHADER_GLOBALS

/* clang-format on */

#ifdef RENDER_DEPTH_DUAL_PARABOLOID

varying highp float dp_clip;

#endif

#ifdef USE_LIGHTING

// This returns the G_GGX function divided by 2 cos_theta_m, where in practice cos_theta_m is either N.L or N.V.
// We're dividing this factor off because the overall term we'll end up looks like
// (see, for example, the first unnumbered equation in B. Burley, "Physically Based Shading at Disney", SIGGRAPH 2012):
//
//   F(L.V) D(N.H) G(N.L) G(N.V) / (4 N.L N.V)
//
// We're basically regouping this as
//
//   F(L.V) D(N.H) [G(N.L)/(2 N.L)] [G(N.V) / (2 N.V)]
//
// and thus, this function implements the [G(N.m)/(2 N.m)] part with m = L or V.
//
// The contents of the D and G (G1) functions (GGX) are taken from
// E. Heitz, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs", J. Comp. Graph. Tech. 3 (2) (2014).
// Eqns 71-72 and 85-86 (see also Eqns 43 and 80).

/*
float G_GGX_2cos(float cos_theta_m, float alpha) {
	// Schlick's approximation
	// C. Schlick, "An Inexpensive BRDF Model for Physically-based Rendering", Computer Graphics Forum. 13 (3): 233 (1994)
	// Eq. (19), although see Heitz (2014) the about the problems with his derivation.
	// It nevertheless approximates GGX well with k = alpha/2.
	float k = 0.5 * alpha;
	return 0.5 / (cos_theta_m * (1.0 - k) + k);

	// float cos2 = cos_theta_m * cos_theta_m;
	// float sin2 = (1.0 - cos2);
	// return 1.0 / (cos_theta_m + sqrt(cos2 + alpha * alpha * sin2));
}
*/

// This approximates G_GGX_2cos(cos_theta_l, alpha) * G_GGX_2cos(cos_theta_v, alpha)
// See Filament docs, Specular G section.
float V_GGX(float cos_theta_l, float cos_theta_v, float alpha) {
	return 0.5 / mix(2.0 * cos_theta_l * cos_theta_v, cos_theta_l + cos_theta_v, alpha);
}

float D_GGX(float cos_theta_m, float alpha) {
	float alpha2 = alpha * alpha;
	float d = 1.0 + (alpha2 - 1.0) * cos_theta_m * cos_theta_m;
	return alpha2 / (M_PI * d * d);
}

/*
float G_GGX_anisotropic_2cos(float cos_theta_m, float alpha_x, float alpha_y, float cos_phi, float sin_phi) {
	float cos2 = cos_theta_m * cos_theta_m;
	float sin2 = (1.0 - cos2);
	float s_x = alpha_x * cos_phi;
	float s_y = alpha_y * sin_phi;
	return 1.0 / max(cos_theta_m + sqrt(cos2 + (s_x * s_x + s_y * s_y) * sin2), 0.001);
}
*/

// This approximates G_GGX_anisotropic_2cos(cos_theta_l, ...) * G_GGX_anisotropic_2cos(cos_theta_v, ...)
// See Filament docs, Anisotropic specular BRDF section.
float V_GGX_anisotropic(float alpha_x, float alpha_y, float TdotV, float TdotL, float BdotV, float BdotL, float NdotV, float NdotL) {
	float Lambda_V = NdotL * length(vec3(alpha_x * TdotV, alpha_y * BdotV, NdotV));
	float Lambda_L = NdotV * length(vec3(alpha_x * TdotL, alpha_y * BdotL, NdotL));
	return 0.5 / (Lambda_V + Lambda_L);
}

float D_GGX_anisotropic(float cos_theta_m, float alpha_x, float alpha_y, float cos_phi, float sin_phi, float NdotH) {
	float alpha2 = alpha_x * alpha_y;
	highp vec3 v = vec3(alpha_y * cos_phi, alpha_x * sin_phi, alpha2 * NdotH);
	highp float v2 = dot(v, v);
	float w2 = alpha2 / v2;
	float D = alpha2 * w2 * w2 * (1.0 / M_PI);
	return D;

	/* float cos2 = cos_theta_m * cos_theta_m;
	float sin2 = (1.0 - cos2);
	float r_x = cos_phi / alpha_x;
	float r_y = sin_phi / alpha_y;
	float d = cos2 + sin2 * (r_x * r_x + r_y * r_y);
	return 1.0 / max(M_PI * alpha_x * alpha_y * d * d, 0.001); */
}

float SchlickFresnel(float u) {
	float m = 1.0 - u;
	float m2 = m * m;
	return m2 * m2 * m; // pow(m,5)
}

float GTR1(float NdotH, float a) {
	if (a >= 1.0)
		return 1.0 / M_PI;
	float a2 = a * a;
	float t = 1.0 + (a2 - 1.0) * NdotH * NdotH;
	return (a2 - 1.0) / (M_PI * log(a2) * t);
}

#ifdef USE_PHYSICAL_LIGHT_ATTENUATION
float get_omni_attenuation(float distance, float inv_range, float decay) {
	float nd = distance * inv_range;
	nd *= nd;
	nd *= nd; // nd^4
	nd = max(1.0 - nd, 0.0);
	nd *= nd; // nd^2
	return nd * pow(max(distance, 0.0001), -decay);
}
#endif

void light_compute(
		vec3 N,
		vec3 L,
		vec3 V,
		vec3 B,
		vec3 T,
		vec3 light_color,
		vec3 attenuation,
		vec3 diffuse_color,
		vec3 transmission,
		float specular_blob_intensity,
		float roughness,
		float metallic,
		float specular,
		float rim,
		float rim_tint,
		float clearcoat,
		float clearcoat_gloss,
		float anisotropy,
		inout vec3 diffuse_light,
		inout vec3 specular_light,
		inout float alpha) {
//this makes lights behave closer to linear, but then addition of lights looks bad
//better left disabled

//#define SRGB_APPROX(m_var) m_var = pow(m_var,0.4545454545);
/*
#define SRGB_APPROX(m_var) {\
	float S1 = sqrt(m_var);\
	float S2 = sqrt(S1);\
	float S3 = sqrt(S2);\
	m_var = 0.662002687 * S1 + 0.684122060 * S2 - 0.323583601 * S3 - 0.0225411470 * m_var;\
	}
*/
#define SRGB_APPROX(m_var)

#if defined(USE_LIGHT_SHADER_CODE)
	// light is written by the light shader

	vec3 normal = N;
	vec3 albedo = diffuse_color;
	vec3 light = L;
	vec3 view = V;

	/* clang-format off */

LIGHT_SHADER_CODE

	/* clang-format on */

#else
	float NdotL = dot(N, L);
	float cNdotL = max(NdotL, 0.0); // clamped NdotL
	float NdotV = dot(N, V);
	float cNdotV = max(abs(NdotV), 1e-6);

/* Make a default specular mode SPECULAR_SCHLICK_GGX. */
#if !defined(SPECULAR_DISABLED) && !defined(SPECULAR_SCHLICK_GGX) && !defined(SPECULAR_BLINN) && !defined(SPECULAR_PHONG) && !defined(SPECULAR_TOON)
#define SPECULAR_SCHLICK_GGX
#endif

#if defined(DIFFUSE_BURLEY) || defined(SPECULAR_BLINN) || defined(SPECULAR_SCHLICK_GGX) || defined(LIGHT_USE_CLEARCOAT)
	vec3 H = normalize(V + L);
#endif

#if defined(SPECULAR_BLINN) || defined(SPECULAR_SCHLICK_GGX) || defined(LIGHT_USE_CLEARCOAT)
	float cNdotH = max(dot(N, H), 0.0);
#endif

#if defined(DIFFUSE_BURLEY) || defined(SPECULAR_SCHLICK_GGX) || defined(LIGHT_USE_CLEARCOAT)
	float cLdotH = max(dot(L, H), 0.0);
#endif

	if (metallic < 1.0) {
#if defined(DIFFUSE_OREN_NAYAR)
		vec3 diffuse_brdf_NL;
#else
		float diffuse_brdf_NL; // BRDF times N.L for calculating diffuse radiance
#endif

#if defined(DIFFUSE_LAMBERT_WRAP)
		// energy conserving lambert wrap shader
		diffuse_brdf_NL = max(0.0, (NdotL + roughness) / ((1.0 + roughness) * (1.0 + roughness)));

#elif defined(DIFFUSE_OREN_NAYAR)

		{
			// see http://mimosa-pudica.net/improved-oren-nayar.html
			float LdotV = dot(L, V);

			float s = LdotV - NdotL * NdotV;
			float t = mix(1.0, max(NdotL, NdotV), step(0.0, s));

			float sigma2 = roughness * roughness; // TODO: this needs checking
			vec3 A = 1.0 + sigma2 * (-0.5 / (sigma2 + 0.33) + 0.17 * diffuse_color / (sigma2 + 0.13));
			float B = 0.45 * sigma2 / (sigma2 + 0.09);

			diffuse_brdf_NL = cNdotL * (A + vec3(B) * s / t) * (1.0 / M_PI);
		}

#elif defined(DIFFUSE_TOON)

		diffuse_brdf_NL = smoothstep(-roughness, max(roughness, 0.01), NdotL);

#elif defined(DIFFUSE_BURLEY)

		{
			float FD90_minus_1 = 2.0 * cLdotH * cLdotH * roughness - 0.5;
			float FdV = 1.0 + FD90_minus_1 * SchlickFresnel(cNdotV);
			float FdL = 1.0 + FD90_minus_1 * SchlickFresnel(cNdotL);
			diffuse_brdf_NL = (1.0 / M_PI) * FdV * FdL * cNdotL;
			/*
			float energyBias = mix(roughness, 0.0, 0.5);
			float energyFactor = mix(roughness, 1.0, 1.0 / 1.51);
			float fd90 = energyBias + 2.0 * VoH * VoH * roughness;
			float f0 = 1.0;
			float lightScatter = f0 + (fd90 - f0) * pow(1.0 - cNdotL, 5.0);
			float viewScatter = f0 + (fd90 - f0) * pow(1.0 - cNdotV, 5.0);

			diffuse_brdf_NL = lightScatter * viewScatter * energyFactor;
			*/
		}
#else
		// lambert
		diffuse_brdf_NL = cNdotL * (1.0 / M_PI);
#endif

		SRGB_APPROX(diffuse_brdf_NL)

		diffuse_light += light_color * diffuse_color * diffuse_brdf_NL * attenuation;

#if defined(TRANSMISSION_USED)
		diffuse_light += light_color * diffuse_color * (vec3(1.0 / M_PI) - diffuse_brdf_NL) * transmission * attenuation;
#endif

#if defined(LIGHT_USE_RIM)
		float rim_light = pow(max(0.0, 1.0 - cNdotV), max(0.0, (1.0 - roughness) * 16.0));
		diffuse_light += rim_light * rim * mix(vec3(1.0), diffuse_color, rim_tint) * light_color;
#endif
	}

	if (roughness > 0.0) {

#if defined(SPECULAR_SCHLICK_GGX) || defined(SPECULAR_BLINN) || defined(SPECULAR_PHONG)
		vec3 specular_brdf_NL = vec3(0.0);
#else
		float specular_brdf_NL = 0.0;
#endif

#if defined(SPECULAR_BLINN)

		//normalized blinn
		float shininess = exp2(15.0 * (1.0 - roughness) + 1.0) * 0.25;
		float blinn = pow(cNdotH, shininess);
		blinn *= (shininess + 2.0) * (1.0 / (8.0 * M_PI));

		specular_brdf_NL = blinn * diffuse_color * specular;

#elif defined(SPECULAR_PHONG)

		vec3 R = normalize(-reflect(L, N));
		float cRdotV = max(0.0, dot(R, V));
		float shininess = exp2(15.0 * (1.0 - roughness) + 1.0) * 0.25;
		float phong = pow(cRdotV, shininess);
		phong *= (shininess + 1.0) * (1.0 / (8.0 * M_PI));

		specular_brdf_NL = phong * diffuse_color * specular;

#elif defined(SPECULAR_TOON)

		vec3 R = normalize(-reflect(L, N));
		float RdotV = dot(R, V);
		float mid = 1.0 - roughness;
		mid *= mid;
		specular_brdf_NL = smoothstep(mid - roughness * 0.5, mid + roughness * 0.5, RdotV) * mid;

#elif defined(SPECULAR_DISABLED)
		// none..
#elif defined(SPECULAR_SCHLICK_GGX)
		// shlick+ggx as default

#if defined(LIGHT_USE_ANISOTROPY)
		float alpha_ggx = roughness * roughness;
		float aspect = sqrt(1.0 - anisotropy * 0.9);
		float ax = alpha_ggx / aspect;
		float ay = alpha_ggx * aspect;
		float XdotH = dot(T, H);
		float YdotH = dot(B, H);
		float D = D_GGX_anisotropic(cNdotH, ax, ay, XdotH, YdotH, cNdotH);
		//float G = G_GGX_anisotropic_2cos(cNdotL, ax, ay, XdotH, YdotH) * G_GGX_anisotropic_2cos(cNdotV, ax, ay, XdotH, YdotH);
		float G = V_GGX_anisotropic(ax, ay, dot(T, V), dot(T, L), dot(B, V), dot(B, L), cNdotV, cNdotL);

#else
		float alpha_ggx = roughness * roughness;
		float D = D_GGX(cNdotH, alpha_ggx);
		//float G = G_GGX_2cos(cNdotL, alpha_ggx) * G_GGX_2cos(cNdotV, alpha_ggx);
		float G = V_GGX(cNdotL, cNdotV, alpha_ggx);
#endif
		// F
		vec3 f0 = F0(metallic, specular, diffuse_color);
		float cLdotH5 = SchlickFresnel(cLdotH);
		vec3 F = mix(vec3(cLdotH5), vec3(1.0), f0);

		specular_brdf_NL = cNdotL * D * F * G;

#endif

		SRGB_APPROX(specular_brdf_NL)
		specular_light += specular_brdf_NL * light_color * specular_blob_intensity * attenuation;

#if defined(LIGHT_USE_CLEARCOAT)

#if !defined(SPECULAR_SCHLICK_GGX)
		float cLdotH5 = SchlickFresnel(cLdotH);
#endif
		float Dr = GTR1(cNdotH, mix(.1, .001, clearcoat_gloss));
		float Fr = mix(.04, 1.0, cLdotH5);
		//float Gr = G_GGX_2cos(cNdotL, .25) * G_GGX_2cos(cNdotV, .25);
		float Gr = V_GGX(cNdotL, cNdotV, 0.25);

		float clearcoat_specular_brdf_NL = 0.25 * clearcoat * Gr * Fr * Dr * cNdotL;

		specular_light += clearcoat_specular_brdf_NL * light_color * specular_blob_intensity * attenuation;
#endif
	}

#ifdef USE_SHADOW_TO_OPACITY
	alpha = min(alpha, clamp(1.0 - length(attenuation), 0.0, 1.0));
#endif

#endif //defined(USE_LIGHT_SHADER_CODE)
}

#endif
// shadows

#ifdef USE_SHADOW

#ifdef USE_RGBA_SHADOWS

#define SHADOW_DEPTH(m_val) dot(m_val, vec4(1.0 / (255.0 * 255.0 * 255.0), 1.0 / (255.0 * 255.0), 1.0 / 255.0, 1.0))

#else

#define SHADOW_DEPTH(m_val) (m_val).r

#endif

#define SAMPLE_SHADOW_TEXEL(p_shadow, p_pos, p_depth) step(p_depth, SHADOW_DEPTH(texture2D(p_shadow, p_pos)))

float sample_shadow(highp sampler2D shadow, highp vec4 spos) {
	spos.xyz /= spos.w;
	vec2 pos = spos.xy;
	float depth = spos.z;

#ifdef SHADOW_MODE_PCF_13
	// This codepath is used for both PCF13 and PCF25 options,
	// as PCF25 is not implemented in GLES2 for performance reasons.
	//
	// Soft PCF filter adapted from three.js:
	// https://github.com/mrdoob/three.js/blob/0c815022849389cbe6de14a93e1c2fc7e4b21c18/src/renderers/shaders/ShaderChunk/shadowmap_pars_fragment.glsl.js#L148-L182
	// This method actually uses 16 shadow samples. This soft filter isn't needed in GLES3
	// as we can use hardware-based linear filtering instead of emulating it in the shader
	// like we're doing here.
	vec2 f = fract(pos * (1.0 / shadow_pixel_size) + 0.5);
	pos -= f * shadow_pixel_size;

	return (
				   SAMPLE_SHADOW_TEXEL(shadow, pos, depth) +
				   SAMPLE_SHADOW_TEXEL(shadow, pos + vec2(shadow_pixel_size.x, 0.0), depth) +
				   SAMPLE_SHADOW_TEXEL(shadow, pos + vec2(0.0, shadow_pixel_size.y), depth) +
				   SAMPLE_SHADOW_TEXEL(shadow, pos + shadow_pixel_size, depth) +
				   mix(
						   SAMPLE_SHADOW_TEXEL(shadow, pos + vec2(-shadow_pixel_size.x, 0.0), depth),
						   SAMPLE_SHADOW_TEXEL(shadow, pos + vec2(2.0 * shadow_pixel_size.x, 0.0), depth),
						   f.x) +
				   mix(
						   SAMPLE_SHADOW_TEXEL(shadow, pos + vec2(-shadow_pixel_size.x, shadow_pixel_size.y), depth),
						   SAMPLE_SHADOW_TEXEL(shadow, pos + vec2(2.0 * shadow_pixel_size.x, shadow_pixel_size.y), depth),
						   f.x) +
				   mix(
						   SAMPLE_SHADOW_TEXEL(shadow, pos + vec2(0.0, -shadow_pixel_size.y), depth),
						   SAMPLE_SHADOW_TEXEL(shadow, pos + vec2(0.0, 2.0 * shadow_pixel_size.y), depth),
						   f.y) +
				   mix(
						   SAMPLE_SHADOW_TEXEL(shadow, pos + vec2(shadow_pixel_size.x, -shadow_pixel_size.y), depth),
						   SAMPLE_SHADOW_TEXEL(shadow, pos + vec2(shadow_pixel_size.x, 2.0 * shadow_pixel_size.y), depth),
						   f.y) +
				   mix(
						   mix(SAMPLE_SHADOW_TEXEL(shadow, pos + vec2(-shadow_pixel_size.x, -shadow_pixel_size.y), depth),
								   SAMPLE_SHADOW_TEXEL(shadow, pos + vec2(2.0 * shadow_pixel_size.x, -shadow_pixel_size.y), depth),
								   f.x),
						   mix(SAMPLE_SHADOW_TEXEL(shadow, pos + vec2(-shadow_pixel_size.x, 2.0 * shadow_pixel_size.y), depth),
								   SAMPLE_SHADOW_TEXEL(shadow, pos + vec2(2.0 * shadow_pixel_size.x, 2.0 * shadow_pixel_size.y), depth),
								   f.x),
						   f.y)) *
			(1.0 / 9.0);
#endif

#ifdef SHADOW_MODE_PCF_5

	float avg = SAMPLE_SHADOW_TEXEL(shadow, pos, depth);
	avg += SAMPLE_SHADOW_TEXEL(shadow, pos + vec2(shadow_pixel_size.x, 0.0), depth);
	avg += SAMPLE_SHADOW_TEXEL(shadow, pos + vec2(-shadow_pixel_size.x, 0.0), depth);
	avg += SAMPLE_SHADOW_TEXEL(shadow, pos + vec2(0.0, shadow_pixel_size.y), depth);
	avg += SAMPLE_SHADOW_TEXEL(shadow, pos + vec2(0.0, -shadow_pixel_size.y), depth);
	return avg * (1.0 / 5.0);

#endif

#if !defined(SHADOW_MODE_PCF_5) && !defined(SHADOW_MODE_PCF_13)

	return SAMPLE_SHADOW_TEXEL(shadow, pos, depth);
#endif
}

#endif

#if defined(FOG_DEPTH_ENABLED) || defined(FOG_HEIGHT_ENABLED)

#if defined(USE_VERTEX_LIGHTING)

varying vec4 fog_interp;

#else
uniform mediump vec4 fog_color_base;
#ifdef LIGHT_MODE_DIRECTIONAL
uniform mediump vec4 fog_sun_color_amount;
#endif

uniform bool fog_transmit_enabled;
uniform mediump float fog_transmit_curve;

#ifdef FOG_DEPTH_ENABLED
uniform highp float fog_depth_begin;
uniform mediump float fog_depth_curve;
uniform mediump float fog_max_distance;
#endif

#ifdef FOG_HEIGHT_ENABLED
uniform highp float fog_height_min;
uniform highp float fog_height_max;
uniform mediump float fog_height_curve;
#endif

#endif //vertex lit
#endif //fog

void main() {
#ifdef RENDER_DEPTH_DUAL_PARABOLOID

	if (dp_clip > 0.0)
		discard;
#endif
	highp vec3 vertex = vertex_interp;
	vec3 view = -normalize(vertex_interp);
	vec3 albedo = vec3(1.0);
	vec3 transmission = vec3(0.0);
	float metallic = 0.0;
	float specular = 0.5;
	vec3 emission = vec3(0.0);
	float roughness = 1.0;
	float rim = 0.0;
	float rim_tint = 0.0;
	float clearcoat = 0.0;
	float clearcoat_gloss = 0.0;
	float anisotropy = 0.0;
	vec2 anisotropy_flow = vec2(1.0, 0.0);
	float sss_strength = 0.0; //unused
	// gl_FragDepth is not available in GLES2, so writing to DEPTH is not converted to gl_FragDepth by Godot compiler resulting in a
	// compile error because DEPTH is not a variable.
	float m_DEPTH = 0.0;

	float alpha = 1.0;
	float side = 1.0;

	float specular_blob_intensity = 1.0;
#if defined(SPECULAR_TOON)
	specular_blob_intensity *= specular * 2.0;
#endif

#if defined(ENABLE_AO)
	float ao = 1.0;
	float ao_light_affect = 0.0;
#endif

#if defined(ENABLE_TANGENT_INTERP) || defined(ENABLE_NORMALMAP)
	vec3 binormal = normalize(binormal_interp) * side;
	vec3 tangent = normalize(tangent_interp) * side;
#else
	vec3 binormal = vec3(0.0);
	vec3 tangent = vec3(0.0);
#endif
	vec3 normal = normalize(normal_interp) * side;

#if defined(ENABLE_NORMALMAP)
	vec3 normalmap = vec3(0.5);
#endif
	float normaldepth = 1.0;

#if defined(ALPHA_SCISSOR_USED)
	float alpha_scissor = 0.5;
#endif

#if defined(SCREEN_UV_USED)
	vec2 screen_uv = gl_FragCoord.xy * screen_pixel_size;
#endif

	{
		/* clang-format off */

FRAGMENT_SHADER_CODE

		/* clang-format on */
	}

#if defined(ENABLE_NORMALMAP)
	normalmap.xy = normalmap.xy * 2.0 - 1.0;
	normalmap.z = sqrt(max(0.0, 1.0 - dot(normalmap.xy, normalmap.xy)));

	normal = normalize(mix(normal_interp, tangent * normalmap.x + binormal * normalmap.y + normal * normalmap.z, normaldepth)) * side;
	//normal = normalmap;
#endif

	normal = normalize(normal);

	vec3 N = normal;

	vec3 specular_light = vec3(0.0, 0.0, 0.0);
	vec3 diffuse_light = vec3(0.0, 0.0, 0.0);
	vec3 ambient_light = vec3(0.0, 0.0, 0.0);

	vec3 eye_position = view;

#if !defined(USE_SHADOW_TO_OPACITY)

#if defined(ALPHA_SCISSOR_USED)
	if (alpha < alpha_scissor) {
		discard;
	}
#endif // ALPHA_SCISSOR_USED

#ifdef USE_DEPTH_PREPASS
#if !defined(ALPHA_SCISSOR_USED)

	if (alpha < 0.1) {
		discard;
	}

#endif // not ALPHA_SCISSOR_USED
#endif // USE_DEPTH_PREPASS

#endif // !USE_SHADOW_TO_OPACITY

#ifdef BASE_PASS

	// IBL precalculations
	float ndotv = clamp(dot(normal, eye_position), 0.0, 1.0);
	vec3 f0 = F0(metallic, specular, albedo);
	vec3 F = f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(1.0 - ndotv, 5.0);

#ifdef AMBIENT_LIGHT_DISABLED
	ambient_light = vec3(0.0, 0.0, 0.0);
#else

#ifdef USE_RADIANCE_MAP

	vec3 ref_vec = reflect(-eye_position, N);
	float horizon = min(1.0 + dot(ref_vec, normal), 1.0);
	ref_vec = normalize((radiance_inverse_xform * vec4(ref_vec, 0.0)).xyz);

	ref_vec.z *= -1.0;

	specular_light = textureCubeLod(radiance_map, ref_vec, roughness * RADIANCE_MAX_LOD).xyz * bg_energy;
	specular_light *= horizon * horizon;
#ifndef USE_LIGHTMAP
	{
		vec3 ambient_dir = normalize((radiance_inverse_xform * vec4(normal, 0.0)).xyz);
		vec3 env_ambient = textureCubeLod(radiance_map, ambient_dir, 4.0).xyz * bg_energy;
		env_ambient *= 1.0 - F;

		ambient_light = mix(ambient_color.rgb, env_ambient, ambient_sky_contribution);
	}
#endif

#else

	ambient_light = ambient_color.rgb;
	specular_light = bg_color.rgb * bg_energy;

#endif
#endif // AMBIENT_LIGHT_DISABLED
	ambient_light *= ambient_energy;

#if defined(USE_REFLECTION_PROBE1) || defined(USE_REFLECTION_PROBE2)

	vec4 ambient_accum = vec4(0.0);
	vec4 reflection_accum = vec4(0.0);

#ifdef USE_REFLECTION_PROBE1

	reflection_process(reflection_probe1,
#ifdef USE_VERTEX_LIGHTING
			refprobe1_reflection_normal_blend.rgb,
#ifndef USE_LIGHTMAP
			refprobe1_ambient_normal,
#endif
			refprobe1_reflection_normal_blend.a,
#else
			normal, vertex_interp, refprobe1_local_matrix,
			refprobe1_use_box_project, refprobe1_box_extents, refprobe1_box_offset,
#endif
			refprobe1_exterior, refprobe1_intensity, refprobe1_ambient, roughness,
			ambient_light, specular_light, reflection_accum, ambient_accum);

#endif // USE_REFLECTION_PROBE1

#ifdef USE_REFLECTION_PROBE2

	reflection_process(reflection_probe2,
#ifdef USE_VERTEX_LIGHTING
			refprobe2_reflection_normal_blend.rgb,
#ifndef USE_LIGHTMAP
			refprobe2_ambient_normal,
#endif
			refprobe2_reflection_normal_blend.a,
#else
			normal, vertex_interp, refprobe2_local_matrix,
			refprobe2_use_box_project, refprobe2_box_extents, refprobe2_box_offset,
#endif
			refprobe2_exterior, refprobe2_intensity, refprobe2_ambient, roughness,
			ambient_light, specular_light, reflection_accum, ambient_accum);

#endif // USE_REFLECTION_PROBE2

	if (reflection_accum.a > 0.0) {
		specular_light = reflection_accum.rgb / reflection_accum.a;
	}

#ifndef USE_LIGHTMAP
	if (ambient_accum.a > 0.0) {
		ambient_light = ambient_accum.rgb / ambient_accum.a;
	}
#endif

#endif // defined(USE_REFLECTION_PROBE1) || defined(USE_REFLECTION_PROBE2)

	// environment BRDF approximation
	{
#if defined(DIFFUSE_TOON)
		//simplify for toon, as
		specular_light *= specular * metallic * albedo * 2.0;
#else

		// scales the specular reflections, needs to be be computed before lighting happens,
		// but after environment and reflection probes are added
		//TODO: this curve is not really designed for gammaspace, should be adjusted
		const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
		const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
		vec4 r = roughness * c0 + c1;
		float a004 = min(r.x * r.x, exp2(-9.28 * ndotv)) * r.x + r.y;
		vec2 env = vec2(-1.04, 1.04) * a004 + r.zw;
		specular_light *= env.x * F + env.y;

#endif
	}

#ifdef USE_LIGHTMAP
//ambient light will come entirely from lightmap is lightmap is used
#if defined(USE_LIGHTMAP_FILTER_BICUBIC)
	ambient_light = texture2D_bicubic(lightmap, uv2_interp).rgb * lightmap_energy;
#else
	ambient_light = texture2D(lightmap, uv2_interp).rgb * lightmap_energy;
#endif
#endif

#ifdef USE_LIGHTMAP_CAPTURE
	{
		vec3 cone_dirs[12];
		cone_dirs[0] = vec3(0.0, 0.0, 1.0);
		cone_dirs[1] = vec3(0.866025, 0.0, 0.5);
		cone_dirs[2] = vec3(0.267617, 0.823639, 0.5);
		cone_dirs[3] = vec3(-0.700629, 0.509037, 0.5);
		cone_dirs[4] = vec3(-0.700629, -0.509037, 0.5);
		cone_dirs[5] = vec3(0.267617, -0.823639, 0.5);
		cone_dirs[6] = vec3(0.0, 0.0, -1.0);
		cone_dirs[7] = vec3(0.866025, 0.0, -0.5);
		cone_dirs[8] = vec3(0.267617, 0.823639, -0.5);
		cone_dirs[9] = vec3(-0.700629, 0.509037, -0.5);
		cone_dirs[10] = vec3(-0.700629, -0.509037, -0.5);
		cone_dirs[11] = vec3(0.267617, -0.823639, -0.5);

		vec3 local_normal = normalize(camera_matrix * vec4(normal, 0.0)).xyz;
		vec4 captured = vec4(0.0);
		float sum = 0.0;
		for (int i = 0; i < 12; i++) {
			float amount = max(0.0, dot(local_normal, cone_dirs[i])); //not correct, but creates a nice wrap around effect
			captured += lightmap_captures[i] * amount;
			sum += amount;
		}

		captured /= sum;

		// Alpha channel is used to indicate if dynamic objects keep the environment lighting
		if (lightmap_captures[0].a > 0.5) {
			ambient_light += captured.rgb;
		} else {
			ambient_light = captured.rgb;
		}
	}
#endif

#endif //BASE PASS

//
// Lighting
//
#ifdef USE_LIGHTING

#ifndef USE_VERTEX_LIGHTING
	vec3 L;
#endif
	vec3 light_att = vec3(1.0);

#ifdef LIGHT_MODE_OMNI

#ifndef USE_VERTEX_LIGHTING
	vec3 light_vec = light_position - vertex;
	float light_length = length(light_vec);

	float normalized_distance = light_length / light_range;
	if (normalized_distance < 1.0) {
#ifdef USE_PHYSICAL_LIGHT_ATTENUATION
		float omni_attenuation = get_omni_attenuation(light_length, 1.0 / light_range, light_attenuation);
#else
		float omni_attenuation = pow(1.0 - normalized_distance, light_attenuation);
#endif

		light_att = vec3(omni_attenuation);
	} else {
		light_att = vec3(0.0);
	}
	L = normalize(light_vec);

#endif

#if !defined(SHADOWS_DISABLED)

#ifdef USE_SHADOW
	{
		highp vec4 splane = shadow_coord;
		float shadow_len = length(splane.xyz);

		splane.xyz = normalize(splane.xyz);

		vec4 clamp_rect = light_clamp;

		if (splane.z >= 0.0) {
			splane.z += 1.0;

			clamp_rect.y += clamp_rect.w;
		} else {
			splane.z = 1.0 - splane.z;
		}

		splane.xy /= splane.z;
		splane.xy = splane.xy * 0.5 + 0.5;
		splane.z = shadow_len / light_range;

		splane.xy = clamp_rect.xy + splane.xy * clamp_rect.zw;
		splane.w = 1.0;

		float shadow = sample_shadow(light_shadow_atlas, splane);

		light_att *= mix(shadow_color.rgb, vec3(1.0), shadow);
	}
#endif

#endif //SHADOWS_DISABLED

#endif //type omni

#ifdef LIGHT_MODE_DIRECTIONAL

#ifndef USE_VERTEX_LIGHTING
	vec3 light_vec = -light_direction;
	L = normalize(light_vec);
#endif
	float depth_z = -vertex.z;

#if !defined(SHADOWS_DISABLED)

#ifdef USE_SHADOW

#ifdef USE_VERTEX_LIGHTING
	//compute shadows in a mobile friendly way

#ifdef LIGHT_USE_PSSM4
	//take advantage of prefetch
	float shadow1 = sample_shadow(light_directional_shadow, shadow_coord);
	float shadow2 = sample_shadow(light_directional_shadow, shadow_coord2);
	float shadow3 = sample_shadow(light_directional_shadow, shadow_coord3);
	float shadow4 = sample_shadow(light_directional_shadow, shadow_coord4);

	if (depth_z < light_split_offsets.w) {
		float shadow_att = 1.0;
#ifdef LIGHT_USE_PSSM_BLEND
		float shadow_att2 = 1.0;
		float pssm_blend = 0.0;
		bool use_blend = true;
#endif
		if (depth_z < light_split_offsets.y) {
			if (depth_z < light_split_offsets.x) {
				shadow_att = shadow1;

#ifdef LIGHT_USE_PSSM_BLEND
				shadow_att2 = shadow2;

				pssm_blend = smoothstep(0.0, light_split_offsets.x, depth_z);
#endif
			} else {
				shadow_att = shadow2;

#ifdef LIGHT_USE_PSSM_BLEND
				shadow_att2 = shadow3;

				pssm_blend = smoothstep(light_split_offsets.x, light_split_offsets.y, depth_z);
#endif
			}
		} else {
			if (depth_z < light_split_offsets.z) {
				shadow_att = shadow3;

#if defined(LIGHT_USE_PSSM_BLEND)
				shadow_att2 = shadow4;
				pssm_blend = smoothstep(light_split_offsets.y, light_split_offsets.z, depth_z);
#endif

			} else {
				shadow_att = shadow4;

#if defined(LIGHT_USE_PSSM_BLEND)
				use_blend = false;
#endif
			}
		}
#if defined(LIGHT_USE_PSSM_BLEND)
		if (use_blend) {
			shadow_att = mix(shadow_att, shadow_att2, pssm_blend);
		}
#endif
		light_att *= mix(shadow_color.rgb, vec3(1.0), shadow_att);
	}

#endif //LIGHT_USE_PSSM4

#ifdef LIGHT_USE_PSSM3
	//take advantage of prefetch
	float shadow1 = sample_shadow(light_directional_shadow, shadow_coord);
	float shadow2 = sample_shadow(light_directional_shadow, shadow_coord2);
	float shadow3 = sample_shadow(light_directional_shadow, shadow_coord3);

	if (depth_z < light_split_offsets.z) {
		float shadow_att = 1.0;
#ifdef LIGHT_USE_PSSM_BLEND
		float shadow_att2 = 1.0;
		float pssm_blend = 0.0;
		bool use_blend = true;
#endif
		if (depth_z < light_split_offsets.y) {
			if (depth_z < light_split_offsets.x) {
				shadow_att = shadow1;

#ifdef LIGHT_USE_PSSM_BLEND
				shadow_att2 = shadow2;

				pssm_blend = smoothstep(0.0, light_split_offsets.x, depth_z);
#endif
			} else {
				shadow_att = shadow2;

#ifdef LIGHT_USE_PSSM_BLEND
				shadow_att2 = shadow3;

				pssm_blend = smoothstep(light_split_offsets.x, light_split_offsets.y, depth_z);
#endif
			}
		} else {
			shadow_att = shadow3;

#if defined(LIGHT_USE_PSSM_BLEND)
			use_blend = false;
#endif
		}
#if defined(LIGHT_USE_PSSM_BLEND)
		if (use_blend) {
			shadow_att = mix(shadow_att, shadow_att2, pssm_blend);
		}
#endif
		light_att *= mix(shadow_color.rgb, vec3(1.0), shadow_att);
	}

#endif //LIGHT_USE_PSSM3

#ifdef LIGHT_USE_PSSM2

	//take advantage of prefetch
	float shadow1 = sample_shadow(light_directional_shadow, shadow_coord);
	float shadow2 = sample_shadow(light_directional_shadow, shadow_coord2);

	if (depth_z < light_split_offsets.y) {
		float shadow_att = 1.0;

#ifdef LIGHT_USE_PSSM_BLEND
		float shadow_att2 = 1.0;
		float pssm_blend = 0.0;
		bool use_blend = true;
#endif
		if (depth_z < light_split_offsets.x) {
			shadow_att = shadow1;

#ifdef LIGHT_USE_PSSM_BLEND
			shadow_att2 = shadow2;
			pssm_blend = smoothstep(0.0, light_split_offsets.x, depth_z);
#endif
		} else {
			shadow_att = shadow2;
#ifdef LIGHT_USE_PSSM_BLEND
			use_blend = false;
#endif
		}
#ifdef LIGHT_USE_PSSM_BLEND
		if (use_blend) {
			shadow_att = mix(shadow_att, shadow_att2, pssm_blend);
		}
#endif
		light_att *= mix(shadow_color.rgb, vec3(1.0), shadow_att);
	}

#endif //LIGHT_USE_PSSM2

#if !defined(LIGHT_USE_PSSM4) && !defined(LIGHT_USE_PSSM3) && !defined(LIGHT_USE_PSSM2)

	light_att *= mix(shadow_color.rgb, vec3(1.0), sample_shadow(light_directional_shadow, shadow_coord));
#endif //orthogonal

#else //fragment version of pssm

	{
#ifdef LIGHT_USE_PSSM4
		if (depth_z < light_split_offsets.w) {
#elif defined(LIGHT_USE_PSSM3)
		if (depth_z < light_split_offsets.z) {
#elif defined(LIGHT_USE_PSSM2)
		if (depth_z < light_split_offsets.y) {
#else
		if (depth_z < light_split_offsets.x) {
#endif //pssm2

			highp vec4 pssm_coord;

#ifdef LIGHT_USE_PSSM_BLEND
			float pssm_blend;
			highp vec4 pssm_coord2;
			bool use_blend = true;
#endif

#ifdef LIGHT_USE_PSSM4

			if (depth_z < light_split_offsets.y) {
				if (depth_z < light_split_offsets.x) {
					pssm_coord = shadow_coord;

#ifdef LIGHT_USE_PSSM_BLEND
					pssm_coord2 = shadow_coord2;

					pssm_blend = smoothstep(0.0, light_split_offsets.x, depth_z);
#endif
				} else {
					pssm_coord = shadow_coord2;

#ifdef LIGHT_USE_PSSM_BLEND
					pssm_coord2 = shadow_coord3;

					pssm_blend = smoothstep(light_split_offsets.x, light_split_offsets.y, depth_z);
#endif
				}
			} else {
				if (depth_z < light_split_offsets.z) {
					pssm_coord = shadow_coord3;

#if defined(LIGHT_USE_PSSM_BLEND)
					pssm_coord2 = shadow_coord4;
					pssm_blend = smoothstep(light_split_offsets.y, light_split_offsets.z, depth_z);
#endif

				} else {
					pssm_coord = shadow_coord4;

#if defined(LIGHT_USE_PSSM_BLEND)
					use_blend = false;
#endif
				}
			}

#endif // LIGHT_USE_PSSM4

#ifdef LIGHT_USE_PSSM3

			if (depth_z < light_split_offsets.y) {
				if (depth_z < light_split_offsets.x) {
					pssm_coord = shadow_coord;

#ifdef LIGHT_USE_PSSM_BLEND
					pssm_coord2 = shadow_coord2;
					pssm_blend = smoothstep(0.0, light_split_offsets.x, depth_z);
#endif
				} else {
					pssm_coord = shadow_coord2;

#ifdef LIGHT_USE_PSSM_BLEND
					pssm_coord2 = shadow_coord3;
					pssm_blend = smoothstep(light_split_offsets.x, light_split_offsets.y, depth_z);
#endif
				}
			} else {
				pssm_coord = shadow_coord3;

#if defined(LIGHT_USE_PSSM_BLEND)
				use_blend = false;
#endif
			}

#endif // LIGHT_USE_PSSM3

#ifdef LIGHT_USE_PSSM2
			if (depth_z < light_split_offsets.x) {
				pssm_coord = shadow_coord;

#ifdef LIGHT_USE_PSSM_BLEND
				pssm_coord2 = shadow_coord2;
				pssm_blend = smoothstep(0.0, light_split_offsets.x, depth_z);
#endif
			} else {
				pssm_coord = shadow_coord2;
#ifdef LIGHT_USE_PSSM_BLEND
				use_blend = false;
#endif
			}

#endif // LIGHT_USE_PSSM2

#if !defined(LIGHT_USE_PSSM4) && !defined(LIGHT_USE_PSSM3) && !defined(LIGHT_USE_PSSM2)
			{
				pssm_coord = shadow_coord;
			}
#endif

			float shadow = sample_shadow(light_directional_shadow, pssm_coord);

#ifdef LIGHT_USE_PSSM_BLEND
			if (use_blend) {
				shadow = mix(shadow, sample_shadow(light_directional_shadow, pssm_coord2), pssm_blend);
			}
#endif

			float pssm_fade = smoothstep(fade_from, fade_to, vertex.z);
			light_att *= mix(mix(shadow_color.rgb, vec3(1.0), shadow), vec3(1.0), pssm_fade);
		}
	}
#endif //use vertex lighting

#endif //use shadow

#endif // SHADOWS_DISABLED

#endif

#ifdef LIGHT_MODE_SPOT

	light_att = vec3(1.0);

#ifndef USE_VERTEX_LIGHTING

	vec3 light_rel_vec = light_position - vertex;
	float light_length = length(light_rel_vec);
	float normalized_distance = light_length / light_range;

	if (normalized_distance < 1.0) {
#ifdef USE_PHYSICAL_LIGHT_ATTENUATION
		float spot_attenuation = get_omni_attenuation(light_length, 1.0 / light_range, light_attenuation);
#else
		float spot_attenuation = pow(1.0 - normalized_distance, light_attenuation);
#endif

		vec3 spot_dir = light_direction;

		float spot_cutoff = light_spot_angle;
		float angle = dot(-normalize(light_rel_vec), spot_dir);

		if (angle > spot_cutoff) {
			float scos = max(angle, spot_cutoff);
			float spot_rim = max(0.0001, (1.0 - scos) / (1.0 - spot_cutoff));
			spot_attenuation *= 1.0 - pow(spot_rim, light_spot_attenuation);

			light_att = vec3(spot_attenuation);
		} else {
			light_att = vec3(0.0);
		}
	} else {
		light_att = vec3(0.0);
	}

	L = normalize(light_rel_vec);

#endif

#if !defined(SHADOWS_DISABLED)

#ifdef USE_SHADOW
	{
		highp vec4 splane = shadow_coord;

		float shadow = sample_shadow(light_shadow_atlas, splane);
		light_att *= mix(shadow_color.rgb, vec3(1.0), shadow);
	}
#endif

#endif // SHADOWS_DISABLED

#endif // LIGHT_MODE_SPOT

#ifdef USE_VERTEX_LIGHTING
	//vertex lighting
	specular_light += specular_interp * albedo * specular * specular_blob_intensity * light_att;
	diffuse_light += diffuse_interp * albedo * light_att;

#else
	//fragment lighting
	light_compute(
			normal,
			L,
			eye_position,
			binormal,
			tangent,
			light_color.xyz,
			light_att,
			albedo,
			transmission,
			specular_blob_intensity * light_specular,
			roughness,
			metallic,
			specular,
			rim,
			rim_tint,
			clearcoat,
			clearcoat_gloss,
			anisotropy,
			diffuse_light,
			specular_light,
			alpha);

#endif //vertex lighting

#endif //USE_LIGHTING
	//compute and merge

#ifdef USE_SHADOW_TO_OPACITY

	alpha = min(alpha, clamp(length(ambient_light), 0.0, 1.0));

#if defined(ALPHA_SCISSOR_USED)
	if (alpha < alpha_scissor) {
		discard;
	}
#endif // ALPHA_SCISSOR_USED

#ifdef USE_DEPTH_PREPASS
#if !defined(ALPHA_SCISSOR_USED)

	if (alpha < 0.1) {
		discard;
	}

#endif // not ALPHA_SCISSOR_USED
#endif // USE_DEPTH_PREPASS

#endif // !USE_SHADOW_TO_OPACITY

	// Instead of writing directly to gl_FragColor,
	// we use an intermediate, and only write
	// to gl_FragColor ONCE at the end of the shader.
	// This is because some hardware can have huge
	// slowdown if you modify gl_FragColor multiple times.
	vec4 frag_color;

#ifndef RENDER_DEPTH

#ifdef SHADELESS

	frag_color = vec4(albedo, alpha);
#else

	ambient_light *= albedo;

#if defined(ENABLE_AO)
	ambient_light *= ao;
	ao_light_affect = mix(1.0, ao, ao_light_affect);
	specular_light *= ao_light_affect;
	diffuse_light *= ao_light_affect;
#endif

	diffuse_light *= 1.0 - metallic;
	ambient_light *= 1.0 - metallic;

	frag_color = vec4(ambient_light + diffuse_light + specular_light, alpha);

	//add emission if in base pass
#ifdef BASE_PASS
	frag_color.rgb += emission;
#endif
	// frag_color = vec4(normal, 1.0);

//apply fog
#if defined(FOG_DEPTH_ENABLED) || defined(FOG_HEIGHT_ENABLED)

#if defined(USE_VERTEX_LIGHTING)

#if defined(BASE_PASS)
	frag_color.rgb = mix(frag_color.rgb, fog_interp.rgb, fog_interp.a);
#else
	frag_color.rgb *= (1.0 - fog_interp.a);
#endif // BASE_PASS

#else //pixel based fog
	float fog_amount = 0.0;

#ifdef LIGHT_MODE_DIRECTIONAL

	vec3 fog_color = mix(fog_color_base.rgb, fog_sun_color_amount.rgb, fog_sun_color_amount.a * pow(max(dot(eye_position, light_direction), 0.0), 8.0));
#else
	vec3 fog_color = fog_color_base.rgb;
#endif

#ifdef FOG_DEPTH_ENABLED

	{
		float fog_z = smoothstep(fog_depth_begin, fog_max_distance, length(vertex));

		fog_amount = pow(fog_z, fog_depth_curve) * fog_color_base.a;

		if (fog_transmit_enabled) {
			vec3 total_light = frag_color.rgb;
			float transmit = pow(fog_z, fog_transmit_curve);
			fog_color = mix(max(total_light, fog_color), fog_color, transmit);
		}
	}
#endif

#ifdef FOG_HEIGHT_ENABLED
	{
		float y = (camera_matrix * vec4(vertex, 1.0)).y;
		fog_amount = max(fog_amount, pow(smoothstep(fog_height_min, fog_height_max, y), fog_height_curve));
	}
#endif

#if defined(BASE_PASS)
	frag_color.rgb = mix(frag_color.rgb, fog_color, fog_amount);
#else
	frag_color.rgb *= (1.0 - fog_amount);
#endif // BASE_PASS

#endif //use vertex lit

#endif // defined(FOG_DEPTH_ENABLED) || defined(FOG_HEIGHT_ENABLED)

#endif //unshaded

#ifdef OUTPUT_LINEAR
	// sRGB -> linear
	frag_color.rgb = mix(pow((frag_color.rgb + vec3(0.055)) * (1.0 / (1.0 + 0.055)), vec3(2.4)), frag_color.rgb * (1.0 / 12.92), vec3(lessThan(frag_color.rgb, vec3(0.04045))));
#endif

	// Write to the final output once and only once.
	// Use a temporary in the rest of the shader.
	// This is for drivers that have a performance drop
	// when the output is read during the shader.
	gl_FragColor = frag_color;

#else // not RENDER_DEPTH
//depth render
#ifdef USE_RGBA_SHADOWS

	highp float depth = ((position_interp.z / position_interp.w) + 1.0) * 0.5 + 0.0; // bias
	highp vec4 comp = fract(depth * vec4(255.0 * 255.0 * 255.0, 255.0 * 255.0, 255.0, 1.0));
	comp -= comp.xxyz * vec4(0.0, 1.0 / 255.0, 1.0 / 255.0, 1.0 / 255.0);
	gl_FragColor = comp;

#endif
#endif
}

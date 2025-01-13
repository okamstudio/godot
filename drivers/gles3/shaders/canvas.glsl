/* clang-format off */
#[modes]

mode_default =

#[specializations]

DISABLE_LIGHTING = true
USE_RGBA_SHADOWS = false
USE_NINEPATCH = false
USE_PRIMITIVE = false
USE_ATTRIBUTES = false
USE_INSTANCING = false

#[vertex]

#ifdef USE_ATTRIBUTES
layout(location = 0) in vec2 vertex_attrib;
layout(location = 3) in vec4 color_attrib;
layout(location = 4) in vec2 uv_attrib;

#ifdef USE_INSTANCING

layout(location = 1) in highp vec4 instance_xform0;
layout(location = 2) in highp vec4 instance_xform1;
layout(location = 5) in highp uvec4 instance_color_custom_data; // Color packed into xy, custom_data packed into zw for compatibility with 3D

#endif // USE_INSTANCING

#endif // USE_ATTRIBUTES

#include "stdlib_inc.glsl"

#if defined(CUSTOM0_USED)
layout(location = 6) in highp vec4 custom0_attrib;
#endif

#if defined(CUSTOM1_USED)
layout(location = 7) in highp vec4 custom1_attrib;
#endif

layout(location = 8) in highp vec4 attrib_A;
layout(location = 9) in highp vec4 attrib_B;
layout(location = 10) in highp vec4 attrib_C;
layout(location = 11) in highp vec4 attrib_D;
layout(location = 12) in highp vec4 attrib_E;
#ifdef USE_PRIMITIVE
layout(location = 13) in highp uvec4 attrib_F;
#else
layout(location = 13) in highp vec4 attrib_F;
#endif
layout(location = 14) in highp uvec4 attrib_G;
layout(location = 15) in highp uvec4 attrib_H;

#define read_draw_data_world_x attrib_A.xy
#define read_draw_data_world_y attrib_A.zw
#define read_draw_data_world_ofs attrib_B.xy
#define read_draw_data_color_texture_pixel_size attrib_B.zw

#ifdef USE_PRIMITIVE

#define read_draw_data_point_a attrib_C.xy
#define read_draw_data_point_b attrib_C.zw
#define read_draw_data_point_c attrib_D.xy
#define read_draw_data_uv_a attrib_D.zw
#define read_draw_data_uv_b attrib_E.xy
#define read_draw_data_uv_c attrib_E.zw

#define read_draw_data_color_a_rg attrib_F.x
#define read_draw_data_color_a_ba attrib_F.y
#define read_draw_data_color_b_rg attrib_F.z
#define read_draw_data_color_b_ba attrib_F.w
#define read_draw_data_color_c_rg attrib_G.x
#define read_draw_data_color_c_ba attrib_G.y

#else

#define read_draw_data_modulation attrib_C
#define read_draw_data_ninepatch_margins attrib_D
#define read_draw_data_dst_rect attrib_E
#define read_draw_data_src_rect attrib_F

#endif

#define read_draw_data_flags attrib_G.z
#define read_draw_data_specular_shininess attrib_G.w
#define read_draw_data_lights attrib_H

// Varyings so the per-instance info can be used in the fragment shader
flat out vec4 varying_A;
flat out vec2 varying_B;
#ifndef USE_PRIMITIVE
flat out vec4 varying_C;
#ifndef USE_ATTRIBUTES
#ifdef USE_NINEPATCH

flat out vec2 varying_D;
#endif
flat out vec4 varying_E;
#endif
#endif
flat out uvec2 varying_F;
flat out uvec4 varying_G;

// This needs to be outside clang-format so the ubo comment is in the right place
#ifdef MATERIAL_UNIFORMS_USED
layout(std140) uniform MaterialUniforms{ //ubo:4

#MATERIAL_UNIFORMS

};
#endif

uniform mediump uint batch_flags;

/* clang-format on */
#include "canvas_uniforms_inc.glsl"

out vec2 uv_interp;
out vec4 color_interp;
out vec2 vertex_interp;

#ifdef USE_NINEPATCH

out vec2 pixel_size_interp;

#endif

#GLOBALS

void main() {
	varying_A = vec4(read_draw_data_world_x, read_draw_data_world_y);
	varying_B = read_draw_data_color_texture_pixel_size;
#ifndef USE_PRIMITIVE
	varying_C = read_draw_data_ninepatch_margins;

#ifndef USE_ATTRIBUTES
#ifdef USE_NINEPATCH
	varying_D = vec2(read_draw_data_dst_rect.z, read_draw_data_dst_rect.w);
#endif // USE_NINEPATCH
	varying_E = read_draw_data_src_rect;
#endif // !USE_ATTRIBUTES
#endif // USE_PRIMITIVE

	varying_F = uvec2(read_draw_data_flags, read_draw_data_specular_shininess);
	varying_G = read_draw_data_lights;

	vec4 instance_custom = vec4(0.0);

#if defined(CUSTOM0_USED)
	vec4 custom0 = vec4(0.0);
#endif
#if defined(CUSTOM1_USED)
	vec4 custom1 = vec4(0.0);
#endif

#ifdef USE_PRIMITIVE
	vec2 vertex;
	vec2 uv;
	vec4 color;

	if (gl_VertexID % 3 == 0) {
		vertex = read_draw_data_point_a;
		uv = read_draw_data_uv_a;
		color.xy = unpackHalf2x16(read_draw_data_color_a_rg);
		color.zw = unpackHalf2x16(read_draw_data_color_a_ba);
	} else if (gl_VertexID % 3 == 1) {
		vertex = read_draw_data_point_b;
		uv = read_draw_data_uv_b;
		color.xy = unpackHalf2x16(read_draw_data_color_b_rg);
		color.zw = unpackHalf2x16(read_draw_data_color_b_ba);
	} else {
		vertex = read_draw_data_point_c;
		uv = read_draw_data_uv_c;
		color.xy = unpackHalf2x16(read_draw_data_color_c_rg);
		color.zw = unpackHalf2x16(read_draw_data_color_c_ba);
	}

#elif defined(USE_ATTRIBUTES)
	vec2 vertex = vertex_attrib;
	vec4 color = color_attrib * read_draw_data_modulation;
	vec2 uv = uv_attrib;

#ifdef USE_INSTANCING
	if (bool(batch_flags & BATCH_FLAGS_INSTANCING_HAS_COLORS)) {
		vec4 instance_color;
		instance_color.xy = unpackHalf2x16(uint(instance_color_custom_data.x));
		instance_color.zw = unpackHalf2x16(uint(instance_color_custom_data.y));
		color *= instance_color;
	}
	if (bool(batch_flags & BATCH_FLAGS_INSTANCING_HAS_CUSTOM_DATA)) {
		instance_custom.xy = unpackHalf2x16(instance_color_custom_data.z);
		instance_custom.zw = unpackHalf2x16(instance_color_custom_data.w);
	}
#endif // !USE_INSTANCING

#else // !USE_ATTRIBUTES

	// crash on Adreno 320/330
	//vec2 vertex_base_arr[6] = vec2[](vec2(0.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 1.0), vec2(1.0, 0.0), vec2(0.0, 0.0), vec2(1.0, 1.0));
	//vec2 vertex_base = vertex_base_arr[gl_VertexID % 6];
	//-----------------------------------------
	// ID |  0  |  1  |  2  |  3  |  4  |  5  |
	//-----------------------------------------
	// X  | 0.0 | 0.0 | 1.0 | 1.0 | 0.0 | 1.0 |
	// Y  | 0.0 | 1.0 | 1.0 | 0.0 | 0.0 | 1.0 |
	//-----------------------------------------
	// no crash or freeze on all Adreno 3xx	with 'if / else if' and slightly faster!
	int vertex_id = gl_VertexID % 6;
	vec2 vertex_base;
	if (vertex_id == 0)
		vertex_base = vec2(0.0, 0.0);
	else if (vertex_id == 1)
		vertex_base = vec2(0.0, 1.0);
	else if (vertex_id == 2)
		vertex_base = vec2(1.0, 1.0);
	else if (vertex_id == 3)
		vertex_base = vec2(1.0, 0.0);
	else if (vertex_id == 4)
		vertex_base = vec2(0.0, 0.0);
	else if (vertex_id == 5)
		vertex_base = vec2(1.0, 1.0);

	vec2 uv = read_draw_data_src_rect.xy + abs(read_draw_data_src_rect.zw) * ((read_draw_data_flags & INSTANCE_FLAGS_TRANSPOSE_RECT) != uint(0) ? vertex_base.yx : vertex_base.xy);
	vec4 color = read_draw_data_modulation;
	vec2 vertex = read_draw_data_dst_rect.xy + abs(read_draw_data_dst_rect.zw) * mix(vertex_base, vec2(1.0, 1.0) - vertex_base, lessThan(read_draw_data_src_rect.zw, vec2(0.0, 0.0)));

#endif // USE_ATTRIBUTES

#if defined(CUSTOM0_USED)
	custom0 = custom0_attrib;
#endif

#if defined(CUSTOM1_USED)
	custom1 = custom1_attrib;
#endif

	mat4 model_matrix = mat4(vec4(read_draw_data_world_x, 0.0, 0.0), vec4(read_draw_data_world_y, 0.0, 0.0), vec4(0.0, 0.0, 1.0, 0.0), vec4(read_draw_data_world_ofs, 0.0, 1.0));

#ifdef USE_INSTANCING
	model_matrix = model_matrix * transpose(mat4(instance_xform0, instance_xform1, vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0)));
#endif // USE_INSTANCING

	vec2 color_texture_pixel_size = read_draw_data_color_texture_pixel_size;

#ifdef USE_POINT_SIZE
	float point_size = 1.0;
#endif

#ifdef USE_WORLD_VERTEX_COORDS
	vertex = (model_matrix * vec4(vertex, 0.0, 1.0)).xy;
#endif
	{
#CODE : VERTEX
	}

#ifdef USE_NINEPATCH
	pixel_size_interp = abs(read_draw_data_dst_rect.zw) * vertex_base;
#endif

#if !defined(SKIP_TRANSFORM_USED) && !defined(USE_WORLD_VERTEX_COORDS)
	vertex = (model_matrix * vec4(vertex, 0.0, 1.0)).xy;
#endif

	color_interp = color;

	vertex = (canvas_transform * vec4(vertex, 0.0, 1.0)).xy;

	if (use_pixel_snap) {
		vertex = floor(vertex + 0.5);
		// precision issue on some hardware creates artifacts within texture
		// offset uv by a small amount to avoid
		uv += 1e-5;
	}

	vertex_interp = vertex;
	uv_interp = uv;

	gl_Position = screen_transform * vec4(vertex, 0.0, 1.0);

#ifdef USE_POINT_SIZE
	gl_PointSize = point_size;
#endif
}

#[fragment]

#include "canvas_uniforms_inc.glsl"
#include "stdlib_inc.glsl"

in vec2 uv_interp;
in vec2 vertex_interp;
in vec4 color_interp;

#ifdef USE_NINEPATCH

in vec2 pixel_size_interp;

#endif

// Can all be flat as they are the same for the whole batched instance
flat in vec4 varying_A;
flat in vec2 varying_B;
#define read_draw_data_world_x varying_A.xy
#define read_draw_data_world_y varying_A.zw
#define read_draw_data_color_texture_pixel_size varying_B

#ifndef USE_PRIMITIVE
flat in vec4 varying_C;
#define read_draw_data_ninepatch_margins varying_C

#ifndef USE_ATTRIBUTES
#ifdef USE_NINEPATCH

flat in vec2 varying_D;
#define read_draw_data_dst_rect_z varying_D.x
#define read_draw_data_dst_rect_w varying_D.y
#endif

flat in vec4 varying_E;
#define read_draw_data_src_rect varying_E
#endif // USE_ATTRIBUTES
#endif // USE_PRIMITIVE

flat in uvec2 varying_F;
flat in uvec4 varying_G;
#define read_draw_data_flags varying_F.x
#define read_draw_data_specular_shininess varying_F.y
#define read_draw_data_lights varying_G

#ifndef DISABLE_LIGHTING
uniform sampler2D atlas_texture; //texunit:-2
uniform sampler2D shadow_atlas_texture; //texunit:-3
#endif // DISABLE_LIGHTING
uniform sampler2D color_buffer; //texunit:-4
uniform sampler2D sdf_texture; //texunit:-5
uniform sampler2D normal_texture; //texunit:-6
uniform sampler2D specular_texture; //texunit:-7

uniform sampler2D color_texture; //texunit:0

uniform mediump uint batch_flags;

layout(location = 0) out vec4 frag_color;

/* clang-format off */
// This needs to be outside clang-format so the ubo comment is in the right place
#ifdef MATERIAL_UNIFORMS_USED
layout(std140) uniform MaterialUniforms{ //ubo:4

#MATERIAL_UNIFORMS

};
#endif
/* clang-format on */

#GLOBALS

float vec4_to_float(vec4 p_vec) {
	return dot(p_vec, vec4(1.0 / (255.0 * 255.0 * 255.0), 1.0 / (255.0 * 255.0), 1.0 / 255.0, 1.0)) * 2.0 - 1.0;
}

vec2 screen_uv_to_sdf(vec2 p_uv) {
	return screen_to_sdf * p_uv;
}

float texture_sdf(vec2 p_sdf) {
	vec2 uv = p_sdf * sdf_to_tex.xy + sdf_to_tex.zw;
	float d = vec4_to_float(texture(sdf_texture, uv));
	d *= SDF_MAX_LENGTH;
	return d * tex_to_sdf;
}

vec2 texture_sdf_normal(vec2 p_sdf) {
	vec2 uv = p_sdf * sdf_to_tex.xy + sdf_to_tex.zw;

	const float EPSILON = 0.001;
	return normalize(vec2(
			vec4_to_float(texture(sdf_texture, uv + vec2(EPSILON, 0.0))) - vec4_to_float(texture(sdf_texture, uv - vec2(EPSILON, 0.0))),
			vec4_to_float(texture(sdf_texture, uv + vec2(0.0, EPSILON))) - vec4_to_float(texture(sdf_texture, uv - vec2(0.0, EPSILON)))));
}

vec2 sdf_to_screen_uv(vec2 p_sdf) {
	return p_sdf * sdf_to_screen;
}

#ifndef DISABLE_LIGHTING
#ifdef LIGHT_CODE_USED

vec4 light_compute(
		vec3 light_vertex,
		vec3 light_position,
		vec3 normal,
		vec4 light_color,
		float light_energy,
		vec4 specular_shininess,
		inout vec4 shadow_modulate,
		vec2 screen_uv,
		vec2 uv,
		vec4 color, bool is_directional) {
	vec4 light = vec4(0.0);
	vec3 light_direction = vec3(0.0);

	if (is_directional) {
		light_direction = normalize(mix(vec3(light_position.xy, 0.0), vec3(0, 0, 1), light_position.z));
		light_position = vec3(0.0);
	} else {
		light_direction = normalize(light_position - light_vertex);
	}

#CODE : LIGHT

	return light;
}

#endif

vec3 light_normal_compute(vec3 light_vec, vec3 normal, vec3 base_color, vec3 light_color, vec4 specular_shininess, bool specular_shininess_used) {
	float cNdotL = max(0.0, dot(normal, light_vec));

	if (specular_shininess_used) {
		//blinn
		vec3 view = vec3(0.0, 0.0, 1.0); // not great but good enough
		vec3 half_vec = normalize(view + light_vec);

		float cNdotV = max(dot(normal, view), 0.0);
		float cNdotH = max(dot(normal, half_vec), 0.0);
		float cVdotH = max(dot(view, half_vec), 0.0);
		float cLdotH = max(dot(light_vec, half_vec), 0.0);
		float shininess = exp2(15.0 * specular_shininess.a + 1.0) * 0.25;
		float blinn = pow(cNdotH, shininess);
		blinn *= (shininess + 8.0) * (1.0 / (8.0 * M_PI));
		float s = (blinn) / max(4.0 * cNdotV * cNdotL, 0.75);

		return specular_shininess.rgb * light_color * s + light_color * base_color * cNdotL;
	} else {
		return light_color * base_color * cNdotL;
	}
}

#ifdef USE_RGBA_SHADOWS

#define SHADOW_DEPTH(m_uv) (dot(textureLod(shadow_atlas_texture, (m_uv), 0.0), vec4(1.0 / (255.0 * 255.0 * 255.0), 1.0 / (255.0 * 255.0), 1.0 / 255.0, 1.0)) * 2.0 - 1.0)

#else

#define SHADOW_DEPTH(m_uv) (textureLod(shadow_atlas_texture, (m_uv), 0.0).r)

#endif

/* clang-format off */
#define SHADOW_TEST(m_uv) { highp float sd = SHADOW_DEPTH(m_uv); shadow += step(sd, shadow_uv.z / shadow_uv.w); }
/* clang-format on */

//float distance = length(shadow_pos);
vec4 light_shadow_compute(uint light_base, vec4 light_color, vec4 shadow_uv
#ifdef LIGHT_CODE_USED
		,
		vec3 shadow_modulate
#endif
) {
	float shadow = 0.0;
	uint shadow_mode = light_array[light_base].flags & LIGHT_FLAGS_FILTER_MASK;

	if (shadow_mode == LIGHT_FLAGS_SHADOW_NEAREST) {
		SHADOW_TEST(shadow_uv.xy);
	} else if (shadow_mode == LIGHT_FLAGS_SHADOW_PCF5) {
		vec2 shadow_pixel_size = vec2(light_array[light_base].shadow_pixel_size, 0.0);
		SHADOW_TEST(shadow_uv.xy - shadow_pixel_size * 2.0);
		SHADOW_TEST(shadow_uv.xy - shadow_pixel_size);
		SHADOW_TEST(shadow_uv.xy);
		SHADOW_TEST(shadow_uv.xy + shadow_pixel_size);
		SHADOW_TEST(shadow_uv.xy + shadow_pixel_size * 2.0);
		shadow /= 5.0;
	} else { //PCF13
		vec2 shadow_pixel_size = vec2(light_array[light_base].shadow_pixel_size, 0.0);
		SHADOW_TEST(shadow_uv.xy - shadow_pixel_size * 6.0);
		SHADOW_TEST(shadow_uv.xy - shadow_pixel_size * 5.0);
		SHADOW_TEST(shadow_uv.xy - shadow_pixel_size * 4.0);
		SHADOW_TEST(shadow_uv.xy - shadow_pixel_size * 3.0);
		SHADOW_TEST(shadow_uv.xy - shadow_pixel_size * 2.0);
		SHADOW_TEST(shadow_uv.xy - shadow_pixel_size);
		SHADOW_TEST(shadow_uv.xy);
		SHADOW_TEST(shadow_uv.xy + shadow_pixel_size);
		SHADOW_TEST(shadow_uv.xy + shadow_pixel_size * 2.0);
		SHADOW_TEST(shadow_uv.xy + shadow_pixel_size * 3.0);
		SHADOW_TEST(shadow_uv.xy + shadow_pixel_size * 4.0);
		SHADOW_TEST(shadow_uv.xy + shadow_pixel_size * 5.0);
		SHADOW_TEST(shadow_uv.xy + shadow_pixel_size * 6.0);
		shadow /= 13.0;
	}

	vec4 shadow_color = godot_unpackUnorm4x8(light_array[light_base].shadow_color);
#ifdef LIGHT_CODE_USED
	shadow_color.rgb *= shadow_modulate;
#endif

	shadow_color.a *= light_color.a; //respect light alpha

	return mix(light_color, shadow_color, shadow);
}

void light_blend_compute(uint light_base, vec4 light_color, inout vec3 color) {
	uint blend_mode = light_array[light_base].flags & LIGHT_FLAGS_BLEND_MASK;

	if (blend_mode == LIGHT_FLAGS_BLEND_MODE_ADD) {
		color.rgb += light_color.rgb * light_color.a;
	} else if (blend_mode == LIGHT_FLAGS_BLEND_MODE_SUB) {
		color.rgb -= light_color.rgb * light_color.a;
	} else if (blend_mode == LIGHT_FLAGS_BLEND_MODE_MIX) {
		color.rgb = mix(color.rgb, light_color.rgb, light_color.a);
	}
}

#endif

#ifdef USE_NINEPATCH

float map_ninepatch_axis(float pixel, float draw_size, float tex_pixel_size, float margin_begin, float margin_end, int np_repeat, inout int draw_center) {
	float tex_size = 1.0 / tex_pixel_size;

	if (pixel < margin_begin) {
		return pixel * tex_pixel_size;
	} else if (pixel >= draw_size - margin_end) {
		return (tex_size - (draw_size - pixel)) * tex_pixel_size;
	} else {
		if (!bool(read_draw_data_flags & INSTANCE_FLAGS_NINEPATCH_DRAW_CENTER)) {
			draw_center--;
		}

		// np_repeat is passed as uniform using NinePatchRect::AxisStretchMode enum.
		if (np_repeat == 0) { // Stretch.
			// Convert to ratio.
			float ratio = (pixel - margin_begin) / (draw_size - margin_begin - margin_end);
			// Scale to source texture.
			return (margin_begin + ratio * (tex_size - margin_begin - margin_end)) * tex_pixel_size;
		} else if (np_repeat == 1) { // Tile.
			// Convert to offset.
			float ofs = mod((pixel - margin_begin), tex_size - margin_begin - margin_end);
			// Scale to source texture.
			return (margin_begin + ofs) * tex_pixel_size;
		} else if (np_repeat == 2) { // Tile Fit.
			// Calculate scale.
			float src_area = draw_size - margin_begin - margin_end;
			float dst_area = tex_size - margin_begin - margin_end;
			float scale = max(1.0, floor(src_area / max(dst_area, 0.0000001) + 0.5));
			// Convert to ratio.
			float ratio = (pixel - margin_begin) / src_area;
			ratio = mod(ratio * scale, 1.0);
			// Scale to source texture.
			return (margin_begin + ratio * dst_area) * tex_pixel_size;
		} else { // Shouldn't happen, but silences compiler warning.
			return 0.0;
		}
	}
}

#endif

float msdf_median(float r, float g, float b, float a) {
	return min(max(min(r, g), min(max(r, g), b)), a);
}

void main() {
	vec4 color = color_interp;
	vec2 uv = uv_interp;
	vec2 vertex = vertex_interp;

#if !defined(USE_ATTRIBUTES) && !defined(USE_PRIMITIVE)

#ifdef USE_NINEPATCH

	int draw_center = 2;
	uv = vec2(
			map_ninepatch_axis(pixel_size_interp.x, abs(read_draw_data_dst_rect_z), read_draw_data_color_texture_pixel_size.x, read_draw_data_ninepatch_margins.x, read_draw_data_ninepatch_margins.z, int(read_draw_data_flags >> INSTANCE_FLAGS_NINEPATCH_H_MODE_SHIFT) & 0x3, draw_center),
			map_ninepatch_axis(pixel_size_interp.y, abs(read_draw_data_dst_rect_w), read_draw_data_color_texture_pixel_size.y, read_draw_data_ninepatch_margins.y, read_draw_data_ninepatch_margins.w, int(read_draw_data_flags >> INSTANCE_FLAGS_NINEPATCH_V_MODE_SHIFT) & 0x3, draw_center));

	if (draw_center == 0) {
		color.a = 0.0;
	}

	uv = uv * read_draw_data_src_rect.zw + read_draw_data_src_rect.xy; //apply region if needed

#endif
	if (bool(read_draw_data_flags & INSTANCE_FLAGS_CLIP_RECT_UV)) {
		vec2 half_texpixel = read_draw_data_color_texture_pixel_size * 0.5;
		uv = clamp(uv, read_draw_data_src_rect.xy + half_texpixel, read_draw_data_src_rect.xy + abs(read_draw_data_src_rect.zw) - half_texpixel);
	}

#endif

#ifndef USE_PRIMITIVE
	if (bool(read_draw_data_flags & INSTANCE_FLAGS_USE_MSDF)) {
		float px_range = read_draw_data_ninepatch_margins.x;
		float outline_thickness = read_draw_data_ninepatch_margins.y;

		vec4 msdf_sample = texture(color_texture, uv);
		vec2 msdf_size = vec2(textureSize(color_texture, 0));
		vec2 dest_size = vec2(1.0) / fwidth(uv);
		float px_size = max(0.5 * dot((vec2(px_range) / msdf_size), dest_size), 1.0);
		float d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b, msdf_sample.a) - 0.5;

		if (outline_thickness > 0.0) {
			float cr = clamp(outline_thickness, 0.0, px_range / 2.0) / px_range;
			float a = clamp((d + cr) * px_size, 0.0, 1.0);
			color.a = a * color.a;
		} else {
			float a = clamp(d * px_size + 0.5, 0.0, 1.0);
			color.a = a * color.a;
		}
	} else if (bool(read_draw_data_flags & INSTANCE_FLAGS_USE_LCD)) {
		vec4 lcd_sample = texture(color_texture, uv);
		if (lcd_sample.a == 1.0) {
			color.rgb = lcd_sample.rgb * color.a;
		} else {
			color = vec4(0.0, 0.0, 0.0, 0.0);
		}
	} else {
#else
	{
#endif
		color *= texture(color_texture, uv);
	}

	uint light_count = read_draw_data_flags & uint(0xF); // Max 16 lights.
	bool using_light = light_count > 0u || directional_light_count > 0u;

	vec3 normal;

#if defined(NORMAL_USED)
	bool normal_used = true;
#else
	bool normal_used = false;
#endif

	if (normal_used || (using_light && bool(batch_flags & BATCH_FLAGS_DEFAULT_NORMAL_MAP_USED))) {
		normal.xy = texture(normal_texture, uv).xy * vec2(2.0, -2.0) - vec2(1.0, -1.0);

#if !defined(USE_ATTRIBUTES) && !defined(USE_PRIMITIVE)
		if (bool(read_draw_data_flags & INSTANCE_FLAGS_TRANSPOSE_RECT)) {
			normal.xy = normal.yx;
		}
		normal.xy *= sign(read_draw_data_src_rect.zw);
#endif

		normal.z = sqrt(max(0.0, 1.0 - dot(normal.xy, normal.xy)));
		normal_used = true;
	} else {
		normal = vec3(0.0, 0.0, 1.0);
	}

	vec4 specular_shininess;

#if defined(SPECULAR_SHININESS_USED)

	bool specular_shininess_used = true;
#else
	bool specular_shininess_used = false;
#endif

	if (specular_shininess_used || (using_light && normal_used && bool(batch_flags & BATCH_FLAGS_DEFAULT_SPECULAR_MAP_USED))) {
		specular_shininess = texture(specular_texture, uv);
		specular_shininess *= godot_unpackUnorm4x8(read_draw_data_specular_shininess);
		specular_shininess_used = true;
	} else {
		specular_shininess = vec4(1.0);
	}

#if defined(SCREEN_UV_USED)
	vec2 screen_uv = gl_FragCoord.xy * screen_pixel_size;
#else
	vec2 screen_uv = vec2(0.0);
#endif

	vec2 color_texture_pixel_size = read_draw_data_color_texture_pixel_size.xy;

	vec3 light_vertex = vec3(vertex, 0.0);
	vec2 shadow_vertex = vertex;

	{
		float normal_map_depth = 1.0;

#if defined(NORMAL_MAP_USED)
		vec3 normal_map = vec3(0.0, 0.0, 1.0);
		normal_used = true;
#endif

#CODE : FRAGMENT

#if defined(NORMAL_MAP_USED)
		normal = mix(vec3(0.0, 0.0, 1.0), normal_map * vec3(2.0, -2.0, 1.0) - vec3(1.0, -1.0, 0.0), normal_map_depth);
#endif
	}

	if (normal_used) {
		//convert by item transform
		normal.xy = mat2(normalize(read_draw_data_world_x), normalize(read_draw_data_world_y)) * normal.xy;
		//convert by canvas transform
		normal = normalize((canvas_normal_transform * vec4(normal, 0.0)).xyz);
	}

	vec4 base_color = color;

#ifdef MODE_LIGHT_ONLY
	float light_only_alpha = 0.0;
#elif !defined(MODE_UNSHADED)
	color *= canvas_modulation;
#endif

#if !defined(DISABLE_LIGHTING) && !defined(MODE_UNSHADED)

	// Directional Lights

	for (uint i = 0u; i < directional_light_count; i++) {
		uint light_base = i;

		vec2 direction = light_array[light_base].position;
		vec4 light_color = light_array[light_base].color;

#ifdef LIGHT_CODE_USED

		vec4 shadow_modulate = vec4(1.0);
		light_color = light_compute(light_vertex, vec3(direction, light_array[light_base].height), normal, light_color, light_color.a, specular_shininess, shadow_modulate, screen_uv, uv, base_color, true);
#else

		if (normal_used) {
			vec3 light_vec = normalize(mix(vec3(direction, 0.0), vec3(0, 0, 1), light_array[light_base].height));
			light_color.rgb = light_normal_compute(light_vec, normal, base_color.rgb, light_color.rgb, specular_shininess, specular_shininess_used);
		} else {
			light_color.rgb *= base_color.rgb;
		}
#endif

		if (bool(light_array[light_base].flags & LIGHT_FLAGS_HAS_SHADOW)) {
			vec2 shadow_pos = (vec4(shadow_vertex, 0.0, 1.0) * mat4(light_array[light_base].shadow_matrix[0], light_array[light_base].shadow_matrix[1], vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0))).xy; //multiply inverse given its transposed. Optimizer removes useless operations.

			vec4 shadow_uv = vec4(shadow_pos.x, light_array[light_base].shadow_y_ofs, shadow_pos.y * light_array[light_base].shadow_zfar_inv, 1.0);

			light_color = light_shadow_compute(light_base, light_color, shadow_uv
#ifdef LIGHT_CODE_USED
					,
					shadow_modulate.rgb
#endif
			);
		}

		light_blend_compute(light_base, light_color, color.rgb);
#ifdef MODE_LIGHT_ONLY
		light_only_alpha += light_color.a;
#endif
	}

	// Positional Lights

	for (uint i = 0u; i < MAX_LIGHTS_PER_ITEM; i++) {
		if (i >= light_count) {
			break;
		}
		uint light_base;
		if (i < 8u) {
			if (i < 4u) {
				light_base = read_draw_data_lights[0];
			} else {
				light_base = read_draw_data_lights[1];
			}
		} else {
			if (i < 12u) {
				light_base = read_draw_data_lights[2];
			} else {
				light_base = read_draw_data_lights[3];
			}
		}
		light_base >>= (i & 3u) * 8u;
		light_base &= uint(0xFF);

		vec2 tex_uv = (vec4(vertex, 0.0, 1.0) * mat4(light_array[light_base].texture_matrix[0], light_array[light_base].texture_matrix[1], vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0))).xy; //multiply inverse given its transposed. Optimizer removes useless operations.
		vec2 tex_uv_atlas = tex_uv * light_array[light_base].atlas_rect.zw + light_array[light_base].atlas_rect.xy;

		if (any(lessThan(tex_uv, vec2(0.0, 0.0))) || any(greaterThanEqual(tex_uv, vec2(1.0, 1.0)))) {
			//if outside the light texture, light color is zero
			continue;
		}

		vec4 light_color = textureLod(atlas_texture, tex_uv_atlas, 0.0);
		vec4 light_base_color = light_array[light_base].color;

#ifdef LIGHT_CODE_USED

		vec4 shadow_modulate = vec4(1.0);
		vec3 light_position = vec3(light_array[light_base].position, light_array[light_base].height);

		light_color.rgb *= light_base_color.rgb;
		light_color = light_compute(light_vertex, light_position, normal, light_color, light_base_color.a, specular_shininess, shadow_modulate, screen_uv, uv, base_color, false);
#else

		light_color.rgb *= light_base_color.rgb * light_base_color.a;

		if (normal_used) {
			vec3 light_pos = vec3(light_array[light_base].position, light_array[light_base].height);
			vec3 pos = light_vertex;
			vec3 light_vec = normalize(light_pos - pos);

			light_color.rgb = light_normal_compute(light_vec, normal, base_color.rgb, light_color.rgb, specular_shininess, specular_shininess_used);
		} else {
			light_color.rgb *= base_color.rgb;
		}
#endif

		if (bool(light_array[light_base].flags & LIGHT_FLAGS_HAS_SHADOW) && bool(read_draw_data_flags & uint(INSTANCE_FLAGS_SHADOW_MASKED << i))) {
			vec2 shadow_pos = (vec4(shadow_vertex, 0.0, 1.0) * mat4(light_array[light_base].shadow_matrix[0], light_array[light_base].shadow_matrix[1], vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0))).xy; //multiply inverse given its transposed. Optimizer removes useless operations.

			vec2 pos_norm = normalize(shadow_pos);
			vec2 pos_abs = abs(pos_norm);
			vec2 pos_box = pos_norm / max(pos_abs.x, pos_abs.y);
			vec2 pos_rot = pos_norm * mat2(vec2(0.7071067811865476, -0.7071067811865476), vec2(0.7071067811865476, 0.7071067811865476)); //is there a faster way to 45 degrees rot?
			float tex_ofs;
			float dist;
			if (pos_rot.y > 0.0) {
				if (pos_rot.x > 0.0) {
					tex_ofs = pos_box.y * 0.125 + 0.125;
					dist = shadow_pos.x;
				} else {
					tex_ofs = pos_box.x * -0.125 + (0.25 + 0.125);
					dist = shadow_pos.y;
				}
			} else {
				if (pos_rot.x < 0.0) {
					tex_ofs = pos_box.y * -0.125 + (0.5 + 0.125);
					dist = -shadow_pos.x;
				} else {
					tex_ofs = pos_box.x * 0.125 + (0.75 + 0.125);
					dist = -shadow_pos.y;
				}
			}

			dist *= light_array[light_base].shadow_zfar_inv;

			//float distance = length(shadow_pos);
			vec4 shadow_uv = vec4(tex_ofs, light_array[light_base].shadow_y_ofs, dist, 1.0);

			light_color = light_shadow_compute(light_base, light_color, shadow_uv
#ifdef LIGHT_CODE_USED
					,
					shadow_modulate.rgb
#endif
			);
		}

		light_blend_compute(light_base, light_color, color.rgb);
#ifdef MODE_LIGHT_ONLY
		light_only_alpha += light_color.a;
#endif
	}
#endif

#ifdef MODE_LIGHT_ONLY
	color.a *= light_only_alpha;
#endif

	frag_color = color;
}

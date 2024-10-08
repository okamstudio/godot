#[vertex]

#version 450

#VERSION_DEFINES

#ifdef USE_ATTRIBUTES
layout(location = 0) in vec2 vertex_attrib;
layout(location = 3) in vec4 color_attrib;
layout(location = 4) in vec2 uv_attrib;

#if defined(CUSTOM0_USED)
layout(location = 6) in vec4 custom0_attrib;
#endif

#if defined(CUSTOM1_USED)
layout(location = 7) in vec4 custom1_attrib;
#endif

layout(location = 10) in uvec4 bone_attrib;
layout(location = 11) in vec4 weight_attrib;

#endif

#include "canvas_uniforms_inc.glsl"

#ifndef USE_ATTRIBUTES

layout(location = 4) out flat uint instance_index_interp;

#endif // USE_ATTRIBUTES

layout(location = 0) out vec2 uv_interp;
layout(location = 1) out vec4 color_interp;
layout(location = 2) out vec2 vertex_interp;

#ifdef USE_NINEPATCH

layout(location = 3) out vec2 pixel_size_interp;

#endif

#ifdef MATERIAL_UNIFORMS_USED
/* clang-format off */
layout(set = 1, binding = 0, std140) uniform MaterialUniforms {
#MATERIAL_UNIFORMS
} material;
/* clang-format on */
#endif

#GLOBALS

#ifdef USE_ATTRIBUTES
vec3 srgb_to_linear(vec3 color) {
	return mix(pow((color.rgb + vec3(0.055)) * (1.0 / (1.0 + 0.055)), vec3(2.4)), color.rgb * (1.0 / 12.92), lessThan(color.rgb, vec3(0.04045)));
}
#endif

void main() {
	vec4 instance_custom = vec4(0.0);
#if defined(CUSTOM0_USED)
	vec4 custom0 = vec4(0.0);
#endif
#if defined(CUSTOM1_USED)
	vec4 custom1 = vec4(0.0);
#endif

#ifdef USE_ATTRIBUTES
	uint instance_index = params.base_instance_index;
#else
	uint instance_index = gl_InstanceIndex + params.base_instance_index;
	instance_index_interp = instance_index;
#endif // USE_ATTRIBUTES
	const InstanceData draw_data = instances.data[instance_index];

#ifdef USE_PRIMITIVE

	//weird bug,
	//this works
	vec2 vertex;
	vec2 uv;
	vec4 color;

	if (gl_VertexIndex == 0) {
		vertex = draw_data.points[0];
		uv = draw_data.uvs[0];
		color = vec4(unpackHalf2x16(draw_data.colors[0]), unpackHalf2x16(draw_data.colors[1]));
	} else if (gl_VertexIndex == 1) {
		vertex = draw_data.points[1];
		uv = draw_data.uvs[1];
		color = vec4(unpackHalf2x16(draw_data.colors[2]), unpackHalf2x16(draw_data.colors[3]));
	} else {
		vertex = draw_data.points[2];
		uv = draw_data.uvs[2];
		color = vec4(unpackHalf2x16(draw_data.colors[4]), unpackHalf2x16(draw_data.colors[5]));
	}
	uvec4 bones = uvec4(0, 0, 0, 0);
	vec4 bone_weights = vec4(0.0);

#elif defined(USE_ATTRIBUTES)

	vec2 vertex = vertex_attrib;
	vec4 color = color_attrib;
	if (bool(draw_data.flags & FLAGS_CONVERT_ATTRIBUTES_TO_LINEAR)) {
		color.rgb = srgb_to_linear(color.rgb);
	}
	color *= draw_data.modulation;
	vec2 uv = uv_attrib;

#if defined(CUSTOM0_USED)
	custom0 = custom0_attrib;
#endif

#if defined(CUSTOM1_USED)
	custom1 = custom1_attrib;
#endif

	uvec4 bones = bone_attrib;
	vec4 bone_weights = weight_attrib;
#else // !USE_ATTRIBUTES

	vec2 vertex_base_arr[4] = vec2[](vec2(0.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 1.0), vec2(1.0, 0.0));
	vec2 vertex_base = vertex_base_arr[gl_VertexIndex];

	vec2 uv = draw_data.src_rect.xy + abs(draw_data.src_rect.zw) * ((draw_data.flags & FLAGS_TRANSPOSE_RECT) != 0 ? vertex_base.yx : vertex_base.xy);
	vec4 color = draw_data.modulation;
	vec2 vertex = draw_data.dst_rect.xy + abs(draw_data.dst_rect.zw) * mix(vertex_base, vec2(1.0, 1.0) - vertex_base, lessThan(draw_data.src_rect.zw, vec2(0.0, 0.0)));
	uvec4 bones = uvec4(0, 0, 0, 0);

#endif // USE_ATTRIBUTES

	mat4 model_matrix = mat4(vec4(draw_data.world_x, 0.0, 0.0), vec4(draw_data.world_y, 0.0, 0.0), vec4(0.0, 0.0, 1.0, 0.0), vec4(draw_data.world_ofs, 0.0, 1.0));

#ifdef USE_ATTRIBUTES

	uint instancing = draw_data.flags & FLAGS_INSTANCING_MASK;

	if (instancing > 1) {
		// trails

		uint stride = 2 + 1 + 1; //particles always uses this format

		uint trail_size = instancing;

		uint offset = trail_size * stride * gl_InstanceIndex;

		vec4 pcolor;
		vec2 new_vertex;
		{
			uint boffset = offset + bone_attrib.x * stride;
			new_vertex = (vec4(vertex, 0.0, 1.0) * mat4(transforms.data[boffset + 0], transforms.data[boffset + 1], vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0))).xy * weight_attrib.x;
			pcolor = transforms.data[boffset + 2] * weight_attrib.x;
		}
		if (weight_attrib.y > 0.001) {
			uint boffset = offset + bone_attrib.y * stride;
			new_vertex += (vec4(vertex, 0.0, 1.0) * mat4(transforms.data[boffset + 0], transforms.data[boffset + 1], vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0))).xy * weight_attrib.y;
			pcolor += transforms.data[boffset + 2] * weight_attrib.y;
		}
		if (weight_attrib.z > 0.001) {
			uint boffset = offset + bone_attrib.z * stride;
			new_vertex += (vec4(vertex, 0.0, 1.0) * mat4(transforms.data[boffset + 0], transforms.data[boffset + 1], vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0))).xy * weight_attrib.z;
			pcolor += transforms.data[boffset + 2] * weight_attrib.z;
		}
		if (weight_attrib.w > 0.001) {
			uint boffset = offset + bone_attrib.w * stride;
			new_vertex += (vec4(vertex, 0.0, 1.0) * mat4(transforms.data[boffset + 0], transforms.data[boffset + 1], vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0))).xy * weight_attrib.w;
			pcolor += transforms.data[boffset + 2] * weight_attrib.w;
		}

		instance_custom = transforms.data[offset + 3];

		vertex = new_vertex;
		color *= pcolor;
	} else if (instancing == 1) {
		uint stride = 2 + bitfieldExtract(draw_data.flags, FLAGS_INSTANCING_HAS_COLORS_SHIFT, 1) + bitfieldExtract(draw_data.flags, FLAGS_INSTANCING_HAS_CUSTOM_DATA_SHIFT, 1);

		uint offset = stride * gl_InstanceIndex;

		mat4 matrix = mat4(transforms.data[offset + 0], transforms.data[offset + 1], vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0));
		offset += 2;

		if (bool(draw_data.flags & FLAGS_INSTANCING_HAS_COLORS)) {
			color *= transforms.data[offset];
			offset += 1;
		}

		if (bool(draw_data.flags & FLAGS_INSTANCING_HAS_CUSTOM_DATA)) {
			instance_custom = transforms.data[offset];
		}

		matrix = transpose(matrix);
		model_matrix = model_matrix * matrix;
	}
#endif // USE_ATTRIBUTES

	float point_size = 1.0;

#ifdef USE_WORLD_VERTEX_COORDS
	vertex = (model_matrix * vec4(vertex, 0.0, 1.0)).xy;
#endif
	{
#CODE : VERTEX
	}

#ifdef USE_NINEPATCH
	pixel_size_interp = abs(draw_data.dst_rect.zw) * vertex_base;
#endif

#if !defined(SKIP_TRANSFORM_USED) && !defined(USE_WORLD_VERTEX_COORDS)
	vertex = (model_matrix * vec4(vertex, 0.0, 1.0)).xy;
#endif

	color_interp = color;

	vertex = (canvas_data.canvas_transform * vec4(vertex, 0.0, 1.0)).xy;

	if (canvas_data.use_pixel_snap) {
		vertex = floor(vertex + 0.5);
		// precision issue on some hardware creates artifacts within texture
		// offset uv by a small amount to avoid
		uv += 1e-5;
	}

	vertex_interp = vertex;
	uv_interp = uv;

	gl_Position = canvas_data.screen_transform * vec4(vertex, 0.0, 1.0);

#ifdef USE_POINT_SIZE
	gl_PointSize = point_size;
#endif
}

#[fragment]

#version 450

#VERSION_DEFINES

#include "canvas_uniforms_inc.glsl"

#ifndef USE_ATTRIBUTES
layout(location = 4) in flat uint instance_index;
#endif // USE_ATTRIBUTES

layout(location = 0) in vec2 uv_interp;
layout(location = 1) in vec4 color_interp;
layout(location = 2) in vec2 vertex_interp;

#ifdef USE_NINEPATCH

layout(location = 3) in vec2 pixel_size_interp;

#endif

layout(location = 0) out vec4 frag_color;

#ifdef MATERIAL_UNIFORMS_USED
/* clang-format off */
layout(set = 1, binding = 0, std140) uniform MaterialUniforms {
#MATERIAL_UNIFORMS
} material;
/* clang-format on */
#endif

vec2 screen_uv_to_sdf(vec2 p_uv) {
	return canvas_data.screen_to_sdf * p_uv;
}

float texture_sdf(vec2 p_sdf) {
	vec2 uv = p_sdf * canvas_data.sdf_to_tex.xy + canvas_data.sdf_to_tex.zw;
	float d = texture(sampler2D(sdf_texture, SAMPLER_LINEAR_CLAMP), uv).r;
	d *= SDF_MAX_LENGTH;
	return d * canvas_data.tex_to_sdf;
}

vec2 texture_sdf_normal(vec2 p_sdf) {
	vec2 uv = p_sdf * canvas_data.sdf_to_tex.xy + canvas_data.sdf_to_tex.zw;

	const float EPSILON = 0.001;
	return normalize(vec2(
			texture(sampler2D(sdf_texture, SAMPLER_LINEAR_CLAMP), uv + vec2(EPSILON, 0.0)).r - texture(sampler2D(sdf_texture, SAMPLER_LINEAR_CLAMP), uv - vec2(EPSILON, 0.0)).r,
			texture(sampler2D(sdf_texture, SAMPLER_LINEAR_CLAMP), uv + vec2(0.0, EPSILON)).r - texture(sampler2D(sdf_texture, SAMPLER_LINEAR_CLAMP), uv - vec2(0.0, EPSILON)).r));
}

vec2 sdf_to_screen_uv(vec2 p_sdf) {
	return p_sdf * canvas_data.sdf_to_screen;
}

#GLOBALS

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

#ifdef USE_NINEPATCH

float map_ninepatch_axis(float pixel, float draw_size, float tex_pixel_size, float margin_begin, float margin_end, int np_repeat, inout int draw_center) {
#ifdef USE_ATTRIBUTES
	const InstanceData draw_data = instances.data[params.base_instance_index];
#else
	const InstanceData draw_data = instances.data[instance_index];
#endif // USE_ATTRIBUTES

	float tex_size = 1.0 / tex_pixel_size;

	if (pixel < margin_begin) {
		return pixel * tex_pixel_size;
	} else if (pixel >= draw_size - margin_end) {
		return (tex_size - (draw_size - pixel)) * tex_pixel_size;
	} else {
		draw_center -= 1 - int(bitfieldExtract(draw_data.flags, FLAGS_NINEPACH_DRAW_CENTER_SHIFT, 1));

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

//float distance = length(shadow_pos);
vec4 light_shadow_compute(uint light_base, vec4 light_color, vec4 shadow_uv
#ifdef LIGHT_CODE_USED
		,
		vec3 shadow_modulate
#endif
) {
	float shadow;
	uint shadow_mode = light_array.data[light_base].flags & LIGHT_FLAGS_FILTER_MASK;

	if (shadow_mode == LIGHT_FLAGS_SHADOW_NEAREST) {
		shadow = textureProjLod(sampler2DShadow(shadow_atlas_texture, shadow_sampler), shadow_uv, 0.0).x;
	} else if (shadow_mode == LIGHT_FLAGS_SHADOW_PCF5) {
		vec4 shadow_pixel_size = vec4(light_array.data[light_base].shadow_pixel_size, 0.0, 0.0, 0.0);
		shadow = 0.0;
		shadow += textureProjLod(sampler2DShadow(shadow_atlas_texture, shadow_sampler), shadow_uv - shadow_pixel_size * 2.0, 0.0).x;
		shadow += textureProjLod(sampler2DShadow(shadow_atlas_texture, shadow_sampler), shadow_uv - shadow_pixel_size, 0.0).x;
		shadow += textureProjLod(sampler2DShadow(shadow_atlas_texture, shadow_sampler), shadow_uv, 0.0).x;
		shadow += textureProjLod(sampler2DShadow(shadow_atlas_texture, shadow_sampler), shadow_uv + shadow_pixel_size, 0.0).x;
		shadow += textureProjLod(sampler2DShadow(shadow_atlas_texture, shadow_sampler), shadow_uv + shadow_pixel_size * 2.0, 0.0).x;
		shadow /= 5.0;
	} else { //PCF13
		vec4 shadow_pixel_size = vec4(light_array.data[light_base].shadow_pixel_size, 0.0, 0.0, 0.0);
		shadow = 0.0;
		shadow += textureProjLod(sampler2DShadow(shadow_atlas_texture, shadow_sampler), shadow_uv - shadow_pixel_size * 6.0, 0.0).x;
		shadow += textureProjLod(sampler2DShadow(shadow_atlas_texture, shadow_sampler), shadow_uv - shadow_pixel_size * 5.0, 0.0).x;
		shadow += textureProjLod(sampler2DShadow(shadow_atlas_texture, shadow_sampler), shadow_uv - shadow_pixel_size * 4.0, 0.0).x;
		shadow += textureProjLod(sampler2DShadow(shadow_atlas_texture, shadow_sampler), shadow_uv - shadow_pixel_size * 3.0, 0.0).x;
		shadow += textureProjLod(sampler2DShadow(shadow_atlas_texture, shadow_sampler), shadow_uv - shadow_pixel_size * 2.0, 0.0).x;
		shadow += textureProjLod(sampler2DShadow(shadow_atlas_texture, shadow_sampler), shadow_uv - shadow_pixel_size, 0.0).x;
		shadow += textureProjLod(sampler2DShadow(shadow_atlas_texture, shadow_sampler), shadow_uv, 0.0).x;
		shadow += textureProjLod(sampler2DShadow(shadow_atlas_texture, shadow_sampler), shadow_uv + shadow_pixel_size, 0.0).x;
		shadow += textureProjLod(sampler2DShadow(shadow_atlas_texture, shadow_sampler), shadow_uv + shadow_pixel_size * 2.0, 0.0).x;
		shadow += textureProjLod(sampler2DShadow(shadow_atlas_texture, shadow_sampler), shadow_uv + shadow_pixel_size * 3.0, 0.0).x;
		shadow += textureProjLod(sampler2DShadow(shadow_atlas_texture, shadow_sampler), shadow_uv + shadow_pixel_size * 4.0, 0.0).x;
		shadow += textureProjLod(sampler2DShadow(shadow_atlas_texture, shadow_sampler), shadow_uv + shadow_pixel_size * 5.0, 0.0).x;
		shadow += textureProjLod(sampler2DShadow(shadow_atlas_texture, shadow_sampler), shadow_uv + shadow_pixel_size * 6.0, 0.0).x;
		shadow /= 13.0;
	}

	vec4 shadow_color = unpackUnorm4x8(light_array.data[light_base].shadow_color);
#ifdef LIGHT_CODE_USED
	shadow_color.rgb *= shadow_modulate;
#endif

	shadow_color.a *= light_color.a; //respect light alpha

	return mix(light_color, shadow_color, shadow);
}

void light_blend_compute(uint light_base, vec4 light_color, inout vec3 color) {
	uint blend_mode = light_array.data[light_base].flags & LIGHT_FLAGS_BLEND_MASK;

	switch (blend_mode) {
		case LIGHT_FLAGS_BLEND_MODE_ADD: {
			color.rgb += light_color.rgb * light_color.a;
		} break;
		case LIGHT_FLAGS_BLEND_MODE_SUB: {
			color.rgb -= light_color.rgb * light_color.a;
		} break;
		case LIGHT_FLAGS_BLEND_MODE_MIX: {
			color.rgb = mix(color.rgb, light_color.rgb, light_color.a);
		} break;
	}
}

float msdf_median(float r, float g, float b, float a) {
	return min(max(min(r, g), min(max(r, g), b)), a);
}

void main() {
	vec4 color = color_interp;
	vec2 uv = uv_interp;
	vec2 vertex = vertex_interp;

#ifdef USE_ATTRIBUTES
	const InstanceData draw_data = instances.data[params.base_instance_index];
#else
	const InstanceData draw_data = instances.data[instance_index];
#endif // USE_ATTRIBUTES

#if !defined(USE_ATTRIBUTES) && !defined(USE_PRIMITIVE)

#ifdef USE_NINEPATCH

	int draw_center = 2;
	uv = vec2(
			map_ninepatch_axis(pixel_size_interp.x, abs(draw_data.dst_rect.z), draw_data.color_texture_pixel_size.x, draw_data.ninepatch_margins.x, draw_data.ninepatch_margins.z, int(bitfieldExtract(draw_data.flags, FLAGS_NINEPATCH_H_MODE_SHIFT, 2)), draw_center),
			map_ninepatch_axis(pixel_size_interp.y, abs(draw_data.dst_rect.w), draw_data.color_texture_pixel_size.y, draw_data.ninepatch_margins.y, draw_data.ninepatch_margins.w, int(bitfieldExtract(draw_data.flags, FLAGS_NINEPATCH_V_MODE_SHIFT, 2)), draw_center));

	if (draw_center == 0) {
		color.a = 0.0;
	}

	uv = uv * draw_data.src_rect.zw + draw_data.src_rect.xy; //apply region if needed

#endif
	if (bool(draw_data.flags & FLAGS_CLIP_RECT_UV)) {
		vec2 half_texpixel = draw_data.color_texture_pixel_size * 0.5;
		uv = clamp(uv, draw_data.src_rect.xy + half_texpixel, draw_data.src_rect.xy + abs(draw_data.src_rect.zw) - half_texpixel);
	}

#endif

#ifndef USE_PRIMITIVE
	if (bool(draw_data.flags & FLAGS_USE_MSDF)) {
		float px_range = draw_data.ninepatch_margins.x;
		float outline_thickness = draw_data.ninepatch_margins.y;
		//float reserved1 = draw_data.ninepatch_margins.z;
		//float reserved2 = draw_data.ninepatch_margins.w;

		vec4 msdf_sample = texture(sampler2D(color_texture, texture_sampler), uv);
		vec2 msdf_size = vec2(textureSize(sampler2D(color_texture, texture_sampler), 0));
		vec2 dest_size = vec2(1.0) / fwidth(uv);
		float px_size = max(0.5 * dot((vec2(px_range) / msdf_size), dest_size), 1.0);
		float d = msdf_median(msdf_sample.r, msdf_sample.g, msdf_sample.b, msdf_sample.a) - 0.5;

		if (outline_thickness > 0) {
			float cr = clamp(outline_thickness, 0.0, px_range / 2) / px_range;
			float a = clamp((d + cr) * px_size, 0.0, 1.0);
			color.a = a * color.a;
		} else {
			float a = clamp(d * px_size + 0.5, 0.0, 1.0);
			color.a = a * color.a;
		}
	} else if (bool(draw_data.flags & FLAGS_USE_LCD)) {
		vec4 lcd_sample = texture(sampler2D(color_texture, texture_sampler), uv);
		if (lcd_sample.a == 1.0) {
			color.rgb = lcd_sample.rgb * color.a;
		} else {
			color = vec4(0.0, 0.0, 0.0, 0.0);
		}
	} else {
#else
	{
#endif
		color *= texture(sampler2D(color_texture, texture_sampler), uv);
	}

	uint light_count = bitfieldExtract(draw_data.flags, FLAGS_LIGHT_COUNT_SHIFT, 4); //max 15 lights
	bool using_light = (light_count + canvas_data.directional_light_count) > 0;

	vec3 normal;

#if defined(NORMAL_USED)
	bool normal_used = true;
#else
	bool normal_used = false;
#endif

	if (normal_used || (using_light && bool(draw_data.flags & FLAGS_DEFAULT_NORMAL_MAP_USED))) {
		normal.xy = texture(sampler2D(normal_texture, texture_sampler), uv).xy * vec2(2.0, -2.0) - vec2(1.0, -1.0);
		if (bool(draw_data.flags & FLAGS_TRANSPOSE_RECT)) {
			normal.xy = normal.yx;
		}
		if (bool(draw_data.flags & FLAGS_FLIP_H)) {
			normal.x = -normal.x;
		}
		if (bool(draw_data.flags & FLAGS_FLIP_V)) {
			normal.y = -normal.y;
		}
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

	if (specular_shininess_used || (using_light && normal_used && bool(draw_data.flags & FLAGS_DEFAULT_SPECULAR_MAP_USED))) {
		specular_shininess = texture(sampler2D(specular_texture, texture_sampler), uv);
		specular_shininess *= unpackUnorm4x8(draw_data.specular_shininess);
		specular_shininess_used = true;
	} else {
		specular_shininess = vec4(1.0);
	}

#if defined(SCREEN_UV_USED)
	vec2 screen_uv = gl_FragCoord.xy * canvas_data.screen_pixel_size;
#else
	vec2 screen_uv = vec2(0.0);
#endif

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
		normal.xy = mat2(normalize(draw_data.world_x), normalize(draw_data.world_y)) * normal.xy;
		//convert by canvas transform
		normal = normalize((canvas_data.canvas_normal_transform * vec4(normal, 0.0)).xyz);
	}

	vec4 base_color = color;

#ifdef MODE_LIGHT_ONLY
	float light_only_alpha = 0.0;
#elif !defined(MODE_UNSHADED)
	color *= canvas_data.canvas_modulation;
#endif

#if !defined(MODE_UNSHADED)
	if (sc_use_lighting()) {
		// Directional Lights

		for (uint i = 0; i < canvas_data.directional_light_count; i++) {
			uint light_base = i;

			vec2 direction = light_array.data[light_base].position;
			vec4 light_color = light_array.data[light_base].color;

#ifdef LIGHT_CODE_USED

			vec4 shadow_modulate = vec4(1.0);
			light_color = light_compute(light_vertex, vec3(direction, light_array.data[light_base].height), normal, light_color, light_color.a, specular_shininess, shadow_modulate, screen_uv, uv, base_color, true);
#else

			if (normal_used) {
				vec3 light_vec = normalize(mix(vec3(direction, 0.0), vec3(0, 0, 1), light_array.data[light_base].height));
				light_color.rgb = light_normal_compute(light_vec, normal, base_color.rgb, light_color.rgb, specular_shininess, specular_shininess_used);
			} else {
				light_color.rgb *= base_color.rgb;
			}
#endif

			if (bool(light_array.data[light_base].flags & LIGHT_FLAGS_HAS_SHADOW)) {
				vec2 shadow_pos = (vec4(shadow_vertex, 0.0, 1.0) * mat4(light_array.data[light_base].shadow_matrix[0], light_array.data[light_base].shadow_matrix[1], vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0))).xy; //multiply inverse given its transposed. Optimizer removes useless operations.

				vec4 shadow_uv = vec4(shadow_pos.x, light_array.data[light_base].shadow_y_ofs, shadow_pos.y * light_array.data[light_base].shadow_zfar_inv, 1.0);

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

		for (uint i = 0; i < MAX_LIGHTS_PER_ITEM; i++) {
			if (i >= light_count) {
				break;
			}
			uint light_base = bitfieldExtract(draw_data.lights[i >> 2], (int(i) & 0x3) * 8, 8);

			vec2 tex_uv = (vec4(vertex, 0.0, 1.0) * mat4(light_array.data[light_base].texture_matrix[0], light_array.data[light_base].texture_matrix[1], vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0))).xy; //multiply inverse given its transposed. Optimizer removes useless operations.
			vec2 tex_uv_atlas = tex_uv * light_array.data[light_base].atlas_rect.zw + light_array.data[light_base].atlas_rect.xy;

			if (any(lessThan(tex_uv, vec2(0.0, 0.0))) || any(greaterThanEqual(tex_uv, vec2(1.0, 1.0)))) {
				//if outside the light texture, light color is zero
				continue;
			}

			vec4 light_color = textureLod(sampler2D(atlas_texture, texture_sampler), tex_uv_atlas, 0.0);
			vec4 light_base_color = light_array.data[light_base].color;

#ifdef LIGHT_CODE_USED

			vec4 shadow_modulate = vec4(1.0);
			vec3 light_position = vec3(light_array.data[light_base].position, light_array.data[light_base].height);

			light_color.rgb *= light_base_color.rgb;
			light_color = light_compute(light_vertex, light_position, normal, light_color, light_base_color.a, specular_shininess, shadow_modulate, screen_uv, uv, base_color, false);
#else

			light_color.rgb *= light_base_color.rgb * light_base_color.a;

			if (normal_used) {
				vec3 light_pos = vec3(light_array.data[light_base].position, light_array.data[light_base].height);
				vec3 pos = light_vertex;
				vec3 light_vec = normalize(light_pos - pos);

				light_color.rgb = light_normal_compute(light_vec, normal, base_color.rgb, light_color.rgb, specular_shininess, specular_shininess_used);
			} else {
				light_color.rgb *= base_color.rgb;
			}
#endif

			if (bool(light_array.data[light_base].flags & LIGHT_FLAGS_HAS_SHADOW)) {
				vec2 shadow_pos = (vec4(shadow_vertex, 0.0, 1.0) * mat4(light_array.data[light_base].shadow_matrix[0], light_array.data[light_base].shadow_matrix[1], vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0))).xy; //multiply inverse given its transposed. Optimizer removes useless operations.

				vec2 pos_norm = normalize(shadow_pos);
				vec2 pos_abs = abs(pos_norm);
				vec2 pos_box = pos_norm / max(pos_abs.x, pos_abs.y);
				vec2 pos_rot = pos_norm * mat2(vec2(0.7071067811865476, -0.7071067811865476), vec2(0.7071067811865476, 0.7071067811865476)); //is there a faster way to 45 degrees rot?
				float tex_ofs;
				float distance;
				if (pos_rot.y > 0) {
					if (pos_rot.x > 0) {
						tex_ofs = pos_box.y * 0.125 + 0.125;
						distance = shadow_pos.x;
					} else {
						tex_ofs = pos_box.x * -0.125 + (0.25 + 0.125);
						distance = shadow_pos.y;
					}
				} else {
					if (pos_rot.x < 0) {
						tex_ofs = pos_box.y * -0.125 + (0.5 + 0.125);
						distance = -shadow_pos.x;
					} else {
						tex_ofs = pos_box.x * 0.125 + (0.75 + 0.125);
						distance = -shadow_pos.y;
					}
				}

				distance *= light_array.data[light_base].shadow_zfar_inv;

				//float distance = length(shadow_pos);
				vec4 shadow_uv = vec4(tex_ofs, light_array.data[light_base].shadow_y_ofs, distance, 1.0);

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
	}
#endif

#ifdef MODE_LIGHT_ONLY
	color.a *= light_only_alpha;
#endif

	frag_color = color;
}

#[compute]

#version 450

#VERSION_DEFINES

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

#define FLAG_HORIZONTAL (1 << 0)
#define FLAG_USE_BLUR_SECTION (1 << 1)
#define FLAG_USE_ORTHOGONAL_PROJECTION (1 << 2)
#define FLAG_DOF_NEAR_FIRST_TAP (1 << 3)
#define FLAG_GLOW_FIRST_PASS (1 << 4)
#define FLAG_FLIP_Y (1 << 5)
#define FLAG_FORCE_LUMINANCE (1 << 6)
#define FLAG_COPY_ALL_SOURCE (1 << 7)
#define FLAG_ALPHA_TO_ONE (1 << 8)

layout(push_constant, std430) uniform Params {
	ivec4 section;
	ivec2 target;
	uint flags;
	uint pad;
	// Glow.
	float glow_strength;
	float glow_bloom;
	float glow_hdr_threshold;
	float glow_hdr_scale;

	float glow_exposure;
	float glow_white;
	float glow_luminance_cap;
	float glow_auto_exposure_scale;
	// DOF.
	float camera_z_far;
	float camera_z_near;
	uint pad2[2];

	vec4 set_color;
}
params;

#ifdef MODE_CUBEMAP_ARRAY_TO_PANORAMA
layout(set = 0, binding = 0) uniform samplerCubeArray source_color;
#elif defined(MODE_CUBEMAP_TO_PANORAMA)
layout(set = 0, binding = 0) uniform samplerCube source_color;
#elif !defined(MODE_SET_COLOR)
layout(set = 0, binding = 0) uniform sampler2D source_color;
#endif

#ifdef GLOW_USE_AUTO_EXPOSURE
layout(set = 1, binding = 0) uniform sampler2D source_auto_exposure;
#endif

#if defined(MODE_LINEARIZE_DEPTH_COPY) || defined(MODE_SIMPLE_COPY_DEPTH)
layout(r32f, set = 3, binding = 0) uniform restrict writeonly image2D dest_buffer;
#elif defined(DST_IMAGE_8BIT)
layout(rgba8, set = 3, binding = 0) uniform restrict writeonly image2D dest_buffer;
#else
layout(rgba16f, set = 3, binding = 0) uniform restrict writeonly image2D dest_buffer;
#endif

#ifdef MODE_GAUSSIAN_BLUR
shared vec4 local_cache[256];
shared vec4 temp_cache[128];
#endif

void main() {
	// Pixel being shaded
	ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

#ifndef MODE_GAUSSIAN_BLUR // Gaussian blur needs the extra threads
	if (any(greaterThanEqual(pos, params.section.zw))) { //too large, do nothing
		return;
	}
#endif

#ifdef MODE_MIPMAP

	ivec2 base_pos = (pos + params.section.xy) << 1;
	vec4 color = texelFetch(source_color, base_pos, 0);
	color += texelFetch(source_color, base_pos + ivec2(0, 1), 0);
	color += texelFetch(source_color, base_pos + ivec2(1, 0), 0);
	color += texelFetch(source_color, base_pos + ivec2(1, 1), 0);
	color /= 4.0;
	color = mix(color, vec4(100.0, 100.0, 100.0, 1.0), isinf(color));
	color = mix(color, vec4(100.0, 100.0, 100.0, 1.0), isnan(color));

	imageStore(dest_buffer, pos + params.target, color);
#endif

#ifdef MODE_GAUSSIAN_BLUR

	// First pass copy texture into 16x16 local memory for every 8x8 thread block
	vec2 quad_center_uv = clamp(vec2(params.section.xy + gl_GlobalInvocationID.xy + gl_LocalInvocationID.xy - 3.5) / params.section.zw, vec2(0.5 / params.section.zw), vec2(1.0 - 1.5 / params.section.zw));
	uint dest_index = gl_LocalInvocationID.x * 2 + gl_LocalInvocationID.y * 2 * 16;

	local_cache[dest_index] = textureLod(source_color, quad_center_uv, 0);
	local_cache[dest_index + 1] = textureLod(source_color, quad_center_uv + vec2(1.0 / params.section.z, 0.0), 0);
	local_cache[dest_index + 16] = textureLod(source_color, quad_center_uv + vec2(0.0, 1.0 / params.section.w), 0);
	local_cache[dest_index + 16 + 1] = textureLod(source_color, quad_center_uv + vec2(1.0 / params.section.zw), 0);

#ifdef MODE_GLOW
	if (bool(params.flags & FLAG_GLOW_FIRST_PASS)) {
		// Tonemap initial samples to reduce weight of fireflies: https://graphicrants.blogspot.com/2013/12/tone-mapping.html
		vec3 tonemap_col = vec3(0.2126, 0.7152, 0.0722) / max(params.glow_luminance_cap, 6.0);
		local_cache[dest_index] /= 1.0 + dot(local_cache[dest_index].rgb, tonemap_col);
		local_cache[dest_index + 1] /= 1.0 + dot(local_cache[dest_index + 1].rgb, tonemap_col);
		local_cache[dest_index + 16] /= 1.0 + dot(local_cache[dest_index + 16].rgb, tonemap_col);
		local_cache[dest_index + 16 + 1] /= 1.0 + dot(local_cache[dest_index + 16 + 1].rgb, tonemap_col);
	}
	const float kernel[5] = { 0.2024, 0.1790, 0.1240, 0.0672, 0.0285 };
#else
	// Simpler blur uses SIGMA2 for the gaussian kernel for a stronger effect.
	const float kernel[4] = { 0.214607, 0.189879, 0.131514, 0.071303 };
#endif
	memoryBarrierShared();
	barrier();

	// Horizontal pass. Needs to copy into 8x16 chunk of local memory so vertical pass has full resolution
	uint read_index = gl_LocalInvocationID.x + gl_LocalInvocationID.y * 32 + 4;
	vec4 color_top = vec4(0.0);
	color_top += local_cache[read_index] * kernel[0];
	color_top += local_cache[read_index + 1] * kernel[1];
	color_top += local_cache[read_index + 2] * kernel[2];
	color_top += local_cache[read_index + 3] * kernel[3];
	color_top += local_cache[read_index - 1] * kernel[1];
	color_top += local_cache[read_index - 2] * kernel[2];
	color_top += local_cache[read_index - 3] * kernel[3];
#ifdef MODE_GLOW
	color_top += local_cache[read_index + 4] * kernel[4];
	color_top += local_cache[read_index - 4] * kernel[4];
#endif // MODE_GLOW

	vec4 color_bottom = vec4(0.0);
	color_bottom += local_cache[read_index + 16] * kernel[0];
	color_bottom += local_cache[read_index + 1 + 16] * kernel[1];
	color_bottom += local_cache[read_index + 2 + 16] * kernel[2];
	color_bottom += local_cache[read_index + 3 + 16] * kernel[3];
	color_bottom += local_cache[read_index - 1 + 16] * kernel[1];
	color_bottom += local_cache[read_index - 2 + 16] * kernel[2];
	color_bottom += local_cache[read_index - 3 + 16] * kernel[3];
#ifdef MODE_GLOW
	color_bottom += local_cache[read_index + 4 + 16] * kernel[4];
	color_bottom += local_cache[read_index - 4 + 16] * kernel[4];
#endif // MODE_GLOW

	// rotate samples to take advantage of cache coherency
	uint write_index = gl_LocalInvocationID.y * 2 + gl_LocalInvocationID.x * 16;

	temp_cache[write_index] = color_top;
	temp_cache[write_index + 1] = color_bottom;

	memoryBarrierShared();
	barrier();

	// If destination outside of texture, can stop doing work now
	if (any(greaterThanEqual(pos, params.section.zw))) {
		return;
	}

	// Vertical pass
	uint index = gl_LocalInvocationID.y + gl_LocalInvocationID.x * 16 + 4;
	vec4 color = vec4(0.0);

	color += temp_cache[index] * kernel[0];
	color += temp_cache[index + 1] * kernel[1];
	color += temp_cache[index + 2] * kernel[2];
	color += temp_cache[index + 3] * kernel[3];
	color += temp_cache[index - 1] * kernel[1];
	color += temp_cache[index - 2] * kernel[2];
	color += temp_cache[index - 3] * kernel[3];
#ifdef MODE_GLOW
	color += temp_cache[index + 4] * kernel[4];
	color += temp_cache[index - 4] * kernel[4];
#endif // MODE_GLOW

#ifdef MODE_GLOW
	if (bool(params.flags & FLAG_GLOW_FIRST_PASS)) {
		// Undo tonemap to restore range: https://graphicrants.blogspot.com/2013/12/tone-mapping.html
		color /= 1.0 - dot(color.rgb, vec3(0.2126, 0.7152, 0.0722) / max(params.glow_luminance_cap, 6.0));
	}

	color *= params.glow_strength;

	if (bool(params.flags & FLAG_GLOW_FIRST_PASS)) {
#ifdef GLOW_USE_AUTO_EXPOSURE

		color /= texelFetch(source_auto_exposure, ivec2(0, 0), 0).r / params.glow_auto_exposure_scale;
#endif
		color *= params.glow_exposure;

		float luminance = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
		float feedback = max(smoothstep(params.glow_hdr_threshold, params.glow_hdr_threshold + params.glow_hdr_scale, luminance), params.glow_bloom);

		color = min(color * feedback, vec4(params.glow_luminance_cap));
	}
#endif // MODE_GLOW
	imageStore(dest_buffer, pos + params.target, color);

#endif // MODE_GAUSSIAN_BLUR

#ifdef MODE_SIMPLE_COPY

	vec4 color;
	if (bool(params.flags & FLAG_COPY_ALL_SOURCE)) {
		vec2 uv = vec2(pos) / vec2(params.section.zw);
		if (bool(params.flags & FLAG_FLIP_Y)) {
			uv.y = 1.0 - uv.y;
		}
		color = textureLod(source_color, uv, 0.0);

	} else {
		color = texelFetch(source_color, pos + params.section.xy, 0);

		if (bool(params.flags & FLAG_FLIP_Y)) {
			pos.y = params.section.w - pos.y - 1;
		}
	}

	if (bool(params.flags & FLAG_FORCE_LUMINANCE)) {
		color.rgb = vec3(max(max(color.r, color.g), color.b));
	}

	if (bool(params.flags & FLAG_ALPHA_TO_ONE)) {
		color.a = 1.0;
	}

	imageStore(dest_buffer, pos + params.target, color);

#endif // MODE_SIMPLE_COPY

#ifdef MODE_SIMPLE_COPY_DEPTH

	vec4 color = texelFetch(source_color, pos + params.section.xy, 0);

	if (bool(params.flags & FLAG_FLIP_Y)) {
		pos.y = params.section.w - pos.y - 1;
	}

	imageStore(dest_buffer, pos + params.target, vec4(color.r));

#endif // MODE_SIMPLE_COPY_DEPTH

#ifdef MODE_LINEARIZE_DEPTH_COPY

	float depth = texelFetch(source_color, pos + params.section.xy, 0).r;
	depth = depth * 2.0 - 1.0;
	depth = 2.0 * params.camera_z_near * params.camera_z_far / (params.camera_z_far + params.camera_z_near - depth * (params.camera_z_far - params.camera_z_near));
	vec4 color = vec4(depth / params.camera_z_far);

	if (bool(params.flags & FLAG_FLIP_Y)) {
		pos.y = params.section.w - pos.y - 1;
	}

	imageStore(dest_buffer, pos + params.target, color);
#endif // MODE_LINEARIZE_DEPTH_COPY

#if defined(MODE_CUBEMAP_TO_PANORAMA) || defined(MODE_CUBEMAP_ARRAY_TO_PANORAMA)

	const float PI = 3.14159265359;
	vec2 uv = vec2(pos) / vec2(params.section.zw);
	if (bool(params.flags & FLAG_FLIP_Y)) {
		uv.y = 1.0 - uv.y;
	}
	float phi = uv.x * 2.0 * PI;
	float theta = uv.y * PI;

	vec3 normal;
	normal.x = sin(phi) * sin(theta) * -1.0;
	normal.y = cos(theta);
	normal.z = cos(phi) * sin(theta) * -1.0;

#ifdef MODE_CUBEMAP_TO_PANORAMA
	vec4 color = textureLod(source_color, normal, params.camera_z_far); //the biggest the lod the least the acne
#else
	vec4 color = textureLod(source_color, vec4(normal, params.camera_z_far), 0.0); //the biggest the lod the least the acne
#endif
	imageStore(dest_buffer, pos + params.target, color);
#endif // defined(MODE_CUBEMAP_TO_PANORAMA) || defined(MODE_CUBEMAP_ARRAY_TO_PANORAMA)

#ifdef MODE_SET_COLOR
	imageStore(dest_buffer, pos + params.target, params.set_color);
#endif
}

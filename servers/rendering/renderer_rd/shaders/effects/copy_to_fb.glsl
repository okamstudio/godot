#[vertex]

#version 450

#VERSION_DEFINES

#ifdef USE_MULTIVIEW
#ifdef has_VK_KHR_multiview
#extension GL_EXT_multiview : enable
#define ViewIndex gl_ViewIndex
#else // has_VK_KHR_multiview
#define ViewIndex 0
#endif // has_VK_KHR_multiview
#endif //USE_MULTIVIEW

#define FLAG_FLIP_Y (1 << 0)
#define FLAG_USE_SECTION (1 << 1)
#define FLAG_FORCE_LUMINANCE (1 << 2)
#define FLAG_ALPHA_TO_ZERO (1 << 3)
#define FLAG_SRGB (1 << 4)
#define FLAG_ALPHA_TO_ONE (1 << 5)
#define FLAG_LINEAR (1 << 6)
#define FLAG_NORMAL (1 << 7)
#define FLAG_USE_SRC_SECTION (1 << 8)

#ifdef USE_MULTIVIEW
layout(location = 0) out vec3 uv_interp;
#else
layout(location = 0) out vec2 uv_interp;
#endif

layout(push_constant, std430) uniform Params {
	vec4 section;
	vec2 pixel_size;
	float luminance_multiplier;
	uint flags;

	vec4 color;
}
params;

void main() {
	vec2 base_arr[4] = vec2[](vec2(0.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 1.0), vec2(1.0, 0.0));
	uv_interp.xy = base_arr[gl_VertexIndex];
#ifdef USE_MULTIVIEW
	uv_interp.z = ViewIndex;
#endif
	vec2 vpos = uv_interp.xy;
	if (bool(params.flags & FLAG_USE_SECTION)) {
		vpos = params.section.xy + vpos * params.section.zw;
	}

	gl_Position = vec4(vpos * 2.0 - 1.0, 0.0, 1.0);

	if (bool(params.flags & FLAG_FLIP_Y)) {
		uv_interp.y = 1.0 - uv_interp.y;
	}

	if (bool(params.flags & FLAG_USE_SRC_SECTION)) {
		uv_interp.xy = params.section.xy + uv_interp.xy * params.section.zw;
	}
}

#[fragment]

#version 450

#VERSION_DEFINES

#ifdef USE_MULTIVIEW
#ifdef has_VK_KHR_multiview
#extension GL_EXT_multiview : enable
#define ViewIndex gl_ViewIndex
#else // has_VK_KHR_multiview
#define ViewIndex 0
#endif // has_VK_KHR_multiview
#endif //USE_MULTIVIEW

#define FLAG_FLIP_Y (1 << 0)
#define FLAG_USE_SECTION (1 << 1)
#define FLAG_FORCE_LUMINANCE (1 << 2)
#define FLAG_ALPHA_TO_ZERO (1 << 3)
#define FLAG_SRGB (1 << 4)
#define FLAG_ALPHA_TO_ONE (1 << 5)
#define FLAG_LINEAR (1 << 6)
#define FLAG_NORMAL (1 << 7)
#define FLAG_OCCLUSION_CULLING_BUFFER (1 << 9)

layout(push_constant, std430) uniform Params {
	vec4 section;
	vec2 pixel_size;
	float luminance_multiplier;
	uint flags;

	vec4 color;
}
params;

#ifndef MODE_SET_COLOR
#ifdef USE_MULTIVIEW
layout(location = 0) in vec3 uv_interp;
#else
layout(location = 0) in vec2 uv_interp;
#endif

#ifdef USE_MULTIVIEW
layout(set = 0, binding = 0) uniform sampler2DArray source_color;
#ifdef MODE_TWO_SOURCES
layout(set = 1, binding = 0) uniform sampler2DArray source_depth;
layout(location = 1) out float depth;
#endif /* MODE_TWO_SOURCES */
#else /* USE_MULTIVIEW */
layout(set = 0, binding = 0) uniform sampler2D source_color;
#ifdef MODE_TWO_SOURCES
layout(set = 1, binding = 0) uniform sampler2D source_color2;
#endif /* MODE_TWO_SOURCES */
#endif /* USE_MULTIVIEW */
#endif /* !SET_COLOR */

layout(location = 0) out vec4 frag_color;

vec3 linear_to_srgb(vec3 color) {
	//if going to srgb, clamp from 0 to 1.
	color = clamp(color, vec3(0.0), vec3(1.0));
	const vec3 a = vec3(0.055f);
	return mix((vec3(1.0f) + a) * pow(color.rgb, vec3(1.0f / 2.4f)) - a, 12.92f * color.rgb, lessThan(color.rgb, vec3(0.0031308f)));
}

vec3 srgb_to_linear(vec3 color) {
	return mix(pow((color.rgb + vec3(0.055)) * (1.0 / (1.0 + 0.055)), vec3(2.4)), color.rgb * (1.0 / 12.92), lessThan(color.rgb, vec3(0.04045)));
}

void main() {
#ifdef MODE_SET_COLOR
	frag_color = params.color;
#else

#ifdef USE_MULTIVIEW
	vec3 uv = uv_interp;
#else
	vec2 uv = uv_interp;
#endif

#ifdef MODE_PANORAMA_TO_DP
	// Note, multiview and panorama should not be mixed at this time

	//obtain normal from dual paraboloid uv
#define M_PI 3.14159265359

	float side;
	uv.y = modf(uv.y * 2.0, side);
	side = side * 2.0 - 1.0;
	vec3 normal = vec3(uv * 2.0 - 1.0, 0.0);
	normal.z = 0.5 - 0.5 * ((normal.x * normal.x) + (normal.y * normal.y));
	normal *= -side;
	normal = normalize(normal);

	//now convert normal to panorama uv

	vec2 st = vec2(atan(normal.x, normal.z), acos(normal.y));

	if (st.x < 0.0) {
		st.x += M_PI * 2.0;
	}

	uv = st / vec2(M_PI * 2.0, M_PI);

	if (side < 0.0) {
		//uv.y = 1.0 - uv.y;
		uv = 1.0 - uv;
	}
#endif /* MODE_PANORAMA_TO_DP */

#ifdef USE_MULTIVIEW
	vec4 color = textureLod(source_color, uv, 0.0);
#ifdef MODE_TWO_SOURCES
	// In multiview our 2nd input will be our depth map
	depth = textureLod(source_depth, uv, 0.0).r;
#endif /* MODE_TWO_SOURCES */

#else /* USE_MULTIVIEW */
	vec4 color = textureLod(source_color, uv, 0.0);
#ifdef MODE_TWO_SOURCES
	color += textureLod(source_color2, uv, 0.0);
#endif /* MODE_TWO_SOURCES */
#endif /* USE_MULTIVIEW */

	if (bool(params.flags & FLAG_FORCE_LUMINANCE)) {
		color.rgb = vec3(max(max(color.r, color.g), color.b));
	}
	if (bool(params.flags & FLAG_ALPHA_TO_ZERO)) {
		color.rgb *= color.a;
	}
	if (bool(params.flags & FLAG_SRGB)) {
		color.rgb = linear_to_srgb(color.rgb);
	}
	if (bool(params.flags & FLAG_ALPHA_TO_ONE)) {
		color.a = 1.0;
	}
	if (bool(params.flags & FLAG_LINEAR)) {
		color.rgb = srgb_to_linear(color.rgb);
	}
	if (bool(params.flags & FLAG_NORMAL)) {
		color.rgb = normalize(color.rgb * 2.0 - 1.0) * 0.5 + 0.5;
	}
	if (bool(params.flags & FLAG_OCCLUSION_CULLING_BUFFER)) {
		// Modify result to make the background partially visible,
		// which helps with 3D navigation.
		// This relies on the additive blend state from the `CopyEffects()` constructor.
		color.a = (1.0 - color.r) * 0.5;
		color.rgb = vec3(1.0, 0.0, 0.0);
	}

	frag_color = color / params.luminance_multiplier;
#endif // MODE_SET_COLOR
}

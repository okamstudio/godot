/* clang-format off */
#[modes]

// Based on Dual filtering glow as explained in Marius Bjørge presentation at Siggraph 2015 "Bandwidth-Efficient Rendering"

mode_filter = #define MODE_FILTER
mode_downsample = #define MODE_DOWNSAMPLE
mode_upsample = #define MODE_UPSAMPLE

#[specializations]

USE_MULTIVIEW = false

#[vertex]
layout(location = 0) in vec2 vertex_attrib;

/* clang-format on */

out vec2 uv_interp;

void main() {
	uv_interp = vertex_attrib * 0.5 + 0.5;
	gl_Position = vec4(vertex_attrib, 1.0, 1.0);
}

/* clang-format off */
#[fragment]
/* clang-format on */

#ifdef MODE_FILTER
#ifdef USE_MULTIVIEW
uniform sampler2DArray source_color; // texunit:0
#else
uniform sampler2D source_color; // texunit:0
#endif // USE_MULTIVIEW
uniform float view;
uniform vec2 pixel_size;
uniform float luminance_multiplier;
uniform float glow_bloom;
uniform float glow_hdr_threshold;
uniform float glow_hdr_scale;
uniform float glow_luminance_cap;
#endif // MODE_FILTER

#ifdef MODE_DOWNSAMPLE
uniform sampler2D source_color; // texunit:0
uniform vec2 pixel_size;
#endif // MODE_DOWNSAMPLE

#ifdef MODE_UPSAMPLE
uniform sampler2D source_color; // texunit:0
uniform vec2 pixel_size;
#endif // MODE_UPSAMPLE

in vec2 uv_interp;

layout(location = 0) out vec4 frag_color;

void main() {
#ifdef MODE_FILTER
	// Note, we read from an image with double resolution, so we average those out
#ifdef USE_MULTIVIEW
	vec2 half_pixel = pixel_size * 0.5;
	vec3 uv = vec3(uv_interp, view);
	vec3 color = textureLod(source_color, uv, 0.0).rgb * 4.0;
	color += textureLod(source_color, uv - vec3(half_pixel, 0.0), 0.0).rgb;
	color += textureLod(source_color, uv + vec3(half_pixel, 0.0), 0.0).rgb;
	color += textureLod(source_color, uv - vec3(half_pixel.x, -half_pixel.y, 0.0), 0.0).rgb;
	color += textureLod(source_color, uv + vec3(half_pixel.x, -half_pixel.y, 0.0), 0.0).rgb;
#else
	vec2 half_pixel = pixel_size * 0.5;
	vec2 uv = uv_interp;
	vec3 color = textureLod(source_color, uv, 0.0).rgb * 4.0;
	color += textureLod(source_color, uv - half_pixel, 0.0).rgb;
	color += textureLod(source_color, uv + half_pixel, 0.0).rgb;
	color += textureLod(source_color, uv - vec2(half_pixel.x, -half_pixel.y), 0.0).rgb;
	color += textureLod(source_color, uv + vec2(half_pixel.x, -half_pixel.y), 0.0).rgb;
#endif // USE_MULTIVIEW
	color /= luminance_multiplier * 8.0;

	float feedback_factor = dot(color, vec3(0.2126, 0.7152, 0.0722));
	float feedback = max(smoothstep(glow_hdr_threshold, glow_hdr_threshold + glow_hdr_scale, feedback_factor), glow_bloom);

	color = min(color * feedback, vec3(glow_luminance_cap));

	frag_color = vec4(luminance_multiplier * color, 1.0);
#endif // MODE_FILTER

#ifdef MODE_DOWNSAMPLE
	vec2 half_pixel = pixel_size * 0.5;
	vec4 color = textureLod(source_color, uv_interp, 0.0) * 4.0;
	color += textureLod(source_color, uv_interp - half_pixel, 0.0);
	color += textureLod(source_color, uv_interp + half_pixel, 0.0);
	color += textureLod(source_color, uv_interp - vec2(half_pixel.x, -half_pixel.y), 0.0);
	color += textureLod(source_color, uv_interp + vec2(half_pixel.x, -half_pixel.y), 0.0);
	frag_color = color / 8.0;
#endif // MODE_DOWNSAMPLE

#ifdef MODE_UPSAMPLE
	vec2 half_pixel = pixel_size * 0.5;

	vec4 color = textureLod(source_color, uv_interp + vec2(-half_pixel.x * 2.0, 0.0), 0.0);
	color += textureLod(source_color, uv_interp + vec2(-half_pixel.x, half_pixel.y), 0.0) * 2.0;
	color += textureLod(source_color, uv_interp + vec2(0.0, half_pixel.y * 2.0), 0.0);
	color += textureLod(source_color, uv_interp + vec2(half_pixel.x, half_pixel.y), 0.0) * 2.0;
	color += textureLod(source_color, uv_interp + vec2(half_pixel.x * 2.0, 0.0), 0.0);
	color += textureLod(source_color, uv_interp + vec2(half_pixel.x, -half_pixel.y), 0.0) * 2.0;
	color += textureLod(source_color, uv_interp + vec2(0.0, -half_pixel.y * 2.0), 0.0);
	color += textureLod(source_color, uv_interp + vec2(-half_pixel.x, -half_pixel.y), 0.0) * 2.0;

	frag_color = color / 12.0;
#endif // MODE_UPSAMPLE
}

#[vertex]

#version 450

#VERSION_DEFINES

layout(push_constant, std140) uniform Pos {
	vec4 src_rect;
	vec4 dst_rect;

	float rotation_sin;
	float rotation_cos;

	vec2 eye_center;
	float k1;
	float k2;

	float upscale;
	float aspect_ratio;
	uint layer;
	bool source_is_srgb;

	uint target_color_space;
	float reference_multiplier;
}
data;

layout(location = 0) out vec2 uv;

void main() {
	mat4 swapchain_transform = mat4(1.0);
	swapchain_transform[0][0] = data.rotation_cos;
	swapchain_transform[0][1] = -data.rotation_sin;
	swapchain_transform[1][0] = data.rotation_sin;
	swapchain_transform[1][1] = data.rotation_cos;

	vec2 base_arr[4] = vec2[](vec2(0.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 1.0), vec2(1.0, 0.0));
	uv = data.src_rect.xy + base_arr[gl_VertexIndex] * data.src_rect.zw;
	vec2 vtx = data.dst_rect.xy + base_arr[gl_VertexIndex] * data.dst_rect.zw;
	gl_Position = swapchain_transform * vec4(vtx * 2.0 - 1.0, 0.0, 1.0);
}

#[fragment]

#version 450

#VERSION_DEFINES

layout(push_constant, std140) uniform Pos {
	vec4 src_rect;
	vec4 dst_rect;

	float rotation_sin;
	float rotation_cos;

	vec2 eye_center;
	float k1;
	float k2;

	float upscale;
	float aspect_ratio;
	uint layer;
	bool source_is_srgb;

	uint target_color_space;
	float reference_multiplier;
}
data;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 color;

#ifdef USE_LAYER
layout(binding = 0) uniform sampler2DArray src_rt;
#else
layout(binding = 0) uniform sampler2D src_rt;
#endif

// Keep in sync with RenderingDeviceCommons::ColorSpace
const uint COLOR_SPACE_SRGB_LINEAR = 0;
const uint COLOR_SPACE_SRGB_NONLINEAR = 1;
const uint COLOR_SPACE_HDR10_ST2084 = 2;

vec3 srgb_to_linear(vec3 color) {
	return mix(pow((color.rgb + vec3(0.055)) * (1.0 / (1.0 + 0.055)), vec3(2.4)), color.rgb * (1.0 / 12.92), lessThan(color.rgb, vec3(0.04045)));
}

vec3 linear_to_srgb(vec3 color) {
	// If going to srgb, clamp from 0 to 1.
	color = clamp(color, vec3(0.0), vec3(1.0));
	const vec3 a = vec3(0.055f);
	return mix((vec3(1.0f) + a) * pow(color.rgb, vec3(1.0f / 2.4f)) - a, 12.92f * color.rgb, lessThan(color.rgb, vec3(0.0031308f)));
}

vec3 rec709_to_rec2020(vec3 color) {
	const mat3 conversion = mat3(
			0.627402, 0.069095, 0.016394,
			0.329292, 0.919544, 0.088028,
			0.043306, 0.011360, 0.895578);
	return conversion * color;
}

vec3 linear_to_st2084(vec3 color) {
	// Prevent negative values from messing with the transfer function.
	color = max(color, vec3(0.0, 0.0, 0.0));

	// Apply ST2084 curve
	const float c1 = 0.8359375;
	const float c2 = 18.8515625;
	const float c3 = 18.6875;
	const float m1 = 0.1593017578125;
	const float m2 = 78.84375;
	vec3 cp = pow(abs(color), vec3(m1));

	return pow((c1 + c2 * cp) / (1 + c3 * cp), vec3(m2));
}

void main() {
#ifdef APPLY_LENS_DISTORTION
	vec2 coords = uv * 2.0 - 1.0;
	vec2 offset = coords - data.eye_center;

	// take aspect ratio into account
	offset.y /= data.aspect_ratio;

	// distort
	vec2 offset_sq = offset * offset;
	float radius_sq = offset_sq.x + offset_sq.y;
	float radius_s4 = radius_sq * radius_sq;
	float distortion_scale = 1.0 + (data.k1 * radius_sq) + (data.k2 * radius_s4);
	offset *= distortion_scale;

	// reapply aspect ratio
	offset.y *= data.aspect_ratio;

	// add our eye center back in
	coords = offset + data.eye_center;
	coords /= data.upscale;

	// and check our color
	if (coords.x < -1.0 || coords.y < -1.0 || coords.x > 1.0 || coords.y > 1.0) {
		color = vec4(0.0, 0.0, 0.0, 1.0);
	} else {
		// layer is always used here
		coords = (coords + vec2(1.0)) / vec2(2.0);
		color = texture(src_rt, vec3(coords, data.layer));
	}
#elif defined(USE_LAYER)
	color = texture(src_rt, vec3(uv, data.layer));
#else
	color = texture(src_rt, uv);
#endif

	// Colorspace conversion for final blit
	switch (data.target_color_space) {
		case COLOR_SPACE_SRGB_LINEAR:
			if (data.source_is_srgb == true) {
				// sRGB -> linear conversion
				color.rgb = srgb_to_linear(color.rgb);
			}

			// Adjust brightness of SDR content to reference luminance
			color.rgb *= data.reference_multiplier;
			break;
		case COLOR_SPACE_SRGB_NONLINEAR:
			if (data.source_is_srgb == false) {
				// linear -> sRGB conversion
				color.rgb = linear_to_srgb(color.rgb);
			}
			break;
		case COLOR_SPACE_HDR10_ST2084:
			if (data.source_is_srgb == true) {
				// sRGB -> linear conversion
				color.rgb = srgb_to_linear(color.rgb);
			}

			// Convert to Rec.2020 primaries
			color.rgb = rec709_to_rec2020(color.rgb);

			// Adjust brightness of SDR content to reference luminance
			color.rgb *= data.reference_multiplier;

			// Apply the ST2084 curve
			color.rgb = linear_to_st2084(color.rgb);
			break;
		default:
			break;
	}
}

#[compute]

#version 450

#VERSION_DEFINES

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(rgba16f, set = 0, binding = 0) uniform restrict readonly image2D source_diffuse;
layout(r32f, set = 0, binding = 1) uniform restrict readonly image2D source_depth;
layout(rgba16f, set = 1, binding = 0) uniform restrict writeonly image2D ssr_image;
#ifdef MODE_ROUGH
layout(r8, set = 1, binding = 1) uniform restrict writeonly image2D blur_radius_image;
#endif
layout(rgba8, set = 2, binding = 0) uniform restrict readonly image2D source_normal_roughness;
layout(set = 3, binding = 0) uniform sampler2D source_metallic;

layout(push_constant, std430) uniform Params {
	vec4 proj_info;

	ivec2 screen_size;
	float camera_z_near;
	float camera_z_far;

	int num_steps;
	float depth_tolerance;
	float distance_fade;
	float curve_fade_in;

	bool orthogonal;
	float filter_mipmap_levels;
	bool use_half_res;
	uint view_index;
}
params;

#include "screen_space_reflection_inc.glsl"

vec2 view_to_screen(vec3 view_pos, out float w) {
	vec4 projected = scene_data.projection[params.view_index] * vec4(view_pos, 1.0);
	projected.xyz /= projected.w;
	projected.xy = projected.xy * 0.5 + 0.5;
	w = projected.w;
	return projected.xy;
}

#define M_PI 3.14159265359

void main() {
	// Pixel being shaded
	ivec2 ssC = ivec2(gl_GlobalInvocationID.xy);

	if (any(greaterThanEqual(ssC.xy, params.screen_size))) { //too large, do nothing
		return;
	}

	vec2 pixel_size = 1.0 / vec2(params.screen_size);
	vec2 uv = vec2(ssC.xy) * pixel_size;

	uv += pixel_size * 0.5;

	float base_depth = imageLoad(source_depth, ssC).r;

	// World space point being shaded
	vec3 vertex = reconstructCSPosition(uv * vec2(params.screen_size), base_depth);

	vec4 normal_roughness = imageLoad(source_normal_roughness, ssC);
	vec3 normal = normalize(normal_roughness.xyz * 2.0 - 1.0);
	float roughness = normal_roughness.w;
	if (roughness > 0.5) {
		roughness = 1.0 - roughness;
	}
	roughness /= (127.0 / 255.0);

	// The roughness cutoff of 0.6 is chosen to match the roughness fadeout from GH-69828.
	if (roughness > 0.6) {
		// Do not compute SSR for rough materials to improve performance at the cost of
		// subtle artifacting.
#ifdef MODE_ROUGH
		imageStore(blur_radius_image, ssC, vec4(0.0));
#endif
		imageStore(ssr_image, ssC, vec4(0.0));
		return;
	}

	normal = normalize(normal);
	normal.y = -normal.y; //because this code reads flipped

	vec3 view_dir;
	if (sc_multiview) {
		view_dir = normalize(vertex + scene_data.eye_offset[params.view_index].xyz);
	} else {
		view_dir = params.orthogonal ? vec3(0.0, 0.0, -1.0) : normalize(vertex);
	}
	vec3 ray_dir = normalize(reflect(view_dir, normal));

	if (dot(ray_dir, normal) < 0.001) {
		imageStore(ssr_image, ssC, vec4(0.0));
		return;
	}

	////////////////

	// make ray length and clip it against the near plane (don't want to trace beyond visible)
	float ray_len = (vertex.z + ray_dir.z * params.camera_z_far) > -params.camera_z_near ? (-params.camera_z_near - vertex.z) / ray_dir.z : params.camera_z_far;
	vec3 ray_end = vertex + ray_dir * ray_len;

	float w_begin;
	vec2 vp_line_begin = view_to_screen(vertex, w_begin);
	float w_end;
	vec2 vp_line_end = view_to_screen(ray_end, w_end);
	vec2 vp_line_dir = vp_line_end - vp_line_begin;

	// we need to interpolate w along the ray, to generate perspective correct reflections
	w_begin = 1.0 / w_begin;
	w_end = 1.0 / w_end;

	float z_begin = vertex.z * w_begin;
	float z_end = ray_end.z * w_end;

	vec2 line_begin = vp_line_begin / pixel_size;
	vec2 line_dir = vp_line_dir / pixel_size;
	float z_dir = z_end - z_begin;
	float w_dir = w_end - w_begin;

	// clip the line to the viewport edges

	float scale_max_x = min(1.0, 0.99 * (1.0 - vp_line_begin.x) / max(1e-5, vp_line_dir.x));
	float scale_max_y = min(1.0, 0.99 * (1.0 - vp_line_begin.y) / max(1e-5, vp_line_dir.y));
	float scale_min_x = min(1.0, 0.99 * vp_line_begin.x / max(1e-5, -vp_line_dir.x));
	float scale_min_y = min(1.0, 0.99 * vp_line_begin.y / max(1e-5, -vp_line_dir.y));
	float line_clip = min(scale_max_x, scale_max_y) * min(scale_min_x, scale_min_y);
	line_dir *= line_clip;
	z_dir *= line_clip;
	w_dir *= line_clip;

	// clip z and w advance to line advance
	vec2 line_advance = normalize(line_dir); // down to pixel
	float step_size = 1.0 / length(line_dir);
	float z_advance = z_dir * step_size; // adapt z advance to line advance
	float w_advance = w_dir * step_size; // adapt w advance to line advance

	// make line advance faster if direction is closer to pixel edges (this avoids sampling the same pixel twice)
	float advance_angle_adj = 1.0 / max(abs(line_advance.x), abs(line_advance.y));
	line_advance *= advance_angle_adj; // adapt z advance to line advance
	z_advance *= advance_angle_adj;
	w_advance *= advance_angle_adj;

	vec2 pos = line_begin;
	float z = z_begin;
	float w = w_begin;
	float z_from = z / w;
	float z_to = z_from;
	float depth;
	vec2 prev_pos = pos;

	if (ivec2(pos + line_advance - 0.5) == ssC) {
		// It is possible for rounding to cause our first pixel to check to be the pixel we're reflecting.
		// Make sure we skip it
		pos += line_advance;
		z += z_advance;
		w += w_advance;
	}

	bool found = false;

	float steps_taken = 0.0;

	for (int i = 0; i < params.num_steps; i++) {
		pos += line_advance;
		z += z_advance;
		w += w_advance;

		// convert to linear depth
		ivec2 test_pos = ivec2(pos - 0.5);
		depth = imageLoad(source_depth, test_pos).r;
		if (sc_multiview) {
			depth = 1.0 - depth * 2.0;
			depth = -(scene_data.projection[params.view_index][3][2] - scene_data.projection[params.view_index][3][3] * depth) / (scene_data.projection[params.view_index][2][3] * depth - scene_data.projection[params.view_index][2][2]);
		}

		z_from = z_to;
		z_to = z / w;

		if (depth > z_to) {
			// Test if our ray is hitting the "right" side of the surface, if not we're likely self reflecting and should skip.
			vec4 test_normal_roughness = imageLoad(source_normal_roughness, test_pos);
			vec3 test_normal = test_normal_roughness.xyz * 2.0 - 1.0;
			test_normal = normalize(test_normal);
			test_normal.y = -test_normal.y; // Because this code reads flipped.

			if (dot(ray_dir, test_normal) < 0.001) {
				// if depth was surpassed
				if (depth <= max(z_to, z_from) + params.depth_tolerance && -depth < params.camera_z_far * 0.95) {
					// check the depth tolerance and far clip
					// check that normal is valid
					found = true;
				}
				break;
			}
		}

		steps_taken += 1.0;
		prev_pos = pos;
	}

	if (found) {
		float margin_blend = 1.0;
		vec2 final_pos = pos;

		vec2 margin = vec2((params.screen_size.x + params.screen_size.y) * 0.05); // make a uniform margin
		if (any(bvec4(lessThan(pos, vec2(0.0, 0.0)), greaterThan(pos, params.screen_size)))) {
			// clip at the screen edges
			imageStore(ssr_image, ssC, vec4(0.0));
			return;
		}

		{
			//blend fading out towards inner margin
			// 0.5 = midpoint of reflection
			vec2 margin_grad = mix(params.screen_size - pos, pos, lessThan(pos, params.screen_size * 0.5));
			margin_blend = smoothstep(0.0, margin.x * margin.y, margin_grad.x * margin_grad.y);
			//margin_blend = 1.0;
		}

		// Fade In / Fade Out
		float grad = (steps_taken + 1.0) / float(params.num_steps);
		float initial_fade = params.curve_fade_in == 0.0 ? 1.0 : pow(clamp(grad, 0.0, 1.0), params.curve_fade_in);
		float fade = pow(clamp(1.0 - grad, 0.0, 1.0), params.distance_fade) * initial_fade;

		// Ensure that precision errors do not introduce any fade. Even if it is just slightly below 1.0,
		// strong specular light can leak through the reflection.
		if (fade > 0.999) {
			fade = 1.0;
		}

		// This is an ad-hoc term to fade out the SSR as roughness increases. Values used
		// are meant to match the visual appearance of a ReflectionProbe.
		float roughness_fade = smoothstep(0.4, 0.7, 1.0 - roughness);

		// Schlick term.
		float metallic = texelFetch(source_metallic, ssC << 1, 0).w;

		// F0 is the reflectance of normally incident light (perpendicular to the surface).
		// Dielectric materials have a widely accepted default value of 0.04. We assume that metals reflect all light, so their F0 is 1.0.
		float f0 = mix(0.04, 1.0, metallic);
		float m = clamp(1.0 - dot(normal, -view_dir), 0.0, 1.0);
		float m2 = m * m;
		m = m2 * m2 * m; // pow(m,5)
		float fresnel_term = f0 + (1.0 - f0) * m; // Fresnel Schlick term.

		// The alpha value of final_color controls the blending with specular light in specular_merge.glsl.
		// Note that the Fresnel term is multiplied with the RGB color instead of being a part of the alpha value.
		// There is a key difference:
		// - multiplying a term with RGB darkens the SSR light without introducing/taking away specular light.
		// - combining a term into the Alpha value introduces specular light at the expense of the SSR light.
		vec4 final_color = vec4(imageLoad(source_diffuse, ivec2(final_pos - 0.5)).rgb * fresnel_term, fade * margin_blend * roughness_fade);

		imageStore(ssr_image, ssC, final_color);

#ifdef MODE_ROUGH

		// if roughness is enabled, do screen space cone tracing
		float blur_radius = 0.0;

		if (roughness > 0.001) {
			float cone_angle = min(roughness, 0.999) * M_PI * 0.5;
			float cone_len = length(final_pos - line_begin);
			float op_len = 2.0 * tan(cone_angle) * cone_len; // opposite side of iso triangle
			{
				// fit to sphere inside cone (sphere ends at end of cone), something like this:
				// ___
				// \O/
				//  V
				//
				// as it avoids bleeding from beyond the reflection as much as possible. As a plus
				// it also makes the rough reflection more elongated.
				float a = op_len;
				float h = cone_len;
				float a2 = a * a;
				float fh2 = 4.0f * h * h;
				blur_radius = (a * (sqrt(a2 + fh2) - a)) / (4.0f * h);
			}
		}

		imageStore(blur_radius_image, ssC, vec4(blur_radius / 255.0)); //stored in r8

#endif // MODE_ROUGH

	} else {
#ifdef MODE_ROUGH
		imageStore(blur_radius_image, ssC, vec4(0.0));
#endif
		imageStore(ssr_image, ssC, vec4(0.0));
	}
}

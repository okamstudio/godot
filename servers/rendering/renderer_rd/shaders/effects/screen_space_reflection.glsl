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
	ivec2 screen_size;
	int num_steps;
	float depth_tolerance;

	float distance_fade;
	float curve_fade_in;
	uint view_index;
	uint pad;
}
params;

#include "screen_space_reflection_inc.glsl"

#define M_PI 3.14159265359

void main() {
	// Pixel being shaded
	ivec2 ssC = ivec2(gl_GlobalInvocationID.xy);

	if (any(greaterThanEqual(ssC.xy, params.screen_size))) { //too large, do nothing
		return;
	}

	vec3 normal;
	float roughness;

	{ // Get normal and roughness at vertex position
		vec4 normal_roughness = imageLoad(source_normal_roughness, ssC);
		normal = normalize(normal_roughness.xyz * 2.0 - 1.0);
		normal.y = -normal.y; //because this code reads flipped
		roughness = normal_roughness.w;
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
	}

	vec2 uv = (vec2(ssC.xy) + 0.5) / vec2(params.screen_size);

	vec3 vertex_view;
	vec4 vertex_hom;
	vec4 vertex_clip;

	{ // Compute vertex position
		float depth_view = imageLoad(source_depth, ssC).r;
		vertex_clip = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
		vertex_clip.z = z_ndc_from_view(vertex_clip.xy, depth_view);
		vertex_hom = scene_data.inv_projection[params.view_index] * vertex_clip;
		vertex_clip /= vertex_hom.w;
		vertex_view = vertex_hom.xyz / vertex_hom.w;
	}

	vec3 view_dir;

	if (sc_multiview) {
		view_dir = normalize(vertex_view + scene_data.eye_offset[params.view_index].xyz);
	} else {
		view_dir = scene_data.projection[params.view_index][3][3] == 1.0 ? vec3(0.0, 0.0, -1.0) : normalize(vertex_view);
	}

	// xy in screen space, z in ndc space
	vec3 dray;
	vec3 ray_start = vec3(uv * vec2(params.screen_size), vertex_clip.z / vertex_clip.w);

	// zw in homogeneous view space
	vec2 ddepth;

	{ // compute ray from shaded vertex
		vec3 ray_dir = normalize(reflect(view_dir, normal));

		if (dot(ray_dir, normal) < 0.001) {
			imageStore(ssr_image, ssC, vec4(0.0));
			return;
		}

		vec4 ray_dir_clip = scene_data.projection[params.view_index] * vec4(ray_dir, 0.0); // Not a unit vector

		// Scale down to prevent w to go negative, which breaks perspective division;
		float scale = vertex_clip.w + ray_dir_clip.w <= 0.01 ? (0.01 - vertex_clip.w) / ray_dir_clip.w : 1.0;
		vec4 ray_end_clip = ray_dir_clip * scale + vertex_clip;
		vec4 ray_end_hom = vec4(ray_dir * scale + vertex_view, 1.0) / ray_end_clip.w;

		vec3 ray_end_ndc = ray_end_clip.xyz / ray_end_clip.w;
		ray_end_ndc.xy = (ray_end_ndc.xy * 0.5 + 0.5) * vec2(params.screen_size);

		dray = ray_end_ndc - ray_start;
		ddepth = ray_end_hom.zw - vertex_hom.zw;
		float nb_steps = max(abs(dray.x), abs(dray.y)); // DDA : normalize advance to 1 px across the fastest axis
		dray /= nb_steps;
		ddepth /= nb_steps;
	}

	float max_steps;

	{ // compute number of steps to the frustum bounds across each axis
		max_steps = (dray.x > 0.0 ? (params.screen_size.x - ray_start.x) : -ray_start.x) / dray.x;
		max_steps = min(max_steps, (dray.y > 0.0 ? (params.screen_size.y - ray_start.y) : -ray_start.y) / dray.y);
		max_steps = min(max_steps, (dray.z > 0.0 ? (1.0 - ray_start.z) : -ray_start.z) / dray.z);
	}

	vec2 final_pos;
	bool found = false;
	float iter;

	{ // raymarch
		// xy in screen space, zw in homogeneous view space
		vec4 step = vec4(dray.xy, ddepth);
		vec4 pos = vec4(ray_start.xy, vertex_hom.zw);

		bool front_facing = true;

		for (iter = 0; iter < params.num_steps && iter < max_steps; iter++) {
			pos += step;
			float depth = imageLoad(source_depth, ivec2(pos.xy - 0.5)).r;
			float pos_depth = pos.z / pos.w;
			if (depth > pos_depth && depth < pos_depth + params.depth_tolerance && front_facing) {
				final_pos = pos.xy;
				found = true;
				break;
			}
			front_facing = depth < pos_depth;
		}
	}

	if (found) {
		float margin_blend = 1.0;

		vec2 margin = vec2((params.screen_size.x + params.screen_size.y) * 0.05); // make a uniform margin
		if (any(bvec4(lessThan(final_pos, vec2(0.0)), greaterThan(final_pos, params.screen_size)))) {
			// clip at the screen edges
			imageStore(ssr_image, ssC, vec4(0.0));
			return;
		}

		{
			//blend fading out towards inner margin
			// 0.5 = midpoint of reflection
			vec2 margin_grad = mix(params.screen_size - final_pos, final_pos, lessThan(final_pos, params.screen_size * 0.5));
			margin_blend = smoothstep(0.0, margin.x * margin.y, margin_grad.x * margin_grad.y);
			//margin_blend = 1.0;
		}

		// Fade In / Fade Out
		float grad = (iter + 1.0) / float(params.num_steps);
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
			float cone_len = length(final_pos - ray_start.xy);
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

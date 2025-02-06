#[compute]

#version 450

#VERSION_DEFINES

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

/* Specialization Constants (Toggles) */

layout(constant_id = 0) const bool sc_multiview = false;

/* inputs */
layout(set = 0, binding = 0) uniform sampler2D source_ssr;
layout(set = 1, binding = 0) uniform sampler2D source_depth;
layout(set = 1, binding = 1) uniform sampler2D source_normal;
layout(rgba16f, set = 2, binding = 0) uniform restrict writeonly image2D dest_ssr;
layout(r32f, set = 3, binding = 0) uniform restrict writeonly image2D dest_depth;
layout(rgba8, set = 3, binding = 1) uniform restrict writeonly image2D dest_normal;

layout(push_constant, std430) uniform Params {
	ivec2 screen_size;
	bool orthogonal;
	bool filtered;

	mat2 proj_zw; // Bottom-right 2x2 corner of the projection matrix with reverse-z and z-remap applied
}
params;

void main() {
	// Pixel being shaded
	ivec2 ssC = ivec2(gl_GlobalInvocationID.xy);

	if (any(greaterThanEqual(ssC.xy, params.screen_size))) { //too large, do nothing
		return;
	}
	//do not filter, SSR will generate artifacts if this is done

	float divisor = 0.0;
	vec4 color;
	float depth;
	vec4 normal;

	if (params.filtered) {
		color = vec4(0.0);
		depth = 0.0;
		normal = vec4(0.0);

		for (int i = 0; i < 4; i++) {
			ivec2 ofs = ssC << 1;
			if (bool(i & 1)) {
				ofs.x += 1;
			}
			if (bool(i & 2)) {
				ofs.y += 1;
			}
			color += texelFetch(source_ssr, ofs, 0);
			float d = texelFetch(source_depth, ofs, 0).r;
			vec4 nr = texelFetch(source_normal, ofs, 0);
			normal.xyz += normalize(nr.xyz * 2.0 - 1.0);
			float roughness = normal.w;
			if (roughness > 0.5) {
				roughness = 1.0 - roughness;
			}
			roughness /= (127.0 / 255.0);
			normal.w += roughness;

			if (sc_multiview) {
				// we're doing a full unproject so we need the value as is.
				depth += d;
			} else {
				d = (params.proj_zw[1][0] - params.proj_zw[1][1] * d) / (params.proj_zw[0][1] * d - params.proj_zw[0][0]);
				depth += d;
			}
		}

		color /= 4.0;
		depth /= 4.0;
		normal.xyz = normalize(normal.xyz / 4.0) * 0.5 + 0.5;
		normal.w /= 4.0;
		normal.w = normal.w * (127.0 / 255.0);
	} else {
		ivec2 ofs = ssC << 1;

		color = texelFetch(source_ssr, ofs, 0);
		depth = texelFetch(source_depth, ofs, 0).r;
		normal = texelFetch(source_normal, ofs, 0);

		if (!sc_multiview) {
			// unproject our Z value so we can use it directly.
			depth = (params.proj_zw[1][0] - params.proj_zw[1][1] * depth) / (params.proj_zw[0][1] * depth - params.proj_zw[0][0]);
		}
	}

	imageStore(dest_ssr, ssC, color);
	imageStore(dest_depth, ssC, vec4(depth));
	imageStore(dest_normal, ssC, normal);
}

// Simplified versions of light functions intended for the vertex shader.

// Converts GGX roughness to Blinn-Phong shininess.
// Roughly approximates `2.0 / roughness * roughness - 2.0` with a much lower high end.
// Range is ~[0.05,656].
mediump float roughness_to_shininess(mediump float roughness) {
	mediump float s = 1.5 - roughness * 0.667;
	s *= s;
	s *= s;
	s *= s;
	s *= s; // pow(s, 16)
	return s;
}

void light_compute_vertex(vec3 N, vec3 L, vec3 V, float A, vec3 light_color, bool is_directional, float roughness, float specular_amount,
		inout vec3 diffuse_light, inout vec3 specular_light) {
	float NdotL = min(A + dot(N, L), 1.0);
	float cNdotL = max(NdotL, 0.0);

#if defined(DIFFUSE_LAMBERT_WRAP)
	// Energy conserving lambert wrap shader.
	// https://web.archive.org/web/20210228210901/http://blog.stevemcauley.com/2011/12/03/energy-conserving-wrapped-diffuse/
	float diffuse_brdf_NL = max(0.0, (NdotL + roughness) / ((1.0 + roughness) * (1.0 + roughness))) * (1.0 / M_PI);
#else
	// lambert
	float diffuse_brdf_NL = cNdotL * (1.0 / M_PI);
#endif

	diffuse_light += light_color * diffuse_brdf_NL;

#if !defined(SPECULAR_DISABLED)
	float specular_brdf_NL = 0.0;
	// Normalized blinn always unless disabled.
	vec3 H = normalize(V + L);
	float cNdotH = clamp(A + dot(N, H), 0.0, 1.0);
	float shininess = roughness_to_shininess(roughness);
	float blinn = pow(cNdotH, shininess);
	blinn *= (shininess + 8.0) * (1.0 / (8.0 * M_PI)) * cNdotL * specular_amount;
	specular_brdf_NL = blinn;
	specular_light += specular_brdf_NL * light_color;
#endif
}

float get_omni_attenuation(float distance, float inv_range, float decay) {
	float nd = distance * inv_range;
	nd *= nd;
	nd *= nd; // nd^4
	nd = max(1.0 - nd, 0.0);
	nd *= nd; // nd^2
	return nd * pow(max(distance, 0.0001), -decay);
}

void light_process_omni_vertex(uint idx, vec3 vertex, vec3 eye_vec, vec3 normal, float roughness,
		inout vec3 diffuse_light, inout vec3 specular_light) {
	vec3 light_rel_vec = omni_lights.data[idx].position - vertex;
	float light_length = length(light_rel_vec);
	float omni_attenuation = get_omni_attenuation(light_length, omni_lights.data[idx].inv_radius, omni_lights.data[idx].attenuation);
	vec3 color = omni_lights.data[idx].color * omni_attenuation;

	// Compute area light solid angle.
	float size_A = 0.0;
	if (sc_use_light_soft_shadows() && omni_lights.data[idx].size > 0.0) {
		float t = omni_lights.data[idx].size / max(0.001, light_length);
		size_A = max(0.0, 1.0 - 1.0 / sqrt(1.0 + t * t));
	}

	light_compute_vertex(normal, normalize(light_rel_vec), eye_vec, size_A, color, false, roughness, omni_lights.data[idx].specular_amount,
			diffuse_light,
			specular_light);
}

void light_process_spot_vertex(uint idx, vec3 vertex, vec3 eye_vec, vec3 normal, float roughness,
		inout vec3 diffuse_light,
		inout vec3 specular_light) {
	vec3 light_rel_vec = spot_lights.data[idx].position - vertex;
	float light_length = length(light_rel_vec);
	float spot_attenuation = get_omni_attenuation(light_length, spot_lights.data[idx].inv_radius, spot_lights.data[idx].attenuation);
	vec3 spot_dir = spot_lights.data[idx].direction;

	// This conversion to a highp float is crucial to prevent light leaking
	// due to precision errors in the following calculations (cone angle is mediump).
	highp float cone_angle = spot_lights.data[idx].cone_angle;
	float scos = max(dot(-normalize(light_rel_vec), spot_dir), cone_angle);
	float spot_rim = max(0.0001, (1.0 - scos) / (1.0 - cone_angle));

	spot_attenuation *= 1.0 - pow(spot_rim, spot_lights.data[idx].cone_attenuation);
	vec3 color = spot_lights.data[idx].color * spot_attenuation;
	float specular_amount = spot_lights.data[idx].specular_amount;

	// Compute area light solid angle.
	float size_A = 0.0;
	if (sc_use_light_soft_shadows() && spot_lights.data[idx].size > 0.0) {
		float t = spot_lights.data[idx].size / max(0.001, light_length);
		size_A = max(0.0, 1.0 - 1.0 / sqrt(1.0 + t * t));
	}

	light_compute_vertex(normal, normalize(light_rel_vec), eye_vec, size_A, color, false, roughness, spot_lights.data[idx].specular_amount,
			diffuse_light, specular_light);
}

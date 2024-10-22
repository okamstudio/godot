#[compute]

#version 450

#VERSION_DEFINES

/* Do not use subgroups here, seems there is not much advantage and causes glitches
#if defined(has_GL_KHR_shader_subgroup_ballot) && defined(has_GL_KHR_shader_subgroup_arithmetic)
#extension GL_KHR_shader_subgroup_ballot: enable
#extension GL_KHR_shader_subgroup_arithmetic: enable

#define USE_SUBGROUPS
#endif
*/

#ifdef MODE_DENSITY
layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;
#else
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
#endif

#include "../cluster_data_inc.glsl"
#include "../light_data_inc.glsl"

#define M_PI 3.14159265359

#define DENSITY_SCALE 1024.0

layout(set = 0, binding = 1) uniform texture2D shadow_atlas;
layout(set = 0, binding = 2) uniform texture2D directional_shadow_atlas;

layout(set = 0, binding = 3, std430) restrict readonly buffer OmniLights {
	LightData data[];
}
omni_lights;

layout(set = 0, binding = 4, std430) restrict readonly buffer SpotLights {
	LightData data[];
}
spot_lights;

layout(set = 0, binding = 5, std140) uniform DirectionalLights {
	DirectionalLightData data[MAX_DIRECTIONAL_LIGHT_DATA_STRUCTS];
}
directional_lights;

layout(set = 0, binding = 6, std430) buffer restrict readonly ClusterBuffer {
	uint data[];
}
cluster_buffer;

layout(set = 0, binding = 7) uniform sampler linear_sampler;

#ifdef MODE_DENSITY
layout(rgba16f, set = 0, binding = 8) uniform restrict writeonly image3D density_map;
#endif

#ifdef MODE_FOG
layout(rgba16f, set = 0, binding = 8) uniform restrict readonly image3D density_map;
layout(rgba16f, set = 0, binding = 9) uniform restrict writeonly image3D fog_map;
#endif

#ifdef MODE_COPY
layout(rgba16f, set = 0, binding = 8) uniform restrict readonly image3D source_map;
layout(rgba16f, set = 0, binding = 9) uniform restrict writeonly image3D dest_map;
#endif

#ifdef MODE_FILTER
layout(rgba16f, set = 0, binding = 8) uniform restrict readonly image3D source_map;
layout(rgba16f, set = 0, binding = 9) uniform restrict writeonly image3D dest_map;
#endif

layout(set = 0, binding = 10) uniform sampler shadow_sampler;

#define MAX_VOXEL_GI_INSTANCES 8

struct VoxelGIData {
	mat4 xform; // 64 - 64

	vec3 bounds; // 12 - 76
	float dynamic_range; // 4 - 80

	float bias; // 4 - 84
	float normal_bias; // 4 - 88
	bool blend_ambient; // 4 - 92
	uint mipmaps; // 4 - 96

	vec3 pad; // 12 - 108
	float exposure_normalization; // 4 - 112
};

layout(set = 0, binding = 11, std140) uniform VoxelGIs {
	VoxelGIData data[MAX_VOXEL_GI_INSTANCES];
}
voxel_gi_instances;

layout(set = 0, binding = 12) uniform texture3D voxel_gi_textures[MAX_VOXEL_GI_INSTANCES];

layout(set = 0, binding = 13) uniform sampler linear_sampler_with_mipmaps;

#ifdef ENABLE_SDFGI

// SDFGI Integration on set 1
#define SDFGI_MAX_CASCADES 8

struct SDFVoxelGICascadeData {
	vec3 position;
	float to_probe;
	ivec3 probe_world_offset;
	float to_cell; // 1/bounds * grid_size
	vec3 pad;
	float exposure_normalization;
};

layout(set = 1, binding = 0, std140) uniform SDFGI {
	vec3 grid_size;
	uint max_cascades;

	bool use_occlusion;
	int probe_axis_size;
	float probe_to_uvw;
	float normal_bias;

	vec3 lightprobe_tex_pixel_size;
	float energy;

	vec3 lightprobe_uv_offset;
	float y_mult;

	vec3 occlusion_clamp;
	uint pad3;

	vec3 occlusion_renormalize;
	uint pad4;

	vec3 cascade_probe_size;
	uint pad5;

	SDFVoxelGICascadeData cascades[SDFGI_MAX_CASCADES];
}
sdfgi;

layout(set = 1, binding = 1) uniform texture2DArray sdfgi_ambient_texture;

layout(set = 1, binding = 2) uniform texture3D sdfgi_occlusion_texture;

#endif //SDFGI

layout(set = 0, binding = 14, std140) uniform Params {
	vec2 fog_frustum_size_begin;
	vec2 fog_frustum_size_end;

	float fog_frustum_end;
	float ambient_inject;
	float z_far;
	int filter_axis;

	vec3 ambient_color;
	float sky_contribution;

	ivec3 fog_volume_size;
	uint directional_light_count;

	vec3 base_emission;
	float base_density;

	vec3 base_scattering;
	float phase_g;

	float detail_spread;
	float gi_inject;
	uint max_voxel_gi_instances;
	uint cluster_type_size;

	vec2 screen_size;
	uint cluster_shift;
	uint cluster_width;

	uint max_cluster_element_count_div_32;
	bool use_temporal_reprojection;
	uint temporal_frame;
	float temporal_blend;

	mat3x4 cam_rotation;
	mat4 to_prev_view;

	mat3 radiance_inverse_xform;
}
params;
#ifndef MODE_COPY
layout(set = 0, binding = 15) uniform texture3D prev_density_texture;

#ifdef NO_IMAGE_ATOMICS
layout(set = 0, binding = 16) buffer density_only_map_buffer {
	uint density_only_map[];
};
layout(set = 0, binding = 17) buffer light_only_map_buffer {
	uint light_only_map[];
};
layout(set = 0, binding = 18) buffer emissive_only_map_buffer {
	uint emissive_only_map[];
};
#else
layout(r32ui, set = 0, binding = 16) uniform uimage3D density_only_map;
layout(r32ui, set = 0, binding = 17) uniform uimage3D light_only_map;
layout(r32ui, set = 0, binding = 18) uniform uimage3D emissive_only_map;
#endif

#ifdef USE_RADIANCE_CUBEMAP_ARRAY
layout(set = 0, binding = 19) uniform textureCubeArray sky_texture;
#else
layout(set = 0, binding = 19) uniform textureCube sky_texture;
#endif
#endif // MODE_COPY

float get_depth_at_pos(float cell_depth_size, int z) {
	float d = float(z) * cell_depth_size + cell_depth_size * 0.5; //center of voxels
	d = pow(d, params.detail_spread);
	return params.fog_frustum_end * d;
}

vec3 hash3f(uvec3 x) {
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = (x >> 16) ^ x;
	return vec3(x & 0xFFFFF) / vec3(float(0xFFFFF));
}

float get_omni_attenuation(float dist, float inv_range, float decay) {
	float nd = dist * inv_range;
	nd *= nd;
	nd *= nd; // nd^4
	nd = max(1.0 - nd, 0.0);
	nd *= nd; // nd^2
	return nd * pow(max(dist, 0.0001), -decay);
}

void cluster_get_item_range(uint p_offset, out uint item_min, out uint item_max, out uint item_from, out uint item_to) {
	uint item_min_max = cluster_buffer.data[p_offset];
	item_min = item_min_max & 0xFFFF;
	item_max = item_min_max >> 16;

	item_from = item_min >> 5;
	item_to = (item_max == 0) ? 0 : ((item_max - 1) >> 5) + 1; //side effect of how it is stored, as item_max 0 means no elements
}

uint cluster_get_range_clip_mask(uint i, uint z_min, uint z_max) {
	int local_min = clamp(int(z_min) - int(i) * 32, 0, 31);
	int mask_width = min(int(z_max) - int(z_min), 32 - local_min);
	return bitfieldInsert(uint(0), uint(0xFFFFFFFF), local_min, mask_width);
}

float henyey_greenstein(float cos_theta, float g) {
	const float k = 0.0795774715459; // 1 / (4 * PI)
	return k * (1.0 - g * g) / (pow(1.0 + g * g - 2.0 * g * cos_theta, 1.5));
}

#define TEMPORAL_FRAMES 16

const vec3 halton_map[TEMPORAL_FRAMES] = vec3[](
		vec3(0.5, 0.33333333, 0.2),
		vec3(0.25, 0.66666667, 0.4),
		vec3(0.75, 0.11111111, 0.6),
		vec3(0.125, 0.44444444, 0.8),
		vec3(0.625, 0.77777778, 0.04),
		vec3(0.375, 0.22222222, 0.24),
		vec3(0.875, 0.55555556, 0.44),
		vec3(0.0625, 0.88888889, 0.64),
		vec3(0.5625, 0.03703704, 0.84),
		vec3(0.3125, 0.37037037, 0.08),
		vec3(0.8125, 0.7037037, 0.28),
		vec3(0.1875, 0.14814815, 0.48),
		vec3(0.6875, 0.48148148, 0.68),
		vec3(0.4375, 0.81481481, 0.88),
		vec3(0.9375, 0.25925926, 0.12),
		vec3(0.03125, 0.59259259, 0.32));

// Higher values will make light in volumetric fog fade out sooner when it's occluded by shadow.
const float INV_FOG_FADE = 10.0;

void main() {
	vec3 fog_cell_size = 1.0 / vec3(params.fog_volume_size);

#ifdef MODE_DENSITY

	ivec3 pos = ivec3(gl_GlobalInvocationID.xyz);
	if (any(greaterThanEqual(pos, params.fog_volume_size))) {
		return; //do not compute
	}
#ifdef NO_IMAGE_ATOMICS
	uint lpos = pos.z * params.fog_volume_size.x * params.fog_volume_size.y + pos.y * params.fog_volume_size.x + pos.x;
#endif

	vec3 posf = vec3(pos);

	//posf += mix(vec3(0.0),vec3(1.0),0.3) * hash3f(uvec3(pos)) * 2.0 - 1.0;

	vec3 fog_unit_pos = posf * fog_cell_size + fog_cell_size * 0.5; //center of voxels

	uvec2 screen_pos = uvec2(fog_unit_pos.xy * params.screen_size);
	uvec2 cluster_pos = screen_pos >> params.cluster_shift;
	uint cluster_offset = (params.cluster_width * cluster_pos.y + cluster_pos.x) * (params.max_cluster_element_count_div_32 + 32);
	//positions in screen are too spread apart, no hopes for optimizing with subgroups

	fog_unit_pos.z = pow(fog_unit_pos.z, params.detail_spread);

	vec3 view_pos;
	view_pos.xy = (fog_unit_pos.xy * 2.0 - 1.0) * mix(params.fog_frustum_size_begin, params.fog_frustum_size_end, vec2(fog_unit_pos.z));
	view_pos.z = -params.fog_frustum_end * fog_unit_pos.z;
	view_pos.y = -view_pos.y;

	vec4 reprojected_density = vec4(0.0);
	float reproject_amount = 0.0;

	if (params.use_temporal_reprojection) {
		vec3 prev_view = (params.to_prev_view * vec4(view_pos, 1.0)).xyz;
		//undo transform into prev view
		prev_view.y = -prev_view.y;
		//z back to unit size
		prev_view.z /= -params.fog_frustum_end;
		//xy back to unit size
		prev_view.xy /= mix(params.fog_frustum_size_begin, params.fog_frustum_size_end, vec2(prev_view.z));
		prev_view.xy = prev_view.xy * 0.5 + 0.5;
		//z back to unspread value
		prev_view.z = pow(prev_view.z, 1.0 / params.detail_spread);

		if (all(greaterThan(prev_view, vec3(0.0))) && all(lessThan(prev_view, vec3(1.0)))) {
			//reprojectinon fits

			reprojected_density = textureLod(sampler3D(prev_density_texture, linear_sampler), prev_view, 0.0);
			reproject_amount = params.temporal_blend;

			// Since we can reproject, now we must jitter the current view pos.
			// This is done here because cells that can't reproject should not jitter.

			fog_unit_pos = posf * fog_cell_size + fog_cell_size * halton_map[params.temporal_frame]; //center of voxels, offset by halton table

			screen_pos = uvec2(fog_unit_pos.xy * params.screen_size);
			cluster_pos = screen_pos >> params.cluster_shift;
			cluster_offset = (params.cluster_width * cluster_pos.y + cluster_pos.x) * (params.max_cluster_element_count_div_32 + 32);
			//positions in screen are too spread apart, no hopes for optimizing with subgroups

			fog_unit_pos.z = pow(fog_unit_pos.z, params.detail_spread);

			view_pos.xy = (fog_unit_pos.xy * 2.0 - 1.0) * mix(params.fog_frustum_size_begin, params.fog_frustum_size_end, vec2(fog_unit_pos.z));
			view_pos.z = -params.fog_frustum_end * fog_unit_pos.z;
			view_pos.y = -view_pos.y;
		}
	}

	uint cluster_z = uint(clamp((abs(view_pos.z) / params.z_far) * 32.0, 0.0, 31.0));

	vec3 total_light = vec3(0.0);

	float total_density = params.base_density;
#ifdef NO_IMAGE_ATOMICS
	uint local_density = density_only_map[lpos];
#else
	uint local_density = imageLoad(density_only_map, pos).x;
#endif

	total_density += float(int(local_density)) / DENSITY_SCALE;
	total_density = max(0.0, total_density);

#ifdef NO_IMAGE_ATOMICS
	uint scattering_u = light_only_map[lpos];
#else
	uint scattering_u = imageLoad(light_only_map, pos).x;
#endif
	vec3 scattering = vec3(scattering_u >> 21, (scattering_u << 11) >> 21, scattering_u % 1024) / vec3(2047.0, 2047.0, 1023.0);
	scattering += params.base_scattering * params.base_density;

#ifdef NO_IMAGE_ATOMICS
	uint emission_u = emissive_only_map[lpos];
#else
	uint emission_u = imageLoad(emissive_only_map, pos).x;
#endif
	vec3 emission = vec3(emission_u >> 21, (emission_u << 11) >> 21, emission_u % 1024) / vec3(511.0, 511.0, 255.0);
	emission += params.base_emission * params.base_density;

	float cell_depth_size = abs(view_pos.z - get_depth_at_pos(fog_cell_size.z, pos.z + 1));
	//compute directional lights

	if (total_density > 0.00005) {
		for (uint i = 0; i < params.directional_light_count; i++) {
			if (directional_lights.data[i].volumetric_fog_energy > 0.001) {
				vec3 shadow_attenuation = vec3(1.0);

				if (directional_lights.data[i].shadow_opacity > 0.001) {
					float depth_z = -view_pos.z;

					vec4 pssm_coord;
					vec3 light_dir = directional_lights.data[i].direction;
					vec4 v = vec4(view_pos, 1.0);
					float z_range;

					if (depth_z < directional_lights.data[i].shadow_split_offsets.x) {
						pssm_coord = (directional_lights.data[i].shadow_matrix1 * v);
						pssm_coord /= pssm_coord.w;
						z_range = directional_lights.data[i].shadow_z_range.x;

					} else if (depth_z < directional_lights.data[i].shadow_split_offsets.y) {
						pssm_coord = (directional_lights.data[i].shadow_matrix2 * v);
						pssm_coord /= pssm_coord.w;
						z_range = directional_lights.data[i].shadow_z_range.y;

					} else if (depth_z < directional_lights.data[i].shadow_split_offsets.z) {
						pssm_coord = (directional_lights.data[i].shadow_matrix3 * v);
						pssm_coord /= pssm_coord.w;
						z_range = directional_lights.data[i].shadow_z_range.z;

					} else {
						pssm_coord = (directional_lights.data[i].shadow_matrix4 * v);
						pssm_coord /= pssm_coord.w;
						z_range = directional_lights.data[i].shadow_z_range.w;
					}

					float depth = texture(sampler2D(directional_shadow_atlas, linear_sampler), pssm_coord.xy).r;
					float shadow = exp(min(0.0, (pssm_coord.z - depth)) * z_range * INV_FOG_FADE);

					shadow = mix(shadow, 1.0, smoothstep(directional_lights.data[i].fade_from, directional_lights.data[i].fade_to, view_pos.z)); //done with negative values for performance

					shadow_attenuation = mix(vec3(1.0 - directional_lights.data[i].shadow_opacity), vec3(1.0), shadow);
				}

				total_light += shadow_attenuation * directional_lights.data[i].color * directional_lights.data[i].energy * henyey_greenstein(dot(normalize(view_pos), normalize(directional_lights.data[i].direction)), params.phase_g) * directional_lights.data[i].volumetric_fog_energy;
			}
		}

		// Compute light from sky
		if (params.ambient_inject > 0.0) {
			vec3 isotropic = vec3(0.0);
			vec3 anisotropic = vec3(0.0);
			if (params.sky_contribution > 0.0) {
				float mip_bias = 2.0 + total_density * (MAX_SKY_LOD - 2.0); // Not physically based, but looks nice
				vec3 scatter_direction = (params.radiance_inverse_xform * normalize(view_pos)) * sign(params.phase_g);
#ifdef USE_RADIANCE_CUBEMAP_ARRAY
				isotropic = texture(samplerCubeArray(sky_texture, linear_sampler_with_mipmaps), vec4(0.0, 1.0, 0.0, mip_bias)).rgb;
				anisotropic = texture(samplerCubeArray(sky_texture, linear_sampler_with_mipmaps), vec4(scatter_direction, mip_bias)).rgb;
#else
				isotropic = textureLod(samplerCube(sky_texture, linear_sampler_with_mipmaps), vec3(0.0, 1.0, 0.0), mip_bias).rgb;
				anisotropic = textureLod(samplerCube(sky_texture, linear_sampler_with_mipmaps), vec3(scatter_direction), mip_bias).rgb;
#endif //USE_RADIANCE_CUBEMAP_ARRAY
			}

			total_light += mix(params.ambient_color, mix(isotropic, anisotropic, abs(params.phase_g)), params.sky_contribution) * params.ambient_inject;
		}

		//compute lights from cluster

		{ //omni lights

			uint cluster_omni_offset = cluster_offset;

			uint item_min;
			uint item_max;
			uint item_from;
			uint item_to;

			cluster_get_item_range(cluster_omni_offset + params.max_cluster_element_count_div_32 + cluster_z, item_min, item_max, item_from, item_to);

#ifdef USE_SUBGROUPS
			item_from = subgroupBroadcastFirst(subgroupMin(item_from));
			item_to = subgroupBroadcastFirst(subgroupMax(item_to));
#endif

			for (uint i = item_from; i < item_to; i++) {
				uint mask = cluster_buffer.data[cluster_omni_offset + i];
				mask &= cluster_get_range_clip_mask(i, item_min, item_max);
#ifdef USE_SUBGROUPS
				uint merged_mask = subgroupBroadcastFirst(subgroupOr(mask));
#else
				uint merged_mask = mask;
#endif

				while (merged_mask != 0) {
					uint bit = findMSB(merged_mask);
					merged_mask &= ~(1 << bit);
#ifdef USE_SUBGROUPS
					if (((1 << bit) & mask) == 0) { //do not process if not originally here
						continue;
					}
#endif
					uint light_index = 32 * i + bit;

					//if (!bool(omni_omni_lights.data[light_index].mask & draw_call.layer_mask)) {
					//	continue; //not masked
					//}

					vec3 light_pos = omni_lights.data[light_index].position;
					float d = distance(omni_lights.data[light_index].position, view_pos);
					float shadow_attenuation = 1.0;

					if (omni_lights.data[light_index].volumetric_fog_energy > 0.001 && d * omni_lights.data[light_index].inv_radius < 1.0) {
						float attenuation = get_omni_attenuation(d, omni_lights.data[light_index].inv_radius, omni_lights.data[light_index].attenuation);

						vec3 light = omni_lights.data[light_index].color;

						if (omni_lights.data[light_index].shadow_opacity > 0.001) {
							//has shadow
							vec4 uv_rect = omni_lights.data[light_index].atlas_rect;
							vec2 flip_offset = omni_lights.data[light_index].direction.xy;

							vec3 local_vert = (omni_lights.data[light_index].shadow_matrix * vec4(view_pos, 1.0)).xyz;

							float shadow_len = length(local_vert); //need to remember shadow len from here
							vec3 shadow_sample = normalize(local_vert);

							if (shadow_sample.z >= 0.0) {
								uv_rect.xy += flip_offset;
							}

							shadow_sample.z = 1.0 + abs(shadow_sample.z);
							vec3 pos = vec3(shadow_sample.xy / shadow_sample.z, shadow_len - omni_lights.data[light_index].shadow_bias);
							pos.z *= omni_lights.data[light_index].inv_radius;
							pos.z = 1.0 - pos.z;

							pos.xy = pos.xy * 0.5 + 0.5;
							pos.xy = uv_rect.xy + pos.xy * uv_rect.zw;

							float depth = texture(sampler2D(shadow_atlas, linear_sampler), pos.xy).r;

							shadow_attenuation = mix(1.0 - omni_lights.data[light_index].shadow_opacity, 1.0, exp(min(0.0, (pos.z - depth)) / omni_lights.data[light_index].inv_radius * INV_FOG_FADE));
						}
						total_light += light * attenuation * shadow_attenuation * henyey_greenstein(dot(normalize(light_pos - view_pos), normalize(view_pos)), params.phase_g) * omni_lights.data[light_index].volumetric_fog_energy;
					}
				}
			}
		}

		{ //spot lights

			uint cluster_spot_offset = cluster_offset + params.cluster_type_size;

			uint item_min;
			uint item_max;
			uint item_from;
			uint item_to;

			cluster_get_item_range(cluster_spot_offset + params.max_cluster_element_count_div_32 + cluster_z, item_min, item_max, item_from, item_to);

#ifdef USE_SUBGROUPS
			item_from = subgroupBroadcastFirst(subgroupMin(item_from));
			item_to = subgroupBroadcastFirst(subgroupMax(item_to));
#endif

			for (uint i = item_from; i < item_to; i++) {
				uint mask = cluster_buffer.data[cluster_spot_offset + i];
				mask &= cluster_get_range_clip_mask(i, item_min, item_max);
#ifdef USE_SUBGROUPS
				uint merged_mask = subgroupBroadcastFirst(subgroupOr(mask));
#else
				uint merged_mask = mask;
#endif

				while (merged_mask != 0) {
					uint bit = findMSB(merged_mask);
					merged_mask &= ~(1 << bit);
#ifdef USE_SUBGROUPS
					if (((1 << bit) & mask) == 0) { //do not process if not originally here
						continue;
					}
#endif

					//if (!bool(omni_lights.data[light_index].mask & draw_call.layer_mask)) {
					//	continue; //not masked
					//}

					uint light_index = 32 * i + bit;

					vec3 light_pos = spot_lights.data[light_index].position;
					vec3 light_rel_vec = spot_lights.data[light_index].position - view_pos;
					float d = length(light_rel_vec);
					float shadow_attenuation = 1.0;

					if (spot_lights.data[light_index].volumetric_fog_energy > 0.001 && d * spot_lights.data[light_index].inv_radius < 1.0) {
						float attenuation = get_omni_attenuation(d, spot_lights.data[light_index].inv_radius, spot_lights.data[light_index].attenuation);

						vec3 spot_dir = spot_lights.data[light_index].direction;
						highp float cone_angle = spot_lights.data[light_index].cone_angle;
						float scos = max(dot(-normalize(light_rel_vec), spot_dir), cone_angle);
						float spot_rim = max(0.0001, (1.0 - scos) / (1.0 - cone_angle));
						attenuation *= 1.0 - pow(spot_rim, spot_lights.data[light_index].cone_attenuation);

						vec3 light = spot_lights.data[light_index].color;

						if (spot_lights.data[light_index].shadow_opacity > 0.001) {
							//has shadow
							vec4 uv_rect = spot_lights.data[light_index].atlas_rect;

							vec4 v = vec4(view_pos, 1.0);

							vec4 splane = (spot_lights.data[light_index].shadow_matrix * v);
							splane.z -= spot_lights.data[light_index].shadow_bias / (d * spot_lights.data[light_index].inv_radius);
							splane /= splane.w;

							vec3 pos = vec3(splane.xy * spot_lights.data[light_index].atlas_rect.zw + spot_lights.data[light_index].atlas_rect.xy, splane.z);

							float depth = texture(sampler2D(shadow_atlas, linear_sampler), pos.xy).r;

							shadow_attenuation = mix(1.0 - spot_lights.data[light_index].shadow_opacity, 1.0, exp(min(0.0, (pos.z - depth)) / spot_lights.data[light_index].inv_radius * INV_FOG_FADE));
						}
						total_light += light * attenuation * shadow_attenuation * henyey_greenstein(dot(normalize(light_rel_vec), normalize(view_pos)), params.phase_g) * spot_lights.data[light_index].volumetric_fog_energy;
					}
				}
			}
		}

		vec3 world_pos = mat3(params.cam_rotation) * view_pos;

		for (uint i = 0; i < params.max_voxel_gi_instances; i++) {
			vec3 position = (voxel_gi_instances.data[i].xform * vec4(world_pos, 1.0)).xyz;

			//this causes corrupted pixels, i have no idea why..
			if (all(bvec2(all(greaterThanEqual(position, vec3(0.0))), all(lessThan(position, voxel_gi_instances.data[i].bounds))))) {
				position /= voxel_gi_instances.data[i].bounds;

				vec4 light = vec4(0.0);
				for (uint j = 0; j < voxel_gi_instances.data[i].mipmaps; j++) {
					vec4 slight = textureLod(sampler3D(voxel_gi_textures[i], linear_sampler_with_mipmaps), position, float(j));
					float a = (1.0 - light.a);
					light += a * slight;
				}

				light.rgb *= voxel_gi_instances.data[i].dynamic_range * params.gi_inject * voxel_gi_instances.data[i].exposure_normalization;

				total_light += light.rgb;
			}
		}

		//sdfgi
#ifdef ENABLE_SDFGI

		{
			float blend = -1.0;
			vec3 ambient_total = vec3(0.0);

			for (uint i = 0; i < sdfgi.max_cascades; i++) {
				vec3 cascade_pos = (world_pos - sdfgi.cascades[i].position) * sdfgi.cascades[i].to_probe;

				if (any(lessThan(cascade_pos, vec3(0.0))) || any(greaterThanEqual(cascade_pos, sdfgi.cascade_probe_size))) {
					continue; //skip cascade
				}

				vec3 base_pos = floor(cascade_pos);
				ivec3 probe_base_pos = ivec3(base_pos);

				vec4 ambient_accum = vec4(0.0);

				ivec3 tex_pos = ivec3(probe_base_pos.xy, int(i));
				tex_pos.x += probe_base_pos.z * sdfgi.probe_axis_size;

				for (uint j = 0; j < 8; j++) {
					ivec3 offset = (ivec3(j) >> ivec3(0, 1, 2)) & ivec3(1, 1, 1);
					ivec3 probe_posi = probe_base_pos;
					probe_posi += offset;

					// Compute weight

					vec3 probe_pos = vec3(probe_posi);
					vec3 probe_to_pos = cascade_pos - probe_pos;

					vec3 trilinear = vec3(1.0) - abs(probe_to_pos);
					float weight = trilinear.x * trilinear.y * trilinear.z;

					// Compute lightprobe occlusion

					if (sdfgi.use_occlusion) {
						ivec3 occ_indexv = abs((sdfgi.cascades[i].probe_world_offset + probe_posi) & ivec3(1, 1, 1)) * ivec3(1, 2, 4);
						vec4 occ_mask = mix(vec4(0.0), vec4(1.0), equal(ivec4(occ_indexv.x | occ_indexv.y), ivec4(0, 1, 2, 3)));

						vec3 occ_pos = clamp(cascade_pos, probe_pos - sdfgi.occlusion_clamp, probe_pos + sdfgi.occlusion_clamp) * sdfgi.probe_to_uvw;
						occ_pos.z += float(i);
						if (occ_indexv.z != 0) { //z bit is on, means index is >=4, so make it switch to the other half of textures
							occ_pos.x += 1.0;
						}

						occ_pos *= sdfgi.occlusion_renormalize;
						float occlusion = dot(textureLod(sampler3D(sdfgi_occlusion_texture, linear_sampler), occ_pos, 0.0), occ_mask);

						weight *= max(occlusion, 0.01);
					}

					// Compute ambient texture position

					ivec3 uvw = tex_pos;
					uvw.xy += offset.xy;
					uvw.x += offset.z * sdfgi.probe_axis_size;

					vec3 ambient = texelFetch(sampler2DArray(sdfgi_ambient_texture, linear_sampler), uvw, 0).rgb;

					ambient_accum.rgb += ambient * weight * sdfgi.cascades[i].exposure_normalization;
					ambient_accum.a += weight;
				}

				if (ambient_accum.a > 0) {
					ambient_accum.rgb /= ambient_accum.a;
				}
				ambient_total = ambient_accum.rgb;
				break;
			}

			total_light += ambient_total * params.gi_inject;
		}

#endif
	}

	vec4 final_density = vec4(total_light * scattering + emission, total_density);

	final_density = mix(final_density, reprojected_density, reproject_amount);

	imageStore(density_map, pos, final_density);
#ifdef NO_IMAGE_ATOMICS
	density_only_map[lpos] = 0;
	light_only_map[lpos] = 0;
	emissive_only_map[lpos] = 0;
#else
	imageStore(density_only_map, pos, uvec4(0));
	imageStore(light_only_map, pos, uvec4(0));
	imageStore(emissive_only_map, pos, uvec4(0));
#endif
#endif

#ifdef MODE_FOG

	ivec3 pos = ivec3(gl_GlobalInvocationID.xy, 0);

	if (any(greaterThanEqual(pos, params.fog_volume_size))) {
		return; //do not compute
	}

	vec4 fog_accum = vec4(0.0, 0.0, 0.0, 1.0);
	float prev_z = 0.0;

	for (int i = 0; i < params.fog_volume_size.z; i++) {
		//compute fog position
		ivec3 fog_pos = pos + ivec3(0, 0, i);
		//get fog value
		vec4 fog = imageLoad(density_map, fog_pos);

		//get depth at cell pos
		float z = get_depth_at_pos(fog_cell_size.z, i);
		//get distance from previous pos
		float d = abs(prev_z - z);
		//compute transmittance using beer's law
		float transmittance = exp(-d * fog.a);

		fog_accum.rgb += ((fog.rgb - fog.rgb * transmittance) / max(fog.a, 0.00001)) * fog_accum.a;
		fog_accum.a *= transmittance;

		prev_z = z;

		imageStore(fog_map, fog_pos, vec4(fog_accum.rgb, 1.0 - fog_accum.a));
	}

#endif

#ifdef MODE_FILTER

	ivec3 pos = ivec3(gl_GlobalInvocationID.xyz);

	const float gauss[7] = float[](0.071303, 0.131514, 0.189879, 0.214607, 0.189879, 0.131514, 0.071303);

	const ivec3 filter_dir[3] = ivec3[](ivec3(1, 0, 0), ivec3(0, 1, 0), ivec3(0, 0, 1));
	ivec3 offset = filter_dir[params.filter_axis];

	vec4 accum = vec4(0.0);
	for (int i = -3; i <= 3; i++) {
		accum += imageLoad(source_map, clamp(pos + offset * i, ivec3(0), params.fog_volume_size - ivec3(1))) * gauss[i + 3];
	}

	imageStore(dest_map, pos, accum);

#endif
#ifdef MODE_COPY
	ivec3 pos = ivec3(gl_GlobalInvocationID.xyz);
	if (any(greaterThanEqual(pos, params.fog_volume_size))) {
		return; //do not compute
	}

	imageStore(dest_map, pos, imageLoad(source_map, pos));

#endif
}

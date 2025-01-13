/**************************************************************************/
/*  particle_process_material.cpp                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "particle_process_material.h"

#include "core/version.h"

Mutex ParticleProcessMaterial::material_mutex;
SelfList<ParticleProcessMaterial>::List ParticleProcessMaterial::dirty_materials;
HashMap<ParticleProcessMaterial::MaterialKey, ParticleProcessMaterial::ShaderData, ParticleProcessMaterial::MaterialKey> ParticleProcessMaterial::shader_map;
RBSet<String> ParticleProcessMaterial::min_max_properties;
ParticleProcessMaterial::ShaderNames *ParticleProcessMaterial::shader_names = nullptr;

void ParticleProcessMaterial::init_shaders() {
	shader_names = memnew(ShaderNames);

	shader_names->direction = "direction";
	shader_names->spread = "spread";
	shader_names->flatness = "flatness";
	shader_names->initial_linear_velocity_min = "initial_linear_velocity_min";
	shader_names->initial_angle_min = "initial_angle_min";
	shader_names->angular_velocity_min = "angular_velocity_min";
	shader_names->orbit_velocity_min = "orbit_velocity_min";
	shader_names->radial_velocity_min = "radial_velocity_min";
	shader_names->linear_accel_min = "linear_accel_min";
	shader_names->radial_accel_min = "radial_accel_min";
	shader_names->tangent_accel_min = "tangent_accel_min";
	shader_names->damping_min = "damping_min";
	shader_names->scale_min = "scale_min";
	shader_names->hue_variation_min = "hue_variation_min";
	shader_names->anim_speed_min = "anim_speed_min";
	shader_names->anim_offset_min = "anim_offset_min";
	shader_names->directional_velocity_min = "directional_velocity_min";
	shader_names->scale_over_velocity_min = "scale_over_velocity_min";

	shader_names->initial_linear_velocity_max = "initial_linear_velocity_max";
	shader_names->initial_angle_max = "initial_angle_max";
	shader_names->angular_velocity_max = "angular_velocity_max";
	shader_names->orbit_velocity_max = "orbit_velocity_max";
	shader_names->radial_velocity_max = "radial_velocity_max";
	shader_names->linear_accel_max = "linear_accel_max";
	shader_names->radial_accel_max = "radial_accel_max";
	shader_names->tangent_accel_max = "tangent_accel_max";
	shader_names->damping_max = "damping_max";
	shader_names->scale_max = "scale_max";
	shader_names->hue_variation_max = "hue_variation_max";
	shader_names->anim_speed_max = "anim_speed_max";
	shader_names->anim_offset_max = "anim_offset_max";
	shader_names->directional_velocity_max = "directional_velocity_max";
	shader_names->scale_over_velocity_max = "scale_over_velocity_max";

	shader_names->angle_texture = "angle_texture";
	shader_names->angular_velocity_texture = "angular_velocity_texture";
	shader_names->orbit_velocity_texture = "orbit_velocity_curve";
	shader_names->radial_velocity_texture = "radial_velocity_curve";
	shader_names->linear_accel_texture = "linear_accel_texture";
	shader_names->radial_accel_texture = "radial_accel_texture";
	shader_names->tangent_accel_texture = "tangent_accel_texture";
	shader_names->damping_texture = "damping_texture";
	shader_names->scale_texture = "scale_curve";
	shader_names->hue_variation_texture = "hue_rot_curve";
	shader_names->anim_speed_texture = "animation_speed_curve";
	shader_names->anim_offset_texture = "animation_offset_curve";
	shader_names->directional_velocity_texture = "directional_velocity_curve";
	shader_names->scale_over_velocity_texture = "scale_over_velocity_curve";

	shader_names->color = "color_value";
	shader_names->color_ramp = "color_ramp";
	shader_names->alpha_ramp = "alpha_curve";
	shader_names->emission_ramp = "emission_curve";
	shader_names->color_initial_ramp = "color_initial_ramp";
	shader_names->velocity_limit_curve = "velocity_limit_curve";
	shader_names->inherit_emitter_velocity_ratio = "inherit_emitter_velocity_ratio";
	shader_names->velocity_pivot = "velocity_pivot";

	shader_names->emission_sphere_radius = "emission_sphere_radius";
	shader_names->emission_box_extents = "emission_box_extents";
	shader_names->emission_texture_point_count = "emission_texture_point_count";
	shader_names->emission_texture_points = "emission_texture_points";
	shader_names->emission_texture_normal = "emission_texture_normal";
	shader_names->emission_texture_color = "emission_texture_color";
	shader_names->emission_ring_axis = "emission_ring_axis";
	shader_names->emission_ring_height = "emission_ring_height";
	shader_names->emission_ring_radius = "emission_ring_radius";
	shader_names->emission_ring_inner_radius = "emission_ring_inner_radius";
	shader_names->emission_ring_cone_angle = "emission_ring_cone_angle";
	shader_names->emission_shape_offset = "emission_shape_offset";
	shader_names->emission_shape_scale = "emission_shape_scale";

	shader_names->turbulence_enabled = "turbulence_enabled";
	shader_names->turbulence_noise_strength = "turbulence_noise_strength";
	shader_names->turbulence_noise_scale = "turbulence_noise_scale";
	shader_names->turbulence_noise_speed = "turbulence_noise_speed";
	shader_names->turbulence_noise_speed_random = "turbulence_noise_speed_random";
	shader_names->turbulence_influence_over_life = "turbulence_influence_over_life";
	shader_names->turbulence_influence_min = "turbulence_influence_min";
	shader_names->turbulence_influence_max = "turbulence_influence_max";
	shader_names->turbulence_initial_displacement_min = "turbulence_initial_displacement_min";
	shader_names->turbulence_initial_displacement_max = "turbulence_initial_displacement_max";

	shader_names->gravity = "gravity";

	shader_names->lifetime_randomness = "lifetime_randomness";

	shader_names->sub_emitter_frequency = "sub_emitter_frequency";
	shader_names->sub_emitter_amount_at_end = "sub_emitter_amount_at_end";
	shader_names->sub_emitter_amount_at_collision = "sub_emitter_amount_at_collision";
	shader_names->sub_emitter_keep_velocity = "sub_emitter_keep_velocity";

	shader_names->collision_friction = "collision_friction";
	shader_names->collision_bounce = "collision_bounce";
}

void ParticleProcessMaterial::finish_shaders() {
	dirty_materials.clear();

	memdelete(shader_names);
	shader_names = nullptr;
}

void ParticleProcessMaterial::_update_shader() {
	MaterialKey mk = _compute_key();
	if (mk == current_key) {
		return; // No update required in the end.
	}

	if (shader_map.has(current_key)) {
		shader_map[current_key].users--;
		if (shader_map[current_key].users == 0) {
			// Deallocate shader, as it's no longer in use.
			RS::get_singleton()->free(shader_map[current_key].shader);
			shader_map.erase(current_key);
		}
	}

	current_key = mk;

	if (shader_map.has(mk)) {
		RS::get_singleton()->material_set_shader(_get_material(), shader_map[mk].shader);
		shader_map[mk].users++;
		return;
	}

	// No pre-existing shader, create one.

	// Add a comment to describe the shader origin (useful when converting to ShaderMaterial).
	String code = "// NOTE: Shader automatically converted from " VERSION_NAME " " VERSION_FULL_CONFIG "'s ParticleProcessMaterial.\n\n";

	code += "shader_type particles;\n";
	code += "render_mode disable_velocity;\n";
	if (collision_scale) {
		code += "render_mode collision_use_scale;\n";
	}
	code += "\n";

	// Define uniforms.

	code += "uniform vec3 direction;\n";
	code += "uniform float spread;\n";
	code += "uniform float flatness;\n";

	code += "uniform float inherit_emitter_velocity_ratio = 0.0;\n";

	code += "uniform float initial_linear_velocity_min;\n";
	code += "uniform float initial_linear_velocity_max;\n";

	code += "uniform float directional_velocity_min;\n";
	code += "uniform float directional_velocity_max;\n";

	code += "uniform float angular_velocity_min;\n";
	code += "uniform float angular_velocity_max;\n";

	code += "uniform float orbit_velocity_min;\n";
	code += "uniform float orbit_velocity_max;\n";

	code += "uniform float radial_velocity_min;\n";
	code += "uniform float radial_velocity_max;\n";

	code += "uniform float linear_accel_min;\n";
	code += "uniform float linear_accel_max;\n";

	code += "uniform float radial_accel_min;\n";
	code += "uniform float radial_accel_max;\n";

	code += "uniform float tangent_accel_min;\n";
	code += "uniform float tangent_accel_max;\n";

	code += "uniform float damping_min;\n";
	code += "uniform float damping_max;\n";

	code += "uniform float initial_angle_min;\n";
	code += "uniform float initial_angle_max;\n";

	code += "uniform float scale_min;\n";
	code += "uniform float scale_max;\n";

	code += "uniform float hue_variation_min;\n";
	code += "uniform float hue_variation_max;\n";

	code += "uniform float anim_speed_min;\n";
	code += "uniform float anim_speed_max;\n";

	code += "uniform float anim_offset_min;\n";
	code += "uniform float anim_offset_max;\n";

	code += "uniform float lifetime_randomness;\n";
	code += "uniform vec3 emission_shape_offset = vec3(0.0);\n";
	code += "uniform vec3 emission_shape_scale = vec3(1.0);\n";

	code += "uniform vec3 velocity_pivot = vec3(0.0);\n";

	if (tex_parameters[PARAM_SCALE_OVER_VELOCITY].is_valid()) {
		code += "uniform float scale_over_velocity_min = 0.0;\n";
		code += "uniform float scale_over_velocity_max = 5.0;\n";
	}

	switch (emission_shape) {
		case EMISSION_SHAPE_POINT: {
			//do none
		} break;
		case EMISSION_SHAPE_SPHERE: {
			code += "uniform float emission_sphere_radius;\n";
		} break;
		case EMISSION_SHAPE_SPHERE_SURFACE: {
			code += "uniform float emission_sphere_radius;\n";
		} break;
		case EMISSION_SHAPE_BOX: {
			code += "uniform vec3 emission_box_extents;\n";
		} break;
		case EMISSION_SHAPE_DIRECTED_POINTS: {
			code += "uniform sampler2D emission_texture_normal : hint_default_black;\n";
			[[fallthrough]];
		}
		case EMISSION_SHAPE_POINTS: {
			code += "uniform sampler2D emission_texture_points : hint_default_black;\n";
			code += "uniform int emission_texture_point_count;\n";
			if (emission_color_texture.is_valid()) {
				code += "uniform sampler2D emission_texture_color : hint_default_white;\n";
			}
		} break;
		case EMISSION_SHAPE_RING: {
			code += "uniform vec3 " + shader_names->emission_ring_axis + ";\n";
			code += "uniform float " + shader_names->emission_ring_height + ";\n";
			code += "uniform float " + shader_names->emission_ring_radius + ";\n";
			code += "uniform float " + shader_names->emission_ring_inner_radius + ";\n";
			code += "uniform float " + shader_names->emission_ring_cone_angle + ";\n";
		} break;
		case EMISSION_SHAPE_MAX: { // Max value for validity check.
			break;
		}
	}

	if (sub_emitter_mode != SUB_EMITTER_DISABLED && !RenderingServer::get_singleton()->is_low_end()) {
		if (sub_emitter_mode == SUB_EMITTER_CONSTANT) {
			code += "uniform float sub_emitter_frequency;\n";
		}
		if (sub_emitter_mode == SUB_EMITTER_AT_END) {
			code += "uniform int sub_emitter_amount_at_end;\n";
		}
		if (sub_emitter_mode == SUB_EMITTER_AT_COLLISION) {
			code += "uniform int sub_emitter_amount_at_collision;\n";
		}
		code += "uniform bool sub_emitter_keep_velocity;\n";
	}

	code += "uniform vec4 color_value : source_color;\n";

	code += "uniform vec3 gravity;\n";

	if (color_ramp.is_valid()) {
		code += "uniform sampler2D color_ramp : repeat_disable;\n";
	}

	if (color_initial_ramp.is_valid()) {
		code += "uniform sampler2D color_initial_ramp : repeat_disable;\n";
	}
	if (alpha_curve.is_valid()) {
		code += "uniform sampler2D alpha_curve : repeat_disable;\n";
	}
	if (emission_curve.is_valid()) {
		code += "uniform sampler2D emission_curve : repeat_disable;\n";
	}

	if (tex_parameters[PARAM_INITIAL_LINEAR_VELOCITY].is_valid()) {
		code += "uniform sampler2D linear_velocity_texture : repeat_disable;\n";
	}
	if (tex_parameters[PARAM_ORBIT_VELOCITY].is_valid()) {
		code += "uniform sampler2D orbit_velocity_curve : repeat_disable;\n";
	}
	if (tex_parameters[PARAM_ANGULAR_VELOCITY].is_valid()) {
		code += "uniform sampler2D angular_velocity_texture : repeat_disable;\n";
	}
	if (tex_parameters[PARAM_LINEAR_ACCEL].is_valid()) {
		code += "uniform sampler2D linear_accel_texture : repeat_disable;\n";
	}
	if (tex_parameters[PARAM_RADIAL_ACCEL].is_valid()) {
		code += "uniform sampler2D radial_accel_texture : repeat_disable;\n";
	}
	if (tex_parameters[PARAM_TANGENTIAL_ACCEL].is_valid()) {
		code += "uniform sampler2D tangent_accel_texture : repeat_disable;\n";
	}
	if (tex_parameters[PARAM_DAMPING].is_valid()) {
		code += "uniform sampler2D damping_texture : repeat_disable;\n";
	}
	if (tex_parameters[PARAM_ANGLE].is_valid()) {
		code += "uniform sampler2D angle_texture : repeat_disable;\n";
	}
	if (tex_parameters[PARAM_SCALE].is_valid()) {
		code += "uniform sampler2D scale_curve : repeat_disable;\n";
	}
	if (tex_parameters[PARAM_HUE_VARIATION].is_valid()) {
		code += "uniform sampler2D hue_rot_curve : repeat_disable;\n";
	}
	if (tex_parameters[PARAM_ANIM_SPEED].is_valid()) {
		code += "uniform sampler2D animation_speed_curve : repeat_disable;\n";
	}
	if (tex_parameters[PARAM_ANIM_OFFSET].is_valid()) {
		code += "uniform sampler2D animation_offset_curve : repeat_disable;\n";
	}
	if (tex_parameters[PARAM_RADIAL_VELOCITY].is_valid()) {
		code += "uniform sampler2D radial_velocity_curve : repeat_disable;\n";
	}
	if (tex_parameters[PARAM_SCALE_OVER_VELOCITY].is_valid()) {
		code += "uniform sampler2D scale_over_velocity_curve : repeat_disable;\n";
	}
	if (tex_parameters[PARAM_DIRECTIONAL_VELOCITY].is_valid()) {
		code += "uniform sampler2D directional_velocity_curve : repeat_disable;\n";
	}
	if (velocity_limit_curve.is_valid()) {
		code += "uniform sampler2D velocity_limit_curve : repeat_disable;\n";
	}

	if (collision_mode == COLLISION_RIGID) {
		code += "uniform float collision_friction;\n";
		code += "uniform float collision_bounce;\n";
	}

	if (turbulence_enabled) {
		code += "uniform float turbulence_noise_strength;\n";
		code += "uniform float turbulence_noise_scale;\n";
		code += "uniform float turbulence_influence_min;\n";
		code += "uniform float turbulence_influence_max;\n";
		code += "uniform float turbulence_initial_displacement_min;\n";
		code += "uniform float turbulence_initial_displacement_max;\n";
		code += "uniform float turbulence_noise_speed_random;\n";
		code += "uniform vec3 turbulence_noise_speed = vec3(1.0, 1.0, 1.0);\n";
		if (tex_parameters[PARAM_TURB_INFLUENCE_OVER_LIFE].is_valid()) {
			code += "uniform sampler2D turbulence_influence_over_life;\n";
		}
		if (turbulence_color_ramp.is_valid()) {
			code += "uniform sampler2D turbulence_color_ramp;\n";
		}
	}

	code += "\n";

	// Define helper functions.

	if (turbulence_enabled) {
		// Functions for 3D noise / turbulence.
		code += "// Functions for 3D noise / turbulence.\n";
		code += "vec4 grad(vec4 p) {\n";
		code += "	p = fract(vec4(\n";
		code += "			dot(p, vec4(0.143081, 0.001724, 0.280166, 0.262771)),\n";
		code += "			dot(p, vec4(0.645401, -0.047791, -0.146698, 0.595016)),\n";
		code += "			dot(p, vec4(-0.499665, -0.095734, 0.425674, -0.207367)),\n";
		code += "			dot(p, vec4(-0.013596, -0.848588, 0.423736, 0.17044))));\n";
		code += "	return fract((p.xyzw * p.yzwx) * 2365.952041) * 2.0 - 1.0;\n";
		code += "}\n\n";

		code += "float noise(vec4 coord) {\n";
		code += "	// Domain rotation to improve the look of XYZ slices + animation patterns.\n";
		code += "	coord = vec4(\n";
		code += "			coord.xyz + dot(coord, vec4(vec3(-0.1666667), -0.5)),\n";
		code += "			dot(coord, vec4(0.5)));\n\n";
		code += "	vec4 base = floor(coord), delta = coord - base;\n\n";
		code += "	vec4 grad_0000 = grad(base + vec4(0.0, 0.0, 0.0, 0.0)), grad_1000 = grad(base + vec4(1.0, 0.0, 0.0, 0.0));\n";
		code += "	vec4 grad_0100 = grad(base + vec4(0.0, 1.0, 0.0, 0.0)), grad_1100 = grad(base + vec4(1.0, 1.0, 0.0, 0.0));\n";
		code += "	vec4 grad_0010 = grad(base + vec4(0.0, 0.0, 1.0, 0.0)), grad_1010 = grad(base + vec4(1.0, 0.0, 1.0, 0.0));\n";
		code += "	vec4 grad_0110 = grad(base + vec4(0.0, 1.0, 1.0, 0.0)), grad_1110 = grad(base + vec4(1.0, 1.0, 1.0, 0.0));\n";
		code += "	vec4 grad_0001 = grad(base + vec4(0.0, 0.0, 0.0, 1.0)), grad_1001 = grad(base + vec4(1.0, 0.0, 0.0, 1.0));\n";
		code += "	vec4 grad_0101 = grad(base + vec4(0.0, 1.0, 0.0, 1.0)), grad_1101 = grad(base + vec4(1.0, 1.0, 0.0, 1.0));\n";
		code += "	vec4 grad_0011 = grad(base + vec4(0.0, 0.0, 1.0, 1.0)), grad_1011 = grad(base + vec4(1.0, 0.0, 1.0, 1.0));\n";
		code += "	vec4 grad_0111 = grad(base + vec4(0.0, 1.0, 1.0, 1.0)), grad_1111 = grad(base + vec4(1.0, 1.0, 1.0, 1.0));\n\n";
		code += "	vec4 result_0123 = vec4(\n";
		code += "			dot(delta - vec4(0.0, 0.0, 0.0, 0.0), grad_0000), dot(delta - vec4(1.0, 0.0, 0.0, 0.0), grad_1000),\n";
		code += "			dot(delta - vec4(0.0, 1.0, 0.0, 0.0), grad_0100), dot(delta - vec4(1.0, 1.0, 0.0, 0.0), grad_1100));\n";
		code += "	vec4 result_4567 = vec4(\n";
		code += "			dot(delta - vec4(0.0, 0.0, 1.0, 0.0), grad_0010), dot(delta - vec4(1.0, 0.0, 1.0, 0.0), grad_1010),\n";
		code += "			dot(delta - vec4(0.0, 1.0, 1.0, 0.0), grad_0110), dot(delta - vec4(1.0, 1.0, 1.0, 0.0), grad_1110));\n";
		code += "	vec4 result_89AB = vec4(\n";
		code += "			dot(delta - vec4(0.0, 0.0, 0.0, 1.0), grad_0001), dot(delta - vec4(1.0, 0.0, 0.0, 1.0), grad_1001),\n";
		code += "			dot(delta - vec4(0.0, 1.0, 0.0, 1.0), grad_0101), dot(delta - vec4(1.0, 1.0, 0.0, 1.0), grad_1101));\n";
		code += "	vec4 result_CDEF = vec4(\n";
		code += "			dot(delta - vec4(0.0, 0.0, 1.0, 1.0), grad_0011), dot(delta - vec4(1.0, 0.0, 1.0, 1.0), grad_1011),\n";
		code += "			dot(delta - vec4(0.0, 1.0, 1.0, 1.0), grad_0111), dot(delta - vec4(1.0, 1.0, 1.0, 1.0), grad_1111));\n\n";
		code += "	vec4 fade = delta * delta * delta * (10.0 + delta * (-15.0 + delta * 6.0));\n";
		code += "	vec4 result_W0 = mix(result_0123, result_89AB, fade.w), result_W1 = mix(result_4567, result_CDEF, fade.w);\n";
		code += "	vec4 result_WZ = mix(result_W0, result_W1, fade.z);\n";
		code += "	vec2 result_WZY = mix(result_WZ.xy, result_WZ.zw, fade.y);\n";
		code += "	return mix(result_WZY.x, result_WZY.y, fade.x);\n";
		code += "}\n\n";

		code += "// Curl 3D and three-noise function with friendly permission by Isaac Cohen.\n";
		code += "// Modified to accept 4D noise.\n";
		code += "vec3 noise_3x(vec4 p) {\n";
		code += "	float s = noise(p);\n";
		code += "	float s1 = noise(p + vec4(vec3(0.0), 1.7320508 * 2048.333333));\n";
		code += "	float s2 = noise(p - vec4(vec3(0.0), 1.7320508 * 2048.333333));\n";
		code += "	vec3 c = vec3(s, s1, s2);\n";
		code += "	return c;\n";
		code += "}\n\n";

		code += "vec3 curl_3d(vec4 p, float c) {\n";
		code += "	float epsilon = 0.001 + c;\n";
		code += "	vec4 dx = vec4(epsilon, 0.0, 0.0, 0.0);\n";
		code += "	vec4 dy = vec4(0.0, epsilon, 0.0, 0.0);\n";
		code += "	vec4 dz = vec4(0.0, 0.0, epsilon, 0.0);\n";
		code += "	vec3 x0 = noise_3x(p - dx).xyz;\n";
		code += "	vec3 x1 = noise_3x(p + dx).xyz;\n";
		code += "	vec3 y0 = noise_3x(p - dy).xyz;\n";
		code += "	vec3 y1 = noise_3x(p + dy).xyz;\n";
		code += "	vec3 z0 = noise_3x(p - dz).xyz;\n";
		code += "	vec3 z1 = noise_3x(p + dz).xyz;\n";
		code += "	float x = (y1.z - y0.z) - (z1.y - z0.y);\n";
		code += "	float y = (z1.x - z0.x) - (x1.z - x0.z);\n";
		code += "	float z = (x1.y - x0.y) - (y1.x - y0.x);\n";
		code += "	return normalize(vec3(x, y, z));\n";
		code += "}\n\n";

		code += "vec3 get_noise_direction(vec3 pos) {\n";
		code += "	float adj_contrast = max((turbulence_noise_strength - 1.0), 0.0) * 70.0;\n";
		code += "	vec4 noise_time = TIME * vec4(turbulence_noise_speed, turbulence_noise_speed_random);\n";
		code += "	vec4 noise_pos = vec4(pos * turbulence_noise_scale, 0.0);\n";
		code += "	vec3 noise_direction = curl_3d(noise_pos + noise_time, adj_contrast);\n";
		code += "	noise_direction = mix(0.9 * noise_direction, noise_direction, turbulence_noise_strength - 9.0);\n";
		code += "	return noise_direction;\n";
		code += "}\n\n";
	}

	code += "vec4 rotate_hue(vec4 current_color, float hue_rot_angle) {\n";
	code += "	float hue_rot_c = cos(hue_rot_angle);\n";
	code += "	float hue_rot_s = sin(hue_rot_angle);\n";
	code += "	mat4 hue_rot_mat =\n";
	code += "			mat4(vec4(0.299, 0.587, 0.114, 0.0),\n";
	code += "					vec4(0.299, 0.587, 0.114, 0.0),\n";
	code += "					vec4(0.299, 0.587, 0.114, 0.0),\n";
	code += "					vec4(0.000, 0.000, 0.000, 1.0)) +\n";
	code += "			mat4(vec4(0.701, -0.587, -0.114, 0.0),\n";
	code += "					vec4(-0.299, 0.413, -0.114, 0.0),\n";
	code += "					vec4(-0.300, -0.588, 0.886, 0.0),\n";
	code += "					vec4(0.000, 0.000, 0.000, 0.0)) *\n";
	code += "					hue_rot_c +\n";
	code += "			mat4(vec4(0.168, 0.330, -0.497, 0.0),\n";
	code += "					vec4(-0.328, 0.035, 0.292, 0.0),\n";
	code += "					vec4(1.250, -1.050, -0.203, 0.0),\n";
	code += "					vec4(0.000, 0.000, 0.000, 0.0)) *\n";
	code += "					hue_rot_s;\n";
	code += "	return hue_rot_mat * current_color;\n";
	code += "}\n\n";

	// Random functions.
	code += "float rand_from_seed(inout uint seed) {\n";
	code += "	int k;\n";
	code += "	int s = int(seed);\n";
	code += "	if (s == 0) {\n";
	code += "		s = 305420679;\n";
	code += "	}\n";
	code += "	k = s / 127773;\n";
	code += "	s = 16807 * (s - k * 127773) - 2836 * k;\n";
	code += "	if (s < 0) {\n";
	code += "		s += 2147483647;\n";
	code += "	}\n";
	code += "	seed = uint(s);\n";
	code += "	return float(seed % uint(65536)) / 65535.0;\n";
	code += "}\n\n";

	code += "float rand_from_seed_m1_p1(inout uint seed) {\n";
	code += "	return rand_from_seed(seed) * 2.0 - 1.0;\n";
	code += "}\n\n";

	// Improve seed quality.
	code += "uint hash(uint x) {\n";
	code += "	x = ((x >> uint(16)) ^ x) * uint(73244475);\n";
	code += "	x = ((x >> uint(16)) ^ x) * uint(73244475);\n";
	code += "	x = (x >> uint(16)) ^ x;\n";
	code += "	return x;\n";
	code += "}\n\n";

	// Define structs.

	code += "struct DisplayParameters {\n";
	code += "	vec3 scale;\n";
	code += "	float hue_rotation;\n";
	code += "	float animation_speed;\n";
	code += "	float animation_offset;\n";
	code += "	float lifetime;\n";
	code += "	vec4 color;\n";
	code += "	float emission_texture_position;\n";
	code += "};\n\n";

	code += "struct DynamicsParameters {\n";
	code += "	float angle;\n";
	code += "	float angular_velocity;\n";
	code += "	float initial_velocity_multiplier;\n";
	code += "	float directional_velocity;\n";
	code += "	float radial_velocity;\n";
	code += "	float orbit_velocity;\n";
	if (turbulence_enabled) {
		code += "	float turb_influence;\n";
	}
	code += "};\n\n";

	code += "struct PhysicalParameters {\n";
	code += "	float linear_accel;\n";
	code += "	float radial_accel;\n";
	code += "	float tangent_accel;\n";
	code += "	float damping;\n";
	code += "};\n\n";

	code += "void calculate_initial_physical_params(inout PhysicalParameters params, inout uint alt_seed) {\n";
	code += "	params.linear_accel = mix(linear_accel_min, linear_accel_max, rand_from_seed(alt_seed));\n";
	code += "	params.radial_accel = mix(radial_accel_min, radial_accel_max, rand_from_seed(alt_seed));\n";
	code += "	params.tangent_accel = mix(tangent_accel_min, tangent_accel_max, rand_from_seed(alt_seed));\n";
	code += "	params.damping = mix(damping_min, damping_max, rand_from_seed(alt_seed));\n";
	code += "}\n\n";

	code += "void calculate_initial_dynamics_params(inout DynamicsParameters params, inout uint alt_seed) {\n";
	code += "	// -------------------- DO NOT REORDER OPERATIONS, IT BREAKS VISUAL COMPATIBILITY\n";
	code += "	// -------------------- ADD NEW OPERATIONS AT THE BOTTOM\n";
	code += "	params.angle = mix(initial_angle_min, initial_angle_max, rand_from_seed(alt_seed));\n";
	code += "	params.angular_velocity = mix(angular_velocity_min, angular_velocity_max, rand_from_seed(alt_seed));\n";
	code += "	params.initial_velocity_multiplier = mix(initial_linear_velocity_min, initial_linear_velocity_max, rand_from_seed(alt_seed));\n";
	code += "	params.directional_velocity = mix(directional_velocity_min, directional_velocity_max, rand_from_seed(alt_seed));\n";
	code += "	params.radial_velocity = mix(radial_velocity_min, radial_velocity_max, rand_from_seed(alt_seed));\n";
	code += "	params.orbit_velocity = mix(orbit_velocity_min, orbit_velocity_max, rand_from_seed(alt_seed));\n";
	if (turbulence_enabled) {
		code += "	params.turb_influence = mix(turbulence_influence_min, turbulence_influence_max, rand_from_seed(alt_seed));\n";
	}
	code += "}\n\n";

	code += "void calculate_initial_display_params(inout DisplayParameters params, inout uint alt_seed) {\n";
	code += "	// -------------------- DO NOT REORDER OPERATIONS, IT BREAKS VISUAL COMPATIBILITY\n";
	code += "	// -------------------- ADD NEW OPERATIONS AT THE BOTTOM\n";
	code += "	float pi = 3.14159;\n";
	code += "	params.scale = vec3(mix(scale_min, scale_max, rand_from_seed(alt_seed)));\n";
	code += "	params.scale = sign(params.scale) * max(abs(params.scale), 0.001);\n";
	code += "	params.hue_rotation = pi * 2.0 * mix(hue_variation_min, hue_variation_max, rand_from_seed(alt_seed));\n";
	code += "	params.animation_speed = mix(anim_speed_min, anim_speed_max, rand_from_seed(alt_seed));\n";
	code += "	params.animation_offset = mix(anim_offset_min, anim_offset_max, rand_from_seed(alt_seed));\n";
	code += "	params.lifetime = (1.0 - lifetime_randomness * rand_from_seed(alt_seed));\n";
	code += "	params.color = color_value;\n";
	if (color_initial_ramp.is_valid()) {
		code += "	params.color *= texture(color_initial_ramp, vec2(rand_from_seed(alt_seed)));\n";
	}
	if (emission_shape == EMISSION_SHAPE_POINTS || emission_shape == EMISSION_SHAPE_DIRECTED_POINTS) {
		code += "	params.emission_texture_position = rand_from_seed(alt_seed);\n";

		if (emission_color_texture.is_valid()) {
			code += "	int point = min(emission_texture_point_count - 1, int(params.emission_texture_position * float(emission_texture_point_count)));\n";
			code += "	ivec2 emission_tex_size = textureSize(emission_texture_points, 0);\n";
			code += "	ivec2 emission_tex_ofs = ivec2(point % emission_tex_size.x, point / emission_tex_size.x);\n";
			code += "	params.color *= texelFetch(emission_texture_color, emission_tex_ofs, 0);\n";
		}
	}
	code += "}\n\n";

	// Process display parameters that are bound solely by lifetime.
	code += "void process_display_param(inout DisplayParameters parameters, float lifetime) {\n";
	code += "	// Compile-time add textures.\n";
	if (tex_parameters[PARAM_SCALE].is_valid()) {
		code += "	parameters.scale *= texture(scale_curve, vec2(lifetime)).rgb;\n";
	}
	if (tex_parameters[PARAM_HUE_VARIATION].is_valid()) {
		code += "	parameters.hue_rotation *= texture(hue_rot_curve, vec2(lifetime)).r;\n";
	}
	if (tex_parameters[PARAM_ANIM_OFFSET].is_valid()) {
		code += "	parameters.animation_offset += texture(animation_offset_curve, vec2(lifetime)).r;\n";
	}
	if (tex_parameters[PARAM_ANIM_SPEED].is_valid()) {
		code += "	parameters.animation_speed *= texture(animation_speed_curve, vec2(lifetime)).r;\n";
	}
	if (color_ramp.is_valid()) {
		code += "	parameters.color *= texture(color_ramp, vec2(lifetime));\n";
	}
	if (alpha_curve.is_valid()) {
		code += "	parameters.color.a *= texture(alpha_curve, vec2(lifetime)).r;\n";
	}
	code += "	parameters.color = rotate_hue(parameters.color, parameters.hue_rotation);\n";
	if (emission_curve.is_valid()) {
		code += "	parameters.color.rgb *= 1.0 + texture(emission_curve, vec2(lifetime)).r;\n";
	}
	code += "}\n\n";

	code += "vec3 calculate_initial_position(inout DisplayParameters params, inout uint alt_seed) {\n";
	code += "	float pi = 3.14159;\n";
	code += "	vec3 pos = vec3(0.0);\n";
	code += "	{ // Emission shape.\n";
	if (emission_shape == EMISSION_SHAPE_POINT) {
		code += "	 	pos = vec3(0.0);\n";
	}
	if (emission_shape == EMISSION_SHAPE_SPHERE) {
		code += "		float s = rand_from_seed(alt_seed) * 2.0 - 1.0;\n";
		code += "		float t = rand_from_seed(alt_seed) * 2.0 * pi;\n";
		code += "		float p = rand_from_seed(alt_seed);\n";
		code += "		float radius = emission_sphere_radius * sqrt(1.0 - s * s);\n";
		code += "		pos = mix(vec3(0.0, 0.0, 0.0), vec3(radius * cos(t), radius * sin(t), emission_sphere_radius * s), p);\n";
	}

	if (emission_shape == EMISSION_SHAPE_SPHERE_SURFACE) {
		code += "		float s = rand_from_seed(alt_seed) * 2.0 - 1.0;\n";
		code += "		float t = rand_from_seed(alt_seed) * 2.0 * pi;\n";
		code += "		float radius = emission_sphere_radius * sqrt(1.0 - s * s);\n";
		code += "		pos = vec3(radius * cos(t), radius * sin(t), emission_sphere_radius * s);\n";
	}
	if (emission_shape == EMISSION_SHAPE_BOX) {
		code += "		pos = vec3(rand_from_seed(alt_seed) * 2.0 - 1.0, rand_from_seed(alt_seed) * 2.0 - 1.0, rand_from_seed(alt_seed) * 2.0 - 1.0) * emission_box_extents;\n";
	}
	if (emission_shape == EMISSION_SHAPE_POINTS || emission_shape == EMISSION_SHAPE_DIRECTED_POINTS) {
		code += "		int point = min(emission_texture_point_count - 1, int(params.emission_texture_position * float(emission_texture_point_count)));\n";
		code += "		ivec2 emission_tex_size = textureSize(emission_texture_points, 0);\n";
		code += "		ivec2 emission_tex_ofs = ivec2(point % emission_tex_size.x, point / emission_tex_size.x);\n";
		code += "		pos = texelFetch(emission_texture_points, emission_tex_ofs, 0).xyz;\n";
	}
	if (emission_shape == EMISSION_SHAPE_RING) {
		code += "		float radius_clamped = max(0.001, emission_ring_radius);\n";
		code += "		float top_radius = max(radius_clamped - tan(radians(90.0 - emission_ring_cone_angle)) * emission_ring_height, 0.0);\n";
		code += "		float y_pos = rand_from_seed(alt_seed);\n";
		code += "		float skew = max(min(radius_clamped, top_radius) / max(radius_clamped, top_radius), 0.5);\n";
		code += "		y_pos = radius_clamped < top_radius ? pow(y_pos, skew) : 1.0 - pow(y_pos, skew);\n";
		code += "		float ring_spawn_angle = rand_from_seed(alt_seed) * 2.0 * pi;\n";
		code += "		float ring_random_radius = sqrt(rand_from_seed(alt_seed) * (radius_clamped * radius_clamped - emission_ring_inner_radius * emission_ring_inner_radius) + emission_ring_inner_radius * emission_ring_inner_radius);\n";
		code += "		ring_random_radius = mix(ring_random_radius, ring_random_radius * (top_radius / radius_clamped), y_pos);\n";
		code += "		vec3 axis = emission_ring_axis == vec3(0.0) ? vec3(0.0, 0.0, 1.0) : normalize(emission_ring_axis);\n";
		code += "		vec3 ortho_axis = vec3(0.0);\n";
		code += "		if (abs(axis) == vec3(1.0, 0.0, 0.0)) {\n";
		code += "			ortho_axis = cross(axis, vec3(0.0, 1.0, 0.0));\n";
		code += "		} else {\n";
		code += " 			ortho_axis = cross(axis, vec3(1.0, 0.0, 0.0));\n";
		code += "		}\n";
		code += "		ortho_axis = normalize(ortho_axis);\n";
		code += "		float s = sin(ring_spawn_angle);\n";
		code += "		float c = cos(ring_spawn_angle);\n";
		code += "		float oc = 1.0 - c;\n";
		code += "		ortho_axis = mat3(\n";
		code += "			vec3(c + axis.x * axis.x * oc, axis.x * axis.y * oc - axis.z * s, axis.x * axis.z *oc + axis.y * s),\n";
		code += "			vec3(axis.x * axis.y * oc + s * axis.z, c + axis.y * axis.y * oc, axis.y * axis.z * oc - axis.x * s),\n";
		code += "			vec3(axis.z * axis.x * oc - axis.y * s, axis.z * axis.y * oc + axis.x * s, c + axis.z * axis.z * oc)\n";
		code += "			) * ortho_axis;\n";
		code += "		ortho_axis = normalize(ortho_axis);\n";
		code += "		pos = ortho_axis * ring_random_radius + (y_pos * emission_ring_height - emission_ring_height / 2.0) * axis;\n";
	}
	code += "	}\n";
	code += "	return pos * emission_shape_scale + emission_shape_offset;\n";
	code += "}\n\n";

	if (tex_parameters[PARAM_ORBIT_VELOCITY].is_valid() || particle_flags[PARTICLE_FLAG_DISABLE_Z]) {
		code += "vec3 process_orbit_displacement(DynamicsParameters param, float lifetime, inout uint alt_seed, mat4 transform, mat4 emission_transform, float delta, float total_lifetime) {\n";
		// No reason to run all these expensive calculation below if we have no orbit velocity
		// HOWEVER
		// May be a bad idea for fps consistency?
		code += "	if (abs(param.orbit_velocity) < 0.01 || delta < 0.001) {\n";
		code += "		return vec3(0.0);\n";
		code += "	}\n";
		code += "	vec3 displacement = vec3(0.0);\n";
		code += "	float pi = 3.14159;\n";
		if (particle_flags[PARTICLE_FLAG_DISABLE_Z]) {
			code += "	float orbit_amount = param.orbit_velocity;\n";
			if (tex_parameters[PARAM_ORBIT_VELOCITY].is_valid()) {
				CurveTexture *texture = Object::cast_to<CurveTexture>(tex_parameters[PARAM_ORBIT_VELOCITY].ptr());
				if (texture) {
					code += "	orbit_amount *= texture(orbit_velocity_curve, vec2(lifetime)).r;\n";
				} else {
					code += "	orbit_amount *= texture(orbit_velocity_curve, vec2(lifetime)).b;\n";
				}
			}
			code += "	if (orbit_amount != 0.0) {\n";
			code += "		vec3 pos = transform[3].xyz;\n";
			code += "		vec3 org = emission_transform[3].xyz;\n";
			code += "		vec3 diff = pos - org;\n";
			code += "		float ang = orbit_amount * pi * 2.0 * delta;\n";
			code += "		mat2 rot = mat2(vec2(cos(ang), -sin(ang)), vec2(sin(ang), cos(ang)));\n";
			code += "		displacement.xy -= diff.xy;\n";
			code += "		displacement.xy += rot * diff.xy;\n";
			code += "	}\n";
		} else {
			code += "	vec3 orbit_velocities = vec3(param.orbit_velocity);\n";
			code += "	orbit_velocities *= texture(orbit_velocity_curve, vec2(lifetime)).rgb;\n";
			code += "	orbit_velocities *= pi * 2.0;\n";
			code += "	orbit_velocities *= delta; // We wanna process those by the delta angle.\n\n";

			code += "	// X axis.\n";
			code += "	vec3 local_pos = (inverse(emission_transform) * transform[3]).xyz;\n";
			code += "	local_pos -= velocity_pivot;\n";
			code += "	local_pos.x = 0.0;\n";
			code += "	mat3 x_rotation_mat = mat3(\n";
			code += "		vec3(1.0, 0.0, 0.0),\n";
			code += "		vec3(0.0, cos(orbit_velocities.x), sin(orbit_velocities.x)),\n";
			code += "		vec3(0.0, -sin(orbit_velocities.x), cos(orbit_velocities.x))\n";
			code += "	);\n";
			code += "	vec3 new_pos = x_rotation_mat * local_pos;\n";
			code += "	displacement = new_pos - local_pos;\n\n";

			code += "	// Y axis.\n";
			code += "	local_pos = (inverse(emission_transform) * transform[3]).xyz;\n";
			code += "	local_pos -= velocity_pivot;\n";
			code += "	local_pos.y = 0.0;\n";
			code += "	mat3 y_rotation_mat = mat3(\n";
			code += "		vec3(cos(orbit_velocities.y), 0.0, -sin(orbit_velocities.y)),\n";
			code += "		vec3(0.0, 1.0, 0.0),\n";
			code += "		vec3(sin(orbit_velocities.y), 0.0, cos(orbit_velocities.y))\n";
			code += "	);\n";
			code += "	new_pos = y_rotation_mat * local_pos;\n";
			code += "	displacement += new_pos - local_pos;\n\n";

			code += "	// Z axis.\n";
			code += "	local_pos = (inverse(emission_transform) * transform[3]).xyz;\n";
			code += "	local_pos -= velocity_pivot;\n";
			code += "	local_pos.z = 0.0;\n";
			code += "	mat3 z_rotation_mat = mat3(\n";
			code += "		vec3(cos(orbit_velocities.z), sin(orbit_velocities.z), 0.0),\n";
			code += "		vec3(-sin(orbit_velocities.z), cos(orbit_velocities.z), 0.0),\n";
			code += "		vec3(0.0, 0.0, 1.0)\n";
			code += "	);\n";
			code += "	new_pos = z_rotation_mat * local_pos;\n";
			code += "	displacement += new_pos - local_pos;\n\n";
		}
		code += "	return (emission_transform * vec4(displacement / delta, 0.0)).xyz;\n";
		code += "}\n\n";
	}

	code += "vec3 get_random_direction_from_spread(inout uint alt_seed, float spread_angle) {\n";
	code += "	float pi = 3.14159;\n";
	code += "	float degree_to_rad = pi / 180.0;\n";
	code += "	float spread_rad = spread_angle * degree_to_rad;\n";
	if (particle_flags[PARTICLE_FLAG_DISABLE_Z]) {
		// Spread calculation for 2D.
		code += "	float angle1_rad = rand_from_seed_m1_p1(alt_seed) * spread_rad;\n";
		code += "	angle1_rad += direction.x != 0.0 ? atan(direction.y, direction.x) : sign(direction.y) * (pi / 2.0);\n";
		code += "	vec3 spread_direction = vec3(cos(angle1_rad), sin(angle1_rad), 0.0);\n";
		code += "	return spread_direction;\n";
	} else {
		// Spread calculation for 3D.
		code += "	float angle1_rad = rand_from_seed_m1_p1(alt_seed) * spread_rad;\n";
		code += "	float angle2_rad = rand_from_seed_m1_p1(alt_seed) * spread_rad * (1.0 - flatness);\n";
		code += "	vec3 direction_xz = vec3(sin(angle1_rad), 0.0, cos(angle1_rad));\n";
		code += "	vec3 direction_yz = vec3(0.0, sin(angle2_rad), cos(angle2_rad));\n";
		code += "	direction_yz.z = direction_yz.z / max(0.0001, sqrt(abs(direction_yz.z))); // Better uniform distribution.\n";
		code += "	vec3 spread_direction = vec3(direction_xz.x * direction_yz.z, direction_yz.y, direction_xz.z * direction_yz.z);\n";
		code += "	vec3 direction_nrm = length(direction) > 0.0 ? normalize(direction) : vec3(0.0, 0.0, 1.0);\n";
		code += "	// Rotate spread to direction.\n";
		code += "	vec3 binormal = cross(vec3(0.0, 1.0, 0.0), direction_nrm);\n";
		code += "	if (length(binormal) < 0.0001) {\n";
		code += "		// Direction is parallel to Y. Choose Z as the binormal.\n";
		code += "		binormal = vec3(0.0, 0.0, 1.0);\n";
		code += "	}\n";
		code += "	binormal = normalize(binormal);\n";
		code += "	vec3 normal = cross(binormal, direction_nrm);\n";
		code += "	spread_direction = binormal * spread_direction.x + normal * spread_direction.y + direction_nrm * spread_direction.z;\n";
		code += "	return normalize(spread_direction);\n";
	}
	code += "}\n\n";

	code += "vec3 process_radial_displacement(DynamicsParameters param, float lifetime, inout uint alt_seed, mat4 transform, mat4 emission_transform, float delta) {\n";
	code += "	vec3 radial_displacement = vec3(0.0);\n";
	code += "	if (delta < 0.001) {\n";
	code += "		return radial_displacement;\n";
	code += "	}\n";
	code += "	float radial_displacement_multiplier = 1.0;\n";
	if (tex_parameters[PARAM_RADIAL_VELOCITY].is_valid()) {
		code += "	radial_displacement_multiplier = texture(radial_velocity_curve, vec2(lifetime)).r;\n";
	}
	code += "	vec3 global_pivot = (emission_transform * vec4(velocity_pivot, 1.0)).xyz;\n";
	code += "	if (length(transform[3].xyz - global_pivot) > 0.01) {\n";
	code += "		radial_displacement = normalize(transform[3].xyz - global_pivot) * radial_displacement_multiplier * param.radial_velocity;\n";
	code += "	} else {\n";
	code += "		radial_displacement = get_random_direction_from_spread(alt_seed, 360.0) * param.radial_velocity;\n";
	code += "	}\n";
	code += "	if (radial_displacement_multiplier * param.radial_velocity < 0.0) {\n";
	code += "		// Prevent inwards velocity to flicker once the point is reached.\n";
	code += "		radial_displacement = normalize(radial_displacement) * min(abs(radial_displacement_multiplier * param.radial_velocity), length(transform[3].xyz - global_pivot) / delta);\n";
	code += "	}\n";
	code += "	return radial_displacement;\n";
	code += "}\n\n";

	if (tex_parameters[PARAM_DIRECTIONAL_VELOCITY].is_valid()) {
		code += "vec3 process_directional_displacement(DynamicsParameters param, float lifetime_percent, mat4 transform, mat4 emission_transform) {\n";
		code += "	vec3 displacement = texture(directional_velocity_curve, vec2(lifetime_percent)).xyz * param.directional_velocity;\n";
		if (directional_velocity_global) {
			code += "	displacement = (emission_transform * vec4(displacement, 0.0)).xyz;\n";
		}
		code += "	return displacement;\n";
		code += "}\n\n";
	}

	code += "void process_physical_parameters(inout PhysicalParameters params, float lifetime_percent) {\n";
	if (tex_parameters[PARAM_LINEAR_ACCEL].is_valid()) {
		code += "	params.linear_accel *= texture(linear_accel_texture, vec2(lifetime_percent)).r;\n";
	}
	if (tex_parameters[PARAM_RADIAL_ACCEL].is_valid()) {
		code += "	params.radial_accel *= texture(radial_accel_texture, vec2(lifetime_percent)).r;\n";
	}
	if (tex_parameters[PARAM_TANGENTIAL_ACCEL].is_valid()) {
		code += "	params.tangent_accel *= texture(tangent_accel_texture, vec2(lifetime_percent)).r;\n";
	}
	if (tex_parameters[PARAM_DAMPING].is_valid()) {
		code += "	params.damping *= texture(damping_texture, vec2(lifetime_percent)).r;\n";
	}
	code += "}\n\n";

	code += "void start() {\n";
	code += "	uint base_number = NUMBER;\n";
	code += "	uint alt_seed = hash(base_number + uint(1) + RANDOM_SEED);\n";
	code += "	DisplayParameters params;\n";
	code += "	calculate_initial_display_params(params, alt_seed);\n";
	code += "	// Reset alt seed?\n";
	code += "	//alt_seed = hash(base_number + uint(1) + RANDOM_SEED);\n";
	code += "	DynamicsParameters dynamic_params;\n";
	code += "	calculate_initial_dynamics_params(dynamic_params, alt_seed);\n";
	code += "	PhysicalParameters physics_params;\n";
	code += "	calculate_initial_physical_params(physics_params, alt_seed);\n";
	code += "	process_display_param(params, 0.0);\n";
	code += "	if (rand_from_seed(alt_seed) > AMOUNT_RATIO) {\n";
	code += "		ACTIVE = false;\n";
	code += "	}\n\n";

	code += "	if (RESTART_CUSTOM) {\n";
	code += "		CUSTOM = vec4(0.0);\n";
	code += "		CUSTOM.w = params.lifetime;\n";
	code += "		CUSTOM.x = dynamic_params.angle;\n";
	code += "	}\n";
	code += "	if (RESTART_COLOR) {\n";
	code += "		COLOR = params.color;\n";
	code += "	}\n";
	code += "	if (RESTART_ROT_SCALE) {\n";
	code += "		TRANSFORM[0].xyz = vec3(1.0, 0.0, 0.0);\n";
	code += "		TRANSFORM[1].xyz = vec3(0.0, 1.0, 0.0);\n";
	code += "		TRANSFORM[2].xyz = vec3(0.0, 0.0, 1.0);\n";
	code += "	}\n";
	code += "	if (RESTART_POSITION) {\n";
	code += "		TRANSFORM[3].xyz = calculate_initial_position(params, alt_seed);\n";
	if (turbulence_enabled) {
		code += "		float initial_turbulence_displacement = mix(turbulence_initial_displacement_min, turbulence_initial_displacement_max, rand_from_seed(alt_seed));\n";
		code += "		vec3 noise_direction = get_noise_direction(TRANSFORM[3].xyz);\n";
		code += "		TRANSFORM[3].xyz += noise_direction * initial_turbulence_displacement;\n";
	}
	code += "		TRANSFORM = EMISSION_TRANSFORM * TRANSFORM;\n";
	code += "	}\n";
	code += "	if (RESTART_VELOCITY) {\n";
	code += "		VELOCITY = get_random_direction_from_spread(alt_seed, spread) * dynamic_params.initial_velocity_multiplier;\n";
	if (emission_shape == EMISSION_SHAPE_DIRECTED_POINTS) {
		code += "		int point = min(emission_texture_point_count - 1, int(params.emission_texture_position * float(emission_texture_point_count)));\n";
		code += "		ivec2 emission_tex_size = textureSize(emission_texture_points, 0);\n";
		code += "		ivec2 emission_tex_ofs = ivec2(point % emission_tex_size.x, point / emission_tex_size.x);\n";
		if (particle_flags[PARTICLE_FLAG_DISABLE_Z]) {
			code += "		{\n";
			code += "			mat2 rotm;\n";
			code += "			rotm[0] = texelFetch(emission_texture_normal, emission_tex_ofs, 0).xy;\n";
			code += "			rotm[1] = rotm[0].yx * vec2(1.0, -1.0);\n";
			code += "			VELOCITY.xy = rotm * VELOCITY.xy;\n";
			code += "		}\n";
		} else {
			code += "		{\n";
			code += "			vec3 normal = texelFetch(emission_texture_normal, emission_tex_ofs, 0).xyz;\n";
			code += "			vec3 v0 = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);\n";
			code += "			vec3 tangent = normalize(cross(v0, normal));\n";
			code += "			vec3 bitangent = normalize(cross(tangent, normal));\n";
			code += "			VELOCITY = mat3(tangent, bitangent, normal) * VELOCITY;\n";
			code += "		}\n";
		}
	}
	code += "	}\n\n";
	code += "	process_display_param(params, 0.0);\n\n";
	code += "	VELOCITY = (EMISSION_TRANSFORM * vec4(VELOCITY, 0.0)).xyz;\n";
	code += "	VELOCITY += EMITTER_VELOCITY * inherit_emitter_velocity_ratio;\n";
	if (particle_flags[PARTICLE_FLAG_DISABLE_Z]) {
		code += "	VELOCITY.z = 0.0;\n";
		code += "	TRANSFORM[3].z = 0.0;\n";
	}
	code += "}\n\n";

	code += "void process() {\n";
	code += "	uint base_number = NUMBER;\n";
	// TODO: Add optional determinism here.
	code += "	//if (repeatable) {\n";
	code += "	//	base_number = INDEX;\n";
	code += "	//}\n";
	code += "	uint alt_seed = hash(base_number + uint(1) + RANDOM_SEED);\n";
	code += "	DisplayParameters params;\n";
	code += "	calculate_initial_display_params(params, alt_seed);\n";
	code += "	DynamicsParameters dynamic_params;\n";
	code += "	calculate_initial_dynamics_params(dynamic_params, alt_seed);\n";
	code += "	PhysicalParameters physics_params;\n";
	code += "	calculate_initial_physical_params(physics_params, alt_seed);\n\n";

	code += "	float pi = 3.14159;\n";
	code += "	float degree_to_rad = pi / 180.0;\n\n";

	code += "	CUSTOM.y += DELTA / LIFETIME;\n";
	code += "	CUSTOM.y = mix(CUSTOM.y, 1.0, INTERPOLATE_TO_END);\n";
	code += "	float lifetime_percent = CUSTOM.y / params.lifetime;\n";
	code += "	if (CUSTOM.y > CUSTOM.w) {\n";
	code += "		ACTIVE = false;\n";
	code += "	}\n\n";

	code += "	// Calculate all velocity.\n";
	code += "	vec3 controlled_displacement = vec3(0.0);\n";
	if (tex_parameters[PARAM_ORBIT_VELOCITY].is_valid() || particle_flags[PARTICLE_FLAG_DISABLE_Z]) {
		code += "	controlled_displacement += process_orbit_displacement(dynamic_params, lifetime_percent, alt_seed, TRANSFORM, EMISSION_TRANSFORM, DELTA, params.lifetime * LIFETIME);\n";
	}
	code += "	controlled_displacement += process_radial_displacement(dynamic_params, lifetime_percent, alt_seed, TRANSFORM, EMISSION_TRANSFORM, DELTA);\n";
	if (tex_parameters[PARAM_DIRECTIONAL_VELOCITY].is_valid()) {
		code += "	controlled_displacement += process_directional_displacement(dynamic_params, lifetime_percent, TRANSFORM, EMISSION_TRANSFORM);\n";
	}
	code += "\n";
	code += "	process_physical_parameters(physics_params, lifetime_percent);\n";
	code += "	vec3 force;\n";
	code += "	{\n";
	code += "		// Copied from previous version.\n";
	code += "		vec3 pos = TRANSFORM[3].xyz;\n";
	code += "		force = gravity;\n";
	code += "		// Apply linear acceleration.\n";
	code += "		force += length(VELOCITY) > 0.0 ? normalize(VELOCITY) * physics_params.linear_accel : vec3(0.0);\n";
	code += "		// Apply radial acceleration.\n";
	code += "		vec3 org = EMISSION_TRANSFORM[3].xyz;\n";
	code += "		vec3 diff = pos - org;\n";
	code += "		force += length(diff) > 0.0 ? normalize(diff) * physics_params.radial_accel : vec3(0.0);\n";
	code += "		// Apply tangential acceleration.\n";
	code += "		float tangent_accel_val = physics_params.tangent_accel;\n";
	if (particle_flags[PARTICLE_FLAG_DISABLE_Z]) {
		code += "		force += length(diff.yx) > 0.0 ? vec3(normalize(diff.yx * vec2(-1.0, 1.0)), 0.0) * tangent_accel_val : vec3(0.0);\n";
	} else {
		code += "		vec3 crossDiff = cross(normalize(diff), normalize(gravity));\n";
		code += "		force += length(crossDiff) > 0.0 ? normalize(crossDiff) * tangent_accel_val : vec3(0.0);\n";
	}
	if (attractor_interaction_enabled) {
		code += "		force += ATTRACTOR_FORCE;\n";
	}
	if (particle_flags[PARTICLE_FLAG_DISABLE_Z]) {
		code += "		force.z = 0.0;\n";
	}
	code += "		// Apply attractor forces.\n";
	code += "		VELOCITY += force * DELTA;\n";
	code += "	}\n";
	code += "	{\n";
	code += "		// Copied from previous version.\n";
	code += "		if (physics_params.damping > 0.0) {\n";
	code += "			float v = length(VELOCITY);\n";
	if (!particle_flags[PARTICLE_FLAG_DAMPING_AS_FRICTION]) {
		code += "			v -= physics_params.damping * DELTA;\n";
	} else {
		code += "			if (v > 0.001) {\n";
		code += "				// Realistic friction formula. We assume the mass of a particle to be 0.05 kg.\n";
		code += "				float damp = v * v * physics_params.damping * 0.05 * DELTA;\n";
		code += "				v -= damp;\n";
		code += "			}\n";
	}
	code += "			if (v < 0.0) {\n";
	code += "				VELOCITY = vec3(0.0);\n";
	code += "			} else {\n";
	code += "				VELOCITY = normalize(VELOCITY) * v;\n";
	code += "			}\n";
	code += "		}\n";
	code += "	}\n\n";

	if (collision_mode == COLLISION_RIGID) {
		code += "	if (COLLIDED) {\n";
		code += "		float collision_response = dot(COLLISION_NORMAL, VELOCITY);\n";
		code += "		float slide_to_bounce_trigger = step(2.0 / clamp(collision_bounce + 1.0, 1.0, 2.0), abs(collision_response));\n";
		code += "		TRANSFORM[3].xyz += COLLISION_NORMAL * COLLISION_DEPTH;\n";
		code += "		// Remove all components of VELOCITY that are not tangential to COLLISION_NORMAL.\n";
		code += "		VELOCITY -= COLLISION_NORMAL * collision_response;\n";
		code += "		// Apply friction only to VELOCITY across the surface (effectively decouples friction and bounce behavior).\n";
		code += "		VELOCITY = mix(VELOCITY, vec3(0.0), clamp(collision_friction, 0.0, 1.0));\n";
		code += "		// Add bounce velocity to VELOCITY.\n";
		code += "		VELOCITY -= COLLISION_NORMAL * collision_response * (collision_bounce * slide_to_bounce_trigger);\n";
		code += "	}\n\n";
	} else if (collision_mode == COLLISION_HIDE_ON_CONTACT) {
		code += "	if (COLLIDED) {\n";
		code += "		ACTIVE = false;\n";
		code += "	}\n\n";
	}

	code += "	// Turbulence before limiting.\n";
	if (turbulence_enabled) {
		if (tex_parameters[PARAM_TURB_INFLUENCE_OVER_LIFE].is_valid()) {
			code += "	float turbulence_influence = textureLod(turbulence_influence_over_life, vec2(lifetime_percent, 0.0), 0.0).r;\n";
		} else {
			code += "	float turbulence_influence = 1.0;\n";
		}
		code += "\n";
		code += "	vec3 noise_direction = get_noise_direction(TRANSFORM[3].xyz);\n";

		// Godot detects when the COLLIDED keyword is used. If it's used anywhere in the shader then Godot will generate the screen space SDF for collisions.
		// We don't need it as long as collision is disabled. Refer to GH-83744 for more info.
		if (collision_mode == COLLISION_RIGID) {
			code += "	if (!COLLIDED) {\n";
		} else {
			code += "	{\n";
		}
		code += "		float vel_mag = length(VELOCITY);\n";
		code += "		float vel_infl = clamp(dynamic_params.turb_influence * turbulence_influence, 0.0, 1.0);\n";
		code += "		VELOCITY = mix(VELOCITY, normalize(noise_direction) * vel_mag * (1.0 + (1.0 - vel_infl) * 0.2), vel_infl);\n";
		code += "		vel_mag = length(controlled_displacement);\n";
		code += "		controlled_displacement = mix(controlled_displacement, normalize(noise_direction) * vel_mag * (1.0 + (1.0 - vel_infl) * 0.2), vel_infl);\n";
		code += "	}\n";
	}
	code += "	vec3 final_velocity = controlled_displacement + VELOCITY;\n\n";

	if (velocity_limit_curve.is_valid()) {
		code += "	// Limit velocity.\n";
		code += "	if (length(final_velocity) > 0.001) {\n";
		code += "		final_velocity = normalize(final_velocity) * min(abs(length(final_velocity)), abs(texture(velocity_limit_curve, vec2(lifetime_percent)).r));\n";
		code += "	}\n\n";
	}

	if (particle_flags[PARTICLE_FLAG_DISABLE_Z]) {
		code += "	final_velocity.z = 0.0;\n\n";
	}

	code += "	TRANSFORM[3].xyz += final_velocity * DELTA;\n\n";

	code += "	process_display_param(params, lifetime_percent);\n\n";

	code += "	float base_angle = dynamic_params.angle;\n";
	if (tex_parameters[PARAM_ANGLE].is_valid()) {
		code += "	base_angle *= texture(angle_texture, vec2(lifetime_percent)).r;\n";
	}
	if (tex_parameters[PARAM_ANGULAR_VELOCITY].is_valid()) {
		code += "	base_angle += CUSTOM.y * LIFETIME * dynamic_params.angular_velocity * texture(angular_velocity_texture, vec2(lifetime_percent)).r;\n";
	} else {
		code += "	base_angle += CUSTOM.y * LIFETIME * dynamic_params.angular_velocity;\n";
	}
	code += "	CUSTOM.x = base_angle * degree_to_rad;\n";
	code += "	COLOR = params.color;\n\n";

	if (particle_flags[PARTICLE_FLAG_DISABLE_Z]) {
		if (particle_flags[PARTICLE_FLAG_ALIGN_Y_TO_VELOCITY]) {
			code += "	if (length(final_velocity) > 0.0) {\n";
			code += "		TRANSFORM[1].xyz = normalize(final_velocity);\n";
			code += "	} else {\n";
			code += "		TRANSFORM[1].xyz = normalize(TRANSFORM[1].xyz);\n";
			code += "	}\n";
			code += "	TRANSFORM[0].xyz = normalize(cross(TRANSFORM[1].xyz, TRANSFORM[2].xyz));\n";
			code += "	TRANSFORM[2] = vec4(0.0, 0.0, 1.0, 0.0);\n";
		} else {
			code += "	TRANSFORM[0] = vec4(cos(CUSTOM.x), -sin(CUSTOM.x), 0.0, 0.0);\n";
			code += "	TRANSFORM[1] = vec4(sin(CUSTOM.x), cos(CUSTOM.x), 0.0, 0.0);\n";
			code += "	TRANSFORM[2] = vec4(0.0, 0.0, 1.0, 0.0);\n";
		}
	} else {
		// Orient particle Y towards velocity.
		if (particle_flags[PARTICLE_FLAG_ALIGN_Y_TO_VELOCITY]) {
			code += "	if (length(final_velocity) > 0.0) {\n";
			code += "		TRANSFORM[1].xyz = normalize(final_velocity);\n";
			code += "	} else {\n";
			code += "		TRANSFORM[1].xyz = normalize(TRANSFORM[1].xyz);\n";
			code += "	}\n";
			code += "	if (TRANSFORM[1].xyz == normalize(TRANSFORM[0].xyz)) {\n";
			code += "		TRANSFORM[0].xyz = normalize(cross(normalize(TRANSFORM[1].xyz), normalize(TRANSFORM[2].xyz)));\n";
			code += "		TRANSFORM[2].xyz = normalize(cross(normalize(TRANSFORM[0].xyz), normalize(TRANSFORM[1].xyz)));\n";
			code += "	} else {\n";
			code += "		TRANSFORM[2].xyz = normalize(cross(normalize(TRANSFORM[0].xyz), normalize(TRANSFORM[1].xyz)));\n";
			code += "		TRANSFORM[0].xyz = normalize(cross(normalize(TRANSFORM[1].xyz), normalize(TRANSFORM[2].xyz)));\n";
			code += "	}\n";
		} else {
			code += "	TRANSFORM[0].xyz = normalize(TRANSFORM[0].xyz);\n";
			code += "	TRANSFORM[1].xyz = normalize(TRANSFORM[1].xyz);\n";
			code += "	TRANSFORM[2].xyz = normalize(TRANSFORM[2].xyz);\n";
		}
		// Turn particle by rotation in Y.
		if (particle_flags[PARTICLE_FLAG_ROTATE_Y]) {
			code += "	vec4 origin = TRANSFORM[3];\n";
			code += "	TRANSFORM = mat4(vec4(cos(CUSTOM.x), 0.0, -sin(CUSTOM.x), 0.0), vec4(0.0, 1.0, 0.0, 0.0), vec4(sin(CUSTOM.x), 0.0, cos(CUSTOM.x), 0.0), vec4(0.0, 0.0, 0.0, 1.0));\n";
			code += "	TRANSFORM[3] = origin;\n";
		}
	}
	if (particle_flags[PARTICLE_FLAG_DISABLE_Z]) {
		code += "	TRANSFORM[3].z = 0.0;\n\n";
	}

	if (tex_parameters[PARAM_SCALE_OVER_VELOCITY].is_valid()) {
		code += "	if (length(final_velocity) > 0.001) {\n";
		code += "		params.scale *= texture(scale_over_velocity_curve, vec2(clamp(length(final_velocity) / (scale_over_velocity_max - scale_over_velocity_min), 0.0, 1.0), 0.0)).rgb;\n";
		code += "	} else {\n";
		code += "		params.scale *= texture(scale_over_velocity_curve, vec2(0.0)).rgb;\n";
		code += "	}\n";
	}
	code += "	TRANSFORM[0].xyz *= sign(params.scale.x) * max(abs(params.scale.x), 0.001);\n";
	code += "	TRANSFORM[1].xyz *= sign(params.scale.y) * max(abs(params.scale.y), 0.001);\n";
	code += "	TRANSFORM[2].xyz *= sign(params.scale.z) * max(abs(params.scale.z), 0.001);\n";
	code += "\n";
	code += "	CUSTOM.z = params.animation_offset + lifetime_percent * params.animation_speed;\n\n";

	if (sub_emitter_mode != SUB_EMITTER_DISABLED && !RenderingServer::get_singleton()->is_low_end()) {
		code += "	int emit_count = 0;\n";
		switch (sub_emitter_mode) {
			case SUB_EMITTER_CONSTANT: {
				code += "	float interval_from = CUSTOM.y * LIFETIME - DELTA;\n";
				code += "	float interval_rem = sub_emitter_frequency - mod(interval_from, sub_emitter_frequency);\n";
				code += "	if (DELTA >= interval_rem) {\n";
				code += "		emit_count = 1;\n";
				code += "	}\n";
			} break;
			case SUB_EMITTER_AT_COLLISION: {
				code += "	if (COLLIDED) {\n";
				code += "		emit_count = sub_emitter_amount_at_collision;\n";
				code += "	}\n";
			} break;
			case SUB_EMITTER_AT_END: {
				code += "	if ((CUSTOM.y / CUSTOM.w * LIFETIME) > (LIFETIME - DELTA)) {\n";
				code += "		emit_count = sub_emitter_amount_at_end;\n";
				code += "	}\n";
			} break;
			default: {
			}
		}
		code += "	for (int i = 0; i < emit_count; i++) {\n";
		code += "		uint flags = FLAG_EMIT_POSITION | FLAG_EMIT_ROT_SCALE;\n";
		code += "		if (sub_emitter_keep_velocity) {\n";
		code += "			flags |= FLAG_EMIT_VELOCITY;\n";
		code += "		}\n";
		code += "		emit_subparticle(TRANSFORM, VELOCITY, vec4(0.0), vec4(0.0), flags);\n";
		code += "	}\n\n";
	}

	code += "	if (CUSTOM.y > CUSTOM.w) {\n";
	code += "		ACTIVE = false;\n";
	code += "	}\n";
	code += "}\n";

	ShaderData shader_data;
	shader_data.shader = RS::get_singleton()->shader_create();
	shader_data.users = 1;

	RS::get_singleton()->shader_set_code(shader_data.shader, code);

	shader_map[mk] = shader_data;

	RS::get_singleton()->material_set_shader(_get_material(), shader_data.shader);
}

void ParticleProcessMaterial::flush_changes() {
	MutexLock lock(material_mutex);

	while (dirty_materials.first()) {
		dirty_materials.first()->self()->_update_shader();
		dirty_materials.first()->remove_from_list();
	}
}

void ParticleProcessMaterial::_queue_shader_change() {
	if (!_is_initialized()) {
		return;
	}

	MutexLock lock(material_mutex);

	if (!element.in_list()) {
		dirty_materials.add(&element);
	}
}

bool ParticleProcessMaterial::has_min_max_property(const String &p_name) {
	return min_max_properties.has(p_name);
}

void ParticleProcessMaterial::set_direction(Vector3 p_direction) {
	direction = p_direction;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->direction, direction);
}

Vector3 ParticleProcessMaterial::get_direction() const {
	return direction;
}

void ParticleProcessMaterial::set_spread(float p_spread) {
	spread = p_spread;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->spread, p_spread);
}

float ParticleProcessMaterial::get_spread() const {
	return spread;
}

void ParticleProcessMaterial::set_flatness(float p_flatness) {
	flatness = p_flatness;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->flatness, p_flatness);
}

float ParticleProcessMaterial::get_flatness() const {
	return flatness;
}

void ParticleProcessMaterial::set_velocity_pivot(const Vector3 &p_pivot) {
	velocity_pivot = p_pivot;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->velocity_pivot, p_pivot);
}

Vector3 ParticleProcessMaterial::get_velocity_pivot() {
	return velocity_pivot;
}

void ParticleProcessMaterial::set_param(Parameter p_param, const Vector2 &p_value) {
	set_param_min(p_param, p_value.x);
	set_param_max(p_param, p_value.y);
}

Vector2 ParticleProcessMaterial::get_param(Parameter p_param) const {
	return Vector2(get_param_min(p_param), get_param_max(p_param));
}

void ParticleProcessMaterial::set_param_min(Parameter p_param, float p_value) {
	ERR_FAIL_INDEX(p_param, PARAM_MAX);

	params_min[p_param] = p_value;
	if (params_min[p_param] > params_max[p_param]) {
		set_param_max(p_param, p_value);
	}

	switch (p_param) {
		case PARAM_INITIAL_LINEAR_VELOCITY: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->initial_linear_velocity_min, p_value);
		} break;
		case PARAM_ANGULAR_VELOCITY: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->angular_velocity_min, p_value);
		} break;
		case PARAM_ORBIT_VELOCITY: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->orbit_velocity_min, p_value);
		} break;
		case PARAM_LINEAR_ACCEL: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->linear_accel_min, p_value);
		} break;
		case PARAM_RADIAL_ACCEL: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->radial_accel_min, p_value);
		} break;
		case PARAM_TANGENTIAL_ACCEL: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->tangent_accel_min, p_value);
		} break;
		case PARAM_DAMPING: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->damping_min, p_value);
		} break;
		case PARAM_ANGLE: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->initial_angle_min, p_value);
		} break;
		case PARAM_SCALE: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->scale_min, p_value);
		} break;
		case PARAM_HUE_VARIATION: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->hue_variation_min, p_value);
		} break;
		case PARAM_ANIM_SPEED: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->anim_speed_min, p_value);
		} break;
		case PARAM_ANIM_OFFSET: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->anim_offset_min, p_value);
		} break;
		case PARAM_TURB_VEL_INFLUENCE: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->turbulence_influence_min, p_value);
		} break;
		case PARAM_TURB_INIT_DISPLACEMENT: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->turbulence_initial_displacement_min, p_value);
		} break;
		case PARAM_TURB_INFLUENCE_OVER_LIFE: {
			// Can't happen, but silences warning
		} break;
		case PARAM_RADIAL_VELOCITY: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->radial_velocity_min, p_value);
		} break;
		case PARAM_SCALE_OVER_VELOCITY: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->scale_over_velocity_min, p_value);
		} break;
		case PARAM_DIRECTIONAL_VELOCITY: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->directional_velocity_min, p_value);
		} break;
		case PARAM_MAX:
			break; // Can't happen, but silences warning
	}
}

float ParticleProcessMaterial::get_param_min(Parameter p_param) const {
	ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);

	return params_min[p_param];
}

void ParticleProcessMaterial::set_param_max(Parameter p_param, float p_value) {
	ERR_FAIL_INDEX(p_param, PARAM_MAX);

	params_max[p_param] = p_value;
	if (params_min[p_param] > params_max[p_param]) {
		set_param_min(p_param, p_value);
	}

	switch (p_param) {
		case PARAM_INITIAL_LINEAR_VELOCITY: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->initial_linear_velocity_max, p_value);
		} break;
		case PARAM_ANGULAR_VELOCITY: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->angular_velocity_max, p_value);
		} break;
		case PARAM_ORBIT_VELOCITY: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->orbit_velocity_max, p_value);
		} break;
		case PARAM_LINEAR_ACCEL: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->linear_accel_max, p_value);
		} break;
		case PARAM_RADIAL_ACCEL: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->radial_accel_max, p_value);
		} break;
		case PARAM_TANGENTIAL_ACCEL: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->tangent_accel_max, p_value);
		} break;
		case PARAM_DAMPING: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->damping_max, p_value);
		} break;
		case PARAM_ANGLE: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->initial_angle_max, p_value);
		} break;
		case PARAM_SCALE: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->scale_max, p_value);
		} break;
		case PARAM_HUE_VARIATION: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->hue_variation_max, p_value);
		} break;
		case PARAM_ANIM_SPEED: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->anim_speed_max, p_value);
		} break;
		case PARAM_ANIM_OFFSET: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->anim_offset_max, p_value);
		} break;
		case PARAM_TURB_VEL_INFLUENCE: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->turbulence_influence_max, p_value);
		} break;
		case PARAM_TURB_INIT_DISPLACEMENT: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->turbulence_initial_displacement_max, p_value);
		} break;
		case PARAM_TURB_INFLUENCE_OVER_LIFE: {
			// Can't happen, but silences warning
		} break;
		case PARAM_RADIAL_VELOCITY: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->radial_velocity_max, p_value);
		} break;
		case PARAM_SCALE_OVER_VELOCITY: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->scale_over_velocity_max, p_value);
		} break;
		case PARAM_DIRECTIONAL_VELOCITY: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->directional_velocity_max, p_value);
		} break;
		case PARAM_MAX:
			break; // Can't happen, but silences warning
	}
}

float ParticleProcessMaterial::get_param_max(Parameter p_param) const {
	ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);

	return params_max[p_param];
}

static void _adjust_curve_range(const Ref<Texture2D> &p_texture, float p_min, float p_max) {
	Ref<CurveTexture> curve_tex = p_texture;
	if (curve_tex.is_valid()) {
		curve_tex->ensure_default_setup(p_min, p_max);
		return;
	}
	Ref<CurveXYZTexture> curve_xyz_tex = p_texture;
	if (curve_xyz_tex.is_valid()) {
		curve_xyz_tex->ensure_default_setup(p_min, p_max);
		return;
	}
}

void ParticleProcessMaterial::set_param_texture(Parameter p_param, const Ref<Texture2D> &p_texture) {
	ERR_FAIL_INDEX(p_param, PARAM_MAX);

	tex_parameters[p_param] = p_texture;

	Variant tex_rid = p_texture.is_valid() ? Variant(p_texture->get_rid()) : Variant();

	switch (p_param) {
		case PARAM_INITIAL_LINEAR_VELOCITY: {
			//do none for this one
		} break;
		case PARAM_ANGULAR_VELOCITY: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->angular_velocity_texture, tex_rid);
			_adjust_curve_range(p_texture, -360, 360);
		} break;
		case PARAM_ORBIT_VELOCITY: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->orbit_velocity_texture, tex_rid);
			_adjust_curve_range(p_texture, -2, 2);
			notify_property_list_changed();
		} break;
		case PARAM_LINEAR_ACCEL: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->linear_accel_texture, tex_rid);
			_adjust_curve_range(p_texture, -200, 200);
		} break;
		case PARAM_RADIAL_ACCEL: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->radial_accel_texture, tex_rid);
			_adjust_curve_range(p_texture, -200, 200);
		} break;
		case PARAM_TANGENTIAL_ACCEL: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->tangent_accel_texture, tex_rid);
			_adjust_curve_range(p_texture, -200, 200);
		} break;
		case PARAM_DAMPING: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->damping_texture, tex_rid);
			_adjust_curve_range(p_texture, 0, 100);
		} break;
		case PARAM_ANGLE: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->angle_texture, tex_rid);
			_adjust_curve_range(p_texture, -360, 360);
		} break;
		case PARAM_SCALE: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->scale_texture, tex_rid);
			_adjust_curve_range(p_texture, 0, 1);
		} break;
		case PARAM_HUE_VARIATION: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->hue_variation_texture, tex_rid);
			_adjust_curve_range(p_texture, -1, 1);
		} break;
		case PARAM_ANIM_SPEED: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->anim_speed_texture, tex_rid);
			_adjust_curve_range(p_texture, 0, 200);
		} break;
		case PARAM_ANIM_OFFSET: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->anim_offset_texture, tex_rid);
		} break;
		case PARAM_TURB_INFLUENCE_OVER_LIFE: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->turbulence_influence_over_life, tex_rid);
			_adjust_curve_range(p_texture, 0, 1);
		} break;
		case PARAM_TURB_VEL_INFLUENCE: {
			// Can't happen, but silences warning
		} break;
		case PARAM_TURB_INIT_DISPLACEMENT: {
			// Can't happen, but silences warning
		} break;
		case PARAM_RADIAL_VELOCITY: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->radial_velocity_texture, tex_rid);
		} break;
		case PARAM_SCALE_OVER_VELOCITY: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->scale_over_velocity_texture, tex_rid);
			_adjust_curve_range(p_texture, 0, 3);
			notify_property_list_changed();
		} break;
		case PARAM_DIRECTIONAL_VELOCITY: {
			RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->directional_velocity_texture, tex_rid);
			notify_property_list_changed();
		} break;
		case PARAM_MAX:
			break; // Can't happen, but silences warning
	}

	_queue_shader_change();
}

Ref<Texture2D> ParticleProcessMaterial::get_param_texture(Parameter p_param) const {
	ERR_FAIL_INDEX_V(p_param, PARAM_MAX, Ref<Texture2D>());

	return tex_parameters[p_param];
}

void ParticleProcessMaterial::set_color(const Color &p_color) {
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->color, p_color);
	color = p_color;
}

Color ParticleProcessMaterial::get_color() const {
	return color;
}

void ParticleProcessMaterial::set_color_ramp(const Ref<Texture2D> &p_texture) {
	color_ramp = p_texture;
	Variant tex_rid = p_texture.is_valid() ? Variant(p_texture->get_rid()) : Variant();
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->color_ramp, tex_rid);
	_queue_shader_change();
	notify_property_list_changed();
}

Ref<Texture2D> ParticleProcessMaterial::get_color_ramp() const {
	return color_ramp;
}

void ParticleProcessMaterial::set_color_initial_ramp(const Ref<Texture2D> &p_texture) {
	color_initial_ramp = p_texture;
	Variant tex_rid = p_texture.is_valid() ? Variant(p_texture->get_rid()) : Variant();
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->color_initial_ramp, tex_rid);
	_queue_shader_change();
	notify_property_list_changed();
}

Ref<Texture2D> ParticleProcessMaterial::get_color_initial_ramp() const {
	return color_initial_ramp;
}

void ParticleProcessMaterial::set_particle_flag(ParticleFlags p_particle_flag, bool p_enable) {
	ERR_FAIL_INDEX(p_particle_flag, PARTICLE_FLAG_MAX);
	particle_flags[p_particle_flag] = p_enable;
	_queue_shader_change();
	if (p_particle_flag == PARTICLE_FLAG_DISABLE_Z) {
		notify_property_list_changed();
	}
}

void ParticleProcessMaterial::set_alpha_curve(const Ref<Texture2D> &p_texture) {
	alpha_curve = p_texture;
	Variant tex_rid = p_texture.is_valid() ? Variant(p_texture->get_rid()) : Variant();
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->alpha_ramp, tex_rid);
	_queue_shader_change();
	notify_property_list_changed();
}

Ref<Texture2D> ParticleProcessMaterial::get_alpha_curve() const {
	return alpha_curve;
}

void ParticleProcessMaterial::set_emission_curve(const Ref<Texture2D> &p_texture) {
	emission_curve = p_texture;
	Variant tex_rid = p_texture.is_valid() ? Variant(p_texture->get_rid()) : Variant();
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->emission_ramp, tex_rid);
	_queue_shader_change();
	notify_property_list_changed();
}

Ref<Texture2D> ParticleProcessMaterial::get_emission_curve() const {
	return emission_curve;
}

void ParticleProcessMaterial::set_velocity_limit_curve(const Ref<Texture2D> &p_texture) {
	velocity_limit_curve = p_texture;
	Variant tex_rid = p_texture.is_valid() ? Variant(p_texture->get_rid()) : Variant();
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->velocity_limit_curve, tex_rid);
	_queue_shader_change();
	notify_property_list_changed();
}

Ref<Texture2D> ParticleProcessMaterial::get_velocity_limit_curve() const {
	return velocity_limit_curve;
}

bool ParticleProcessMaterial::get_particle_flag(ParticleFlags p_particle_flag) const {
	ERR_FAIL_INDEX_V(p_particle_flag, PARTICLE_FLAG_MAX, false);
	return particle_flags[p_particle_flag];
}

void ParticleProcessMaterial::set_emission_shape(EmissionShape p_shape) {
	ERR_FAIL_INDEX(p_shape, EMISSION_SHAPE_MAX);
	emission_shape = p_shape;
	notify_property_list_changed();
	_queue_shader_change();
#ifdef TOOLS_ENABLED
	emit_signal("emission_shape_changed");
#endif
}

void ParticleProcessMaterial::set_emission_sphere_radius(real_t p_radius) {
	emission_sphere_radius = p_radius;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->emission_sphere_radius, p_radius);
#ifdef TOOLS_ENABLED
	emit_signal("emission_shape_changed");
#endif
}

void ParticleProcessMaterial::set_emission_box_extents(Vector3 p_extents) {
	emission_box_extents = p_extents;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->emission_box_extents, p_extents);
#ifdef TOOLS_ENABLED
	emit_signal("emission_shape_changed");
#endif
}

void ParticleProcessMaterial::set_emission_point_texture(const Ref<Texture2D> &p_points) {
	emission_point_texture = p_points;
	Variant tex_rid = p_points.is_valid() ? Variant(p_points->get_rid()) : Variant();
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->emission_texture_points, tex_rid);
}

void ParticleProcessMaterial::set_emission_normal_texture(const Ref<Texture2D> &p_normals) {
	emission_normal_texture = p_normals;
	Variant tex_rid = p_normals.is_valid() ? Variant(p_normals->get_rid()) : Variant();
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->emission_texture_normal, tex_rid);
}

void ParticleProcessMaterial::set_emission_color_texture(const Ref<Texture2D> &p_colors) {
	emission_color_texture = p_colors;
	Variant tex_rid = p_colors.is_valid() ? Variant(p_colors->get_rid()) : Variant();
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->emission_texture_color, tex_rid);
	_queue_shader_change();
}

void ParticleProcessMaterial::set_emission_point_count(int p_count) {
	emission_point_count = p_count;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->emission_texture_point_count, p_count);
}

void ParticleProcessMaterial::set_emission_ring_axis(Vector3 p_axis) {
	emission_ring_axis = p_axis;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->emission_ring_axis, p_axis);
#ifdef TOOLS_ENABLED
	emit_signal("emission_shape_changed");
#endif
}

void ParticleProcessMaterial::set_emission_ring_height(real_t p_height) {
	emission_ring_height = p_height;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->emission_ring_height, p_height);
#ifdef TOOLS_ENABLED
	emit_signal("emission_shape_changed");
#endif
}

void ParticleProcessMaterial::set_emission_ring_radius(real_t p_radius) {
	emission_ring_radius = p_radius;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->emission_ring_radius, p_radius);
#ifdef TOOLS_ENABLED
	emit_signal("emission_shape_changed");
#endif
}

void ParticleProcessMaterial::set_emission_ring_inner_radius(real_t p_radius) {
	emission_ring_inner_radius = p_radius;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->emission_ring_inner_radius, p_radius);
#ifdef TOOLS_ENABLED
	emit_signal("emission_shape_changed");
#endif
}

void ParticleProcessMaterial::set_emission_ring_cone_angle(real_t p_angle) {
	emission_ring_cone_angle = p_angle;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->emission_ring_cone_angle, p_angle);
#ifdef TOOLS_ENABLED
	emit_signal("emission_shape_changed");
#endif
}

void ParticleProcessMaterial::set_inherit_velocity_ratio(double p_ratio) {
	inherit_emitter_velocity_ratio = p_ratio;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->inherit_emitter_velocity_ratio, p_ratio);
}

ParticleProcessMaterial::EmissionShape ParticleProcessMaterial::get_emission_shape() const {
	return emission_shape;
}

real_t ParticleProcessMaterial::get_emission_sphere_radius() const {
	return emission_sphere_radius;
}

Vector3 ParticleProcessMaterial::get_emission_box_extents() const {
	return emission_box_extents;
}

Ref<Texture2D> ParticleProcessMaterial::get_emission_point_texture() const {
	return emission_point_texture;
}

Ref<Texture2D> ParticleProcessMaterial::get_emission_normal_texture() const {
	return emission_normal_texture;
}

Ref<Texture2D> ParticleProcessMaterial::get_emission_color_texture() const {
	return emission_color_texture;
}

int ParticleProcessMaterial::get_emission_point_count() const {
	return emission_point_count;
}

Vector3 ParticleProcessMaterial::get_emission_ring_axis() const {
	return emission_ring_axis;
}

real_t ParticleProcessMaterial::get_emission_ring_height() const {
	return emission_ring_height;
}

real_t ParticleProcessMaterial::get_emission_ring_radius() const {
	return emission_ring_radius;
}

real_t ParticleProcessMaterial::get_emission_ring_inner_radius() const {
	return emission_ring_inner_radius;
}

real_t ParticleProcessMaterial::get_emission_ring_cone_angle() const {
	return emission_ring_cone_angle;
}

void ParticleProcessMaterial::set_emission_shape_offset(const Vector3 &p_emission_shape_offset) {
	emission_shape_offset = p_emission_shape_offset;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->emission_shape_offset, p_emission_shape_offset);
#ifdef TOOLS_ENABLED
	emit_signal("emission_shape_changed");
#endif
}

Vector3 ParticleProcessMaterial::get_emission_shape_offset() const {
	return emission_shape_offset;
}

void ParticleProcessMaterial::set_emission_shape_scale(const Vector3 &p_emission_shape_scale) {
	emission_shape_scale = p_emission_shape_scale;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->emission_shape_scale, p_emission_shape_scale);
#ifdef TOOLS_ENABLED
	emit_signal("emission_shape_changed");
#endif
}

Vector3 ParticleProcessMaterial::get_emission_shape_scale() const {
	return emission_shape_scale;
}

double ParticleProcessMaterial::get_inherit_velocity_ratio() {
	return inherit_emitter_velocity_ratio;
}

void ParticleProcessMaterial::set_turbulence_enabled(const bool p_turbulence_enabled) {
	turbulence_enabled = p_turbulence_enabled;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->turbulence_enabled, turbulence_enabled);
	_queue_shader_change();
	notify_property_list_changed();
}

bool ParticleProcessMaterial::get_turbulence_enabled() const {
	return turbulence_enabled;
}

void ParticleProcessMaterial::set_turbulence_noise_strength(float p_turbulence_noise_strength) {
	turbulence_noise_strength = p_turbulence_noise_strength;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->turbulence_noise_strength, p_turbulence_noise_strength);
}

float ParticleProcessMaterial::get_turbulence_noise_strength() const {
	return turbulence_noise_strength;
}

void ParticleProcessMaterial::set_turbulence_noise_scale(float p_turbulence_noise_scale) {
	turbulence_noise_scale = p_turbulence_noise_scale;
	const float noise_frequency_when_slider_is_zero = 4.0;
	const float max_slider_value = 10.0;
	const float curve_exponent = 0.25;
	const float curve_rescale = noise_frequency_when_slider_is_zero / pow(max_slider_value, curve_exponent);
	float shader_turbulence_noise_scale = pow(p_turbulence_noise_scale, curve_exponent) * curve_rescale - noise_frequency_when_slider_is_zero;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->turbulence_noise_scale, shader_turbulence_noise_scale);
}

float ParticleProcessMaterial::get_turbulence_noise_scale() const {
	return turbulence_noise_scale;
}

void ParticleProcessMaterial::set_turbulence_noise_speed_random(float p_turbulence_noise_speed_random) {
	turbulence_noise_speed_random = p_turbulence_noise_speed_random;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->turbulence_noise_speed_random, p_turbulence_noise_speed_random);
}

float ParticleProcessMaterial::get_turbulence_noise_speed_random() const {
	return turbulence_noise_speed_random;
}

void ParticleProcessMaterial::set_turbulence_noise_speed(const Vector3 &p_turbulence_noise_speed) {
	turbulence_noise_speed = p_turbulence_noise_speed;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->turbulence_noise_speed, turbulence_noise_speed);
}

Vector3 ParticleProcessMaterial::get_turbulence_noise_speed() const {
	return turbulence_noise_speed;
}

void ParticleProcessMaterial::set_gravity(const Vector3 &p_gravity) {
	gravity = p_gravity;
	Vector3 gset = gravity;
	if (gset == Vector3()) {
		gset = Vector3(0, -0.000001, 0); //as gravity is used as upvector in some calculations
	}
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->gravity, gset);
}

Vector3 ParticleProcessMaterial::get_gravity() const {
	return gravity;
}

void ParticleProcessMaterial::set_lifetime_randomness(double p_lifetime) {
	lifetime_randomness = p_lifetime;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->lifetime_randomness, lifetime_randomness);
}

double ParticleProcessMaterial::get_lifetime_randomness() const {
	return lifetime_randomness;
}

RID ParticleProcessMaterial::get_shader_rid() const {
	ERR_FAIL_COND_V(!shader_map.has(current_key), RID());
	return shader_map[current_key].shader;
}

void ParticleProcessMaterial::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "emission_sphere_radius" && (emission_shape != EMISSION_SHAPE_SPHERE && emission_shape != EMISSION_SHAPE_SPHERE_SURFACE)) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}

	if (p_property.name == "emission_box_extents" && emission_shape != EMISSION_SHAPE_BOX) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}

	if ((p_property.name == "emission_point_texture" || p_property.name == "emission_color_texture") && (emission_shape != EMISSION_SHAPE_POINTS && emission_shape != EMISSION_SHAPE_DIRECTED_POINTS)) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}

	if (p_property.name == "emission_normal_texture" && emission_shape != EMISSION_SHAPE_DIRECTED_POINTS) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}

	if (p_property.name == "emission_point_count" && (emission_shape != EMISSION_SHAPE_POINTS && emission_shape != EMISSION_SHAPE_DIRECTED_POINTS)) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}

	if (p_property.name.begins_with("emission_ring_") && emission_shape != EMISSION_SHAPE_RING) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}

	if (p_property.name == "sub_emitter_frequency" && sub_emitter_mode != SUB_EMITTER_CONSTANT) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}

	if (p_property.name == "sub_emitter_amount_at_end" && sub_emitter_mode != SUB_EMITTER_AT_END) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}

	if (p_property.name == "sub_emitter_amount_at_collision" && sub_emitter_mode != SUB_EMITTER_AT_COLLISION) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}

	if (!turbulence_enabled) {
		if (p_property.name == "turbulence_noise_strength" ||
				p_property.name == "turbulence_noise_scale" ||
				p_property.name == "turbulence_noise_speed" ||
				p_property.name == "turbulence_noise_speed_random" ||
				p_property.name == "turbulence_influence_over_life" ||
				p_property.name == "turbulence_influence" ||
				p_property.name == "turbulence_initial_displacement") {
			p_property.usage &= ~PROPERTY_USAGE_EDITOR;
		}
	}

	if (p_property.name == "collision_friction" && collision_mode != COLLISION_RIGID) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}

	if (p_property.name == "collision_bounce" && collision_mode != COLLISION_RIGID) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
	if ((p_property.name == "directional_velocity_min" || p_property.name == "directional_velocity_max") && !tex_parameters[PARAM_DIRECTIONAL_VELOCITY].is_valid()) {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
	if ((p_property.name == "scale_over_velocity_min" || p_property.name == "scale_over_velocity_max") && !tex_parameters[PARAM_SCALE_OVER_VELOCITY].is_valid()) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
	if ((p_property.name == "orbit_velocity_min" || p_property.name == "orbit_velocity_max") && (!tex_parameters[PARAM_ORBIT_VELOCITY].is_valid() && !particle_flags[PARTICLE_FLAG_DISABLE_Z])) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}

	if (p_property.usage & PROPERTY_USAGE_EDITOR && (p_property.name.ends_with("_min") || p_property.name.ends_with("_max"))) {
		p_property.usage &= ~PROPERTY_USAGE_EDITOR;
	}
}

void ParticleProcessMaterial::set_sub_emitter_mode(SubEmitterMode p_sub_emitter_mode) {
	sub_emitter_mode = p_sub_emitter_mode;
	_queue_shader_change();
	notify_property_list_changed();
	if (sub_emitter_mode != SUB_EMITTER_DISABLED && RenderingServer::get_singleton()->is_low_end()) {
		WARN_PRINT_ONCE_ED("Sub-emitter modes other than SUB_EMITTER_DISABLED are not supported in the Compatibility renderer.");
	}
}

ParticleProcessMaterial::SubEmitterMode ParticleProcessMaterial::get_sub_emitter_mode() const {
	return sub_emitter_mode;
}

void ParticleProcessMaterial::set_sub_emitter_frequency(double p_frequency) {
	sub_emitter_frequency = p_frequency;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->sub_emitter_frequency, 1.0 / p_frequency); //pass delta instead of frequency, since its easier to compute
}

double ParticleProcessMaterial::get_sub_emitter_frequency() const {
	return sub_emitter_frequency;
}

void ParticleProcessMaterial::set_sub_emitter_amount_at_end(int p_amount) {
	sub_emitter_amount_at_end = p_amount;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->sub_emitter_amount_at_end, p_amount);
}

int ParticleProcessMaterial::get_sub_emitter_amount_at_end() const {
	return sub_emitter_amount_at_end;
}

void ParticleProcessMaterial::set_sub_emitter_amount_at_collision(int p_amount) {
	sub_emitter_amount_at_collision = p_amount;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->sub_emitter_amount_at_collision, p_amount);
}

int ParticleProcessMaterial::get_sub_emitter_amount_at_collision() const {
	return sub_emitter_amount_at_collision;
}

void ParticleProcessMaterial::set_sub_emitter_keep_velocity(bool p_enable) {
	sub_emitter_keep_velocity = p_enable;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->sub_emitter_keep_velocity, p_enable);
}
bool ParticleProcessMaterial::get_sub_emitter_keep_velocity() const {
	return sub_emitter_keep_velocity;
}

void ParticleProcessMaterial::set_attractor_interaction_enabled(bool p_enable) {
	attractor_interaction_enabled = p_enable;
	_queue_shader_change();
}

bool ParticleProcessMaterial::is_attractor_interaction_enabled() const {
	return attractor_interaction_enabled;
}

void ParticleProcessMaterial::set_collision_mode(CollisionMode p_collision_mode) {
	collision_mode = p_collision_mode;
	_queue_shader_change();
	notify_property_list_changed();
}

ParticleProcessMaterial::CollisionMode ParticleProcessMaterial::get_collision_mode() const {
	return collision_mode;
}

void ParticleProcessMaterial::set_collision_use_scale(bool p_scale) {
	collision_scale = p_scale;
	_queue_shader_change();
}

bool ParticleProcessMaterial::is_collision_using_scale() const {
	return collision_scale;
}

void ParticleProcessMaterial::set_collision_friction(float p_friction) {
	collision_friction = p_friction;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->collision_friction, p_friction);
}

float ParticleProcessMaterial::get_collision_friction() const {
	return collision_friction;
}

void ParticleProcessMaterial::set_collision_bounce(float p_bounce) {
	collision_bounce = p_bounce;
	RenderingServer::get_singleton()->material_set_param(_get_material(), shader_names->collision_bounce, p_bounce);
}

float ParticleProcessMaterial::get_collision_bounce() const {
	return collision_bounce;
}

Shader::Mode ParticleProcessMaterial::get_shader_mode() const {
	return Shader::MODE_PARTICLES;
}

void ParticleProcessMaterial::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_direction", "degrees"), &ParticleProcessMaterial::set_direction);
	ClassDB::bind_method(D_METHOD("get_direction"), &ParticleProcessMaterial::get_direction);

	ClassDB::bind_method(D_METHOD("set_inherit_velocity_ratio", "ratio"), &ParticleProcessMaterial::set_inherit_velocity_ratio);
	ClassDB::bind_method(D_METHOD("get_inherit_velocity_ratio"), &ParticleProcessMaterial::get_inherit_velocity_ratio);

	ClassDB::bind_method(D_METHOD("set_spread", "degrees"), &ParticleProcessMaterial::set_spread);
	ClassDB::bind_method(D_METHOD("get_spread"), &ParticleProcessMaterial::get_spread);

	ClassDB::bind_method(D_METHOD("set_flatness", "amount"), &ParticleProcessMaterial::set_flatness);
	ClassDB::bind_method(D_METHOD("get_flatness"), &ParticleProcessMaterial::get_flatness);

	ClassDB::bind_method(D_METHOD("set_param", "param", "value"), &ParticleProcessMaterial::set_param);
	ClassDB::bind_method(D_METHOD("get_param", "param"), &ParticleProcessMaterial::get_param);

	ClassDB::bind_method(D_METHOD("set_param_min", "param", "value"), &ParticleProcessMaterial::set_param_min);
	ClassDB::bind_method(D_METHOD("get_param_min", "param"), &ParticleProcessMaterial::get_param_min);

	ClassDB::bind_method(D_METHOD("set_param_max", "param", "value"), &ParticleProcessMaterial::set_param_max);
	ClassDB::bind_method(D_METHOD("get_param_max", "param"), &ParticleProcessMaterial::get_param_max);

	ClassDB::bind_method(D_METHOD("set_param_texture", "param", "texture"), &ParticleProcessMaterial::set_param_texture);
	ClassDB::bind_method(D_METHOD("get_param_texture", "param"), &ParticleProcessMaterial::get_param_texture);

	ClassDB::bind_method(D_METHOD("set_color", "color"), &ParticleProcessMaterial::set_color);
	ClassDB::bind_method(D_METHOD("get_color"), &ParticleProcessMaterial::get_color);

	ClassDB::bind_method(D_METHOD("set_color_ramp", "ramp"), &ParticleProcessMaterial::set_color_ramp);
	ClassDB::bind_method(D_METHOD("get_color_ramp"), &ParticleProcessMaterial::get_color_ramp);
	ClassDB::bind_method(D_METHOD("set_alpha_curve", "curve"), &ParticleProcessMaterial::set_alpha_curve);
	ClassDB::bind_method(D_METHOD("get_alpha_curve"), &ParticleProcessMaterial::get_alpha_curve);

	ClassDB::bind_method(D_METHOD("set_emission_curve", "curve"), &ParticleProcessMaterial::set_emission_curve);
	ClassDB::bind_method(D_METHOD("get_emission_curve"), &ParticleProcessMaterial::get_emission_curve);

	ClassDB::bind_method(D_METHOD("set_color_initial_ramp", "ramp"), &ParticleProcessMaterial::set_color_initial_ramp);
	ClassDB::bind_method(D_METHOD("get_color_initial_ramp"), &ParticleProcessMaterial::get_color_initial_ramp);

	ClassDB::bind_method(D_METHOD("set_velocity_limit_curve", "curve"), &ParticleProcessMaterial::set_velocity_limit_curve);
	ClassDB::bind_method(D_METHOD("get_velocity_limit_curve"), &ParticleProcessMaterial::get_velocity_limit_curve);

	ClassDB::bind_method(D_METHOD("set_particle_flag", "particle_flag", "enable"), &ParticleProcessMaterial::set_particle_flag);
	ClassDB::bind_method(D_METHOD("get_particle_flag", "particle_flag"), &ParticleProcessMaterial::get_particle_flag);

	ClassDB::bind_method(D_METHOD("set_velocity_pivot", "pivot"), &ParticleProcessMaterial::set_velocity_pivot);
	ClassDB::bind_method(D_METHOD("get_velocity_pivot"), &ParticleProcessMaterial::get_velocity_pivot);

	ClassDB::bind_method(D_METHOD("set_emission_shape", "shape"), &ParticleProcessMaterial::set_emission_shape);
	ClassDB::bind_method(D_METHOD("get_emission_shape"), &ParticleProcessMaterial::get_emission_shape);

	ClassDB::bind_method(D_METHOD("set_emission_sphere_radius", "radius"), &ParticleProcessMaterial::set_emission_sphere_radius);
	ClassDB::bind_method(D_METHOD("get_emission_sphere_radius"), &ParticleProcessMaterial::get_emission_sphere_radius);

	ClassDB::bind_method(D_METHOD("set_emission_box_extents", "extents"), &ParticleProcessMaterial::set_emission_box_extents);
	ClassDB::bind_method(D_METHOD("get_emission_box_extents"), &ParticleProcessMaterial::get_emission_box_extents);

	ClassDB::bind_method(D_METHOD("set_emission_point_texture", "texture"), &ParticleProcessMaterial::set_emission_point_texture);
	ClassDB::bind_method(D_METHOD("get_emission_point_texture"), &ParticleProcessMaterial::get_emission_point_texture);

	ClassDB::bind_method(D_METHOD("set_emission_normal_texture", "texture"), &ParticleProcessMaterial::set_emission_normal_texture);
	ClassDB::bind_method(D_METHOD("get_emission_normal_texture"), &ParticleProcessMaterial::get_emission_normal_texture);

	ClassDB::bind_method(D_METHOD("set_emission_color_texture", "texture"), &ParticleProcessMaterial::set_emission_color_texture);
	ClassDB::bind_method(D_METHOD("get_emission_color_texture"), &ParticleProcessMaterial::get_emission_color_texture);

	ClassDB::bind_method(D_METHOD("set_emission_point_count", "point_count"), &ParticleProcessMaterial::set_emission_point_count);
	ClassDB::bind_method(D_METHOD("get_emission_point_count"), &ParticleProcessMaterial::get_emission_point_count);

	ClassDB::bind_method(D_METHOD("set_emission_ring_axis", "axis"), &ParticleProcessMaterial::set_emission_ring_axis);
	ClassDB::bind_method(D_METHOD("get_emission_ring_axis"), &ParticleProcessMaterial::get_emission_ring_axis);

	ClassDB::bind_method(D_METHOD("set_emission_ring_height", "height"), &ParticleProcessMaterial::set_emission_ring_height);
	ClassDB::bind_method(D_METHOD("get_emission_ring_height"), &ParticleProcessMaterial::get_emission_ring_height);

	ClassDB::bind_method(D_METHOD("set_emission_ring_radius", "radius"), &ParticleProcessMaterial::set_emission_ring_radius);
	ClassDB::bind_method(D_METHOD("get_emission_ring_radius"), &ParticleProcessMaterial::get_emission_ring_radius);

	ClassDB::bind_method(D_METHOD("set_emission_ring_inner_radius", "inner_radius"), &ParticleProcessMaterial::set_emission_ring_inner_radius);
	ClassDB::bind_method(D_METHOD("get_emission_ring_inner_radius"), &ParticleProcessMaterial::get_emission_ring_inner_radius);

	ClassDB::bind_method(D_METHOD("set_emission_ring_cone_angle", "cone_angle"), &ParticleProcessMaterial::set_emission_ring_cone_angle);
	ClassDB::bind_method(D_METHOD("get_emission_ring_cone_angle"), &ParticleProcessMaterial::get_emission_ring_cone_angle);

	ClassDB::bind_method(D_METHOD("set_emission_shape_offset", "emission_shape_offset"), &ParticleProcessMaterial::set_emission_shape_offset);
	ClassDB::bind_method(D_METHOD("get_emission_shape_offset"), &ParticleProcessMaterial::get_emission_shape_offset);

	ClassDB::bind_method(D_METHOD("set_emission_shape_scale", "emission_shape_scale"), &ParticleProcessMaterial::set_emission_shape_scale);
	ClassDB::bind_method(D_METHOD("get_emission_shape_scale"), &ParticleProcessMaterial::get_emission_shape_scale);

	ClassDB::bind_method(D_METHOD("get_turbulence_enabled"), &ParticleProcessMaterial::get_turbulence_enabled);
	ClassDB::bind_method(D_METHOD("set_turbulence_enabled", "turbulence_enabled"), &ParticleProcessMaterial::set_turbulence_enabled);

	ClassDB::bind_method(D_METHOD("get_turbulence_noise_strength"), &ParticleProcessMaterial::get_turbulence_noise_strength);
	ClassDB::bind_method(D_METHOD("set_turbulence_noise_strength", "turbulence_noise_strength"), &ParticleProcessMaterial::set_turbulence_noise_strength);

	ClassDB::bind_method(D_METHOD("get_turbulence_noise_scale"), &ParticleProcessMaterial::get_turbulence_noise_scale);
	ClassDB::bind_method(D_METHOD("set_turbulence_noise_scale", "turbulence_noise_scale"), &ParticleProcessMaterial::set_turbulence_noise_scale);

	ClassDB::bind_method(D_METHOD("get_turbulence_noise_speed_random"), &ParticleProcessMaterial::get_turbulence_noise_speed_random);
	ClassDB::bind_method(D_METHOD("set_turbulence_noise_speed_random", "turbulence_noise_speed_random"), &ParticleProcessMaterial::set_turbulence_noise_speed_random);

	ClassDB::bind_method(D_METHOD("get_turbulence_noise_speed"), &ParticleProcessMaterial::get_turbulence_noise_speed);
	ClassDB::bind_method(D_METHOD("set_turbulence_noise_speed", "turbulence_noise_speed"), &ParticleProcessMaterial::set_turbulence_noise_speed);

	ClassDB::bind_method(D_METHOD("get_gravity"), &ParticleProcessMaterial::get_gravity);
	ClassDB::bind_method(D_METHOD("set_gravity", "accel_vec"), &ParticleProcessMaterial::set_gravity);

	ClassDB::bind_method(D_METHOD("set_lifetime_randomness", "randomness"), &ParticleProcessMaterial::set_lifetime_randomness);
	ClassDB::bind_method(D_METHOD("get_lifetime_randomness"), &ParticleProcessMaterial::get_lifetime_randomness);

	ClassDB::bind_method(D_METHOD("get_sub_emitter_mode"), &ParticleProcessMaterial::get_sub_emitter_mode);
	ClassDB::bind_method(D_METHOD("set_sub_emitter_mode", "mode"), &ParticleProcessMaterial::set_sub_emitter_mode);

	ClassDB::bind_method(D_METHOD("get_sub_emitter_frequency"), &ParticleProcessMaterial::get_sub_emitter_frequency);
	ClassDB::bind_method(D_METHOD("set_sub_emitter_frequency", "hz"), &ParticleProcessMaterial::set_sub_emitter_frequency);

	ClassDB::bind_method(D_METHOD("get_sub_emitter_amount_at_end"), &ParticleProcessMaterial::get_sub_emitter_amount_at_end);
	ClassDB::bind_method(D_METHOD("set_sub_emitter_amount_at_end", "amount"), &ParticleProcessMaterial::set_sub_emitter_amount_at_end);

	ClassDB::bind_method(D_METHOD("get_sub_emitter_amount_at_collision"), &ParticleProcessMaterial::get_sub_emitter_amount_at_collision);
	ClassDB::bind_method(D_METHOD("set_sub_emitter_amount_at_collision", "amount"), &ParticleProcessMaterial::set_sub_emitter_amount_at_collision);

	ClassDB::bind_method(D_METHOD("get_sub_emitter_keep_velocity"), &ParticleProcessMaterial::get_sub_emitter_keep_velocity);
	ClassDB::bind_method(D_METHOD("set_sub_emitter_keep_velocity", "enable"), &ParticleProcessMaterial::set_sub_emitter_keep_velocity);

	ClassDB::bind_method(D_METHOD("set_attractor_interaction_enabled", "enabled"), &ParticleProcessMaterial::set_attractor_interaction_enabled);
	ClassDB::bind_method(D_METHOD("is_attractor_interaction_enabled"), &ParticleProcessMaterial::is_attractor_interaction_enabled);

	ClassDB::bind_method(D_METHOD("set_collision_mode", "mode"), &ParticleProcessMaterial::set_collision_mode);
	ClassDB::bind_method(D_METHOD("get_collision_mode"), &ParticleProcessMaterial::get_collision_mode);

	ClassDB::bind_method(D_METHOD("set_collision_use_scale", "radius"), &ParticleProcessMaterial::set_collision_use_scale);
	ClassDB::bind_method(D_METHOD("is_collision_using_scale"), &ParticleProcessMaterial::is_collision_using_scale);

	ClassDB::bind_method(D_METHOD("set_collision_friction", "friction"), &ParticleProcessMaterial::set_collision_friction);
	ClassDB::bind_method(D_METHOD("get_collision_friction"), &ParticleProcessMaterial::get_collision_friction);

	ClassDB::bind_method(D_METHOD("set_collision_bounce", "bounce"), &ParticleProcessMaterial::set_collision_bounce);
	ClassDB::bind_method(D_METHOD("get_collision_bounce"), &ParticleProcessMaterial::get_collision_bounce);

#define ADD_MIN_MAX_PROPERTY(m_property, m_range, m_parameter_name)                                                                                                                       \
	ADD_PROPERTYI(PropertyInfo(Variant::VECTOR2, m_property, PROPERTY_HINT_RANGE, m_range, PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_INTERNAL), "set_param", "get_param", m_parameter_name); \
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, m_property "_min", PROPERTY_HINT_RANGE, m_range), "set_param_min", "get_param_min", m_parameter_name);                                     \
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, m_property "_max", PROPERTY_HINT_RANGE, m_range), "set_param_max", "get_param_max", m_parameter_name);                                     \
	min_max_properties.insert(m_property);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lifetime_randomness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_lifetime_randomness", "get_lifetime_randomness");
	ADD_GROUP("Particle Flags", "particle_flag_");
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "particle_flag_align_y"), "set_particle_flag", "get_particle_flag", PARTICLE_FLAG_ALIGN_Y_TO_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "particle_flag_rotate_y"), "set_particle_flag", "get_particle_flag", PARTICLE_FLAG_ROTATE_Y);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "particle_flag_disable_z"), "set_particle_flag", "get_particle_flag", PARTICLE_FLAG_DISABLE_Z);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "particle_flag_damping_as_friction"), "set_particle_flag", "get_particle_flag", PARTICLE_FLAG_DAMPING_AS_FRICTION);
	ADD_GROUP("Spawn", "");
	ADD_SUBGROUP("Position", "");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "emission_shape_offset"), "set_emission_shape_offset", "get_emission_shape_offset");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "emission_shape_scale"), "set_emission_shape_scale", "get_emission_shape_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "emission_shape", PROPERTY_HINT_ENUM, "Point,Sphere,Sphere Surface,Box,Points,Directed Points,Ring"), "set_emission_shape", "get_emission_shape");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "emission_sphere_radius", PROPERTY_HINT_RANGE, "0.01,128,0.01,or_greater"), "set_emission_sphere_radius", "get_emission_sphere_radius");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "emission_box_extents"), "set_emission_box_extents", "get_emission_box_extents");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "emission_point_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_emission_point_texture", "get_emission_point_texture");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "emission_normal_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_emission_normal_texture", "get_emission_normal_texture");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "emission_color_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_emission_color_texture", "get_emission_color_texture");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "emission_point_count", PROPERTY_HINT_RANGE, "0,1000000,1"), "set_emission_point_count", "get_emission_point_count");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "emission_ring_axis"), "set_emission_ring_axis", "get_emission_ring_axis");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "emission_ring_height", PROPERTY_HINT_RANGE, "0,1000,0.01,or_greater"), "set_emission_ring_height", "get_emission_ring_height");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "emission_ring_radius", PROPERTY_HINT_RANGE, "0,1000,0.01,or_greater"), "set_emission_ring_radius", "get_emission_ring_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "emission_ring_inner_radius", PROPERTY_HINT_RANGE, "0,1000,0.01,or_greater"), "set_emission_ring_inner_radius", "get_emission_ring_inner_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "emission_ring_cone_angle", PROPERTY_HINT_RANGE, "0,90,0.01,degrees"), "set_emission_ring_cone_angle", "get_emission_ring_cone_angle");
	ADD_SUBGROUP("Angle", "");
	ADD_MIN_MAX_PROPERTY("angle", "-720,720,0.1,or_less,or_greater,degrees", PARAM_ANGLE);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "angle_curve", PROPERTY_HINT_RESOURCE_TYPE, "CurveTexture"), "set_param_texture", "get_param_texture", PARAM_ANGLE);
	ADD_SUBGROUP("Velocity", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "inherit_velocity_ratio", PROPERTY_HINT_RANGE, "0.0,1.0,0.001,or_less,or_greater"), "set_inherit_velocity_ratio", "get_inherit_velocity_ratio");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "velocity_pivot"), "set_velocity_pivot", "get_velocity_pivot");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "direction"), "set_direction", "get_direction");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spread", PROPERTY_HINT_RANGE, "0,180,0.001"), "set_spread", "get_spread");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "flatness", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_flatness", "get_flatness");
	ADD_MIN_MAX_PROPERTY("initial_velocity", "0,1000,0.01,or_less,or_greater", PARAM_INITIAL_LINEAR_VELOCITY);
	ADD_GROUP("Animated Velocity", "");
	ADD_SUBGROUP("Velocity Limit", "");
	ADD_SUBGROUP("Angular Velocity", "angular_");
	ADD_MIN_MAX_PROPERTY("angular_velocity", "-720,720,0.01,or_less,or_greater", PARAM_ANGULAR_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "angular_velocity_curve", PROPERTY_HINT_RESOURCE_TYPE, "CurveTexture"), "set_param_texture", "get_param_texture", PARAM_ANGULAR_VELOCITY);
	ADD_SUBGROUP("Directional Velocity", "directional_");
	ADD_MIN_MAX_PROPERTY("directional_velocity", "-720,720,0.01,or_less,or_greater", PARAM_DIRECTIONAL_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "directional_velocity_curve", PROPERTY_HINT_RESOURCE_TYPE, "CurveXYZTexture"), "set_param_texture", "get_param_texture", PARAM_DIRECTIONAL_VELOCITY);
	ADD_SUBGROUP("Orbit Velocity", "orbit_");
	ADD_MIN_MAX_PROPERTY("orbit_velocity", "-2,2,0.001,or_less,or_greater", PARAM_ORBIT_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "orbit_velocity_curve", PROPERTY_HINT_RESOURCE_TYPE, "CurveTexture,CurveXYZTexture"), "set_param_texture", "get_param_texture", PARAM_ORBIT_VELOCITY);
	ADD_SUBGROUP("Radial Velocity", "radial_");
	ADD_MIN_MAX_PROPERTY("radial_velocity", "-1000,1000,0.01,or_less,or_greater", PARAM_RADIAL_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "radial_velocity_curve", PROPERTY_HINT_RESOURCE_TYPE, "CurveTexture"), "set_param_texture", "get_param_texture", PARAM_RADIAL_VELOCITY);
	ADD_SUBGROUP("Velocity Limit", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "velocity_limit_curve", PROPERTY_HINT_RESOURCE_TYPE, "CurveTexture"), "set_velocity_limit_curve", "get_velocity_limit_curve");
	ADD_GROUP("Accelerations", "");
	ADD_SUBGROUP("Gravity", "");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "gravity"), "set_gravity", "get_gravity");
	ADD_SUBGROUP("Linear Accel", "linear_");
	ADD_MIN_MAX_PROPERTY("linear_accel", "-100,100,0.01,or_less,or_greater", PARAM_LINEAR_ACCEL);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "linear_accel_curve", PROPERTY_HINT_RESOURCE_TYPE, "CurveTexture"), "set_param_texture", "get_param_texture", PARAM_LINEAR_ACCEL);
	ADD_SUBGROUP("Radial Accel", "radial_");
	ADD_MIN_MAX_PROPERTY("radial_accel", "-100,100,0.01,or_less,or_greater", PARAM_RADIAL_ACCEL);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "radial_accel_curve", PROPERTY_HINT_RESOURCE_TYPE, "CurveTexture"), "set_param_texture", "get_param_texture", PARAM_RADIAL_ACCEL);
	ADD_SUBGROUP("Tangential Accel", "tangential_");
	ADD_MIN_MAX_PROPERTY("tangential_accel", "-100,100,0.01,or_less,or_greater", PARAM_TANGENTIAL_ACCEL);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "tangential_accel_curve", PROPERTY_HINT_RESOURCE_TYPE, "CurveTexture"), "set_param_texture", "get_param_texture", PARAM_TANGENTIAL_ACCEL);
	ADD_SUBGROUP("Damping", "");
	ADD_MIN_MAX_PROPERTY("damping", "0,100,0.001,or_greater", PARAM_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "damping_curve", PROPERTY_HINT_RESOURCE_TYPE, "CurveTexture"), "set_param_texture", "get_param_texture", PARAM_DAMPING);
	ADD_SUBGROUP("Attractor Interaction", "attractor_interaction_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "attractor_interaction_enabled"), "set_attractor_interaction_enabled", "is_attractor_interaction_enabled");

	ADD_GROUP("Display", "");
	ADD_SUBGROUP("Scale", "");
	ADD_MIN_MAX_PROPERTY("scale", "0,1000,0.01,or_greater", PARAM_SCALE);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "scale_curve", PROPERTY_HINT_RESOURCE_TYPE, "CurveTexture,CurveXYZTexture"), "set_param_texture", "get_param_texture", PARAM_SCALE);
	ADD_SUBGROUP("Scale Over Velocity", "");
	ADD_MIN_MAX_PROPERTY("scale_over_velocity", "0,1000,0.01,or_greater", PARAM_SCALE_OVER_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "scale_over_velocity_curve", PROPERTY_HINT_RESOURCE_TYPE, "CurveTexture,CurveXYZTexture"), "set_param_texture", "get_param_texture", PARAM_SCALE_OVER_VELOCITY);

	ADD_SUBGROUP("Color Curves", "");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_color", "get_color");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "color_ramp", PROPERTY_HINT_RESOURCE_TYPE, "GradientTexture1D"), "set_color_ramp", "get_color_ramp");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "color_initial_ramp", PROPERTY_HINT_RESOURCE_TYPE, "GradientTexture1D"), "set_color_initial_ramp", "get_color_initial_ramp");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "alpha_curve", PROPERTY_HINT_RESOURCE_TYPE, "CurveTexture"), "set_alpha_curve", "get_alpha_curve");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "emission_curve", PROPERTY_HINT_RESOURCE_TYPE, "CurveTexture"), "set_emission_curve", "get_emission_curve");
	ADD_SUBGROUP("Hue Variation", "hue_");
	ADD_MIN_MAX_PROPERTY("hue_variation", "-1,1,0.01", PARAM_HUE_VARIATION);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "hue_variation_curve", PROPERTY_HINT_RESOURCE_TYPE, "CurveTexture"), "set_param_texture", "get_param_texture", PARAM_HUE_VARIATION);
	ADD_SUBGROUP("Animation", "anim_");
	ADD_MIN_MAX_PROPERTY("anim_speed", "0,16,0.01,or_less,or_greater", PARAM_ANIM_SPEED);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "anim_speed_curve", PROPERTY_HINT_RESOURCE_TYPE, "CurveTexture"), "set_param_texture", "get_param_texture", PARAM_ANIM_SPEED);
	ADD_MIN_MAX_PROPERTY("anim_offset", "0,1,0.0001", PARAM_ANIM_OFFSET);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "anim_offset_curve", PROPERTY_HINT_RESOURCE_TYPE, "CurveTexture"), "set_param_texture", "get_param_texture", PARAM_ANIM_OFFSET);

	ADD_GROUP("Turbulence", "turbulence_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "turbulence_enabled"), "set_turbulence_enabled", "get_turbulence_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "turbulence_noise_strength", PROPERTY_HINT_RANGE, "0,20,0.01"), "set_turbulence_noise_strength", "get_turbulence_noise_strength");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "turbulence_noise_scale", PROPERTY_HINT_RANGE, "0,10,0.001,or_greater"), "set_turbulence_noise_scale", "get_turbulence_noise_scale");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "turbulence_noise_speed"), "set_turbulence_noise_speed", "get_turbulence_noise_speed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "turbulence_noise_speed_random", PROPERTY_HINT_RANGE, "0,4,0.01"), "set_turbulence_noise_speed_random", "get_turbulence_noise_speed_random");
	ADD_MIN_MAX_PROPERTY("turbulence_influence", "0,1,0.001", PARAM_TURB_VEL_INFLUENCE);
	ADD_MIN_MAX_PROPERTY("turbulence_initial_displacement", "-100,100,0.1", PARAM_TURB_INIT_DISPLACEMENT);
	ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "turbulence_influence_over_life", PROPERTY_HINT_RESOURCE_TYPE, "CurveTexture"), "set_param_texture", "get_param_texture", PARAM_TURB_INFLUENCE_OVER_LIFE);

	ADD_GROUP("Collision", "collision_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mode", PROPERTY_HINT_ENUM, "Disabled,Rigid,Hide On Contact"), "set_collision_mode", "get_collision_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_friction", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_collision_friction", "get_collision_friction");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_bounce", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_collision_bounce", "get_collision_bounce");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collision_use_scale"), "set_collision_use_scale", "is_collision_using_scale");
	ADD_GROUP("Sub Emitter", "sub_emitter_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sub_emitter_mode", PROPERTY_HINT_ENUM, "Disabled,Constant,At End,At Collision"), "set_sub_emitter_mode", "get_sub_emitter_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sub_emitter_frequency", PROPERTY_HINT_RANGE, "0.01,100,0.01,suffix:Hz"), "set_sub_emitter_frequency", "get_sub_emitter_frequency");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sub_emitter_amount_at_end", PROPERTY_HINT_RANGE, "1,32,1"), "set_sub_emitter_amount_at_end", "get_sub_emitter_amount_at_end");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sub_emitter_amount_at_collision", PROPERTY_HINT_RANGE, "1,32,1"), "set_sub_emitter_amount_at_collision", "get_sub_emitter_amount_at_collision");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "sub_emitter_keep_velocity"), "set_sub_emitter_keep_velocity", "get_sub_emitter_keep_velocity");

	ADD_SIGNAL(MethodInfo("emission_shape_changed"));

	BIND_ENUM_CONSTANT(PARAM_INITIAL_LINEAR_VELOCITY);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_VELOCITY);
	BIND_ENUM_CONSTANT(PARAM_ORBIT_VELOCITY);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_ACCEL);
	BIND_ENUM_CONSTANT(PARAM_RADIAL_ACCEL);
	BIND_ENUM_CONSTANT(PARAM_TANGENTIAL_ACCEL);
	BIND_ENUM_CONSTANT(PARAM_DAMPING);
	BIND_ENUM_CONSTANT(PARAM_ANGLE);
	BIND_ENUM_CONSTANT(PARAM_SCALE);
	BIND_ENUM_CONSTANT(PARAM_HUE_VARIATION);
	BIND_ENUM_CONSTANT(PARAM_ANIM_SPEED);
	BIND_ENUM_CONSTANT(PARAM_ANIM_OFFSET);
	BIND_ENUM_CONSTANT(PARAM_RADIAL_VELOCITY);
	BIND_ENUM_CONSTANT(PARAM_DIRECTIONAL_VELOCITY);
	BIND_ENUM_CONSTANT(PARAM_SCALE_OVER_VELOCITY);
	BIND_ENUM_CONSTANT(PARAM_MAX);

	BIND_ENUM_CONSTANT(PARTICLE_FLAG_ALIGN_Y_TO_VELOCITY);
	BIND_ENUM_CONSTANT(PARTICLE_FLAG_ROTATE_Y);
	BIND_ENUM_CONSTANT(PARTICLE_FLAG_DISABLE_Z);
	BIND_ENUM_CONSTANT(PARTICLE_FLAG_DAMPING_AS_FRICTION);
	BIND_ENUM_CONSTANT(PARTICLE_FLAG_MAX);

	BIND_ENUM_CONSTANT(EMISSION_SHAPE_POINT);
	BIND_ENUM_CONSTANT(EMISSION_SHAPE_SPHERE);
	BIND_ENUM_CONSTANT(EMISSION_SHAPE_SPHERE_SURFACE);
	BIND_ENUM_CONSTANT(EMISSION_SHAPE_BOX);
	BIND_ENUM_CONSTANT(EMISSION_SHAPE_POINTS);
	BIND_ENUM_CONSTANT(EMISSION_SHAPE_DIRECTED_POINTS);
	BIND_ENUM_CONSTANT(EMISSION_SHAPE_RING);
	BIND_ENUM_CONSTANT(EMISSION_SHAPE_MAX);

	BIND_ENUM_CONSTANT(PARAM_TURB_VEL_INFLUENCE);
	BIND_ENUM_CONSTANT(PARAM_TURB_INIT_DISPLACEMENT);
	BIND_ENUM_CONSTANT(PARAM_TURB_INFLUENCE_OVER_LIFE);

	BIND_ENUM_CONSTANT(SUB_EMITTER_DISABLED);
	BIND_ENUM_CONSTANT(SUB_EMITTER_CONSTANT);
	BIND_ENUM_CONSTANT(SUB_EMITTER_AT_END);
	BIND_ENUM_CONSTANT(SUB_EMITTER_AT_COLLISION);
	BIND_ENUM_CONSTANT(SUB_EMITTER_MAX);

	BIND_ENUM_CONSTANT(COLLISION_DISABLED);
	BIND_ENUM_CONSTANT(COLLISION_RIGID);
	BIND_ENUM_CONSTANT(COLLISION_HIDE_ON_CONTACT);
	BIND_ENUM_CONSTANT(COLLISION_MAX);

#undef ADD_MIN_MAX_PROPERTY
}

ParticleProcessMaterial::ParticleProcessMaterial() :
		element(this) {
	_set_material(RS::get_singleton()->material_create());

	set_direction(Vector3(1, 0, 0));
	set_spread(45);
	set_flatness(0);
	set_param_min(PARAM_INITIAL_LINEAR_VELOCITY, 0);
	set_param_min(PARAM_ANGULAR_VELOCITY, 0);
	set_param_min(PARAM_ORBIT_VELOCITY, 0);
	set_param_min(PARAM_LINEAR_ACCEL, 0);
	set_param_min(PARAM_RADIAL_ACCEL, 0);
	set_param_min(PARAM_TANGENTIAL_ACCEL, 0);
	set_param_min(PARAM_DAMPING, 0);
	set_param_min(PARAM_ANGLE, 0);
	set_param_min(PARAM_SCALE, 1);
	set_param_min(PARAM_HUE_VARIATION, 0);
	set_param_min(PARAM_ANIM_SPEED, 0);
	set_param_min(PARAM_ANIM_OFFSET, 0);
	set_param_max(PARAM_INITIAL_LINEAR_VELOCITY, 0);
	set_param_max(PARAM_ANGULAR_VELOCITY, 0);
	set_param_max(PARAM_ORBIT_VELOCITY, 0);
	set_param_max(PARAM_LINEAR_ACCEL, 0);
	set_param_max(PARAM_RADIAL_ACCEL, 0);
	set_param_max(PARAM_TANGENTIAL_ACCEL, 0);
	set_param_max(PARAM_DAMPING, 0);
	set_param_max(PARAM_ANGLE, 0);
	set_param_max(PARAM_SCALE, 1);
	set_param_max(PARAM_HUE_VARIATION, 0);
	set_param_max(PARAM_ANIM_SPEED, 0);
	set_param_max(PARAM_ANIM_OFFSET, 0);
	set_param_min(PARAM_DIRECTIONAL_VELOCITY, 1.0);
	set_param_max(PARAM_DIRECTIONAL_VELOCITY, 1.0);
	set_emission_shape(EMISSION_SHAPE_POINT);
	set_emission_sphere_radius(1);
	set_emission_box_extents(Vector3(1, 1, 1));
	set_emission_ring_axis(Vector3(0, 0, 1.0));
	set_emission_ring_height(1);
	set_emission_ring_radius(1);
	set_emission_ring_inner_radius(0);
	set_emission_ring_cone_angle(90);
	set_emission_shape_offset(Vector3(0.0, 0.0, 0.0));
	set_emission_shape_scale(Vector3(1.0, 1.0, 1.0));

	set_turbulence_enabled(false);
	set_turbulence_noise_speed(Vector3(0.0, 0.0, 0.0));
	set_turbulence_noise_strength(1);
	set_turbulence_noise_scale(9);
	set_turbulence_noise_speed_random(0.2);
	set_param_min(PARAM_TURB_VEL_INFLUENCE, 0.1);
	set_param_max(PARAM_TURB_VEL_INFLUENCE, 0.1);
	set_param_min(PARAM_TURB_INIT_DISPLACEMENT, 0.0);
	set_param_max(PARAM_TURB_INIT_DISPLACEMENT, 0.0);

	set_gravity(Vector3(0, -9.8, 0));
	set_lifetime_randomness(0);

	set_sub_emitter_mode(SUB_EMITTER_DISABLED);
	set_sub_emitter_frequency(4);
	set_sub_emitter_amount_at_end(1);
	set_sub_emitter_amount_at_collision(1);
	set_sub_emitter_keep_velocity(false);

	set_attractor_interaction_enabled(true);
	set_collision_mode(COLLISION_DISABLED);
	set_collision_bounce(0.0);
	set_collision_friction(0.0);
	set_collision_use_scale(false);

	for (int i = 0; i < PARTICLE_FLAG_MAX; i++) {
		particle_flags[i] = false;
	}

	set_color(Color(1, 1, 1, 1));

	current_key.invalid_key = 1;

	_mark_initialized(callable_mp(this, &ParticleProcessMaterial::_queue_shader_change), callable_mp(this, &ParticleProcessMaterial::_update_shader));
}

ParticleProcessMaterial::~ParticleProcessMaterial() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	MutexLock lock(material_mutex);

	if (shader_map.has(current_key)) {
		shader_map[current_key].users--;
		if (shader_map[current_key].users == 0) {
			//deallocate shader, as it's no longer in use
			RS::get_singleton()->free(shader_map[current_key].shader);
			shader_map.erase(current_key);
		}

		RS::get_singleton()->material_set_shader(_get_material(), RID());
	}
}

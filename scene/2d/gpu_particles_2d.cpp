/**************************************************************************/
/*  gpu_particles_2d.cpp                                                  */
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

#include "gpu_particles_2d.h"

#include "scene/2d/cpu_particles_2d.h"
#include "scene/resources/atlas_texture.h"
#include "scene/resources/canvas_item_material.h"
#include "scene/resources/curve_texture.h"
#include "scene/resources/gradient_texture.h"
#include "scene/resources/particle_process_material.h"

void GPUParticles2D::set_emitting(bool p_emitting) {
	// Do not return even if `p_emitting == emitting` because `emitting` is just an approximation.

	if (p_emitting && one_shot) {
		if (!active && !emitting) {
			// Last cycle ended.
			active = true;
			time = 0;
			signal_canceled = false;
			emission_time = lifetime;
			active_time = lifetime * (2 - explosiveness_ratio);
		} else {
			signal_canceled = true;
		}
		set_process_internal(true);
	} else if (!p_emitting) {
		if (one_shot) {
			set_process_internal(true);
		} else {
			set_process_internal(false);
		}
	}

	emitting = p_emitting;
	RS::get_singleton()->particles_set_emitting(particles, p_emitting);
}

void GPUParticles2D::set_amount(int p_amount) {
	ERR_FAIL_COND_MSG(p_amount < 1, "Amount of particles cannot be smaller than 1.");
	amount = p_amount;
	RS::get_singleton()->particles_set_amount(particles, amount);
}

void GPUParticles2D::set_lifetime(double p_lifetime) {
	ERR_FAIL_COND_MSG(p_lifetime <= 0, "Particles lifetime must be greater than 0.");
	lifetime = p_lifetime;
	RS::get_singleton()->particles_set_lifetime(particles, lifetime);
}

void GPUParticles2D::set_one_shot(bool p_enable) {
	one_shot = p_enable;
	RS::get_singleton()->particles_set_one_shot(particles, one_shot);

	if (is_emitting()) {
		set_process_internal(true);
		if (!one_shot) {
			RenderingServer::get_singleton()->particles_restart(particles);
		}
	}

	if (!one_shot) {
		set_process_internal(false);
	}
}

void GPUParticles2D::set_pre_process_time(double p_time) {
	pre_process_time = p_time;
	RS::get_singleton()->particles_set_pre_process_time(particles, pre_process_time);
}

void GPUParticles2D::set_explosiveness_ratio(real_t p_ratio) {
	explosiveness_ratio = p_ratio;
	RS::get_singleton()->particles_set_explosiveness_ratio(particles, explosiveness_ratio);
}

void GPUParticles2D::set_randomness_ratio(real_t p_ratio) {
	randomness_ratio = p_ratio;
	RS::get_singleton()->particles_set_randomness_ratio(particles, randomness_ratio);
}

void GPUParticles2D::set_visibility_rect(const Rect2 &p_visibility_rect) {
	visibility_rect = p_visibility_rect;
	AABB aabb;
	aabb.position.x = p_visibility_rect.position.x;
	aabb.position.y = p_visibility_rect.position.y;
	aabb.size.x = p_visibility_rect.size.x;
	aabb.size.y = p_visibility_rect.size.y;

	RS::get_singleton()->particles_set_custom_aabb(particles, aabb);

	queue_redraw();
}

void GPUParticles2D::set_use_local_coordinates(bool p_enable) {
	local_coords = p_enable;
	RS::get_singleton()->particles_set_use_local_coordinates(particles, local_coords);
	set_notify_transform(!p_enable);
	if (!p_enable && is_inside_tree()) {
		_update_particle_emission_transform();
	}
}

void GPUParticles2D::_update_particle_emission_transform() {
	Transform2D xf2d = get_global_transform();
	Transform3D xf;
	xf.basis.set_column(0, Vector3(xf2d.columns[0].x, xf2d.columns[0].y, 0));
	xf.basis.set_column(1, Vector3(xf2d.columns[1].x, xf2d.columns[1].y, 0));
	xf.set_origin(Vector3(xf2d.get_origin().x, xf2d.get_origin().y, 0));

	RS::get_singleton()->particles_set_emission_transform(particles, xf);
}

void GPUParticles2D::set_process_material(const Ref<Material> &p_material) {
	process_material = p_material;
	Ref<ParticleProcessMaterial> pm = p_material;
	if (pm.is_valid() && !pm->get_particle_flag(ParticleProcessMaterial::PARTICLE_FLAG_DISABLE_Z) && pm->get_gravity() == Vector3(0, -9.8, 0)) {
		// Likely a new (3D) material, modify it to match 2D space
		pm->set_particle_flag(ParticleProcessMaterial::PARTICLE_FLAG_DISABLE_Z, true);
		pm->set_gravity(Vector3(0, 98, 0));
	}
	RID material_rid;
	if (process_material.is_valid()) {
		material_rid = process_material->get_rid();
	}
	RS::get_singleton()->particles_set_process_material(particles, material_rid);

	update_configuration_warnings();
}

void GPUParticles2D::set_trail_enabled(bool p_enabled) {
	trail_enabled = p_enabled;
	RS::get_singleton()->particles_set_trails(particles, trail_enabled, trail_lifetime);
	queue_redraw();
	update_configuration_warnings();

	RS::get_singleton()->particles_set_transform_align(particles, p_enabled ? RS::PARTICLES_TRANSFORM_ALIGN_Y_TO_VELOCITY : RS::PARTICLES_TRANSFORM_ALIGN_DISABLED);
}

void GPUParticles2D::set_trail_lifetime(double p_seconds) {
	ERR_FAIL_COND(p_seconds < 0.01);
	trail_lifetime = p_seconds;
	RS::get_singleton()->particles_set_trails(particles, trail_enabled, trail_lifetime);
	queue_redraw();
}

void GPUParticles2D::set_trail_sections(int p_sections) {
	ERR_FAIL_COND(p_sections < 2);
	ERR_FAIL_COND(p_sections > 128);

	trail_sections = p_sections;
	queue_redraw();
}

void GPUParticles2D::set_trail_section_subdivisions(int p_subdivisions) {
	ERR_FAIL_COND(p_subdivisions < 1);
	ERR_FAIL_COND(p_subdivisions > 1024);

	trail_section_subdivisions = p_subdivisions;
	queue_redraw();
}

void GPUParticles2D::set_interp_to_end(float p_interp) {
	interp_to_end_factor = CLAMP(p_interp, 0.0, 1.0);
	RS::get_singleton()->particles_set_interp_to_end(particles, interp_to_end_factor);
}

#ifdef TOOLS_ENABLED
void GPUParticles2D::set_show_visibility_rect(bool p_show_visibility_rect) {
	show_visibility_rect = p_show_visibility_rect;
	queue_redraw();
}
#endif

bool GPUParticles2D::is_trail_enabled() const {
	return trail_enabled;
}

double GPUParticles2D::get_trail_lifetime() const {
	return trail_lifetime;
}

void GPUParticles2D::_update_collision_size() {
	real_t csize = collision_base_size;

	if (texture.is_valid()) {
		csize *= (texture->get_width() + texture->get_height()) / 4.0; //half size since its a radius
	}

	RS::get_singleton()->particles_set_collision_base_size(particles, csize);
}

void GPUParticles2D::set_collision_base_size(real_t p_size) {
	collision_base_size = p_size;
	_update_collision_size();
}

real_t GPUParticles2D::get_collision_base_size() const {
	return collision_base_size;
}

void GPUParticles2D::set_speed_scale(double p_scale) {
	speed_scale = p_scale;
	RS::get_singleton()->particles_set_speed_scale(particles, p_scale);
}

bool GPUParticles2D::is_emitting() const {
	return emitting;
}

int GPUParticles2D::get_amount() const {
	return amount;
}

double GPUParticles2D::get_lifetime() const {
	return lifetime;
}

int GPUParticles2D::get_trail_sections() const {
	return trail_sections;
}
int GPUParticles2D::get_trail_section_subdivisions() const {
	return trail_section_subdivisions;
}

bool GPUParticles2D::get_one_shot() const {
	return one_shot;
}

double GPUParticles2D::get_pre_process_time() const {
	return pre_process_time;
}

real_t GPUParticles2D::get_explosiveness_ratio() const {
	return explosiveness_ratio;
}

real_t GPUParticles2D::get_randomness_ratio() const {
	return randomness_ratio;
}

Rect2 GPUParticles2D::get_visibility_rect() const {
	return visibility_rect;
}

bool GPUParticles2D::get_use_local_coordinates() const {
	return local_coords;
}

Ref<Material> GPUParticles2D::get_process_material() const {
	return process_material;
}

double GPUParticles2D::get_speed_scale() const {
	return speed_scale;
}

void GPUParticles2D::set_draw_order(DrawOrder p_order) {
	draw_order = p_order;
	RS::get_singleton()->particles_set_draw_order(particles, RS::ParticlesDrawOrder(p_order));
}

GPUParticles2D::DrawOrder GPUParticles2D::get_draw_order() const {
	return draw_order;
}

void GPUParticles2D::set_fixed_fps(int p_count) {
	fixed_fps = p_count;
	RS::get_singleton()->particles_set_fixed_fps(particles, p_count);
}

int GPUParticles2D::get_fixed_fps() const {
	return fixed_fps;
}

void GPUParticles2D::set_fractional_delta(bool p_enable) {
	fractional_delta = p_enable;
	RS::get_singleton()->particles_set_fractional_delta(particles, p_enable);
}

bool GPUParticles2D::get_fractional_delta() const {
	return fractional_delta;
}

void GPUParticles2D::set_interpolate(bool p_enable) {
	interpolate = p_enable;
	RS::get_singleton()->particles_set_interpolate(particles, p_enable);
}

bool GPUParticles2D::get_interpolate() const {
	return interpolate;
}

float GPUParticles2D::get_interp_to_end() const {
	return interp_to_end_factor;
}

PackedStringArray GPUParticles2D::get_configuration_warnings() const {
	PackedStringArray warnings = Node2D::get_configuration_warnings();

	if (process_material.is_null()) {
		warnings.push_back(RTR("A material to process the particles is not assigned, so no behavior is imprinted."));
	} else {
		CanvasItemMaterial *mat = Object::cast_to<CanvasItemMaterial>(get_material().ptr());

		if (get_material().is_null() || (mat && !mat->get_particles_animation())) {
			const ParticleProcessMaterial *process = Object::cast_to<ParticleProcessMaterial>(process_material.ptr());
			if (process &&
					(process->get_param_max(ParticleProcessMaterial::PARAM_ANIM_SPEED) != 0.0 || process->get_param_max(ParticleProcessMaterial::PARAM_ANIM_OFFSET) != 0.0 ||
							process->get_param_texture(ParticleProcessMaterial::PARAM_ANIM_SPEED).is_valid() || process->get_param_texture(ParticleProcessMaterial::PARAM_ANIM_OFFSET).is_valid())) {
				warnings.push_back(RTR("Particles2D animation requires the usage of a CanvasItemMaterial with \"Particles Animation\" enabled."));
			}
		}
	}

	if (trail_enabled && OS::get_singleton()->get_current_rendering_method() == "gl_compatibility") {
		warnings.push_back(RTR("Particle trails are only available when using the Forward+ or Mobile renderers."));
	}

	if (sub_emitter != NodePath() && OS::get_singleton()->get_current_rendering_method() == "gl_compatibility") {
		warnings.push_back(RTR("Particle sub-emitters are not available when using the Compatibility renderer."));
	}

	return warnings;
}

Rect2 GPUParticles2D::capture_rect() const {
	AABB aabb = RS::get_singleton()->particles_get_current_aabb(particles);
	Rect2 r;
	r.position.x = aabb.position.x;
	r.position.y = aabb.position.y;
	r.size.x = aabb.size.x;
	r.size.y = aabb.size.y;
	return r;
}

void GPUParticles2D::set_texture(const Ref<Texture2D> &p_texture) {
	if (texture.is_valid()) {
		texture->disconnect_changed(callable_mp(this, &GPUParticles2D::_texture_changed));
	}

	texture = p_texture;

	if (texture.is_valid()) {
		texture->connect_changed(callable_mp(this, &GPUParticles2D::_texture_changed));
	}
	_update_collision_size();
	queue_redraw();
}

Ref<Texture2D> GPUParticles2D::get_texture() const {
	return texture;
}

void GPUParticles2D::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "emitting") {
		p_property.hint = one_shot ? PROPERTY_HINT_ONESHOT : PROPERTY_HINT_NONE;
	}
}

void GPUParticles2D::emit_particle(const Transform2D &p_transform2d, const Vector2 &p_velocity2d, const Color &p_color, const Color &p_custom, uint32_t p_emit_flags) {
	Transform3D emit_transform;
	emit_transform.basis.set_column(0, Vector3(p_transform2d.columns[0].x, p_transform2d.columns[0].y, 0));
	emit_transform.basis.set_column(1, Vector3(p_transform2d.columns[1].x, p_transform2d.columns[1].y, 0));
	emit_transform.set_origin(Vector3(p_transform2d.get_origin().x, p_transform2d.get_origin().y, 0));
	Vector3 velocity = Vector3(p_velocity2d.x, p_velocity2d.y, 0);

	RS::get_singleton()->particles_emit(particles, emit_transform, velocity, p_color, p_custom, p_emit_flags);
}

void GPUParticles2D::_attach_sub_emitter() {
	Node *n = get_node_or_null(sub_emitter);
	if (n) {
		GPUParticles2D *sen = Object::cast_to<GPUParticles2D>(n);
		if (sen && sen != this) {
			RS::get_singleton()->particles_set_subemitter(particles, sen->particles);
		}
	}
}

void GPUParticles2D::_texture_changed() {
	// Changes to the texture need to trigger an update to make
	// the editor redraw the sprite with the updated texture.
	if (texture.is_valid()) {
		queue_redraw();
	}
}

void GPUParticles2D::set_sub_emitter(const NodePath &p_path) {
	if (is_inside_tree()) {
		RS::get_singleton()->particles_set_subemitter(particles, RID());
	}

	sub_emitter = p_path;

	if (is_inside_tree() && sub_emitter != NodePath()) {
		_attach_sub_emitter();
	}
	update_configuration_warnings();
}

NodePath GPUParticles2D::get_sub_emitter() const {
	return sub_emitter;
}

void GPUParticles2D::set_amount_ratio(float p_ratio) {
	amount_ratio = p_ratio;
	RenderingServer::get_singleton()->particles_set_amount_ratio(particles, p_ratio);
}

float GPUParticles2D::get_amount_ratio() const {
	return amount_ratio;
}

void GPUParticles2D::restart() {
	RS::get_singleton()->particles_restart(particles);
	RS::get_singleton()->particles_set_emitting(particles, true);

	emitting = true;
	active = true;
	signal_canceled = false;
	time = 0;
	emission_time = lifetime;
	active_time = lifetime * (2 - explosiveness_ratio);
	if (one_shot) {
		set_process_internal(true);
	}
}

void GPUParticles2D::convert_from_particles(Node *p_particles) {
	CPUParticles2D *cpu_particles = Object::cast_to<CPUParticles2D>(p_particles);
	ERR_FAIL_NULL_MSG(cpu_particles, "Only CPUParticles2D nodes can be converted to GPUParticles2D.");

	set_emitting(cpu_particles->is_emitting());
	set_amount(cpu_particles->get_amount());
	set_lifetime(cpu_particles->get_lifetime());
	set_one_shot(cpu_particles->get_one_shot());
	set_pre_process_time(cpu_particles->get_pre_process_time());
	set_explosiveness_ratio(cpu_particles->get_explosiveness_ratio());
	set_randomness_ratio(cpu_particles->get_randomness_ratio());
	set_use_local_coordinates(cpu_particles->get_use_local_coordinates());
	set_fixed_fps(cpu_particles->get_fixed_fps());
	set_fractional_delta(cpu_particles->get_fractional_delta());
	set_speed_scale(cpu_particles->get_speed_scale());
	set_draw_order(DrawOrder(cpu_particles->get_draw_order()));
	set_texture(cpu_particles->get_texture());

	Ref<Material> mat = cpu_particles->get_material();
	if (mat.is_valid()) {
		set_material(mat);
	}

	Ref<ParticleProcessMaterial> proc_mat = memnew(ParticleProcessMaterial);
	set_process_material(proc_mat);
	Vector2 dir = cpu_particles->get_direction();
	proc_mat->set_direction(Vector3(dir.x, dir.y, 0));
	proc_mat->set_spread(cpu_particles->get_spread());
	proc_mat->set_color(cpu_particles->get_color());

	Ref<Gradient> color_grad = cpu_particles->get_color_ramp();
	if (color_grad.is_valid()) {
		Ref<GradientTexture1D> tex = memnew(GradientTexture1D);
		tex->set_gradient(color_grad);
		proc_mat->set_color_ramp(tex);
	}

	Ref<Gradient> color_init_grad = cpu_particles->get_color_initial_ramp();
	if (color_init_grad.is_valid()) {
		Ref<GradientTexture1D> tex = memnew(GradientTexture1D);
		tex->set_gradient(color_init_grad);
		proc_mat->set_color_initial_ramp(tex);
	}

	proc_mat->set_particle_flag(ParticleProcessMaterial::PARTICLE_FLAG_ALIGN_Y_TO_VELOCITY, cpu_particles->get_particle_flag(CPUParticles2D::PARTICLE_FLAG_ALIGN_Y_TO_VELOCITY));

	proc_mat->set_emission_shape(ParticleProcessMaterial::EmissionShape(cpu_particles->get_emission_shape()));
	proc_mat->set_emission_sphere_radius(cpu_particles->get_emission_sphere_radius());

	Vector2 rect_extents = cpu_particles->get_emission_rect_extents();
	proc_mat->set_emission_box_extents(Vector3(rect_extents.x, rect_extents.y, 0));

	if (cpu_particles->get_split_scale()) {
		Ref<CurveXYZTexture> scale3D = memnew(CurveXYZTexture);
		scale3D->set_curve_x(cpu_particles->get_scale_curve_x());
		scale3D->set_curve_y(cpu_particles->get_scale_curve_y());
		proc_mat->set_param_texture(ParticleProcessMaterial::PARAM_SCALE, scale3D);
	}

	Vector2 gravity = cpu_particles->get_gravity();
	proc_mat->set_gravity(Vector3(gravity.x, gravity.y, 0));
	proc_mat->set_lifetime_randomness(cpu_particles->get_lifetime_randomness());

#define CONVERT_PARAM(m_param)                                                                                        \
	proc_mat->set_param_min(ParticleProcessMaterial::m_param, cpu_particles->get_param_min(CPUParticles2D::m_param)); \
	{                                                                                                                 \
		Ref<Curve> curve = cpu_particles->get_param_curve(CPUParticles2D::m_param);                                   \
		if (curve.is_valid()) {                                                                                       \
			Ref<CurveTexture> tex = memnew(CurveTexture);                                                             \
			tex->set_curve(curve);                                                                                    \
			proc_mat->set_param_texture(ParticleProcessMaterial::m_param, tex);                                       \
		}                                                                                                             \
	}                                                                                                                 \
	proc_mat->set_param_max(ParticleProcessMaterial::m_param, cpu_particles->get_param_max(CPUParticles2D::m_param));

	CONVERT_PARAM(PARAM_INITIAL_LINEAR_VELOCITY);
	CONVERT_PARAM(PARAM_ANGULAR_VELOCITY);
	CONVERT_PARAM(PARAM_ORBIT_VELOCITY);
	CONVERT_PARAM(PARAM_LINEAR_ACCEL);
	CONVERT_PARAM(PARAM_RADIAL_ACCEL);
	CONVERT_PARAM(PARAM_TANGENTIAL_ACCEL);
	CONVERT_PARAM(PARAM_DAMPING);
	CONVERT_PARAM(PARAM_ANGLE);
	CONVERT_PARAM(PARAM_SCALE);
	CONVERT_PARAM(PARAM_HUE_VARIATION);
	CONVERT_PARAM(PARAM_ANIM_SPEED);
	CONVERT_PARAM(PARAM_ANIM_OFFSET);

#undef CONVERT_PARAM
}

void GPUParticles2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			RID texture_rid;
			Size2 size;
			if (texture.is_valid()) {
				texture_rid = texture->get_rid();
				size = texture->get_size();
			} else {
				size = Size2(1, 1);
			}

			if (trail_enabled) {
				RS::get_singleton()->mesh_clear(mesh);
				PackedVector2Array points;
				PackedVector2Array uvs;
				PackedInt32Array bone_indices;
				PackedFloat32Array bone_weights;
				PackedInt32Array indices;

				int total_segments = trail_sections * trail_section_subdivisions;
				real_t depth = size.height * trail_sections;

				for (int j = 0; j <= total_segments; j++) {
					real_t v = j;
					v /= total_segments;

					real_t y = depth * v;
					y = (depth * 0.5) - y;

					int bone = j / trail_section_subdivisions;
					real_t blend = 1.0 - real_t(j % trail_section_subdivisions) / real_t(trail_section_subdivisions);

					real_t s = size.width;

					points.push_back(Vector2(-s * 0.5, 0));
					points.push_back(Vector2(+s * 0.5, 0));

					uvs.push_back(Vector2(0, v));
					uvs.push_back(Vector2(1, v));

					for (int i = 0; i < 2; i++) {
						bone_indices.push_back(bone);
						bone_indices.push_back(MIN(trail_sections, bone + 1));
						bone_indices.push_back(0);
						bone_indices.push_back(0);

						bone_weights.push_back(blend);
						bone_weights.push_back(1.0 - blend);
						bone_weights.push_back(0);
						bone_weights.push_back(0);
					}

					if (j > 0) {
						int base = j * 2 - 2;
						indices.push_back(base + 0);
						indices.push_back(base + 1);
						indices.push_back(base + 2);

						indices.push_back(base + 1);
						indices.push_back(base + 3);
						indices.push_back(base + 2);
					}
				}

				Array arr;
				arr.resize(RS::ARRAY_MAX);
				arr[RS::ARRAY_VERTEX] = points;
				arr[RS::ARRAY_TEX_UV] = uvs;
				arr[RS::ARRAY_BONES] = bone_indices;
				arr[RS::ARRAY_WEIGHTS] = bone_weights;
				arr[RS::ARRAY_INDEX] = indices;

				RS::get_singleton()->mesh_add_surface_from_arrays(mesh, RS::PRIMITIVE_TRIANGLES, arr, Array(), Dictionary(), RS::ARRAY_FLAG_USE_2D_VERTICES);

				Vector<Transform3D> xforms;
				for (int i = 0; i <= trail_sections; i++) {
					Transform3D xform;
					/*
					xform.origin.y = depth / 2.0 - size.height * real_t(i);
					xform.origin.y = -xform.origin.y; //bind is an inverse transform, so negate y */
					xforms.push_back(xform);
				}

				RS::get_singleton()->particles_set_trail_bind_poses(particles, xforms);

			} else {
				RS::get_singleton()->mesh_clear(mesh);

				Vector<Vector2> points = {
					Vector2(-size.x / 2.0, -size.y / 2.0),
					Vector2(size.x / 2.0, -size.y / 2.0),
					Vector2(size.x / 2.0, size.y / 2.0),
					Vector2(-size.x / 2.0, size.y / 2.0)
				};

				Vector<Vector2> uvs;
				AtlasTexture *atlas_texure = Object::cast_to<AtlasTexture>(*texture);
				if (atlas_texure && atlas_texure->get_atlas().is_valid()) {
					Rect2 region_rect = atlas_texure->get_region();
					Size2 atlas_size = atlas_texure->get_atlas()->get_size();
					uvs.push_back(Vector2(region_rect.position.x / atlas_size.x, region_rect.position.y / atlas_size.y));
					uvs.push_back(Vector2((region_rect.position.x + region_rect.size.x) / atlas_size.x, region_rect.position.y / atlas_size.y));
					uvs.push_back(Vector2((region_rect.position.x + region_rect.size.x) / atlas_size.x, (region_rect.position.y + region_rect.size.y) / atlas_size.y));
					uvs.push_back(Vector2(region_rect.position.x / atlas_size.x, (region_rect.position.y + region_rect.size.y) / atlas_size.y));
				} else {
					uvs.push_back(Vector2(0, 0));
					uvs.push_back(Vector2(1, 0));
					uvs.push_back(Vector2(1, 1));
					uvs.push_back(Vector2(0, 1));
				}

				Vector<int> indices = { 0, 1, 2, 0, 2, 3 };

				Array arr;
				arr.resize(RS::ARRAY_MAX);
				arr[RS::ARRAY_VERTEX] = points;
				arr[RS::ARRAY_TEX_UV] = uvs;
				arr[RS::ARRAY_INDEX] = indices;

				RS::get_singleton()->mesh_add_surface_from_arrays(mesh, RS::PRIMITIVE_TRIANGLES, arr, Array(), Dictionary(), RS::ARRAY_FLAG_USE_2D_VERTICES);
				RS::get_singleton()->particles_set_trail_bind_poses(particles, Vector<Transform3D>());
			}
			RS::get_singleton()->canvas_item_add_particles(get_canvas_item(), particles, texture_rid);

#ifdef TOOLS_ENABLED
			if (show_visibility_rect) {
				draw_rect(visibility_rect, Color(0, 0.7, 0.9, 0.4), false);
			}
#endif
		} break;

		case NOTIFICATION_ENTER_TREE: {
			if (sub_emitter != NodePath()) {
				_attach_sub_emitter();
			}
			if (can_process()) {
				RS::get_singleton()->particles_set_speed_scale(particles, speed_scale);
			} else {
				RS::get_singleton()->particles_set_speed_scale(particles, 0);
			}
			set_process_internal(true);
			set_physics_process_internal(true);
			previous_position = get_global_position();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			RS::get_singleton()->particles_set_subemitter(particles, RID());
		} break;

		case NOTIFICATION_SUSPENDED:
		case NOTIFICATION_UNSUSPENDED:
		case NOTIFICATION_PAUSED:
		case NOTIFICATION_UNPAUSED: {
			if (is_inside_tree()) {
				if (can_process()) {
					RS::get_singleton()->particles_set_speed_scale(particles, speed_scale);
				} else {
					RS::get_singleton()->particles_set_speed_scale(particles, 0);
				}
			}
		} break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			_update_particle_emission_transform();
		} break;

		case NOTIFICATION_INTERNAL_PROCESS: {
			if (one_shot) {
				time += get_process_delta_time();
				if (time > emission_time) {
					emitting = false;
					if (!active) {
						set_process_internal(false);
					}
				}
				if (time > active_time) {
					if (active && !signal_canceled) {
						emit_signal(SceneStringName(finished));
					}
					active = false;
					if (!emitting) {
						set_process_internal(false);
					}
				}
			}
		} break;

		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			// Update velocity in physics process, so that velocity calculations remain correct
			// if the physics tick rate is lower than the rendered framerate (especially without physics interpolation).
			const Vector3 velocity = Vector3((get_global_position() - previous_position).x, (get_global_position() - previous_position).y, 0.0) /
					get_physics_process_delta_time();

			if (velocity != previous_velocity) {
				RS::get_singleton()->particles_set_emitter_velocity(particles, velocity);
				previous_velocity = velocity;
			}
			previous_position = get_global_position();
		} break;
	}
}

void GPUParticles2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_emitting", "emitting"), &GPUParticles2D::set_emitting);
	ClassDB::bind_method(D_METHOD("set_amount", "amount"), &GPUParticles2D::set_amount);
	ClassDB::bind_method(D_METHOD("set_lifetime", "secs"), &GPUParticles2D::set_lifetime);
	ClassDB::bind_method(D_METHOD("set_one_shot", "secs"), &GPUParticles2D::set_one_shot);
	ClassDB::bind_method(D_METHOD("set_pre_process_time", "secs"), &GPUParticles2D::set_pre_process_time);
	ClassDB::bind_method(D_METHOD("set_explosiveness_ratio", "ratio"), &GPUParticles2D::set_explosiveness_ratio);
	ClassDB::bind_method(D_METHOD("set_randomness_ratio", "ratio"), &GPUParticles2D::set_randomness_ratio);
	ClassDB::bind_method(D_METHOD("set_visibility_rect", "visibility_rect"), &GPUParticles2D::set_visibility_rect);
	ClassDB::bind_method(D_METHOD("set_use_local_coordinates", "enable"), &GPUParticles2D::set_use_local_coordinates);
	ClassDB::bind_method(D_METHOD("set_fixed_fps", "fps"), &GPUParticles2D::set_fixed_fps);
	ClassDB::bind_method(D_METHOD("set_fractional_delta", "enable"), &GPUParticles2D::set_fractional_delta);
	ClassDB::bind_method(D_METHOD("set_interpolate", "enable"), &GPUParticles2D::set_interpolate);
	ClassDB::bind_method(D_METHOD("set_process_material", "material"), &GPUParticles2D::set_process_material);
	ClassDB::bind_method(D_METHOD("set_speed_scale", "scale"), &GPUParticles2D::set_speed_scale);
	ClassDB::bind_method(D_METHOD("set_collision_base_size", "size"), &GPUParticles2D::set_collision_base_size);
	ClassDB::bind_method(D_METHOD("set_interp_to_end", "interp"), &GPUParticles2D::set_interp_to_end);

	ClassDB::bind_method(D_METHOD("is_emitting"), &GPUParticles2D::is_emitting);
	ClassDB::bind_method(D_METHOD("get_amount"), &GPUParticles2D::get_amount);
	ClassDB::bind_method(D_METHOD("get_lifetime"), &GPUParticles2D::get_lifetime);
	ClassDB::bind_method(D_METHOD("get_one_shot"), &GPUParticles2D::get_one_shot);
	ClassDB::bind_method(D_METHOD("get_pre_process_time"), &GPUParticles2D::get_pre_process_time);
	ClassDB::bind_method(D_METHOD("get_explosiveness_ratio"), &GPUParticles2D::get_explosiveness_ratio);
	ClassDB::bind_method(D_METHOD("get_randomness_ratio"), &GPUParticles2D::get_randomness_ratio);
	ClassDB::bind_method(D_METHOD("get_visibility_rect"), &GPUParticles2D::get_visibility_rect);
	ClassDB::bind_method(D_METHOD("get_use_local_coordinates"), &GPUParticles2D::get_use_local_coordinates);
	ClassDB::bind_method(D_METHOD("get_fixed_fps"), &GPUParticles2D::get_fixed_fps);
	ClassDB::bind_method(D_METHOD("get_fractional_delta"), &GPUParticles2D::get_fractional_delta);
	ClassDB::bind_method(D_METHOD("get_interpolate"), &GPUParticles2D::get_interpolate);
	ClassDB::bind_method(D_METHOD("get_process_material"), &GPUParticles2D::get_process_material);
	ClassDB::bind_method(D_METHOD("get_speed_scale"), &GPUParticles2D::get_speed_scale);
	ClassDB::bind_method(D_METHOD("get_collision_base_size"), &GPUParticles2D::get_collision_base_size);
	ClassDB::bind_method(D_METHOD("get_interp_to_end"), &GPUParticles2D::get_interp_to_end);

	ClassDB::bind_method(D_METHOD("set_draw_order", "order"), &GPUParticles2D::set_draw_order);
	ClassDB::bind_method(D_METHOD("get_draw_order"), &GPUParticles2D::get_draw_order);

	ClassDB::bind_method(D_METHOD("set_texture", "texture"), &GPUParticles2D::set_texture);
	ClassDB::bind_method(D_METHOD("get_texture"), &GPUParticles2D::get_texture);

	ClassDB::bind_method(D_METHOD("capture_rect"), &GPUParticles2D::capture_rect);

	ClassDB::bind_method(D_METHOD("restart"), &GPUParticles2D::restart);

	ClassDB::bind_method(D_METHOD("set_sub_emitter", "path"), &GPUParticles2D::set_sub_emitter);
	ClassDB::bind_method(D_METHOD("get_sub_emitter"), &GPUParticles2D::get_sub_emitter);

	ClassDB::bind_method(D_METHOD("emit_particle", "xform", "velocity", "color", "custom", "flags"), &GPUParticles2D::emit_particle);

	ClassDB::bind_method(D_METHOD("set_trail_enabled", "enabled"), &GPUParticles2D::set_trail_enabled);
	ClassDB::bind_method(D_METHOD("set_trail_lifetime", "secs"), &GPUParticles2D::set_trail_lifetime);

	ClassDB::bind_method(D_METHOD("is_trail_enabled"), &GPUParticles2D::is_trail_enabled);
	ClassDB::bind_method(D_METHOD("get_trail_lifetime"), &GPUParticles2D::get_trail_lifetime);

	ClassDB::bind_method(D_METHOD("set_trail_sections", "sections"), &GPUParticles2D::set_trail_sections);
	ClassDB::bind_method(D_METHOD("get_trail_sections"), &GPUParticles2D::get_trail_sections);

	ClassDB::bind_method(D_METHOD("set_trail_section_subdivisions", "subdivisions"), &GPUParticles2D::set_trail_section_subdivisions);
	ClassDB::bind_method(D_METHOD("get_trail_section_subdivisions"), &GPUParticles2D::get_trail_section_subdivisions);

	ClassDB::bind_method(D_METHOD("convert_from_particles", "particles"), &GPUParticles2D::convert_from_particles);

	ClassDB::bind_method(D_METHOD("set_amount_ratio", "ratio"), &GPUParticles2D::set_amount_ratio);
	ClassDB::bind_method(D_METHOD("get_amount_ratio"), &GPUParticles2D::get_amount_ratio);

	ADD_SIGNAL(MethodInfo("finished"));

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "emitting", PROPERTY_HINT_ONESHOT), "set_emitting", "is_emitting");
	ADD_PROPERTY_DEFAULT("emitting", true); // Workaround for doctool in headless mode, as dummy rasterizer always returns false.
	ADD_PROPERTY(PropertyInfo(Variant::INT, "amount", PROPERTY_HINT_RANGE, "1,1000000,1,exp"), "set_amount", "get_amount"); // FIXME: Evaluate support for `exp` in integer properties, or remove this.
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "amount_ratio", PROPERTY_HINT_RANGE, "0,1,0.0001"), "set_amount_ratio", "get_amount_ratio");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "sub_emitter", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "GPUParticles2D"), "set_sub_emitter", "get_sub_emitter");
	ADD_GROUP("Time", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lifetime", PROPERTY_HINT_RANGE, "0.01,600.0,0.01,or_greater,exp,suffix:s"), "set_lifetime", "get_lifetime");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "interp_to_end", PROPERTY_HINT_RANGE, "0.00,1.0,0.001"), "set_interp_to_end", "get_interp_to_end");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "one_shot"), "set_one_shot", "get_one_shot");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "preprocess", PROPERTY_HINT_RANGE, "0.00,10.0,0.01,or_greater,exp,suffix:s"), "set_pre_process_time", "get_pre_process_time");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed_scale", PROPERTY_HINT_RANGE, "0,64,0.01"), "set_speed_scale", "get_speed_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "explosiveness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_explosiveness_ratio", "get_explosiveness_ratio");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "randomness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_randomness_ratio", "get_randomness_ratio");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "fixed_fps", PROPERTY_HINT_RANGE, "0,1000,1,suffix:FPS"), "set_fixed_fps", "get_fixed_fps");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "interpolate"), "set_interpolate", "get_interpolate");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "fract_delta"), "set_fractional_delta", "get_fractional_delta");
	ADD_GROUP("Collision", "collision_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_base_size", PROPERTY_HINT_RANGE, "0,128,0.01,or_greater"), "set_collision_base_size", "get_collision_base_size");
	ADD_GROUP("Drawing", "");
	ADD_PROPERTY(PropertyInfo(Variant::RECT2, "visibility_rect", PROPERTY_HINT_NONE, "suffix:px"), "set_visibility_rect", "get_visibility_rect");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "local_coords"), "set_use_local_coordinates", "get_use_local_coordinates");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "draw_order", PROPERTY_HINT_ENUM, "Index,Lifetime,Reverse Lifetime"), "set_draw_order", "get_draw_order");
	ADD_GROUP("Trails", "trail_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "trail_enabled"), "set_trail_enabled", "is_trail_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "trail_lifetime", PROPERTY_HINT_RANGE, "0.01,10,0.01,or_greater,suffix:s"), "set_trail_lifetime", "get_trail_lifetime");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "trail_sections", PROPERTY_HINT_RANGE, "2,128,1"), "set_trail_sections", "get_trail_sections");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "trail_section_subdivisions", PROPERTY_HINT_RANGE, "1,1024,1"), "set_trail_section_subdivisions", "get_trail_section_subdivisions");
	ADD_GROUP("Process Material", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_texture", "get_texture");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "process_material", PROPERTY_HINT_RESOURCE_TYPE, "ParticleProcessMaterial,ShaderMaterial"), "set_process_material", "get_process_material");
	BIND_ENUM_CONSTANT(DRAW_ORDER_INDEX);
	BIND_ENUM_CONSTANT(DRAW_ORDER_LIFETIME);
	BIND_ENUM_CONSTANT(DRAW_ORDER_REVERSE_LIFETIME);

	BIND_ENUM_CONSTANT(EMIT_FLAG_POSITION);
	BIND_ENUM_CONSTANT(EMIT_FLAG_ROTATION_SCALE);
	BIND_ENUM_CONSTANT(EMIT_FLAG_VELOCITY);
	BIND_ENUM_CONSTANT(EMIT_FLAG_COLOR);
	BIND_ENUM_CONSTANT(EMIT_FLAG_CUSTOM);
}

GPUParticles2D::GPUParticles2D() {
	particles = RS::get_singleton()->particles_create();
	RS::get_singleton()->particles_set_mode(particles, RS::PARTICLES_MODE_2D);

	mesh = RS::get_singleton()->mesh_create();
	RS::get_singleton()->particles_set_draw_passes(particles, 1);
	RS::get_singleton()->particles_set_draw_pass_mesh(particles, 0, mesh);

	one_shot = false; // Needed so that set_emitting doesn't access uninitialized values
	set_emitting(true);
	set_one_shot(false);
	set_amount(8);
	set_amount_ratio(1.0);
	set_lifetime(1);
	set_fixed_fps(0);
	set_fractional_delta(true);
	set_interpolate(true);
	set_pre_process_time(0);
	set_explosiveness_ratio(0);
	set_randomness_ratio(0);
	set_visibility_rect(Rect2(Vector2(-100, -100), Vector2(200, 200)));
	set_use_local_coordinates(false);
	set_draw_order(DRAW_ORDER_LIFETIME);
	set_speed_scale(1);
	set_fixed_fps(30);
	set_collision_base_size(collision_base_size);
}

GPUParticles2D::~GPUParticles2D() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RS::get_singleton()->free(particles);
	RS::get_singleton()->free(mesh);
}

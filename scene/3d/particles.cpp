/**************************************************************************/
/*  particles.cpp                                                         */
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

#include "particles.h"

#include "core/os/os.h"
#include "scene/resources/particles_material.h"
#include "scene/scene_string_names.h"

#include "servers/visual_server.h"

AABB Particles::get_aabb() const {
	return AABB();
}
PoolVector<Face3> Particles::get_faces(uint32_t p_usage_flags) const {
	return PoolVector<Face3>();
}

void Particles::set_emitting(bool p_emitting) {
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
		set_process_internal(false);
		if (one_shot) {
			set_process_internal(true);
		} else {
			set_process_internal(false);
		}
	}

	emitting = p_emitting;
	VS::get_singleton()->particles_set_emitting(particles, p_emitting);
}

void Particles::set_amount(int p_amount) {
	ERR_FAIL_COND_MSG(p_amount < 1, "Amount of particles cannot be smaller than 1.");
	amount = p_amount;
	VS::get_singleton()->particles_set_amount(particles, amount);
}
void Particles::set_lifetime(float p_lifetime) {
	ERR_FAIL_COND_MSG(p_lifetime <= 0, "Particles lifetime must be greater than 0.");
	lifetime = p_lifetime;
	VS::get_singleton()->particles_set_lifetime(particles, lifetime);
}
void Particles::set_lifetime_infinite(bool p_lifetime_infinite) {
	lifetime_infinite = p_lifetime_infinite;
	VS::get_singleton()->particles_set_lifetime_infinite(particles, p_lifetime_infinite);
	_change_notify();
}

void Particles::set_one_shot(bool p_one_shot) {
	one_shot = p_one_shot;
	VS::get_singleton()->particles_set_one_shot(particles, one_shot);

	if (is_emitting()) {
		set_process_internal(true);
		if (!one_shot) {
			VisualServer::get_singleton()->particles_restart(particles);
		}
	}

	if (!one_shot) {
		set_process_internal(false);
	}
}

void Particles::set_pre_process_time(float p_time) {
	pre_process_time = p_time;
	VS::get_singleton()->particles_set_pre_process_time(particles, pre_process_time);
}
void Particles::set_explosiveness_ratio(float p_ratio) {
	explosiveness_ratio = p_ratio;
	VS::get_singleton()->particles_set_explosiveness_ratio(particles, explosiveness_ratio);
}
void Particles::set_randomness_ratio(float p_ratio) {
	randomness_ratio = p_ratio;
	VS::get_singleton()->particles_set_randomness_ratio(particles, randomness_ratio);
}
void Particles::set_visibility_aabb(const AABB &p_aabb) {
	visibility_aabb = p_aabb;
	VS::get_singleton()->particles_set_custom_aabb(particles, visibility_aabb);
	update_gizmo();
	_change_notify("visibility_aabb");
}
void Particles::set_use_local_coordinates(bool p_enable) {
	local_coords = p_enable;
	VS::get_singleton()->particles_set_use_local_coordinates(particles, local_coords);
}
void Particles::set_process_material(const Ref<Material> &p_material) {
	process_material = p_material;
	RID material_rid;
	if (process_material.is_valid()) {
		material_rid = process_material->get_rid();
	}
	VS::get_singleton()->particles_set_process_material(particles, material_rid);

	update_configuration_warning();
}

void Particles::set_speed_scale(float p_scale) {
	speed_scale = p_scale;
	VS::get_singleton()->particles_set_speed_scale(particles, p_scale);
}

bool Particles::is_emitting() const {
	return emitting;
}
int Particles::get_amount() const {
	return amount;
}
float Particles::get_lifetime() const {
	return lifetime;
}
bool Particles::is_lifetime_infinite() const {
	return lifetime_infinite;
}
bool Particles::get_one_shot() const {
	return one_shot;
}

float Particles::get_pre_process_time() const {
	return pre_process_time;
}
float Particles::get_explosiveness_ratio() const {
	return explosiveness_ratio;
}
float Particles::get_randomness_ratio() const {
	return randomness_ratio;
}
AABB Particles::get_visibility_aabb() const {
	return visibility_aabb;
}
bool Particles::get_use_local_coordinates() const {
	return local_coords;
}
Ref<Material> Particles::get_process_material() const {
	return process_material;
}

float Particles::get_speed_scale() const {
	return speed_scale;
}

void Particles::set_draw_order(DrawOrder p_order) {
	draw_order = p_order;
	VS::get_singleton()->particles_set_draw_order(particles, VS::ParticlesDrawOrder(p_order));
}

Particles::DrawOrder Particles::get_draw_order() const {
	return draw_order;
}

void Particles::set_draw_passes(int p_count) {
	ERR_FAIL_COND(p_count < 1);
	draw_passes.resize(p_count);
	VS::get_singleton()->particles_set_draw_passes(particles, p_count);
	_change_notify();
}
int Particles::get_draw_passes() const {
	return draw_passes.size();
}

void Particles::set_draw_pass_mesh(int p_pass, const Ref<Mesh> &p_mesh) {
	ERR_FAIL_INDEX(p_pass, draw_passes.size());

	draw_passes.write[p_pass] = p_mesh;

	RID mesh_rid;
	if (p_mesh.is_valid()) {
		mesh_rid = p_mesh->get_rid();
	}

	VS::get_singleton()->particles_set_draw_pass_mesh(particles, p_pass, mesh_rid);

	update_configuration_warning();
}

Ref<Mesh> Particles::get_draw_pass_mesh(int p_pass) const {
	ERR_FAIL_INDEX_V(p_pass, draw_passes.size(), Ref<Mesh>());

	return draw_passes[p_pass];
}

void Particles::set_fixed_fps(int p_count) {
	fixed_fps = p_count;
	VS::get_singleton()->particles_set_fixed_fps(particles, p_count);
}

int Particles::get_fixed_fps() const {
	return fixed_fps;
}

void Particles::set_fractional_delta(bool p_enable) {
	fractional_delta = p_enable;
	VS::get_singleton()->particles_set_fractional_delta(particles, p_enable);
}

bool Particles::get_fractional_delta() const {
	return fractional_delta;
}

String Particles::get_configuration_warning() const {
	String warnings = GeometryInstance::get_configuration_warning();

	if (OS::get_singleton()->get_current_video_driver() == OS::VIDEO_DRIVER_GLES2) {
		if (warnings != String()) {
			warnings += "\n\n";
		}
		warnings += "- " + TTR("GPU-based particles are not supported by the GLES2 video driver.\nUse the CPUParticles node instead. You can use the \"Convert to CPUParticles\" toolbar option for this purpose.");
		return warnings;
	}

#ifdef OSX_ENABLED
	if (warnings != String()) {
		warnings += "\n\n";
	}

	warnings += "- " + TTR("On macOS, Particles rendering is much slower than CPUParticles due to transform feedback being implemented on the CPU instead of the GPU.\nConsider using CPUParticles instead when targeting macOS.\nYou can use the \"Convert to CPUParticles\" toolbar option for this purpose.");
#endif

	bool meshes_found = false;
	bool anim_material_found = false;

	for (int i = 0; i < draw_passes.size(); i++) {
		if (draw_passes[i].is_valid()) {
			meshes_found = true;
			for (int j = 0; j < draw_passes[i]->get_surface_count(); j++) {
				anim_material_found = Object::cast_to<ShaderMaterial>(draw_passes[i]->surface_get_material(j).ptr()) != nullptr;
				Material3D *spat = Object::cast_to<Material3D>(draw_passes[i]->surface_get_material(j).ptr());
				anim_material_found = anim_material_found || (spat && spat->get_billboard_mode() == Material3D::BILLBOARD_PARTICLES);
			}
			if (anim_material_found) {
				break;
			}
		}
	}

	anim_material_found = anim_material_found || Object::cast_to<ShaderMaterial>(get_material_override().ptr()) != nullptr;
	Material3D *spat = Object::cast_to<Material3D>(get_material_override().ptr());
	anim_material_found = anim_material_found || (spat && spat->get_billboard_mode() == Material3D::BILLBOARD_PARTICLES);

	if (!meshes_found) {
		if (warnings != String()) {
			warnings += "\n\n";
		}
		warnings += "- " + TTR("Nothing is visible because meshes have not been assigned to draw passes.");
	}

	if (process_material.is_null()) {
		if (warnings != String()) {
			warnings += "\n";
		}
		warnings += "- " + TTR("A material to process the particles is not assigned, so no behavior is imprinted.");
	} else {
		const ParticlesMaterial *process = Object::cast_to<ParticlesMaterial>(process_material.ptr());
		if (!anim_material_found && process &&
				(process->get_param(ParticlesMaterial::PARAM_ANIM_SPEED) != 0.0 || process->get_param(ParticlesMaterial::PARAM_ANIM_OFFSET) != 0.0 ||
						process->get_param_texture(ParticlesMaterial::PARAM_ANIM_SPEED).is_valid() || process->get_param_texture(ParticlesMaterial::PARAM_ANIM_OFFSET).is_valid())) {
			if (warnings != String()) {
				warnings += "\n";
			}
			warnings += "- " + TTR("Particles animation requires the usage of a SpatialMaterial whose Billboard Mode is set to \"Particle Billboard\".");
		}
	}

	return warnings;
}

void Particles::restart() {
	VisualServer::get_singleton()->particles_restart(particles);
	VisualServer::get_singleton()->particles_set_emitting(particles, true);

	emitting = true;
	active = true;
	signal_canceled = false;
	time = 0;
	emission_time = lifetime * (1 - explosiveness_ratio);
	active_time = lifetime * (2 - explosiveness_ratio);
	if (one_shot) {
		set_process_internal(true);
	}
}

AABB Particles::capture_aabb() const {
	return VS::get_singleton()->particles_get_current_aabb(particles);
}

void Particles::_validate_property(PropertyInfo &property) const {
	if (property.name == "lifetime" && lifetime_infinite) {
		// The lifetime property is ignored when lifetime is infinite, so hide it.
		property.usage = 0;
		return;
	}

	if (property.name.begins_with("draw_pass_")) {
		int index = property.name.get_slicec('_', 2).to_int() - 1;
		if (index >= draw_passes.size()) {
			property.usage = 0;
			return;
		}
	}
}

void Particles::_notification(int p_what) {
	if (p_what == NOTIFICATION_PAUSED || p_what == NOTIFICATION_UNPAUSED) {
		if (can_process()) {
			VS::get_singleton()->particles_set_speed_scale(particles, speed_scale);
		} else {
			VS::get_singleton()->particles_set_speed_scale(particles, 0);
		}
	}

	// Use internal process when emitting and one_shot are on so that when
	// the shot ends the editor can properly update
	if (p_what == NOTIFICATION_INTERNAL_PROCESS) {
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
					emit_signal(SceneStringNames::get_singleton()->finished);
				}
				active = false;
				if (!emitting) {
					set_process_internal(false);
				}
			}
		}
	}

	if (p_what == NOTIFICATION_VISIBILITY_CHANGED) {
		// make sure particles are updated before rendering occurs if they were active before
		if (is_visible_in_tree() && !VS::get_singleton()->particles_is_inactive(particles)) {
			VS::get_singleton()->particles_request_process(particles);
		}
	}
}

void Particles::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_emitting", "emitting"), &Particles::set_emitting);
	ClassDB::bind_method(D_METHOD("set_amount", "amount"), &Particles::set_amount);
	ClassDB::bind_method(D_METHOD("set_lifetime", "secs"), &Particles::set_lifetime);
	ClassDB::bind_method(D_METHOD("set_lifetime_infinite", "infinite"), &Particles::set_lifetime_infinite);
	ClassDB::bind_method(D_METHOD("set_one_shot", "enable"), &Particles::set_one_shot);
	ClassDB::bind_method(D_METHOD("set_pre_process_time", "secs"), &Particles::set_pre_process_time);
	ClassDB::bind_method(D_METHOD("set_explosiveness_ratio", "ratio"), &Particles::set_explosiveness_ratio);
	ClassDB::bind_method(D_METHOD("set_randomness_ratio", "ratio"), &Particles::set_randomness_ratio);
	ClassDB::bind_method(D_METHOD("set_visibility_aabb", "aabb"), &Particles::set_visibility_aabb);
	ClassDB::bind_method(D_METHOD("set_use_local_coordinates", "enable"), &Particles::set_use_local_coordinates);
	ClassDB::bind_method(D_METHOD("set_fixed_fps", "fps"), &Particles::set_fixed_fps);
	ClassDB::bind_method(D_METHOD("set_fractional_delta", "enable"), &Particles::set_fractional_delta);
	ClassDB::bind_method(D_METHOD("set_process_material", "material"), &Particles::set_process_material);
	ClassDB::bind_method(D_METHOD("set_speed_scale", "scale"), &Particles::set_speed_scale);

	ClassDB::bind_method(D_METHOD("is_emitting"), &Particles::is_emitting);
	ClassDB::bind_method(D_METHOD("get_amount"), &Particles::get_amount);
	ClassDB::bind_method(D_METHOD("get_lifetime"), &Particles::get_lifetime);
	ClassDB::bind_method(D_METHOD("is_lifetime_infinite"), &Particles::is_lifetime_infinite);
	ClassDB::bind_method(D_METHOD("get_one_shot"), &Particles::get_one_shot);
	ClassDB::bind_method(D_METHOD("get_pre_process_time"), &Particles::get_pre_process_time);
	ClassDB::bind_method(D_METHOD("get_explosiveness_ratio"), &Particles::get_explosiveness_ratio);
	ClassDB::bind_method(D_METHOD("get_randomness_ratio"), &Particles::get_randomness_ratio);
	ClassDB::bind_method(D_METHOD("get_visibility_aabb"), &Particles::get_visibility_aabb);
	ClassDB::bind_method(D_METHOD("get_use_local_coordinates"), &Particles::get_use_local_coordinates);
	ClassDB::bind_method(D_METHOD("get_fixed_fps"), &Particles::get_fixed_fps);
	ClassDB::bind_method(D_METHOD("get_fractional_delta"), &Particles::get_fractional_delta);
	ClassDB::bind_method(D_METHOD("get_process_material"), &Particles::get_process_material);
	ClassDB::bind_method(D_METHOD("get_speed_scale"), &Particles::get_speed_scale);

	ClassDB::bind_method(D_METHOD("set_draw_order", "order"), &Particles::set_draw_order);

	ClassDB::bind_method(D_METHOD("get_draw_order"), &Particles::get_draw_order);

	ClassDB::bind_method(D_METHOD("set_draw_passes", "passes"), &Particles::set_draw_passes);
	ClassDB::bind_method(D_METHOD("set_draw_pass_mesh", "pass", "mesh"), &Particles::set_draw_pass_mesh);

	ClassDB::bind_method(D_METHOD("get_draw_passes"), &Particles::get_draw_passes);
	ClassDB::bind_method(D_METHOD("get_draw_pass_mesh", "pass"), &Particles::get_draw_pass_mesh);

	ClassDB::bind_method(D_METHOD("restart"), &Particles::restart);
	ClassDB::bind_method(D_METHOD("capture_aabb"), &Particles::capture_aabb);

	ADD_SIGNAL(MethodInfo("finished"));

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "emitting"), "set_emitting", "is_emitting");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "amount", PROPERTY_HINT_EXP_RANGE, "1,1000000,1"), "set_amount", "get_amount");
	ADD_GROUP("Time", "");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "lifetime", PROPERTY_HINT_EXP_RANGE, "0.01,600.0,0.01,or_greater"), "set_lifetime", "get_lifetime");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "lifetime_infinite"), "set_lifetime_infinite", "is_lifetime_infinite");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "one_shot"), "set_one_shot", "get_one_shot");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "preprocess", PROPERTY_HINT_EXP_RANGE, "0.00,600.0,0.01"), "set_pre_process_time", "get_pre_process_time");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "speed_scale", PROPERTY_HINT_RANGE, "0,64,0.01"), "set_speed_scale", "get_speed_scale");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "explosiveness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_explosiveness_ratio", "get_explosiveness_ratio");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "randomness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_randomness_ratio", "get_randomness_ratio");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "fixed_fps", PROPERTY_HINT_RANGE, "0,1000,1"), "set_fixed_fps", "get_fixed_fps");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "fract_delta"), "set_fractional_delta", "get_fractional_delta");
	ADD_GROUP("Drawing", "");
	ADD_PROPERTY(PropertyInfo(Variant::AABB, "visibility_aabb"), "set_visibility_aabb", "get_visibility_aabb");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "local_coords"), "set_use_local_coordinates", "get_use_local_coordinates");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "draw_order", PROPERTY_HINT_ENUM, "Index,Lifetime,View Depth"), "set_draw_order", "get_draw_order");
	ADD_GROUP("Process Material", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "process_material", PROPERTY_HINT_RESOURCE_TYPE, "ShaderMaterial,ParticlesMaterial"), "set_process_material", "get_process_material");
	ADD_GROUP("Draw Passes", "draw_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "draw_passes", PROPERTY_HINT_RANGE, "0," + itos(MAX_DRAW_PASSES) + ",1"), "set_draw_passes", "get_draw_passes");
	for (int i = 0; i < MAX_DRAW_PASSES; i++) {
		ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "draw_pass_" + itos(i + 1), PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_draw_pass_mesh", "get_draw_pass_mesh", i);
	}

	BIND_ENUM_CONSTANT(DRAW_ORDER_INDEX);
	BIND_ENUM_CONSTANT(DRAW_ORDER_LIFETIME);
	BIND_ENUM_CONSTANT(DRAW_ORDER_VIEW_DEPTH);

	BIND_CONSTANT(MAX_DRAW_PASSES);
}

Particles::Particles() {
	particles = RID_PRIME(VS::get_singleton()->particles_create());
	set_base(particles);
	one_shot = false; // Needed so that set_emitting doesn't access uninitialized values
	set_emitting(true);
	set_one_shot(false);
	set_amount(8);
	set_lifetime(1);
	set_lifetime_infinite(false);
	set_fixed_fps(0);
	set_fractional_delta(true);
	set_pre_process_time(0);
	set_explosiveness_ratio(0);
	set_randomness_ratio(0);
	set_visibility_aabb(AABB(Vector3(-4, -4, -4), Vector3(8, 8, 8)));
	set_use_local_coordinates(true);
	set_draw_passes(1);
	set_draw_order(DRAW_ORDER_INDEX);
	set_speed_scale(1);
}

Particles::~Particles() {
	if (particles.is_valid()) {
		VS::get_singleton()->free(particles);
	}
}

/**************************************************************************/
/*  reflection_probe.cpp                                                  */
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

#include "reflection_probe.h"

void ReflectionProbe::set_intensity(float p_intensity) {
	intensity = p_intensity;
	RS::get_singleton()->reflection_probe_set_intensity(probe, p_intensity);
}

float ReflectionProbe::get_intensity() const {
	return intensity;
}

void ReflectionProbe::set_blend_distance(float p_blend_distance) {
	blend_distance = p_blend_distance;
	RS::get_singleton()->reflection_probe_set_blend_distance(probe, p_blend_distance);
	update_gizmos();
}

float ReflectionProbe::get_blend_distance() const {
	return blend_distance;
}

void ReflectionProbe::set_ambient_mode(AmbientMode p_mode) {
	ambient_mode = p_mode;
	RS::get_singleton()->reflection_probe_set_ambient_mode(probe, RS::ReflectionProbeAmbientMode(p_mode));
	notify_property_list_changed();
}

ReflectionProbe::AmbientMode ReflectionProbe::get_ambient_mode() const {
	return ambient_mode;
}

void ReflectionProbe::set_ambient_color(Color p_ambient) {
	ambient_color = p_ambient;
	RS::get_singleton()->reflection_probe_set_ambient_color(probe, p_ambient);
}

void ReflectionProbe::set_ambient_color_energy(float p_energy) {
	ambient_color_energy = p_energy;
	RS::get_singleton()->reflection_probe_set_ambient_energy(probe, p_energy);
}

float ReflectionProbe::get_ambient_color_energy() const {
	return ambient_color_energy;
}

Color ReflectionProbe::get_ambient_color() const {
	return ambient_color;
}

void ReflectionProbe::set_max_distance(float p_distance) {
	max_distance = CLAMP(p_distance, 0.0, 262'144.0);
	// Reflection rendering breaks if distance exceeds 262,144 units (due to floating-point precision with the near plane being 0.01).
	RS::get_singleton()->reflection_probe_set_max_distance(probe, max_distance);
}

float ReflectionProbe::get_max_distance() const {
	return max_distance;
}

void ReflectionProbe::set_mesh_lod_threshold(float p_pixels) {
	mesh_lod_threshold = p_pixels;
	RS::get_singleton()->reflection_probe_set_mesh_lod_threshold(probe, p_pixels);
}

float ReflectionProbe::get_mesh_lod_threshold() const {
	return mesh_lod_threshold;
}

void ReflectionProbe::set_size(const Vector3 &p_size) {
	size = p_size;

	for (int i = 0; i < 3; i++) {
		float half_size = size[i] / 2;
		if (half_size < 0.01) {
			half_size = 0.01;
		}

		if (half_size - 0.01 < ABS(origin_offset[i])) {
			origin_offset[i] = SIGN(origin_offset[i]) * (half_size - 0.01);
		}
	}

	RS::get_singleton()->reflection_probe_set_size(probe, size);
	RS::get_singleton()->reflection_probe_set_origin_offset(probe, origin_offset);

	update_gizmos();
}

Vector3 ReflectionProbe::get_size() const {
	return size;
}

void ReflectionProbe::set_origin_offset(const Vector3 &p_offset) {
	origin_offset = p_offset;

	for (int i = 0; i < 3; i++) {
		float half_size = size[i] / 2;
		if (half_size - 0.01 < ABS(origin_offset[i])) {
			origin_offset[i] = SIGN(origin_offset[i]) * (half_size - 0.01);
		}
	}
	RS::get_singleton()->reflection_probe_set_size(probe, size);
	RS::get_singleton()->reflection_probe_set_origin_offset(probe, origin_offset);

	update_gizmos();
}

Vector3 ReflectionProbe::get_origin_offset() const {
	return origin_offset;
}

void ReflectionProbe::set_enable_box_projection(bool p_enable) {
	box_projection = p_enable;
	RS::get_singleton()->reflection_probe_set_enable_box_projection(probe, p_enable);
}

bool ReflectionProbe::is_box_projection_enabled() const {
	return box_projection;
}

void ReflectionProbe::set_as_interior(bool p_enable) {
	interior = p_enable;
	RS::get_singleton()->reflection_probe_set_as_interior(probe, interior);
}

bool ReflectionProbe::is_set_as_interior() const {
	return interior;
}

void ReflectionProbe::set_enable_shadows(bool p_enable) {
	enable_shadows = p_enable;
	RS::get_singleton()->reflection_probe_set_enable_shadows(probe, p_enable);
}

bool ReflectionProbe::are_shadows_enabled() const {
	return enable_shadows;
}

void ReflectionProbe::set_cull_mask(uint32_t p_layers) {
	cull_mask = p_layers;
	RS::get_singleton()->reflection_probe_set_cull_mask(probe, p_layers);
}

uint32_t ReflectionProbe::get_cull_mask() const {
	return cull_mask;
}

void ReflectionProbe::set_reflection_mask(uint32_t p_layers) {
	reflection_mask = p_layers;
	RS::get_singleton()->reflection_probe_set_reflection_mask(probe, p_layers);
}

uint32_t ReflectionProbe::get_reflection_mask() const {
	return reflection_mask;
}

void ReflectionProbe::set_update_mode(UpdateMode p_mode) {
	update_mode = p_mode;
	RS::get_singleton()->reflection_probe_set_update_mode(probe, RS::ReflectionProbeUpdateMode(p_mode));
}

ReflectionProbe::UpdateMode ReflectionProbe::get_update_mode() const {
	return update_mode;
}

AABB ReflectionProbe::get_aabb() const {
	AABB aabb;
	aabb.position = -size / 2;
	aabb.size = size;
	return aabb;
}

void ReflectionProbe::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "ambient_color" || p_property.name == "ambient_color_energy") {
		if (ambient_mode != AMBIENT_COLOR) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		}
	}
}

void ReflectionProbe::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_intensity", "intensity"), &ReflectionProbe::set_intensity);
	ClassDB::bind_method(D_METHOD("get_intensity"), &ReflectionProbe::get_intensity);

	ClassDB::bind_method(D_METHOD("set_blend_distance", "blend_distance"), &ReflectionProbe::set_blend_distance);
	ClassDB::bind_method(D_METHOD("get_blend_distance"), &ReflectionProbe::get_blend_distance);

	ClassDB::bind_method(D_METHOD("set_ambient_mode", "ambient"), &ReflectionProbe::set_ambient_mode);
	ClassDB::bind_method(D_METHOD("get_ambient_mode"), &ReflectionProbe::get_ambient_mode);

	ClassDB::bind_method(D_METHOD("set_ambient_color", "ambient"), &ReflectionProbe::set_ambient_color);
	ClassDB::bind_method(D_METHOD("get_ambient_color"), &ReflectionProbe::get_ambient_color);

	ClassDB::bind_method(D_METHOD("set_ambient_color_energy", "ambient_energy"), &ReflectionProbe::set_ambient_color_energy);
	ClassDB::bind_method(D_METHOD("get_ambient_color_energy"), &ReflectionProbe::get_ambient_color_energy);

	ClassDB::bind_method(D_METHOD("set_max_distance", "max_distance"), &ReflectionProbe::set_max_distance);
	ClassDB::bind_method(D_METHOD("get_max_distance"), &ReflectionProbe::get_max_distance);

	ClassDB::bind_method(D_METHOD("set_mesh_lod_threshold", "ratio"), &ReflectionProbe::set_mesh_lod_threshold);
	ClassDB::bind_method(D_METHOD("get_mesh_lod_threshold"), &ReflectionProbe::get_mesh_lod_threshold);

	ClassDB::bind_method(D_METHOD("set_size", "size"), &ReflectionProbe::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &ReflectionProbe::get_size);

	ClassDB::bind_method(D_METHOD("set_origin_offset", "origin_offset"), &ReflectionProbe::set_origin_offset);
	ClassDB::bind_method(D_METHOD("get_origin_offset"), &ReflectionProbe::get_origin_offset);

	ClassDB::bind_method(D_METHOD("set_as_interior", "enable"), &ReflectionProbe::set_as_interior);
	ClassDB::bind_method(D_METHOD("is_set_as_interior"), &ReflectionProbe::is_set_as_interior);

	ClassDB::bind_method(D_METHOD("set_enable_box_projection", "enable"), &ReflectionProbe::set_enable_box_projection);
	ClassDB::bind_method(D_METHOD("is_box_projection_enabled"), &ReflectionProbe::is_box_projection_enabled);

	ClassDB::bind_method(D_METHOD("set_enable_shadows", "enable"), &ReflectionProbe::set_enable_shadows);
	ClassDB::bind_method(D_METHOD("are_shadows_enabled"), &ReflectionProbe::are_shadows_enabled);

	ClassDB::bind_method(D_METHOD("set_cull_mask", "layers"), &ReflectionProbe::set_cull_mask);
	ClassDB::bind_method(D_METHOD("get_cull_mask"), &ReflectionProbe::get_cull_mask);

	ClassDB::bind_method(D_METHOD("set_reflection_mask", "layers"), &ReflectionProbe::set_reflection_mask);
	ClassDB::bind_method(D_METHOD("get_reflection_mask"), &ReflectionProbe::get_reflection_mask);

	ClassDB::bind_method(D_METHOD("set_update_mode", "mode"), &ReflectionProbe::set_update_mode);
	ClassDB::bind_method(D_METHOD("get_update_mode"), &ReflectionProbe::get_update_mode);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "update_mode", PROPERTY_HINT_ENUM, "Once (Fast),Always (Slow)"), "set_update_mode", "get_update_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "intensity", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_intensity", "get_intensity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "blend_distance", PROPERTY_HINT_RANGE, "0,8,0.01,or_greater,suffix:m"), "set_blend_distance", "get_blend_distance");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_distance", PROPERTY_HINT_RANGE, "0,16384,0.1,or_greater,exp,suffix:m"), "set_max_distance", "get_max_distance");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "size", PROPERTY_HINT_NONE, "suffix:m"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "origin_offset", PROPERTY_HINT_NONE, "suffix:m"), "set_origin_offset", "get_origin_offset");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "box_projection"), "set_enable_box_projection", "is_box_projection_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "interior"), "set_as_interior", "is_set_as_interior");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enable_shadows"), "set_enable_shadows", "are_shadows_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cull_mask", PROPERTY_HINT_LAYERS_3D_RENDER), "set_cull_mask", "get_cull_mask");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "reflection_mask", PROPERTY_HINT_LAYERS_3D_RENDER), "set_reflection_mask", "get_reflection_mask");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mesh_lod_threshold", PROPERTY_HINT_RANGE, "0,1024,0.1"), "set_mesh_lod_threshold", "get_mesh_lod_threshold");

	ADD_GROUP("Ambient", "ambient_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ambient_mode", PROPERTY_HINT_ENUM, "Disabled,Environment,Constant Color"), "set_ambient_mode", "get_ambient_mode");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "ambient_color", PROPERTY_HINT_COLOR_NO_ALPHA), "set_ambient_color", "get_ambient_color");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ambient_color_energy", PROPERTY_HINT_RANGE, "0,16,0.01"), "set_ambient_color_energy", "get_ambient_color_energy");

	BIND_ENUM_CONSTANT(UPDATE_ONCE);
	BIND_ENUM_CONSTANT(UPDATE_ALWAYS);

	BIND_ENUM_CONSTANT(AMBIENT_DISABLED);
	BIND_ENUM_CONSTANT(AMBIENT_ENVIRONMENT);
	BIND_ENUM_CONSTANT(AMBIENT_COLOR);
}

#ifndef DISABLE_DEPRECATED
bool ReflectionProbe::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "extents") { // Compatibility with Godot 3.x.
		set_size((Vector3)p_value * 2);
		return true;
	}
	return false;
}

bool ReflectionProbe::_get(const StringName &p_name, Variant &r_property) const {
	if (p_name == "extents") { // Compatibility with Godot 3.x.
		r_property = size / 2;
		return true;
	}
	return false;
}
#endif // DISABLE_DEPRECATED

ReflectionProbe::ReflectionProbe() {
	probe = RenderingServer::get_singleton()->reflection_probe_create();
	RS::get_singleton()->instance_set_base(get_instance(), probe);
	set_disable_scale(true);
}

ReflectionProbe::~ReflectionProbe() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RS::get_singleton()->free(probe);
}

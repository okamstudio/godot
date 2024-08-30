/**************************************************************************/
/*  importer_mesh_instance_3d.cpp                                         */
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

#include "importer_mesh_instance_3d.h"

#include "scene/resources/3d/importer_mesh.h"

void ImporterMeshInstance3D::set_mesh(const Ref<ImporterMesh> &p_mesh) {
	mesh = p_mesh;
}
Ref<ImporterMesh> ImporterMeshInstance3D::get_mesh() const {
	return mesh;
}

void ImporterMeshInstance3D::set_skin(const Ref<Skin> &p_skin) {
	skin = p_skin;
}
Ref<Skin> ImporterMeshInstance3D::get_skin() const {
	return skin;
}

void ImporterMeshInstance3D::set_surface_material(int p_idx, const Ref<Material> &p_material) {
	ERR_FAIL_COND(p_idx < 0);
	if (p_idx >= surface_materials.size()) {
		surface_materials.resize(p_idx + 1);
	}

	surface_materials.write[p_idx] = p_material;
}
Ref<Material> ImporterMeshInstance3D::get_surface_material(int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, Ref<Material>());
	if (p_idx >= surface_materials.size()) {
		return Ref<Material>();
	}
	return surface_materials[p_idx];
}

void ImporterMeshInstance3D::set_skeleton_path(const NodePath &p_path) {
	skeleton_path = p_path;
}
NodePath ImporterMeshInstance3D::get_skeleton_path() const {
	return skeleton_path;
}

uint32_t ImporterMeshInstance3D::get_layer_mask() const {
	return layer_mask;
}

void ImporterMeshInstance3D::set_layer_mask(const uint32_t p_layer_mask) {
	layer_mask = p_layer_mask;
}

void ImporterMeshInstance3D::set_cast_shadows_setting(GeometryInstance3D::ShadowCastingSetting p_shadow_casting_setting) {
	shadow_casting_setting = p_shadow_casting_setting;
}

GeometryInstance3D::ShadowCastingSetting ImporterMeshInstance3D::get_cast_shadows_setting() const {
	return shadow_casting_setting;
}

void ImporterMeshInstance3D::set_visibility_range_begin(float p_dist) {
	visibility_range_begin = p_dist;
	update_configuration_info();
}

float ImporterMeshInstance3D::get_visibility_range_begin() const {
	return visibility_range_begin;
}

void ImporterMeshInstance3D::set_visibility_range_end(float p_dist) {
	visibility_range_end = p_dist;
	update_configuration_info();
}

float ImporterMeshInstance3D::get_visibility_range_end() const {
	return visibility_range_end;
}

void ImporterMeshInstance3D::set_visibility_range_begin_margin(float p_dist) {
	visibility_range_begin_margin = p_dist;
	update_configuration_info();
}

float ImporterMeshInstance3D::get_visibility_range_begin_margin() const {
	return visibility_range_begin_margin;
}

void ImporterMeshInstance3D::set_visibility_range_end_margin(float p_dist) {
	visibility_range_end_margin = p_dist;
	update_configuration_info();
}

float ImporterMeshInstance3D::get_visibility_range_end_margin() const {
	return visibility_range_end_margin;
}

void ImporterMeshInstance3D::set_visibility_range_fade_mode(GeometryInstance3D::VisibilityRangeFadeMode p_mode) {
	visibility_range_fade_mode = p_mode;
	update_configuration_info();
}

GeometryInstance3D::VisibilityRangeFadeMode ImporterMeshInstance3D::get_visibility_range_fade_mode() const {
	return visibility_range_fade_mode;
}

void ImporterMeshInstance3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mesh", "mesh"), &ImporterMeshInstance3D::set_mesh);
	ClassDB::bind_method(D_METHOD("get_mesh"), &ImporterMeshInstance3D::get_mesh);

	ClassDB::bind_method(D_METHOD("set_skin", "skin"), &ImporterMeshInstance3D::set_skin);
	ClassDB::bind_method(D_METHOD("get_skin"), &ImporterMeshInstance3D::get_skin);

	ClassDB::bind_method(D_METHOD("set_skeleton_path", "skeleton_path"), &ImporterMeshInstance3D::set_skeleton_path);
	ClassDB::bind_method(D_METHOD("get_skeleton_path"), &ImporterMeshInstance3D::get_skeleton_path);

	ClassDB::bind_method(D_METHOD("set_layer_mask", "layer_mask"), &ImporterMeshInstance3D::set_layer_mask);
	ClassDB::bind_method(D_METHOD("get_layer_mask"), &ImporterMeshInstance3D::get_layer_mask);

	ClassDB::bind_method(D_METHOD("set_cast_shadows_setting", "shadow_casting_setting"), &ImporterMeshInstance3D::set_cast_shadows_setting);
	ClassDB::bind_method(D_METHOD("get_cast_shadows_setting"), &ImporterMeshInstance3D::get_cast_shadows_setting);

	ClassDB::bind_method(D_METHOD("set_visibility_range_end_margin", "distance"), &ImporterMeshInstance3D::set_visibility_range_end_margin);
	ClassDB::bind_method(D_METHOD("get_visibility_range_end_margin"), &ImporterMeshInstance3D::get_visibility_range_end_margin);

	ClassDB::bind_method(D_METHOD("set_visibility_range_end", "distance"), &ImporterMeshInstance3D::set_visibility_range_end);
	ClassDB::bind_method(D_METHOD("get_visibility_range_end"), &ImporterMeshInstance3D::get_visibility_range_end);

	ClassDB::bind_method(D_METHOD("set_visibility_range_begin_margin", "distance"), &ImporterMeshInstance3D::set_visibility_range_begin_margin);
	ClassDB::bind_method(D_METHOD("get_visibility_range_begin_margin"), &ImporterMeshInstance3D::get_visibility_range_begin_margin);

	ClassDB::bind_method(D_METHOD("set_visibility_range_begin", "distance"), &ImporterMeshInstance3D::set_visibility_range_begin);
	ClassDB::bind_method(D_METHOD("get_visibility_range_begin"), &ImporterMeshInstance3D::get_visibility_range_begin);

	ClassDB::bind_method(D_METHOD("set_visibility_range_fade_mode", "mode"), &ImporterMeshInstance3D::set_visibility_range_fade_mode);
	ClassDB::bind_method(D_METHOD("get_visibility_range_fade_mode"), &ImporterMeshInstance3D::get_visibility_range_fade_mode);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh", PROPERTY_HINT_RESOURCE_TYPE, "ImporterMesh"), "set_mesh", "get_mesh");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "skin", PROPERTY_HINT_RESOURCE_TYPE, "Skin"), "set_skin", "get_skin");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "skeleton_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Skeleton"), "set_skeleton_path", "get_skeleton_path");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "layer_mask", PROPERTY_HINT_LAYERS_3D_RENDER), "set_layer_mask", "get_layer_mask");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "cast_shadow", PROPERTY_HINT_ENUM, "Off,On,Double-Sided,Shadows Only"), "set_cast_shadows_setting", "get_cast_shadows_setting");

	ADD_GROUP("Visibility Range", "visibility_range_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "visibility_range_begin", PROPERTY_HINT_RANGE, "0.0,4096.0,0.01,or_greater,suffix:m"), "set_visibility_range_begin", "get_visibility_range_begin");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "visibility_range_begin_margin", PROPERTY_HINT_RANGE, "0.0,4096.0,0.01,or_greater,suffix:m"), "set_visibility_range_begin_margin", "get_visibility_range_begin_margin");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "visibility_range_end", PROPERTY_HINT_RANGE, "0.0,4096.0,0.01,or_greater,suffix:m"), "set_visibility_range_end", "get_visibility_range_end");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "visibility_range_end_margin", PROPERTY_HINT_RANGE, "0.0,4096.0,0.01,or_greater,suffix:m"), "set_visibility_range_end_margin", "get_visibility_range_end_margin");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "visibility_range_fade_mode", PROPERTY_HINT_ENUM, "Disabled,Self,Dependencies"), "set_visibility_range_fade_mode", "get_visibility_range_fade_mode");
}

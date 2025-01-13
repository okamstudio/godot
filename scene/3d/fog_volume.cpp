/**************************************************************************/
/*  fog_volume.cpp                                                        */
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

#include "fog_volume.h"

#include "scene/main/viewport.h"
#include "scene/resources/environment.h"

///////////////////////////

void FogVolume::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_size", "size"), &FogVolume::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &FogVolume::get_size);
	ClassDB::bind_method(D_METHOD("set_shape", "shape"), &FogVolume::set_shape);
	ClassDB::bind_method(D_METHOD("get_shape"), &FogVolume::get_shape);
	ClassDB::bind_method(D_METHOD("set_material", "material"), &FogVolume::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &FogVolume::get_material);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "size", PROPERTY_HINT_RANGE, "0.01,1024,0.01,or_greater,suffix:m"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "shape", PROPERTY_HINT_ENUM, "Ellipsoid (Local),Cone (Local),Cylinder (Local),Box (Local),World (Global)"), "set_shape", "get_shape");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "FogMaterial,ShaderMaterial"), "set_material", "get_material");
}

void FogVolume::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "size" && shape == RS::FOG_VOLUME_SHAPE_WORLD) {
		p_property.usage = PROPERTY_USAGE_NONE;
		return;
	}
}

#ifndef DISABLE_DEPRECATED
bool FogVolume::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "extents") { // Compatibility with Godot 3.x.
		set_size((Vector3)p_value * 2);
		return true;
	}
	return false;
}

bool FogVolume::_get(const StringName &p_name, Variant &r_property) const {
	if (p_name == "extents") { // Compatibility with Godot 3.x.
		r_property = size / 2;
		return true;
	}
	return false;
}
#endif // DISABLE_DEPRECATED

void FogVolume::set_size(const Vector3 &p_size) {
	size = p_size;
	size = size.maxf(0);
	RS::get_singleton()->fog_volume_set_size(_get_volume(), size);
	update_gizmos();
}

Vector3 FogVolume::get_size() const {
	return size;
}

void FogVolume::set_shape(RS::FogVolumeShape p_type) {
	shape = p_type;
	RS::get_singleton()->fog_volume_set_shape(_get_volume(), shape);
	RS::get_singleton()->instance_set_ignore_culling(get_instance(), shape == RS::FOG_VOLUME_SHAPE_WORLD);
	update_gizmos();
	notify_property_list_changed();
}

RS::FogVolumeShape FogVolume::get_shape() const {
	return shape;
}

void FogVolume::set_material(const Ref<Material> &p_material) {
	material = p_material;
	RID material_rid;
	if (material.is_valid()) {
		material_rid = material->get_rid();
	}
	RS::get_singleton()->fog_volume_set_material(_get_volume(), material_rid);
	update_gizmos();
}

Ref<Material> FogVolume::get_material() const {
	return material;
}

AABB FogVolume::get_aabb() const {
	if (shape != RS::FOG_VOLUME_SHAPE_WORLD) {
		return AABB(-size / 2, size);
	}
	return AABB();
}

PackedStringArray FogVolume::get_configuration_warnings() const {
	PackedStringArray warnings = VisualInstance3D::get_configuration_warnings();

	Ref<Environment> environment = get_viewport()->find_world_3d()->get_environment();

	if (OS::get_singleton()->get_current_rendering_method() != "forward_plus") {
		warnings.push_back(RTR("Fog Volumes are only visible when using the Forward+ renderer."));
		return warnings;
	}

	if (environment.is_valid() && !environment->is_volumetric_fog_enabled()) {
		warnings.push_back(RTR("Fog Volumes need volumetric fog to be enabled in the scene's Environment in order to be visible."));
	}

	return warnings;
}

FogVolume::FogVolume() {
	volume = RS::get_singleton()->fog_volume_create();
	RS::get_singleton()->fog_volume_set_shape(volume, RS::FOG_VOLUME_SHAPE_BOX);
	set_base(volume);
}

FogVolume::~FogVolume() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RS::get_singleton()->free(volume);
}

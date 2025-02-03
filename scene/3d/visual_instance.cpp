/**************************************************************************/
/*  visual_instance.cpp                                                   */
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

#include "visual_instance.h"

#include "scene/scene_string_names.h"
#include "servers/visual_server.h"
#include "skeleton.h"

AABB VisualInstance::get_aabb() const {
	return AABB();
}

PoolVector<Face3> VisualInstance::get_faces(uint32_t p_usage_flags) const {
	return PoolVector<Face3>();
}

AABB VisualInstance::get_transformed_aabb() const {
	return get_global_transform().xform(get_aabb());
}

void VisualInstance::_refresh_portal_mode() {
	VisualServer::get_singleton()->instance_set_portal_mode(instance, (VisualServer::InstancePortalMode)get_portal_mode());
}

void VisualInstance::_update_visibility() {
	if (!is_inside_tree()) {
		return;
	}

	bool visible = is_visible_in_tree();

	// keep a quick flag available in each node.
	// no need to call is_visible_in_tree all over the place,
	// providing it is propagated with a notification.
	bool already_visible = _is_vi_visible();
	_set_vi_visible(visible);

	// if making visible, make sure the visual server is up to date with the transform
	if (visible && (!already_visible)) {
		if (!_is_using_identity_transform()) {
			Transform gt = get_global_transform();
			VisualServer::get_singleton()->instance_set_transform(instance, gt);
		}
	}

	_change_notify("visible");
	VS::get_singleton()->instance_set_visible(get_instance(), visible);
}

void VisualInstance::set_instance_use_identity_transform(bool p_enable) {
	// prevent sending instance transforms when using global coords
	_set_use_identity_transform(p_enable);

	if (is_inside_tree()) {
		if (p_enable) {
			// want to make sure instance is using identity transform
			VisualServer::get_singleton()->instance_set_transform(instance, Transform());
		} else {
			// want to make sure instance is up to date
			VisualServer::get_singleton()->instance_set_transform(instance, get_global_transform());
		}
	}
}

void VisualInstance::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_WORLD: {
			// CHECK SKELETON => moving skeleton attaching logic to MeshInstance
			/*
			Skeleton *skeleton=Object::cast_to<Skeleton>(get_parent());
			if (skeleton)
				VisualServer::get_singleton()->instance_attach_skeleton( instance, skeleton->get_skeleton() );
			*/
			ERR_FAIL_COND(get_world().is_null());
			VisualServer::get_singleton()->instance_set_scenario(instance, get_world()->get_scenario());
			_update_visibility();

		} break;
		case NOTIFICATION_TRANSFORM_CHANGED: {
			if (_is_vi_visible() || is_physics_interpolated_and_enabled()) {
				if (!_is_using_identity_transform()) {
					VisualServer::get_singleton()->instance_set_transform(instance, get_global_transform());

					// For instance when first adding to the tree, when the previous transform is
					// unset, to prevent streaking from the origin.
					if (_is_physics_interpolation_reset_requested() && is_physics_interpolated_and_enabled() && is_inside_tree()) {
						if (_is_vi_visible()) {
							_notification(NOTIFICATION_RESET_PHYSICS_INTERPOLATION);
						}
						_set_physics_interpolation_reset_requested(false);
					}
				}
			}
		} break;
		case NOTIFICATION_RESET_PHYSICS_INTERPOLATION: {
			if (_is_vi_visible() && is_physics_interpolated() && is_inside_tree()) {
				// We must ensure the VisualServer transform is up to date before resetting.
				// This is because NOTIFICATION_TRANSFORM_CHANGED is deferred,
				// and cannot be relied to be called in order before NOTIFICATION_RESET_PHYSICS_INTERPOLATION.
				if (!_is_using_identity_transform()) {
					VisualServer::get_singleton()->instance_set_transform(instance, get_global_transform());
				}

				VisualServer::get_singleton()->instance_reset_physics_interpolation(instance);
			}
		} break;
		case NOTIFICATION_EXIT_WORLD: {
			VisualServer::get_singleton()->instance_set_scenario(instance, RID());
			VisualServer::get_singleton()->instance_attach_skeleton(instance, RID());
			//VS::get_singleton()->instance_geometry_set_baked_light_sampler(instance, RID() );

			// the vi visible flag is always set to invisible when outside the tree,
			// so it can detect re-entering the tree and becoming visible, and send
			// the transform to the visual server
			_set_vi_visible(false);
		} break;
		case NOTIFICATION_VISIBILITY_CHANGED: {
			_update_visibility();
		} break;
	}
}

void VisualInstance::_physics_interpolated_changed() {
	VisualServer::get_singleton()->instance_set_interpolated(instance, is_physics_interpolated());
}

RID VisualInstance::get_instance() const {
	return instance;
}

RID VisualInstance::_get_visual_instance_rid() const {
	return instance;
}

void VisualInstance::set_layer_mask(uint32_t p_mask) {
	layers = p_mask;
	VisualServer::get_singleton()->instance_set_layer_mask(instance, p_mask);
}

uint32_t VisualInstance::get_layer_mask() const {
	return layers;
}

void VisualInstance::set_layer_mask_bit(int p_layer, bool p_enable) {
	ERR_FAIL_INDEX(p_layer, 32);
	if (p_enable) {
		set_layer_mask(layers | (1 << p_layer));
	} else {
		set_layer_mask(layers & (~(1 << p_layer)));
	}
}

bool VisualInstance::get_layer_mask_bit(int p_layer) const {
	ERR_FAIL_INDEX_V(p_layer, 32, false);
	return (layers & (1 << p_layer));
}

void VisualInstance::set_sorting_offset(float p_offset) {
	sorting_offset = p_offset;
	VisualServer::get_singleton()->instance_set_pivot_data(instance, sorting_offset, sorting_use_aabb_center);
}

float VisualInstance::get_sorting_offset() {
	return sorting_offset;
}

void VisualInstance::set_sorting_use_aabb_center(bool p_enabled) {
	sorting_use_aabb_center = p_enabled;
	VisualServer::get_singleton()->instance_set_pivot_data(instance, sorting_offset, sorting_use_aabb_center);
}

bool VisualInstance::is_sorting_use_aabb_center() {
	return sorting_use_aabb_center;
}

void VisualInstance::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_get_visual_instance_rid"), &VisualInstance::_get_visual_instance_rid);
	ClassDB::bind_method(D_METHOD("set_base", "base"), &VisualInstance::set_base);
	ClassDB::bind_method(D_METHOD("get_base"), &VisualInstance::get_base);
	ClassDB::bind_method(D_METHOD("get_instance"), &VisualInstance::get_instance);
	ClassDB::bind_method(D_METHOD("set_layer_mask", "mask"), &VisualInstance::set_layer_mask);
	ClassDB::bind_method(D_METHOD("get_layer_mask"), &VisualInstance::get_layer_mask);
	ClassDB::bind_method(D_METHOD("set_layer_mask_bit", "layer", "enabled"), &VisualInstance::set_layer_mask_bit);
	ClassDB::bind_method(D_METHOD("get_layer_mask_bit", "layer"), &VisualInstance::get_layer_mask_bit);
	ClassDB::bind_method(D_METHOD("get_transformed_aabb"), &VisualInstance::get_transformed_aabb);
	ClassDB::bind_method(D_METHOD("set_sorting_offset", "offset"), &VisualInstance::set_sorting_offset);
	ClassDB::bind_method(D_METHOD("get_sorting_offset"), &VisualInstance::get_sorting_offset);
	ClassDB::bind_method(D_METHOD("set_sorting_use_aabb_center", "enabled"), &VisualInstance::set_sorting_use_aabb_center);
	ClassDB::bind_method(D_METHOD("is_sorting_use_aabb_center"), &VisualInstance::is_sorting_use_aabb_center);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "layers", PROPERTY_HINT_LAYERS_3D_RENDER), "set_layer_mask", "get_layer_mask");

	ADD_GROUP("Sorting", "sorting_");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "sorting_offset"), "set_sorting_offset", "get_sorting_offset");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "sorting_use_aabb_center"), "set_sorting_use_aabb_center", "is_sorting_use_aabb_center");
}

void VisualInstance::set_base(const RID &p_base) {
	VisualServer::get_singleton()->instance_set_base(instance, p_base);
	base = p_base;
}

RID VisualInstance::get_base() const {
	return base;
}

VisualInstance::VisualInstance() {
	instance = RID_PRIME(VisualServer::get_singleton()->instance_create());
	VisualServer::get_singleton()->instance_attach_object_instance_id(instance, get_instance_id());
	layers = 1;
	sorting_offset = 0.0f;
	sorting_use_aabb_center = true;
	set_notify_transform(true);
}

VisualInstance::~VisualInstance() {
	VisualServer::get_singleton()->free(instance);
}

void GeometryInstance::set_material_override(const Ref<Material> &p_material) {
	material_override = p_material;
	VS::get_singleton()->instance_geometry_set_material_override(get_instance(), p_material.is_valid() ? p_material->get_rid() : RID());
}

Ref<Material> GeometryInstance::get_material_override() const {
	return material_override;
}

void GeometryInstance::set_material_overlay(const Ref<Material> &p_material) {
	material_overlay = p_material;
	VS::get_singleton()->instance_geometry_set_material_overlay(get_instance(), p_material.is_valid() ? p_material->get_rid() : RID());
}

Ref<Material> GeometryInstance::get_material_overlay() const {
	return material_overlay;
}

void GeometryInstance::set_generate_lightmap(bool p_enabled) {
	generate_lightmap = p_enabled;
}

bool GeometryInstance::get_generate_lightmap() const {
	return generate_lightmap;
}

void GeometryInstance::set_lightmap_scale(LightmapScale p_scale) {
	ERR_FAIL_INDEX(p_scale, LIGHTMAP_SCALE_MAX);
	lightmap_scale = p_scale;
}

GeometryInstance::LightmapScale GeometryInstance::get_lightmap_scale() const {
	return lightmap_scale;
}

void GeometryInstance::_notification(int p_what) {
}

void GeometryInstance::set_flag(Flags p_flag, bool p_value) {
	ERR_FAIL_INDEX(p_flag, FLAG_MAX);
	if (flags[p_flag] == p_value) {
		return;
	}

	flags[p_flag] = p_value;
	VS::get_singleton()->instance_geometry_set_flag(get_instance(), (VS::InstanceFlags)p_flag, p_value);
}

bool GeometryInstance::get_flag(Flags p_flag) const {
	ERR_FAIL_INDEX_V(p_flag, FLAG_MAX, false);

	return flags[p_flag];
}

void GeometryInstance::set_cast_shadows_setting(ShadowCastingSetting p_shadow_casting_setting) {
	if (p_shadow_casting_setting != shadow_casting_setting) {
		shadow_casting_setting = p_shadow_casting_setting;
		VS::get_singleton()->instance_geometry_set_cast_shadows_setting(get_instance(), (VS::ShadowCastingSetting)p_shadow_casting_setting);
	}
}

GeometryInstance::ShadowCastingSetting GeometryInstance::get_cast_shadows_setting() const {
	return shadow_casting_setting;
}

void GeometryInstance::set_extra_cull_margin(float p_margin) {
	ERR_FAIL_COND(p_margin < 0);
	if (p_margin != extra_cull_margin) {
		extra_cull_margin = p_margin;
		VS::get_singleton()->instance_set_extra_visibility_margin(get_instance(), extra_cull_margin);
	}
}

float GeometryInstance::get_extra_cull_margin() const {
	return extra_cull_margin;
}

void GeometryInstance::set_custom_aabb(AABB aabb) {
	VS::get_singleton()->instance_set_custom_aabb(get_instance(), aabb);
}

void GeometryInstance::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_material_override", "material"), &GeometryInstance::set_material_override);
	ClassDB::bind_method(D_METHOD("get_material_override"), &GeometryInstance::get_material_override);

	ClassDB::bind_method(D_METHOD("set_material_overlay", "material"), &GeometryInstance::set_material_overlay);
	ClassDB::bind_method(D_METHOD("get_material_overlay"), &GeometryInstance::get_material_overlay);

	ClassDB::bind_method(D_METHOD("set_flag", "flag", "value"), &GeometryInstance::set_flag);
	ClassDB::bind_method(D_METHOD("get_flag", "flag"), &GeometryInstance::get_flag);

	ClassDB::bind_method(D_METHOD("set_cast_shadows_setting", "shadow_casting_setting"), &GeometryInstance::set_cast_shadows_setting);
	ClassDB::bind_method(D_METHOD("get_cast_shadows_setting"), &GeometryInstance::get_cast_shadows_setting);

	ClassDB::bind_method(D_METHOD("set_generate_lightmap", "enabled"), &GeometryInstance::set_generate_lightmap);
	ClassDB::bind_method(D_METHOD("get_generate_lightmap"), &GeometryInstance::get_generate_lightmap);

	ClassDB::bind_method(D_METHOD("set_lightmap_scale", "scale"), &GeometryInstance::set_lightmap_scale);
	ClassDB::bind_method(D_METHOD("get_lightmap_scale"), &GeometryInstance::get_lightmap_scale);

	ClassDB::bind_method(D_METHOD("set_extra_cull_margin", "margin"), &GeometryInstance::set_extra_cull_margin);
	ClassDB::bind_method(D_METHOD("get_extra_cull_margin"), &GeometryInstance::get_extra_cull_margin);

	ClassDB::bind_method(D_METHOD("set_custom_aabb", "aabb"), &GeometryInstance::set_custom_aabb);

	ClassDB::bind_method(D_METHOD("get_aabb"), &GeometryInstance::get_aabb);

	ADD_GROUP("Geometry", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material_override", PROPERTY_HINT_RESOURCE_TYPE, "ShaderMaterial,SpatialMaterial,ORMSpatialMaterial"), "set_material_override", "get_material_override");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material_overlay", PROPERTY_HINT_RESOURCE_TYPE, "ShaderMaterial,SpatialMaterial,ORMSpatialMaterial"), "set_material_overlay", "get_material_overlay");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cast_shadow", PROPERTY_HINT_ENUM, "Off,On,Double-Sided,Shadows Only"), "set_cast_shadows_setting", "get_cast_shadows_setting");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "extra_cull_margin", PROPERTY_HINT_RANGE, "0,16384,0.01"), "set_extra_cull_margin", "get_extra_cull_margin");

	ADD_GROUP("Baked Light", "");
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "use_in_baked_light"), "set_flag", "get_flag", FLAG_USE_BAKED_LIGHT);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "generate_lightmap"), "set_generate_lightmap", "get_generate_lightmap");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "lightmap_scale", PROPERTY_HINT_ENUM, "1x,2x,4x,8x"), "set_lightmap_scale", "get_lightmap_scale");

	//ADD_SIGNAL( MethodInfo("visibility_changed"));

	BIND_ENUM_CONSTANT(LIGHTMAP_SCALE_1X);
	BIND_ENUM_CONSTANT(LIGHTMAP_SCALE_2X);
	BIND_ENUM_CONSTANT(LIGHTMAP_SCALE_4X);
	BIND_ENUM_CONSTANT(LIGHTMAP_SCALE_8X);
	BIND_ENUM_CONSTANT(LIGHTMAP_SCALE_MAX);

	BIND_ENUM_CONSTANT(SHADOW_CASTING_SETTING_OFF);
	BIND_ENUM_CONSTANT(SHADOW_CASTING_SETTING_ON);
	BIND_ENUM_CONSTANT(SHADOW_CASTING_SETTING_DOUBLE_SIDED);
	BIND_ENUM_CONSTANT(SHADOW_CASTING_SETTING_SHADOWS_ONLY);

	BIND_ENUM_CONSTANT(FLAG_USE_BAKED_LIGHT);
	BIND_ENUM_CONSTANT(FLAG_DRAW_NEXT_FRAME_IF_VISIBLE);
	BIND_ENUM_CONSTANT(FLAG_MAX);
}

GeometryInstance::GeometryInstance() {
	for (int i = 0; i < FLAG_MAX; i++) {
		flags[i] = false;
	}

	shadow_casting_setting = SHADOW_CASTING_SETTING_ON;
	extra_cull_margin = 0;
	generate_lightmap = true;
	lightmap_scale = LightmapScale::LIGHTMAP_SCALE_1X;
	//VS::get_singleton()->instance_geometry_set_baked_light_texture_index(get_instance(),0);
}

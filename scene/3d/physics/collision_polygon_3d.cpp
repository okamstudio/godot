/**************************************************************************/
/*  collision_polygon_3d.cpp                                              */
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

#include "collision_polygon_3d.h"

#include "core/math/geometry_2d.h"
#include "scene/3d/physics/collision_object_3d.h"
#include "scene/resources/3d/convex_polygon_shape_3d.h"

void CollisionPolygon3D::_build_polygon() {
	if (!collision_object) {
		return;
	}

	collision_object->shape_owner_clear_shapes(owner_id);

	if (polygon.size() == 0) {
		return;
	}

	Vector<Vector<Vector2>> decomp = Geometry2D::decompose_polygon_in_convex(polygon);
	if (decomp.size() == 0) {
		return;
	}

	//here comes the sun, lalalala
	//decompose concave into multiple convex polygons and add them

	for (int i = 0; i < decomp.size(); i++) {
		Ref<ConvexPolygonShape3D> convex = memnew(ConvexPolygonShape3D);
		Vector<Vector3> cp;
		int cs = decomp[i].size();
		cp.resize(cs * 2);
		{
			Vector3 *w = cp.ptrw();
			int idx = 0;
			for (int j = 0; j < cs; j++) {
				Vector2 d = decomp[i][j];
				w[idx++] = Vector3(d.x, d.y, depth * 0.5);
				w[idx++] = Vector3(d.x, d.y, -depth * 0.5);
			}
		}

		convex->set_points(cp);
		convex->set_margin(margin);
		convex->set_debug_color(debug_color);
		convex->set_debug_fill(debug_fill);
		collision_object->shape_owner_add_shape(owner_id, convex);
		collision_object->shape_owner_set_disabled(owner_id, disabled);
	}
}

void CollisionPolygon3D::_update_in_shape_owner(bool p_xform_only) {
	collision_object->shape_owner_set_transform(owner_id, get_transform());
	if (p_xform_only) {
		return;
	}
	collision_object->shape_owner_set_disabled(owner_id, disabled);
}

void CollisionPolygon3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PARENTED: {
			collision_object = Object::cast_to<CollisionObject3D>(get_parent());
			if (collision_object) {
				owner_id = collision_object->create_shape_owner(this);
				_build_polygon();
				_update_in_shape_owner();
			}
		} break;

		case NOTIFICATION_ENTER_TREE: {
			if (collision_object) {
				_update_in_shape_owner();
			}
		} break;

		case NOTIFICATION_LOCAL_TRANSFORM_CHANGED: {
			if (collision_object) {
				_update_in_shape_owner(true);
			}
			update_configuration_warnings();
		} break;

		case NOTIFICATION_UNPARENTED: {
			if (collision_object) {
				collision_object->remove_shape_owner(owner_id);
			}
			owner_id = 0;
			collision_object = nullptr;
		} break;
	}
}

void CollisionPolygon3D::set_polygon(const Vector<Point2> &p_polygon) {
	polygon = p_polygon;
	if (collision_object) {
		_build_polygon();
	}
	update_configuration_warnings();
	update_gizmos();
}

Vector<Point2> CollisionPolygon3D::get_polygon() const {
	return polygon;
}

AABB CollisionPolygon3D::get_item_rect() const {
	return aabb;
}

void CollisionPolygon3D::set_depth(real_t p_depth) {
	depth = p_depth;
	_build_polygon();
	update_gizmos();
}

real_t CollisionPolygon3D::get_depth() const {
	return depth;
}

void CollisionPolygon3D::set_disabled(bool p_disabled) {
	disabled = p_disabled;
	update_gizmos();

	if (collision_object) {
		collision_object->shape_owner_set_disabled(owner_id, p_disabled);
	}
}

bool CollisionPolygon3D::is_disabled() const {
	return disabled;
}

Color CollisionPolygon3D::_get_default_debug_color() const {
	const SceneTree *st = SceneTree::get_singleton();
	return st ? st->get_debug_collisions_color() : Color(0.0, 0.0, 0.0, 0.0);
}

void CollisionPolygon3D::set_debug_color(const Color &p_color) {
	if (debug_color == p_color) {
		return;
	}

	debug_color = p_color;

	update_gizmos();
}

Color CollisionPolygon3D::get_debug_color() const {
	return debug_color;
}

void CollisionPolygon3D::set_debug_fill_enabled(bool p_enable) {
	if (debug_fill == p_enable) {
		return;
	}

	debug_fill = p_enable;

	update_gizmos();
}

bool CollisionPolygon3D::get_debug_fill_enabled() const {
	return debug_fill;
}

#ifdef DEBUG_ENABLED

bool CollisionPolygon3D::_property_can_revert(const StringName &p_name) const {
	if (p_name == "debug_color") {
		return true;
	}
	return false;
}

bool CollisionPolygon3D::_property_get_revert(const StringName &p_name, Variant &r_property) const {
	if (p_name == "debug_color") {
		r_property = _get_default_debug_color();
		return true;
	}
	return false;
}

void CollisionPolygon3D::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "debug_color") {
		if (debug_color == _get_default_debug_color()) {
			p_property.usage = PROPERTY_USAGE_DEFAULT & ~PROPERTY_USAGE_STORAGE;
		} else {
			p_property.usage = PROPERTY_USAGE_DEFAULT;
		}
	}
}

#endif // DEBUG_ENABLED

real_t CollisionPolygon3D::get_margin() const {
	return margin;
}

void CollisionPolygon3D::set_margin(real_t p_margin) {
	margin = p_margin;
	if (collision_object) {
		_build_polygon();
	}
}

PackedStringArray CollisionPolygon3D::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();

	if (!Object::cast_to<CollisionObject3D>(get_parent())) {
		warnings.push_back(RTR("CollisionPolygon3D only serves to provide a collision shape to a CollisionObject3D derived node.\nPlease only use it as a child of Area3D, StaticBody3D, RigidBody3D, CharacterBody3D, etc. to give them a shape."));
	}

	if (polygon.is_empty()) {
		warnings.push_back(RTR("An empty CollisionPolygon3D has no effect on collision."));
	}

	Vector3 scale = get_transform().get_basis().get_scale();
	if (!(Math::is_zero_approx(scale.x - scale.y) && Math::is_zero_approx(scale.y - scale.z))) {
		warnings.push_back(RTR("A non-uniformly scaled CollisionPolygon3D node will probably not function as expected.\nPlease make its scale uniform (i.e. the same on all axes), and change its polygon's vertices instead."));
	}

	return warnings;
}

bool CollisionPolygon3D::_is_editable_3d_polygon() const {
	return true;
}

void CollisionPolygon3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_depth", "depth"), &CollisionPolygon3D::set_depth);
	ClassDB::bind_method(D_METHOD("get_depth"), &CollisionPolygon3D::get_depth);

	ClassDB::bind_method(D_METHOD("set_polygon", "polygon"), &CollisionPolygon3D::set_polygon);
	ClassDB::bind_method(D_METHOD("get_polygon"), &CollisionPolygon3D::get_polygon);

	ClassDB::bind_method(D_METHOD("set_disabled", "disabled"), &CollisionPolygon3D::set_disabled);
	ClassDB::bind_method(D_METHOD("is_disabled"), &CollisionPolygon3D::is_disabled);

	ClassDB::bind_method(D_METHOD("set_debug_color", "color"), &CollisionPolygon3D::set_debug_color);
	ClassDB::bind_method(D_METHOD("get_debug_color"), &CollisionPolygon3D::get_debug_color);

	ClassDB::bind_method(D_METHOD("set_enable_debug_fill", "enable"), &CollisionPolygon3D::set_debug_fill_enabled);
	ClassDB::bind_method(D_METHOD("get_enable_debug_fill"), &CollisionPolygon3D::get_debug_fill_enabled);

	ClassDB::bind_method(D_METHOD("set_margin", "margin"), &CollisionPolygon3D::set_margin);
	ClassDB::bind_method(D_METHOD("get_margin"), &CollisionPolygon3D::get_margin);

	ClassDB::bind_method(D_METHOD("_is_editable_3d_polygon"), &CollisionPolygon3D::_is_editable_3d_polygon);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "depth", PROPERTY_HINT_NONE, "suffix:m"), "set_depth", "get_depth");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disabled"), "set_disabled", "is_disabled");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "polygon"), "set_polygon", "get_polygon");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "margin", PROPERTY_HINT_RANGE, "0.001,10,0.001,suffix:m"), "set_margin", "get_margin");

	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "debug_color"), "set_debug_color", "get_debug_color");
	// Default value depends on a project setting, override for doc generation purposes.
	ADD_PROPERTY_DEFAULT("debug_color", Color(0.0, 0.0, 0.0, 0.0));

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_fill"), "set_enable_debug_fill", "get_enable_debug_fill");
}

CollisionPolygon3D::CollisionPolygon3D() {
	set_notify_local_transform(true);
	debug_color = _get_default_debug_color();
}

/**************************************************************************/
/*  collision_shape_3d.cpp                                                */
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

#include "collision_shape_3d.h"

#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/physics/character_body_3d.h"
#include "scene/3d/physics/vehicle_body_3d.h"
#include "scene/resources/3d/concave_polygon_shape_3d.h"
#include "scene/resources/3d/convex_polygon_shape_3d.h"
#include "scene/resources/3d/world_boundary_shape_3d.h"

void CollisionShape3D::make_convex_from_siblings() {
	Node *p = get_parent();
	if (!p) {
		return;
	}

	Vector<Vector3> vertices;

	for (int i = 0; i < p->get_child_count(); i++) {
		Node *n = p->get_child(i);
		MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(n);
		if (mi) {
			Ref<Mesh> m = mi->get_mesh();
			if (m.is_valid()) {
				for (int j = 0; j < m->get_surface_count(); j++) {
					Array a = m->surface_get_arrays(j);
					if (!a.is_empty()) {
						Vector<Vector3> v = a[RenderingServer::ARRAY_VERTEX];
						for (int k = 0; k < v.size(); k++) {
							vertices.append(mi->get_transform().xform(v[k]));
						}
					}
				}
			}
		}
	}

	Ref<ConvexPolygonShape3D> shape_new = memnew(ConvexPolygonShape3D);
	shape_new->set_points(vertices);
	set_shape(shape_new);
}

void CollisionShape3D::_update_in_shape_owner(bool p_xform_only) {
	collision_object->shape_owner_set_transform(owner_id, get_transform());
	if (p_xform_only) {
		return;
	}
	collision_object->shape_owner_set_disabled(owner_id, disabled);
}

void CollisionShape3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PARENTED: {
			collision_object = Object::cast_to<CollisionObject3D>(get_parent());
			if (collision_object) {
				owner_id = collision_object->create_shape_owner(this);
				if (shape.is_valid()) {
					collision_object->shape_owner_add_shape(owner_id, shape);
				}

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

#ifndef DISABLE_DEPRECATED
void CollisionShape3D::resource_changed(Ref<Resource> res) {
}
#endif

PackedStringArray CollisionShape3D::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();

	CollisionObject3D *col_object = Object::cast_to<CollisionObject3D>(get_parent());
	if (col_object == nullptr) {
		warnings.push_back(RTR("CollisionShape3D only serves to provide a collision shape to a CollisionObject3D derived node.\nPlease only use it as a child of Area3D, StaticBody3D, RigidBody3D, CharacterBody3D, etc. to give them a shape."));
	}

	if (shape.is_null()) {
		warnings.push_back(RTR("A shape must be provided for CollisionShape3D to function. Please create a shape resource for it."));
	}

	if (shape.is_valid() && Object::cast_to<RigidBody3D>(col_object)) {
		String body_type = "RigidBody3D";
		if (Object::cast_to<VehicleBody3D>(col_object)) {
			body_type = "VehicleBody3D";
		}

		if (Object::cast_to<ConcavePolygonShape3D>(*shape)) {
			warnings.push_back(vformat(RTR("When used for collision, ConcavePolygonShape3D is intended to work with static CollisionObject3D nodes like StaticBody3D.\nIt will likely not behave well for %ss (except when frozen and freeze_mode set to FREEZE_MODE_STATIC)."), body_type));
		} else if (Object::cast_to<WorldBoundaryShape3D>(*shape)) {
			warnings.push_back(RTR("WorldBoundaryShape3D doesn't support RigidBody3D in another mode than static."));
		}
	}

	if (shape.is_valid() && Object::cast_to<CharacterBody3D>(col_object)) {
		if (Object::cast_to<ConcavePolygonShape3D>(*shape)) {
			warnings.push_back(RTR("When used for collision, ConcavePolygonShape3D is intended to work with static CollisionObject3D nodes like StaticBody3D.\nIt will likely not behave well for CharacterBody3Ds."));
		}
	}

	if (!get_transform().get_basis().is_conformal() || !get_global_transform().get_basis().is_conformal()) {
		warnings.push_back(RTR("A non-uniformly scaled CollisionShape3D node will probably not function as expected.\nPlease make its scale uniform (i.e. the same on all axes), and change the size of its shape resource instead."));
	}

	return warnings;
}

void CollisionShape3D::_bind_methods() {
#ifndef DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("resource_changed", "resource"), &CollisionShape3D::resource_changed);
#endif
	ClassDB::bind_method(D_METHOD("set_shape", "shape"), &CollisionShape3D::set_shape);
	ClassDB::bind_method(D_METHOD("get_shape"), &CollisionShape3D::get_shape);
	ClassDB::bind_method(D_METHOD("set_disabled", "enable"), &CollisionShape3D::set_disabled);
	ClassDB::bind_method(D_METHOD("is_disabled"), &CollisionShape3D::is_disabled);

	ClassDB::bind_method(D_METHOD("make_convex_from_siblings"), &CollisionShape3D::make_convex_from_siblings);
	ClassDB::set_method_flags("CollisionShape3D", "make_convex_from_siblings", METHOD_FLAGS_DEFAULT | METHOD_FLAG_EDITOR);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "shape", PROPERTY_HINT_RESOURCE_TYPE, "Shape3D"), "set_shape", "get_shape");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disabled"), "set_disabled", "is_disabled");

	ClassDB::bind_method(D_METHOD("set_debug_color", "color"), &CollisionShape3D::set_debug_color);
	ClassDB::bind_method(D_METHOD("get_debug_color"), &CollisionShape3D::get_debug_color);

	ClassDB::bind_method(D_METHOD("set_enable_debug_fill", "enable"), &CollisionShape3D::set_debug_fill_enabled);
	ClassDB::bind_method(D_METHOD("get_enable_debug_fill"), &CollisionShape3D::get_debug_fill_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "debug_color"), "set_debug_color", "get_debug_color");
	// Default value depends on a project setting, override for doc generation purposes.
	ADD_PROPERTY_DEFAULT("debug_color", Color(0.0, 0.0, 0.0, 0.0));

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_fill"), "set_enable_debug_fill", "get_enable_debug_fill");
}

void CollisionShape3D::set_shape(const Ref<Shape3D> &p_shape) {
	if (p_shape == shape) {
		return;
	}
	if (shape.is_valid()) {
#ifdef DEBUG_ENABLED
		shape->disconnect_changed(callable_mp(this, &CollisionShape3D::_shape_changed));
#endif // DEBUG_ENABLED
		shape->disconnect_changed(callable_mp((Node3D *)this, &Node3D::update_gizmos));
	}
	shape = p_shape;
	if (shape.is_valid()) {
#ifdef DEBUG_ENABLED
		if (shape->are_debug_properties_edited()) {
			set_debug_color(shape->get_debug_color());
			set_debug_fill_enabled(shape->get_debug_fill());
		} else {
			shape->set_debug_color(debug_color);
			shape->set_debug_fill(debug_fill);
		}
#endif // DEBUG_ENABLED

		shape->connect_changed(callable_mp((Node3D *)this, &Node3D::update_gizmos));
#ifdef DEBUG_ENABLED
		shape->connect_changed(callable_mp(this, &CollisionShape3D::_shape_changed));
#endif // DEBUG_ENABLED
	}
	update_gizmos();
	if (collision_object) {
		collision_object->shape_owner_clear_shapes(owner_id);
		if (shape.is_valid()) {
			collision_object->shape_owner_add_shape(owner_id, shape);
		}
	}

	if (is_inside_tree() && collision_object) {
		// If this is a heightfield shape our center may have changed
		_update_in_shape_owner(true);
	}
	update_configuration_warnings();
}

Ref<Shape3D> CollisionShape3D::get_shape() const {
	return shape;
}

void CollisionShape3D::set_disabled(bool p_disabled) {
	disabled = p_disabled;
	update_gizmos();
	if (collision_object) {
		collision_object->shape_owner_set_disabled(owner_id, p_disabled);
	}
}

bool CollisionShape3D::is_disabled() const {
	return disabled;
}

Color CollisionShape3D::_get_default_debug_color() const {
	const SceneTree *st = SceneTree::get_singleton();
	return st ? st->get_debug_collisions_color() : Color(0.0, 0.0, 0.0, 0.0);
}

void CollisionShape3D::set_debug_color(const Color &p_color) {
	if (debug_color == p_color) {
		return;
	}

	debug_color = p_color;

	if (shape.is_valid()) {
		shape->set_debug_color(p_color);
	}
}

Color CollisionShape3D::get_debug_color() const {
	return debug_color;
}

void CollisionShape3D::set_debug_fill_enabled(bool p_enable) {
	if (debug_fill == p_enable) {
		return;
	}

	debug_fill = p_enable;

	if (shape.is_valid()) {
		shape->set_debug_fill(p_enable);
	}
}

bool CollisionShape3D::get_debug_fill_enabled() const {
	return debug_fill;
}

#ifdef DEBUG_ENABLED

bool CollisionShape3D::_property_can_revert(const StringName &p_name) const {
	if (p_name == "debug_color") {
		return true;
	}
	return false;
}

bool CollisionShape3D::_property_get_revert(const StringName &p_name, Variant &r_property) const {
	if (p_name == "debug_color") {
		r_property = _get_default_debug_color();
		return true;
	}
	return false;
}

void CollisionShape3D::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "debug_color") {
		if (debug_color == _get_default_debug_color()) {
			p_property.usage = PROPERTY_USAGE_DEFAULT & ~PROPERTY_USAGE_STORAGE;
		} else {
			p_property.usage = PROPERTY_USAGE_DEFAULT;
		}
	}
}

void CollisionShape3D::_shape_changed() {
	if (shape->get_debug_color() != debug_color) {
		set_debug_color(shape->get_debug_color());
	}
	if (shape->get_debug_fill() != debug_fill) {
		set_debug_fill_enabled(shape->get_debug_fill());
	}
}

#endif // DEBUG_ENABLED

CollisionShape3D::CollisionShape3D() {
	//indicator = RenderingServer::get_singleton()->mesh_create();
	set_notify_local_transform(true);
	debug_color = _get_default_debug_color();
}

CollisionShape3D::~CollisionShape3D() {
	//RenderingServer::get_singleton()->free(indicator);
}

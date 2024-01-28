/**************************************************************************/
/*  collision_shape_2d.cpp                                                */
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

#include "collision_shape_2d.h"

#include "scene/2d/physics/area_2d.h"
#include "scene/2d/physics/collision_object_2d.h"
#include "scene/resources/2d/concave_polygon_shape_2d.h"
#include "scene/resources/2d/convex_polygon_shape_2d.h"

void CollisionShape2D::_shape_changed() {
	queue_redraw();
}

CollisionObject2D *CollisionShape2D::_get_ancestor_collision_object() const {
	Node *parent = get_parent();
	while (parent) {
		CanvasItem *parent_2d = Object::cast_to<CanvasItem>(parent);
		if (unlikely(!parent_2d)) {
			return nullptr;
		}
		CollisionObject2D *co = Object::cast_to<CollisionObject2D>(parent);
		if (likely(co)) {
			return co;
		}
		parent = parent->get_parent();
	}
	return nullptr;
}

Transform2D CollisionShape2D::_get_transform_to_collision_object() const {
	Transform2D transform_to_col_obj = get_transform();
	Node *parent = get_parent();
	while (parent != collision_object) {
		CanvasItem *parent_2d = Object::cast_to<CanvasItem>(parent);
		if (unlikely(!parent_2d)) {
			break;
		}
		transform_to_col_obj = parent_2d->get_transform() * transform_to_col_obj;
		parent = parent->get_parent();
	}
	return transform_to_col_obj;
}

void CollisionShape2D::_set_transform_notifications() {
	if (collision_object == get_parent()) {
		set_notify_local_transform(true);
		set_notify_transform(false);
	} else {
		set_notify_local_transform(false);
		set_notify_transform(true);
	}
}

void CollisionShape2D::_update_transform_in_shape_owner() {
	Transform2D transform_to_col_obj = _get_transform_to_collision_object();
	if (transform_to_col_obj == transform_to_col_obj_cache) {
		return;
	}
	transform_to_col_obj_cache = transform_to_col_obj;
	collision_object->shape_owner_set_transform(owner_id, transform_to_col_obj);
}

void CollisionShape2D::_update_in_shape_owner() {
	_update_transform_in_shape_owner();
	collision_object->shape_owner_set_disabled(owner_id, disabled);
	collision_object->shape_owner_set_one_way_collision(owner_id, one_way_collision);
	collision_object->shape_owner_set_one_way_collision_margin(owner_id, one_way_collision_margin);
}

void CollisionShape2D::_create_shape_owner_in_collision_object() {
	owner_id = collision_object->create_shape_owner(this);
	if (shape.is_valid()) {
		collision_object->shape_owner_add_shape(owner_id, shape);
	}
	_set_transform_notifications();
	_update_in_shape_owner();
}

void CollisionShape2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PARENTED: {
			collision_object = _get_ancestor_collision_object();
			if (collision_object) {
				_create_shape_owner_in_collision_object();
			}
		} break;
		case NOTIFICATION_ENTER_TREE: {
			CollisionObject2D *ancestor_col_obj = _get_ancestor_collision_object();
			if (ancestor_col_obj != collision_object) {
				collision_object = ancestor_col_obj;
				if (collision_object) {
					_create_shape_owner_in_collision_object();
				}
			}
		} break;

		case NOTIFICATION_LOCAL_TRANSFORM_CHANGED:
		case NOTIFICATION_TRANSFORM_CHANGED: {
			if (collision_object) {
				_update_transform_in_shape_owner();
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (collision_object) {
				collision_object->remove_shape_owner(owner_id);
			}
			owner_id = 0;
			collision_object = nullptr;
		} break;

		case NOTIFICATION_DRAW: {
			ERR_FAIL_COND(!is_inside_tree());

			if (!Engine::get_singleton()->is_editor_hint() && !get_tree()->is_debugging_collisions_hint()) {
				break;
			}

			if (shape.is_null()) {
				break;
			}

			rect = Rect2();

			Color draw_col = debug_color;
			if (disabled) {
				float g = draw_col.get_v();
				draw_col.r = g;
				draw_col.g = g;
				draw_col.b = g;
				draw_col.a *= 0.5;
			}
			shape->draw(get_canvas_item(), draw_col);

			rect = shape->get_rect();
			rect = rect.grow(3);

			if (one_way_collision) {
				// Draw an arrow indicating the one-way collision direction
				draw_col = debug_color.inverted();
				if (disabled) {
					draw_col = draw_col.darkened(0.25);
				}
				Vector2 line_to(0, 20);
				draw_line(Vector2(), line_to, draw_col, 2);
				real_t tsize = 8;

				Vector<Vector2> pts{
					line_to + Vector2(0, tsize),
					line_to + Vector2(Math_SQRT12 * tsize, 0),
					line_to + Vector2(-Math_SQRT12 * tsize, 0)
				};

				Vector<Color> cols{ draw_col, draw_col, draw_col };

				draw_primitive(pts, cols, Vector<Vector2>());
			}
		} break;
	}
}

void CollisionShape2D::set_shape(const Ref<Shape2D> &p_shape) {
	if (p_shape == shape) {
		return;
	}
	if (shape.is_valid()) {
		shape->disconnect_changed(callable_mp(this, &CollisionShape2D::_shape_changed));
	}
	shape = p_shape;
	queue_redraw();
	if (collision_object) {
		collision_object->shape_owner_clear_shapes(owner_id);
		if (shape.is_valid()) {
			collision_object->shape_owner_add_shape(owner_id, shape);
		}
		_update_in_shape_owner();
	}

	if (shape.is_valid()) {
		shape->connect_changed(callable_mp(this, &CollisionShape2D::_shape_changed));
	}

	update_configuration_warnings();
}

Ref<Shape2D> CollisionShape2D::get_shape() const {
	return shape;
}

bool CollisionShape2D::_edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const {
	if (shape.is_null()) {
		return false;
	}

	return shape->_edit_is_selected_on_click(p_point, p_tolerance);
}

PackedStringArray CollisionShape2D::get_configuration_warnings() const {
	PackedStringArray warnings = Node2D::get_configuration_warnings();

	CollisionObject2D *col_object = _get_ancestor_collision_object();
	if (col_object == nullptr) {
		warnings.push_back(RTR("CollisionShape2D only serves to provide a collision shape to a CollisionObject2D derived node.\nPlease only use it as a descendant of Area2D, StaticBody2D, RigidBody2D, CharacterBody2D, etc. to give them a shape."));
	}
	if (shape.is_null()) {
		warnings.push_back(RTR("A shape must be provided for CollisionShape2D to function. Please create a shape resource for it!"));
	}
	if (one_way_collision && Object::cast_to<Area2D>(col_object)) {
		warnings.push_back(RTR("The One Way Collision property will be ignored when the collision object is an Area2D."));
	}

	Ref<ConvexPolygonShape2D> convex = shape;
	Ref<ConcavePolygonShape2D> concave = shape;
	if (convex.is_valid() || concave.is_valid()) {
		warnings.push_back(RTR("The CollisionShape2D node has limited editing options for polygon-based shapes. Consider using a CollisionPolygon2D node instead."));
	}

	return warnings;
}

void CollisionShape2D::set_disabled(bool p_disabled) {
	disabled = p_disabled;
	queue_redraw();
	if (collision_object) {
		collision_object->shape_owner_set_disabled(owner_id, p_disabled);
	}
}

bool CollisionShape2D::is_disabled() const {
	return disabled;
}

void CollisionShape2D::set_one_way_collision(bool p_enable) {
	one_way_collision = p_enable;
	queue_redraw();
	if (collision_object) {
		collision_object->shape_owner_set_one_way_collision(owner_id, p_enable);
	}
	update_configuration_warnings();
}

bool CollisionShape2D::is_one_way_collision_enabled() const {
	return one_way_collision;
}

void CollisionShape2D::set_one_way_collision_margin(real_t p_margin) {
	one_way_collision_margin = p_margin;
	if (collision_object) {
		collision_object->shape_owner_set_one_way_collision_margin(owner_id, one_way_collision_margin);
	}
}

real_t CollisionShape2D::get_one_way_collision_margin() const {
	return one_way_collision_margin;
}

Color CollisionShape2D::_get_default_debug_color() const {
	const SceneTree *st = SceneTree::get_singleton();
	return st ? st->get_debug_collisions_color() : Color(0.0, 0.0, 0.0, 0.0);
}

void CollisionShape2D::set_debug_color(const Color &p_color) {
	if (debug_color == p_color) {
		return;
	}

	debug_color = p_color;
	queue_redraw();
}

Color CollisionShape2D::get_debug_color() const {
	return debug_color;
}

#ifdef DEBUG_ENABLED

bool CollisionShape2D::_property_can_revert(const StringName &p_name) const {
	if (p_name == "debug_color") {
		return true;
	}
	return false;
}

bool CollisionShape2D::_property_get_revert(const StringName &p_name, Variant &r_property) const {
	if (p_name == "debug_color") {
		r_property = _get_default_debug_color();
		return true;
	}
	return false;
}

void CollisionShape2D::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "debug_color") {
		if (debug_color == _get_default_debug_color()) {
			p_property.usage = PROPERTY_USAGE_DEFAULT & ~PROPERTY_USAGE_STORAGE;
		} else {
			p_property.usage = PROPERTY_USAGE_DEFAULT;
		}
	}
}

#endif // DEBUG_ENABLED

void CollisionShape2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_shape", "shape"), &CollisionShape2D::set_shape);
	ClassDB::bind_method(D_METHOD("get_shape"), &CollisionShape2D::get_shape);
	ClassDB::bind_method(D_METHOD("set_disabled", "disabled"), &CollisionShape2D::set_disabled);
	ClassDB::bind_method(D_METHOD("is_disabled"), &CollisionShape2D::is_disabled);
	ClassDB::bind_method(D_METHOD("set_one_way_collision", "enabled"), &CollisionShape2D::set_one_way_collision);
	ClassDB::bind_method(D_METHOD("is_one_way_collision_enabled"), &CollisionShape2D::is_one_way_collision_enabled);
	ClassDB::bind_method(D_METHOD("set_one_way_collision_margin", "margin"), &CollisionShape2D::set_one_way_collision_margin);
	ClassDB::bind_method(D_METHOD("get_one_way_collision_margin"), &CollisionShape2D::get_one_way_collision_margin);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "shape", PROPERTY_HINT_RESOURCE_TYPE, "Shape2D"), "set_shape", "get_shape");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disabled"), "set_disabled", "is_disabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "one_way_collision"), "set_one_way_collision", "is_one_way_collision_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "one_way_collision_margin", PROPERTY_HINT_RANGE, "0,128,0.1,suffix:px"), "set_one_way_collision_margin", "get_one_way_collision_margin");

	ClassDB::bind_method(D_METHOD("set_debug_color", "color"), &CollisionShape2D::set_debug_color);
	ClassDB::bind_method(D_METHOD("get_debug_color"), &CollisionShape2D::get_debug_color);

	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "debug_color"), "set_debug_color", "get_debug_color");
	// Default value depends on a project setting, override for doc generation purposes.
	ADD_PROPERTY_DEFAULT("debug_color", Color(0.0, 0.0, 0.0, 0.0));
}

CollisionShape2D::CollisionShape2D() {
	set_hide_clip_children(true);
	debug_color = _get_default_debug_color();
}

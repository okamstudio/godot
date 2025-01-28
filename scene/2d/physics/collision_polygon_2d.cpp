/**************************************************************************/
/*  collision_polygon_2d.cpp                                              */
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

#include "collision_polygon_2d.h"

#include "core/math/geometry_2d.h"
#include "scene/2d/physics/area_2d.h"
#include "scene/2d/physics/collision_object_2d.h"
#include "scene/resources/2d/concave_polygon_shape_2d.h"
#include "scene/resources/2d/convex_polygon_shape_2d.h"

void CollisionPolygon2D::_build_polygon() {
	collision_object->shape_owner_clear_shapes(owner_id);

	bool solids = build_mode == BUILD_SOLIDS;

	if (solids) {
		if (polygon.size() < 3) {
			return;
		}

		//here comes the sun, lalalala
		//decompose concave into multiple convex polygons and add them
		Vector<Vector<Vector2>> decomp = _decompose_in_convex();
		for (int i = 0; i < decomp.size(); i++) {
			Ref<ConvexPolygonShape2D> convex = memnew(ConvexPolygonShape2D);
			convex->set_points(decomp[i]);
			collision_object->shape_owner_add_shape(owner_id, convex);
		}

	} else {
		if (polygon.size() < 2) {
			return;
		}

		Ref<ConcavePolygonShape2D> concave = memnew(ConcavePolygonShape2D);

		Vector<Vector2> segments;
		segments.resize(polygon.size() * 2);
		Vector2 *w = segments.ptrw();

		for (int i = 0; i < polygon.size(); i++) {
			w[(i << 1) + 0] = polygon[i];
			w[(i << 1) + 1] = polygon[(i + 1) % polygon.size()];
		}

		concave->set_segments(segments);

		collision_object->shape_owner_add_shape(owner_id, concave);
	}
}

Vector<Vector<Vector2>> CollisionPolygon2D::_decompose_in_convex() {
	Vector<Vector<Vector2>> decomp = Geometry2D::decompose_polygon_in_convex(polygon);
	return decomp;
}

void CollisionPolygon2D::_update_in_shape_owner(bool p_xform_only) {
	collision_object->shape_owner_set_transform(owner_id, get_transform());
	if (p_xform_only) {
		return;
	}
	collision_object->shape_owner_set_disabled(owner_id, disabled);
	collision_object->shape_owner_set_one_way_collision(owner_id, one_way_collision);
	collision_object->shape_owner_set_one_way_collision_margin(owner_id, one_way_collision_margin);
}

void CollisionPolygon2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PARENTED: {
			collision_object = Object::cast_to<CollisionObject2D>(get_parent());
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
		} break;

		case NOTIFICATION_UNPARENTED: {
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

			if (polygon.size() > 2) {
#ifdef TOOLS_ENABLED
				if (build_mode == BUILD_SOLIDS) {
					Vector<Vector<Vector2>> decomp = _decompose_in_convex();

					Color c(0.4, 0.9, 0.1);
					for (int i = 0; i < decomp.size(); i++) {
						c.set_hsv(Math::fmod(c.get_h() + 0.738, 1), c.get_s(), c.get_v(), 0.5);
						draw_colored_polygon(decomp[i], c);
					}
				}
#endif

				const Color stroke_color = get_tree()->get_debug_collisions_color();
				draw_polyline(polygon, stroke_color);
				// Draw the last segment.
				draw_line(polygon[polygon.size() - 1], polygon[0], stroke_color);
			}

			if (one_way_collision) {
				Color dcol = get_tree()->get_debug_collisions_color(); //0.9,0.2,0.2,0.4);
				dcol.a = 1.0;
				Vector2 line_to(0, 20);
				draw_line(Vector2(), line_to, dcol, 3);
				real_t tsize = 8;

				Vector<Vector2> pts = {
					line_to + Vector2(0, tsize),
					line_to + Vector2(Math_SQRT12 * tsize, 0),
					line_to + Vector2(-Math_SQRT12 * tsize, 0)
				};

				Vector<Color> cols{ dcol, dcol, dcol };

				draw_primitive(pts, cols, Vector<Vector2>()); //small arrow
			}
		} break;
	}
}

void CollisionPolygon2D::set_polygon(const Vector<Point2> &p_polygon) {
	polygon = p_polygon;

	{
		for (int i = 0; i < polygon.size(); i++) {
			if (i == 0) {
				aabb = Rect2(polygon[i], Size2());
			} else {
				aabb.expand_to(polygon[i]);
			}
		}
		if (aabb == Rect2()) {
			aabb = Rect2(-10, -10, 20, 20);
		} else {
			aabb.position -= aabb.size * 0.3;
			aabb.size += aabb.size * 0.6;
		}
	}

	if (collision_object) {
		_build_polygon();
		_update_in_shape_owner();
	}
	queue_redraw();
	update_configuration_warnings();
}

Vector<Point2> CollisionPolygon2D::get_polygon() const {
	return polygon;
}

void CollisionPolygon2D::set_build_mode(BuildMode p_mode) {
	ERR_FAIL_INDEX((int)p_mode, 2);
	build_mode = p_mode;
	if (collision_object) {
		_build_polygon();
		_update_in_shape_owner();
	}
	queue_redraw();
	update_configuration_warnings();
}

CollisionPolygon2D::BuildMode CollisionPolygon2D::get_build_mode() const {
	return build_mode;
}

#ifdef DEBUG_ENABLED
Rect2 CollisionPolygon2D::_edit_get_rect() const {
	return aabb;
}

bool CollisionPolygon2D::_edit_use_rect() const {
	return true;
}

bool CollisionPolygon2D::_edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const {
	return Geometry2D::is_point_in_polygon(p_point, Variant(polygon));
}
#endif

PackedStringArray CollisionPolygon2D::get_configuration_warnings() const {
	PackedStringArray warnings = Node2D::get_configuration_warnings();

	if (!Object::cast_to<CollisionObject2D>(get_parent())) {
		warnings.push_back(RTR("CollisionPolygon2D only serves to provide a collision shape to a CollisionObject2D derived node. Please only use it as a child of Area2D, StaticBody2D, RigidBody2D, CharacterBody2D, etc. to give them a shape."));
	}

	int polygon_count = polygon.size();
	if (polygon_count == 0) {
		warnings.push_back(RTR("An empty CollisionPolygon2D has no effect on collision."));
	} else {
		bool solids = build_mode == BUILD_SOLIDS;
		if (solids) {
			if (polygon_count < 3) {
				warnings.push_back(RTR("Invalid polygon. At least 3 points are needed in 'Solids' build mode."));
			}
		} else if (polygon_count < 2) {
			warnings.push_back(RTR("Invalid polygon. At least 2 points are needed in 'Segments' build mode."));
		}
	}
	if (one_way_collision && Object::cast_to<Area2D>(get_parent())) {
		warnings.push_back(RTR("The One Way Collision property will be ignored when the collision object is an Area2D."));
	}

	return warnings;
}

void CollisionPolygon2D::set_disabled(bool p_disabled) {
	disabled = p_disabled;
	queue_redraw();
	if (collision_object) {
		collision_object->shape_owner_set_disabled(owner_id, p_disabled);
	}
}

bool CollisionPolygon2D::is_disabled() const {
	return disabled;
}

void CollisionPolygon2D::set_one_way_collision(bool p_enable) {
	one_way_collision = p_enable;
	queue_redraw();
	if (collision_object) {
		collision_object->shape_owner_set_one_way_collision(owner_id, p_enable);
	}
	update_configuration_warnings();
}

bool CollisionPolygon2D::is_one_way_collision_enabled() const {
	return one_way_collision;
}

void CollisionPolygon2D::set_one_way_collision_margin(real_t p_margin) {
	one_way_collision_margin = p_margin;
	if (collision_object) {
		collision_object->shape_owner_set_one_way_collision_margin(owner_id, one_way_collision_margin);
	}
}

real_t CollisionPolygon2D::get_one_way_collision_margin() const {
	return one_way_collision_margin;
}

void CollisionPolygon2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_polygon", "polygon"), &CollisionPolygon2D::set_polygon);
	ClassDB::bind_method(D_METHOD("get_polygon"), &CollisionPolygon2D::get_polygon);

	ClassDB::bind_method(D_METHOD("set_build_mode", "build_mode"), &CollisionPolygon2D::set_build_mode);
	ClassDB::bind_method(D_METHOD("get_build_mode"), &CollisionPolygon2D::get_build_mode);
	ClassDB::bind_method(D_METHOD("set_disabled", "disabled"), &CollisionPolygon2D::set_disabled);
	ClassDB::bind_method(D_METHOD("is_disabled"), &CollisionPolygon2D::is_disabled);
	ClassDB::bind_method(D_METHOD("set_one_way_collision", "enabled"), &CollisionPolygon2D::set_one_way_collision);
	ClassDB::bind_method(D_METHOD("is_one_way_collision_enabled"), &CollisionPolygon2D::is_one_way_collision_enabled);
	ClassDB::bind_method(D_METHOD("set_one_way_collision_margin", "margin"), &CollisionPolygon2D::set_one_way_collision_margin);
	ClassDB::bind_method(D_METHOD("get_one_way_collision_margin"), &CollisionPolygon2D::get_one_way_collision_margin);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "build_mode", PROPERTY_HINT_ENUM, "Solids,Segments"), "set_build_mode", "get_build_mode");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "polygon"), "set_polygon", "get_polygon");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disabled"), "set_disabled", "is_disabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "one_way_collision"), "set_one_way_collision", "is_one_way_collision_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "one_way_collision_margin", PROPERTY_HINT_RANGE, "0,128,0.1,suffix:px"), "set_one_way_collision_margin", "get_one_way_collision_margin");

	BIND_ENUM_CONSTANT(BUILD_SOLIDS);
	BIND_ENUM_CONSTANT(BUILD_SEGMENTS);
}

CollisionPolygon2D::CollisionPolygon2D() {
	set_notify_local_transform(true);
	set_hide_clip_children(true);
}

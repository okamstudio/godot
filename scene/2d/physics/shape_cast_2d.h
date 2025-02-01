/**************************************************************************/
/*  shape_cast_2d.h                                                       */
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

#pragma once

#include "scene/2d/node_2d.h"
#include "scene/resources/2d/shape_2d.h"
#include "servers/physics_server_2d.h"

class CollisionObject2D;

class ShapeCast2D : public Node2D {
	GDCLASS(ShapeCast2D, Node2D);

	bool enabled = true;

	Ref<Shape2D> shape;
	RID shape_rid;
	Vector2 target_position = Vector2(0, 50);

	HashSet<RID> exclude;
	real_t margin = 0.0;
	uint32_t collision_mask = 1;
	bool exclude_parent_body = true;
	bool collide_with_areas = false;
	bool collide_with_bodies = true;

	// Result
	int max_results = 32;
	Vector<PhysicsDirectSpaceState2D::ShapeRestInfo> result;
	bool collided = false;
	real_t collision_safe_fraction = 1.0;
	real_t collision_unsafe_fraction = 1.0;

	void _shape_changed();

protected:
	void _notification(int p_what);
	void _update_shapecast_state();
	static void _bind_methods();

public:
	void set_collide_with_areas(bool p_clip);
	bool is_collide_with_areas_enabled() const;

	void set_collide_with_bodies(bool p_clip);
	bool is_collide_with_bodies_enabled() const;

	void set_enabled(bool p_enabled);
	bool is_enabled() const;

	void set_shape(const Ref<Shape2D> &p_shape);
	Ref<Shape2D> get_shape() const;

	void set_target_position(const Vector2 &p_point);
	Vector2 get_target_position() const;

	void set_margin(real_t p_margin);
	real_t get_margin() const;

	void set_max_results(int p_max_results);
	int get_max_results() const;

	void set_collision_mask(uint32_t p_mask);
	uint32_t get_collision_mask() const;

	void set_collision_mask_value(int p_layer_number, bool p_value);
	bool get_collision_mask_value(int p_layer_number) const;

	void set_exclude_parent_body(bool p_exclude_parent_body);
	bool get_exclude_parent_body() const;

	void force_shapecast_update();
	bool is_colliding() const;

	Array get_collision_result() const;
	int get_collision_count() const;
	Object *get_collider(int p_idx) const;
	RID get_collider_rid(int p_idx) const;
	int get_collider_shape(int p_idx) const;
	Vector2 get_collision_point(int p_idx) const;
	Vector2 get_collision_normal(int p_idx) const;

	real_t get_closest_collision_safe_fraction() const;
	real_t get_closest_collision_unsafe_fraction() const;

	void add_exception_rid(const RID &p_rid);
	void add_exception(const CollisionObject2D *p_node);
	void remove_exception_rid(const RID &p_rid);
	void remove_exception(const CollisionObject2D *p_node);
	void clear_exceptions();

	PackedStringArray get_configuration_warnings() const override;

	ShapeCast2D();
};

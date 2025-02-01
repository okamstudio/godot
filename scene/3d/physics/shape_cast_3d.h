/**************************************************************************/
/*  shape_cast_3d.h                                                       */
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

#include "scene/3d/node_3d.h"
#include "scene/resources/3d/shape_3d.h"

class CollisionObject3D;

class ShapeCast3D : public Node3D {
	GDCLASS(ShapeCast3D, Node3D);

	bool enabled = true;
#ifndef DISABLE_DEPRECATED
	void resource_changed(Ref<Resource> p_res);
#endif

	Ref<Shape3D> shape;
	RID shape_rid;
	Vector3 target_position = Vector3(0, -1, 0);

	HashSet<RID> exclude;
	real_t margin = 0.0;
	uint32_t collision_mask = 1;
	bool exclude_parent_body = true;
	bool collide_with_areas = false;
	bool collide_with_bodies = true;

	Ref<Material> debug_material;
	Color debug_shape_custom_color = Color(0.0, 0.0, 0.0);
	Vector<Vector3> debug_shape_vertices;
	Vector<Vector3> debug_line_vertices;

	void _create_debug_shape();
	void _update_debug_shape();
	void _update_debug_shape_material(bool p_check_collision = false);
	void _update_debug_shape_vertices();
	void _clear_debug_shape();

	// Result
	int max_results = 32;
	Vector<PhysicsDirectSpaceState3D::ShapeRestInfo> result;
	bool collided = false;
	real_t collision_safe_fraction = 1.0;
	real_t collision_unsafe_fraction = 1.0;

	RID debug_instance;
	Ref<ArrayMesh> debug_mesh;

protected:
	void _notification(int p_what);
	void _update_shapecast_state();
	void _shape_changed();
	static void _bind_methods();

public:
	void set_collide_with_areas(bool p_clip);
	bool is_collide_with_areas_enabled() const;

	void set_collide_with_bodies(bool p_clip);
	bool is_collide_with_bodies_enabled() const;

	void set_enabled(bool p_enabled);
	bool is_enabled() const;

	void set_shape(const Ref<Shape3D> &p_shape);
	Ref<Shape3D> get_shape() const;

	void set_target_position(const Vector3 &p_point);
	Vector3 get_target_position() const;

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

	const Color &get_debug_shape_custom_color() const;
	void set_debug_shape_custom_color(const Color &p_color);

	const Vector<Vector3> &get_debug_shape_vertices() const;
	const Vector<Vector3> &get_debug_line_vertices() const;

	Ref<StandardMaterial3D> get_debug_material();

	Array get_collision_result() const;
	int get_collision_count() const;
	Object *get_collider(int p_idx) const;
	RID get_collider_rid(int p_idx) const;
	int get_collider_shape(int p_idx) const;
	Vector3 get_collision_point(int p_idx) const;
	Vector3 get_collision_normal(int p_idx) const;

	real_t get_closest_collision_safe_fraction() const;
	real_t get_closest_collision_unsafe_fraction() const;

	void force_shapecast_update();
	bool is_colliding() const;

	void add_exception_rid(const RID &p_rid);
	void add_exception(const CollisionObject3D *p_node);
	void remove_exception_rid(const RID &p_rid);
	void remove_exception(const CollisionObject3D *p_node);
	void clear_exceptions();

	virtual PackedStringArray get_configuration_warnings() const override;
};

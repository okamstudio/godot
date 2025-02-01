/**************************************************************************/
/*  ray_cast_3d.h                                                         */
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

class CollisionObject3D;

class RayCast3D : public Node3D {
	GDCLASS(RayCast3D, Node3D);

	bool enabled = true;
	bool collided = false;
	ObjectID against;
	RID against_rid;
	int against_shape = 0;
	Vector3 collision_point;
	Vector3 collision_normal;
	int collision_face_index = -1;

	Vector3 target_position = Vector3(0, -1, 0);
	HashSet<RID> exclude;

	uint32_t collision_mask = 1;
	bool exclude_parent_body = true;

	Ref<Material> debug_material;
	Color debug_shape_custom_color = Color(0.0, 0.0, 0.0);
	int debug_shape_thickness = 2;
	Vector<Vector3> debug_shape_vertices;
	Vector<Vector3> debug_line_vertices;

	void _create_debug_shape();
	void _update_debug_shape();
	void _update_debug_shape_material(bool p_check_collision = false);
	void _update_debug_shape_vertices();
	void _clear_debug_shape();

	bool collide_with_areas = false;
	bool collide_with_bodies = true;

	bool hit_from_inside = false;
	bool hit_back_faces = true;

	RID debug_instance;
	Ref<ArrayMesh> debug_mesh;

protected:
	void _notification(int p_what);
	void _update_raycast_state();
	static void _bind_methods();

public:
	void set_collide_with_areas(bool p_enabled);
	bool is_collide_with_areas_enabled() const;

	void set_collide_with_bodies(bool p_enabled);
	bool is_collide_with_bodies_enabled() const;

	void set_hit_from_inside(bool p_enabled);
	bool is_hit_from_inside_enabled() const;

	void set_hit_back_faces(bool p_enabled);
	bool is_hit_back_faces_enabled() const;

	void set_enabled(bool p_enabled);
	bool is_enabled() const;

	void set_target_position(const Vector3 &p_point);
	Vector3 get_target_position() const;

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

	int get_debug_shape_thickness() const;
	void set_debug_shape_thickness(const int p_debug_thickness);

	void force_raycast_update();
	bool is_colliding() const;
	Object *get_collider() const;
	RID get_collider_rid() const;
	int get_collider_shape() const;
	Vector3 get_collision_point() const;
	Vector3 get_collision_normal() const;
	int get_collision_face_index() const;

	void add_exception_rid(const RID &p_rid);
	void add_exception(const CollisionObject3D *p_node);
	void remove_exception_rid(const RID &p_rid);
	void remove_exception(const CollisionObject3D *p_node);
	void clear_exceptions();

	RayCast3D();
};

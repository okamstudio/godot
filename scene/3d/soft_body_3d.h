/**************************************************************************/
/*  soft_body_3d.h                                                        */
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

#include "scene/3d/mesh_instance_3d.h"
#include "servers/physics_server_3d.h"

class PhysicsBody3D;

class SoftBodyRenderingServerHandler : public PhysicsServer3DRenderingServerHandler {
	friend class SoftBody3D;

	RID mesh;
	int surface = 0;
	Vector<uint8_t> buffer;
	uint32_t stride = 0;
	uint32_t normal_stride = 0;
	uint32_t offset_vertices = 0;
	uint32_t offset_normal = 0;

	uint8_t *write_buffer = nullptr;

private:
	SoftBodyRenderingServerHandler();
	bool is_ready(RID p_mesh_rid) const { return mesh.is_valid() && mesh == p_mesh_rid; }
	void prepare(RID p_mesh_rid, int p_surface);
	void clear();
	void open();
	void close();
	void commit_changes();

public:
	void set_vertex(int p_vertex_id, const Vector3 &p_vertex) override;
	void set_normal(int p_vertex_id, const Vector3 &p_normal) override;
	void set_aabb(const AABB &p_aabb) override;
};

class SoftBody3D : public MeshInstance3D {
	GDCLASS(SoftBody3D, MeshInstance3D);

public:
	enum DisableMode {
		DISABLE_MODE_REMOVE,
		DISABLE_MODE_KEEP_ACTIVE,
	};

	struct PinnedPoint {
		int point_index = -1;
		NodePath spatial_attachment_path;
		Node3D *spatial_attachment = nullptr; // Cache
		Vector3 offset;

		PinnedPoint();
		PinnedPoint(const PinnedPoint &obj_tocopy);
		void operator=(const PinnedPoint &obj);
	};

private:
	SoftBodyRenderingServerHandler *rendering_server_handler = nullptr;

	RID physics_rid;

	DisableMode disable_mode = DISABLE_MODE_REMOVE;

	RID owned_mesh;
	uint32_t collision_mask = 1;
	uint32_t collision_layer = 1;
	NodePath parent_collision_ignore;
	Vector<PinnedPoint> pinned_points;
	bool simulation_started = false;
	bool pinned_points_cache_dirty = true;

	Ref<ArrayMesh> debug_mesh_cache;
	class MeshInstance3D *debug_mesh = nullptr;

	bool capture_input_on_drag = false;
	bool ray_pickable = true;

	void _update_pickable();

	void _update_physics_server();
	void _draw_soft_mesh();

	void _prepare_physics_server();
	void _become_mesh_owner();

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	bool _set_property_pinned_points_indices(const Array &p_indices);
	bool _set_property_pinned_points_attachment(int p_item, const String &p_what, const Variant &p_value);
	bool _get_property_pinned_points(int p_item, const String &p_what, Variant &r_ret) const;

	void _notification(int p_what);
	static void _bind_methods();

#ifndef DISABLE_DEPRECATED
	void _pin_point_bind_compat_94684(int p_point_index, bool pin, const NodePath &p_spatial_attachment_path = NodePath());
	static void _bind_compatibility_methods();
#endif

	PackedStringArray get_configuration_warnings() const override;

public:
	RID get_physics_rid() const { return physics_rid; }

	void set_collision_mask(uint32_t p_mask);
	uint32_t get_collision_mask() const;

	void set_collision_layer(uint32_t p_layer);
	uint32_t get_collision_layer() const;

	void set_collision_layer_value(int p_layer_number, bool p_value);
	bool get_collision_layer_value(int p_layer_number) const;

	void set_collision_mask_value(int p_layer_number, bool p_value);
	bool get_collision_mask_value(int p_layer_number) const;

	void set_disable_mode(DisableMode p_mode);
	DisableMode get_disable_mode() const;

	void set_parent_collision_ignore(const NodePath &p_parent_collision_ignore);
	const NodePath &get_parent_collision_ignore() const;

	void set_pinned_points_indices(Vector<PinnedPoint> p_pinned_points_indices);
	Vector<PinnedPoint> get_pinned_points_indices();

	void set_simulation_precision(int p_simulation_precision);
	int get_simulation_precision();

	void set_total_mass(real_t p_total_mass);
	real_t get_total_mass();

	void set_linear_stiffness(real_t p_linear_stiffness);
	real_t get_linear_stiffness();

	void set_pressure_coefficient(real_t p_pressure_coefficient);
	real_t get_pressure_coefficient();

	void set_damping_coefficient(real_t p_damping_coefficient);
	real_t get_damping_coefficient();

	void set_drag_coefficient(real_t p_drag_coefficient);
	real_t get_drag_coefficient();

	TypedArray<PhysicsBody3D> get_collision_exceptions();
	void add_collision_exception_with(Node *p_node);
	void remove_collision_exception_with(Node *p_node);

	Vector3 get_point_transform(int p_point_index);

	void pin_point_toggle(int p_point_index);
	void pin_point(int p_point_index, bool pin, const NodePath &p_spatial_attachment_path = NodePath(), int p_insert_at = -1);
	bool is_point_pinned(int p_point_index) const;

	void _pin_point_deferred(int p_point_index, bool pin, const NodePath p_spatial_attachment_path);

	void set_ray_pickable(bool p_ray_pickable);
	bool is_ray_pickable() const;

	SoftBody3D();
	~SoftBody3D();

private:
	void _make_cache_dirty();
	void _update_cache_pin_points_datas();

	void _pin_point_on_physics_server(int p_point_index, bool pin);
	void _add_pinned_point(int p_point_index, const NodePath &p_spatial_attachment_path, int p_insert_at = -1);
	void _reset_points_offsets();

	void _remove_pinned_point(int p_point_index);
	int _get_pinned_point(int p_point_index, PinnedPoint *&r_point) const;
	int _has_pinned_point(int p_point_index) const;
};

VARIANT_ENUM_CAST(SoftBody3D::DisableMode);

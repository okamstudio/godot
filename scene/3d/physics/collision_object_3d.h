/**************************************************************************/
/*  collision_object_3d.h                                                 */
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

#include "scene/3d/camera_3d.h"
#include "scene/3d/node_3d.h"

class CollisionObject3D : public Node3D {
	GDCLASS(CollisionObject3D, Node3D);

public:
	enum DisableMode {
		DISABLE_MODE_REMOVE,
		DISABLE_MODE_MAKE_STATIC,
		DISABLE_MODE_KEEP_ACTIVE,
	};

private:
	uint32_t collision_layer = 1;
	uint32_t collision_mask = 1;
	real_t collision_priority = 1.0;

	bool area = false;

	RID rid;
	uint32_t callback_lock = 0;

	DisableMode disable_mode = DISABLE_MODE_REMOVE;

	PhysicsServer3D::BodyMode body_mode = PhysicsServer3D::BODY_MODE_STATIC;

	struct ShapeData {
		ObjectID owner_id;
		Transform3D xform;
		struct ShapeBase {
			RID debug_shape;
			Ref<Shape3D> shape;
			int index = 0;
		};

		Vector<ShapeBase> shapes;
		bool disabled = false;
	};

	int total_subshapes = 0;

	RBMap<uint32_t, ShapeData> shapes;

	bool only_update_transform_changes = false; // This is used for sync to physics.

	bool capture_input_on_drag = false;
	bool ray_pickable = true;

	HashSet<uint32_t> debug_shapes_to_update;
	int debug_shapes_count = 0;
	Transform3D debug_shape_old_transform;

	void _update_pickable();

	bool _are_collision_shapes_visible();
	void _update_shape_data(uint32_t p_owner);
	void _shape_changed(const Ref<Shape3D> &p_shape);
	void _update_debug_shapes();
	void _clear_debug_shapes();

	void _apply_disabled();
	void _apply_enabled();

protected:
	CollisionObject3D(RID p_rid, bool p_area);

	_FORCE_INLINE_ void lock_callback() { callback_lock++; }
	_FORCE_INLINE_ void unlock_callback() {
		ERR_FAIL_COND(callback_lock == 0);
		callback_lock--;
	}

	void _notification(int p_what);
	static void _bind_methods();

	void _on_transform_changed();

	friend class Viewport;
	virtual void _input_event_call(Camera3D *p_camera, const Ref<InputEvent> &p_input_event, const Vector3 &p_pos, const Vector3 &p_normal, int p_shape);
	virtual void _mouse_enter();
	virtual void _mouse_exit();

	void set_body_mode(PhysicsServer3D::BodyMode p_mode);

	virtual void _space_changed(const RID &p_new_space);

	void set_only_update_transform_changes(bool p_enable);
	bool is_only_update_transform_changes_enabled() const;

	GDVIRTUAL5(_input_event, Camera3D *, Ref<InputEvent>, Vector3, Vector3, int)
	GDVIRTUAL0(_mouse_enter)
	GDVIRTUAL0(_mouse_exit)
public:
	void set_collision_layer(uint32_t p_layer);
	uint32_t get_collision_layer() const;

	void set_collision_mask(uint32_t p_mask);
	uint32_t get_collision_mask() const;

	void set_collision_layer_value(int p_layer_number, bool p_value);
	bool get_collision_layer_value(int p_layer_number) const;

	void set_collision_mask_value(int p_layer_number, bool p_value);
	bool get_collision_mask_value(int p_layer_number) const;

	void set_collision_priority(real_t p_priority);
	real_t get_collision_priority() const;

	void set_disable_mode(DisableMode p_mode);
	DisableMode get_disable_mode() const;

	uint32_t create_shape_owner(Object *p_owner);
	void remove_shape_owner(uint32_t owner);
	void get_shape_owners(List<uint32_t> *r_owners);
	PackedInt32Array _get_shape_owners();

	void shape_owner_set_transform(uint32_t p_owner, const Transform3D &p_transform);
	Transform3D shape_owner_get_transform(uint32_t p_owner) const;
	Object *shape_owner_get_owner(uint32_t p_owner) const;

	void shape_owner_set_disabled(uint32_t p_owner, bool p_disabled);
	bool is_shape_owner_disabled(uint32_t p_owner) const;

	void shape_owner_add_shape(uint32_t p_owner, const Ref<Shape3D> &p_shape);
	int shape_owner_get_shape_count(uint32_t p_owner) const;
	Ref<Shape3D> shape_owner_get_shape(uint32_t p_owner, int p_shape) const;
	int shape_owner_get_shape_index(uint32_t p_owner, int p_shape) const;

	void shape_owner_remove_shape(uint32_t p_owner, int p_shape);
	void shape_owner_clear_shapes(uint32_t p_owner);

	uint32_t shape_find_owner(int p_shape_index) const;

	void set_ray_pickable(bool p_ray_pickable);
	bool is_ray_pickable() const;

	void set_capture_input_on_drag(bool p_capture);
	bool get_capture_input_on_drag() const;

	_FORCE_INLINE_ RID get_rid() const { return rid; }

	PackedStringArray get_configuration_warnings() const override;

	CollisionObject3D();
	~CollisionObject3D();
};

VARIANT_ENUM_CAST(CollisionObject3D::DisableMode);

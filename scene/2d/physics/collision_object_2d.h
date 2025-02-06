/**************************************************************************/
/*  collision_object_2d.h                                                 */
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
#include "scene/main/viewport.h"
#include "scene/resources/2d/shape_2d.h"
#include "servers/physics_server_2d.h"

class CollisionObject2D : public Node2D {
	GDCLASS(CollisionObject2D, Node2D);

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
	bool pickable = false;

	DisableMode disable_mode = DISABLE_MODE_REMOVE;

	PhysicsServer2D::BodyMode body_mode = PhysicsServer2D::BODY_MODE_STATIC;

	struct ShapeData {
		ObjectID owner_id;
		Transform2D xform;
		struct Shape {
			Ref<Shape2D> shape;
			int index = 0;
		};

		Vector<Shape> shapes;

		bool disabled = false;
		bool one_way_collision = false;
		real_t one_way_collision_margin = 0.0;
	};

	int total_subshapes = 0;

	RBMap<uint32_t, ShapeData> shapes;
	bool only_update_transform_changes = false; // This is used for sync to physics.

	void _apply_disabled();
	void _apply_enabled();

protected:
	_FORCE_INLINE_ void lock_callback() { callback_lock++; }
	_FORCE_INLINE_ void unlock_callback() {
		ERR_FAIL_COND(callback_lock == 0);
		callback_lock--;
	}

	CollisionObject2D(RID p_rid, bool p_area);

	void _notification(int p_what);
	static void _bind_methods();

	void _update_pickable();
	friend class Viewport;
	void _input_event_call(Viewport *p_viewport, const Ref<InputEvent> &p_input_event, int p_shape);
	void _mouse_enter();
	void _mouse_exit();

	void _mouse_shape_enter(int p_shape);
	void _mouse_shape_exit(int p_shape);

	void set_only_update_transform_changes(bool p_enable);
	bool is_only_update_transform_changes_enabled() const;

	void set_body_mode(PhysicsServer2D::BodyMode p_mode);

	virtual void _space_changed(const RID &p_new_space);

	GDVIRTUAL3(_input_event, Viewport *, Ref<InputEvent>, int)
	GDVIRTUAL0(_mouse_enter)
	GDVIRTUAL0(_mouse_exit)
	GDVIRTUAL1(_mouse_shape_enter, int)
	GDVIRTUAL1(_mouse_shape_exit, int)
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

	void shape_owner_set_transform(uint32_t p_owner, const Transform2D &p_transform);
	Transform2D shape_owner_get_transform(uint32_t p_owner) const;
	Object *shape_owner_get_owner(uint32_t p_owner) const;

	void shape_owner_set_disabled(uint32_t p_owner, bool p_disabled);
	bool is_shape_owner_disabled(uint32_t p_owner) const;

	void shape_owner_set_one_way_collision(uint32_t p_owner, bool p_enable);
	bool is_shape_owner_one_way_collision_enabled(uint32_t p_owner) const;

	void shape_owner_set_one_way_collision_margin(uint32_t p_owner, real_t p_margin);
	real_t get_shape_owner_one_way_collision_margin(uint32_t p_owner) const;

	void shape_owner_add_shape(uint32_t p_owner, const Ref<Shape2D> &p_shape);
	int shape_owner_get_shape_count(uint32_t p_owner) const;
	Ref<Shape2D> shape_owner_get_shape(uint32_t p_owner, int p_shape) const;
	int shape_owner_get_shape_index(uint32_t p_owner, int p_shape) const;

	void shape_owner_remove_shape(uint32_t p_owner, int p_shape);
	void shape_owner_clear_shapes(uint32_t p_owner);

	uint32_t shape_find_owner(int p_shape_index) const;

	void set_pickable(bool p_enabled);
	bool is_pickable() const;

	PackedStringArray get_configuration_warnings() const override;

	_FORCE_INLINE_ RID get_rid() const { return rid; }

	CollisionObject2D();
	~CollisionObject2D();
};

VARIANT_ENUM_CAST(CollisionObject2D::DisableMode);

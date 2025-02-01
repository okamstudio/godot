/**************************************************************************/
/*  godot_collision_object_2d.h                                           */
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

#include "godot_broad_phase_2d.h"
#include "godot_shape_2d.h"

#include "core/templates/self_list.h"
#include "servers/physics_server_2d.h"

class GodotSpace2D;

class GodotCollisionObject2D : public GodotShapeOwner2D {
public:
	enum Type {
		TYPE_AREA,
		TYPE_BODY
	};

private:
	Type type;
	RID self;
	ObjectID instance_id;
	ObjectID canvas_instance_id;
	bool pickable = true;

	struct Shape {
		Transform2D xform;
		Transform2D xform_inv;
		GodotBroadPhase2D::ID bpid = 0;
		Rect2 aabb_cache; //for rayqueries
		GodotShape2D *shape = nullptr;
		bool disabled = false;
		bool one_way_collision = false;
		real_t one_way_collision_margin = 0.0;
	};

	Vector<Shape> shapes;
	GodotSpace2D *space = nullptr;
	Transform2D transform;
	Transform2D inv_transform;
	uint32_t collision_mask = 1;
	uint32_t collision_layer = 1;
	real_t collision_priority = 1.0;
	bool _static = true;

	SelfList<GodotCollisionObject2D> pending_shape_update_list;

	void _update_shapes();

protected:
	void _update_shapes_with_motion(const Vector2 &p_motion);
	void _unregister_shapes();

	_FORCE_INLINE_ void _set_transform(const Transform2D &p_transform, bool p_update_shapes = true) {
		transform = p_transform;
		if (p_update_shapes) {
			_update_shapes();
		}
	}
	_FORCE_INLINE_ void _set_inv_transform(const Transform2D &p_transform) { inv_transform = p_transform; }
	void _set_static(bool p_static);

	virtual void _shapes_changed() = 0;
	void _set_space(GodotSpace2D *p_space);

	GodotCollisionObject2D(Type p_type);

public:
	_FORCE_INLINE_ void set_self(const RID &p_self) { self = p_self; }
	_FORCE_INLINE_ RID get_self() const { return self; }

	_FORCE_INLINE_ void set_instance_id(const ObjectID &p_instance_id) { instance_id = p_instance_id; }
	_FORCE_INLINE_ ObjectID get_instance_id() const { return instance_id; }

	_FORCE_INLINE_ void set_canvas_instance_id(const ObjectID &p_canvas_instance_id) { canvas_instance_id = p_canvas_instance_id; }
	_FORCE_INLINE_ ObjectID get_canvas_instance_id() const { return canvas_instance_id; }

	void _shape_changed() override;

	_FORCE_INLINE_ Type get_type() const { return type; }
	void add_shape(GodotShape2D *p_shape, const Transform2D &p_transform = Transform2D(), bool p_disabled = false);
	void set_shape(int p_index, GodotShape2D *p_shape);
	void set_shape_transform(int p_index, const Transform2D &p_transform);

	_FORCE_INLINE_ int get_shape_count() const { return shapes.size(); }
	_FORCE_INLINE_ GodotShape2D *get_shape(int p_index) const {
		CRASH_BAD_INDEX(p_index, shapes.size());
		return shapes[p_index].shape;
	}
	_FORCE_INLINE_ const Transform2D &get_shape_transform(int p_index) const {
		CRASH_BAD_INDEX(p_index, shapes.size());
		return shapes[p_index].xform;
	}
	_FORCE_INLINE_ const Transform2D &get_shape_inv_transform(int p_index) const {
		CRASH_BAD_INDEX(p_index, shapes.size());
		return shapes[p_index].xform_inv;
	}
	_FORCE_INLINE_ const Rect2 &get_shape_aabb(int p_index) const {
		CRASH_BAD_INDEX(p_index, shapes.size());
		return shapes[p_index].aabb_cache;
	}

	_FORCE_INLINE_ const Transform2D &get_transform() const { return transform; }
	_FORCE_INLINE_ const Transform2D &get_inv_transform() const { return inv_transform; }
	_FORCE_INLINE_ GodotSpace2D *get_space() const { return space; }

	void set_shape_disabled(int p_idx, bool p_disabled);
	_FORCE_INLINE_ bool is_shape_disabled(int p_idx) const {
		ERR_FAIL_INDEX_V(p_idx, shapes.size(), false);
		return shapes[p_idx].disabled;
	}

	_FORCE_INLINE_ void set_shape_as_one_way_collision(int p_idx, bool p_one_way_collision, real_t p_margin) {
		CRASH_BAD_INDEX(p_idx, shapes.size());
		shapes.write[p_idx].one_way_collision = p_one_way_collision;
		shapes.write[p_idx].one_way_collision_margin = p_margin;
	}
	_FORCE_INLINE_ bool is_shape_set_as_one_way_collision(int p_idx) const {
		CRASH_BAD_INDEX(p_idx, shapes.size());
		return shapes[p_idx].one_way_collision;
	}

	_FORCE_INLINE_ real_t get_shape_one_way_collision_margin(int p_idx) const {
		CRASH_BAD_INDEX(p_idx, shapes.size());
		return shapes[p_idx].one_way_collision_margin;
	}

	void set_collision_mask(uint32_t p_mask) {
		collision_mask = p_mask;
		_shape_changed();
	}
	_FORCE_INLINE_ uint32_t get_collision_mask() const { return collision_mask; }

	void set_collision_layer(uint32_t p_layer) {
		collision_layer = p_layer;
		_shape_changed();
	}
	_FORCE_INLINE_ uint32_t get_collision_layer() const { return collision_layer; }

	_FORCE_INLINE_ void set_collision_priority(real_t p_priority) {
		ERR_FAIL_COND_MSG(p_priority <= 0, "Priority must be greater than 0.");
		collision_priority = p_priority;
		_shape_changed();
	}
	_FORCE_INLINE_ real_t get_collision_priority() const { return collision_priority; }

	void remove_shape(GodotShape2D *p_shape) override;
	void remove_shape(int p_index);

	virtual void set_space(GodotSpace2D *p_space) = 0;

	_FORCE_INLINE_ bool is_static() const { return _static; }

	void set_pickable(bool p_pickable) { pickable = p_pickable; }
	_FORCE_INLINE_ bool is_pickable() const { return pickable; }

	_FORCE_INLINE_ bool collides_with(GodotCollisionObject2D *p_other) const {
		return p_other->collision_layer & collision_mask;
	}

	_FORCE_INLINE_ bool interacts_with(const GodotCollisionObject2D *p_other) const {
		return collision_layer & p_other->collision_mask || p_other->collision_layer & collision_mask;
	}

	virtual ~GodotCollisionObject2D() {}
};

/**************************************************************************/
/*  godot_collision_object_3d.h                                           */
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

#include "godot_broad_phase_3d.h"
#include "godot_shape_3d.h"

#include "core/templates/self_list.h"
#include "servers/physics_server_3d.h"

#ifdef DEBUG_ENABLED
#define MAX_OBJECT_DISTANCE 3.1622776601683791e+18

#define MAX_OBJECT_DISTANCE_X2 (MAX_OBJECT_DISTANCE * MAX_OBJECT_DISTANCE)
#endif

class GodotSpace3D;

class GodotCollisionObject3D : public GodotShapeOwner3D {
public:
	enum Type {
		TYPE_AREA,
		TYPE_BODY,
		TYPE_SOFT_BODY,
	};

private:
	Type type;
	RID self;
	ObjectID instance_id;
	uint32_t collision_layer = 1;
	uint32_t collision_mask = 1;
	real_t collision_priority = 1.0;

	struct Shape {
		Transform3D xform;
		Transform3D xform_inv;
		GodotBroadPhase3D::ID bpid;
		AABB aabb_cache; //for rayqueries
		real_t area_cache = 0.0;
		GodotShape3D *shape = nullptr;
		bool disabled = false;
	};

	Vector<Shape> shapes;
	GodotSpace3D *space = nullptr;
	Transform3D transform;
	Transform3D inv_transform;
	bool _static = true;

	SelfList<GodotCollisionObject3D> pending_shape_update_list;

	void _update_shapes();

protected:
	void _update_shapes_with_motion(const Vector3 &p_motion);
	void _unregister_shapes();

	_FORCE_INLINE_ void _set_transform(const Transform3D &p_transform, bool p_update_shapes = true) {
#ifdef DEBUG_ENABLED

		ERR_FAIL_COND_MSG(p_transform.origin.length_squared() > MAX_OBJECT_DISTANCE_X2, "Object went too far away (more than '" + itos(MAX_OBJECT_DISTANCE) + "' units from origin).");
#endif

		transform = p_transform;
		if (p_update_shapes) {
			_update_shapes();
		}
	}
	_FORCE_INLINE_ void _set_inv_transform(const Transform3D &p_transform) { inv_transform = p_transform; }
	void _set_static(bool p_static);

	virtual void _shapes_changed() = 0;
	void _set_space(GodotSpace3D *p_space);

	bool ray_pickable = true;

	GodotCollisionObject3D(Type p_type);

public:
	_FORCE_INLINE_ void set_self(const RID &p_self) { self = p_self; }
	_FORCE_INLINE_ RID get_self() const { return self; }

	_FORCE_INLINE_ void set_instance_id(const ObjectID &p_instance_id) { instance_id = p_instance_id; }
	_FORCE_INLINE_ ObjectID get_instance_id() const { return instance_id; }

	void _shape_changed() override;

	_FORCE_INLINE_ Type get_type() const { return type; }
	void add_shape(GodotShape3D *p_shape, const Transform3D &p_transform = Transform3D(), bool p_disabled = false);
	void set_shape(int p_index, GodotShape3D *p_shape);
	void set_shape_transform(int p_index, const Transform3D &p_transform);
	_FORCE_INLINE_ int get_shape_count() const { return shapes.size(); }
	_FORCE_INLINE_ GodotShape3D *get_shape(int p_index) const {
		CRASH_BAD_INDEX(p_index, shapes.size());
		return shapes[p_index].shape;
	}
	_FORCE_INLINE_ const Transform3D &get_shape_transform(int p_index) const {
		CRASH_BAD_INDEX(p_index, shapes.size());
		return shapes[p_index].xform;
	}
	_FORCE_INLINE_ const Transform3D &get_shape_inv_transform(int p_index) const {
		CRASH_BAD_INDEX(p_index, shapes.size());
		return shapes[p_index].xform_inv;
	}
	_FORCE_INLINE_ const AABB &get_shape_aabb(int p_index) const {
		CRASH_BAD_INDEX(p_index, shapes.size());
		return shapes[p_index].aabb_cache;
	}
	_FORCE_INLINE_ real_t get_shape_area(int p_index) const {
		CRASH_BAD_INDEX(p_index, shapes.size());
		return shapes[p_index].area_cache;
	}

	_FORCE_INLINE_ const Transform3D &get_transform() const { return transform; }
	_FORCE_INLINE_ const Transform3D &get_inv_transform() const { return inv_transform; }
	_FORCE_INLINE_ GodotSpace3D *get_space() const { return space; }

	_FORCE_INLINE_ void set_ray_pickable(bool p_enable) { ray_pickable = p_enable; }
	_FORCE_INLINE_ bool is_ray_pickable() const { return ray_pickable; }

	void set_shape_disabled(int p_idx, bool p_disabled);
	_FORCE_INLINE_ bool is_shape_disabled(int p_idx) const {
		ERR_FAIL_INDEX_V(p_idx, shapes.size(), false);
		return shapes[p_idx].disabled;
	}

	_FORCE_INLINE_ void set_collision_layer(uint32_t p_layer) {
		collision_layer = p_layer;
		_shape_changed();
	}
	_FORCE_INLINE_ uint32_t get_collision_layer() const { return collision_layer; }

	_FORCE_INLINE_ void set_collision_mask(uint32_t p_mask) {
		collision_mask = p_mask;
		_shape_changed();
	}
	_FORCE_INLINE_ uint32_t get_collision_mask() const { return collision_mask; }

	_FORCE_INLINE_ void set_collision_priority(real_t p_priority) {
		ERR_FAIL_COND_MSG(p_priority <= 0, "Priority must be greater than 0.");
		collision_priority = p_priority;
		_shape_changed();
	}
	_FORCE_INLINE_ real_t get_collision_priority() const { return collision_priority; }

	_FORCE_INLINE_ bool collides_with(GodotCollisionObject3D *p_other) const {
		return p_other->collision_layer & collision_mask;
	}

	_FORCE_INLINE_ bool interacts_with(const GodotCollisionObject3D *p_other) const {
		return collision_layer & p_other->collision_mask || p_other->collision_layer & collision_mask;
	}

	void remove_shape(GodotShape3D *p_shape) override;
	void remove_shape(int p_index);

	virtual void set_space(GodotSpace3D *p_space) = 0;

	_FORCE_INLINE_ bool is_static() const { return _static; }

	virtual ~GodotCollisionObject3D() {}
};

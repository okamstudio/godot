/**************************************************************************/
/*  jolt_object_3d.h                                                      */
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

#ifndef JOLT_OBJECT_3D_H
#define JOLT_OBJECT_3D_H

#include "../shapes/jolt_shape_instance_3d.h"

#include "core/math/vector3.h"
#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/templates/local_vector.h"
#include "core/templates/rid.h"

#include "Jolt/Jolt.h"

#include "Jolt/Physics/Body/Body.h"
#include "Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h"
#include "Jolt/Physics/Collision/ObjectLayer.h"

class JoltArea3D;
class JoltBody3D;
class JoltShapedObject3D;
class JoltShape3D;
class JoltSoftBody3D;
class JoltSpace3D;

class JoltObject3D {
public:
	enum ObjectType : char {
		OBJECT_TYPE_INVALID,
		OBJECT_TYPE_BODY,
		OBJECT_TYPE_SOFT_BODY,
		OBJECT_TYPE_AREA,
	};

protected:
	LocalVector<JoltShapeInstance3D> shapes;

	RID rid;
	ObjectID instance_id;
	JoltSpace3D *space = nullptr;
	JPH::BodyID jolt_id;

	uint32_t collision_layer = 1;
	uint32_t collision_mask = 1;

	ObjectType object_type = OBJECT_TYPE_INVALID;

	bool pickable = false;

	virtual JPH::BroadPhaseLayer _get_broad_phase_layer() const = 0;
	virtual JPH::ObjectLayer _get_object_layer() const = 0;

	virtual void _add_to_space() = 0;
	virtual void _remove_from_space();

	void _reset_space();

	void _update_object_layer();

	virtual void _collision_layer_changed();
	virtual void _collision_mask_changed();

	virtual void _space_changing() {}
	virtual void _space_changed() {}

public:
	explicit JoltObject3D(ObjectType p_object_type);
	virtual ~JoltObject3D() = 0;

	ObjectType get_type() const { return object_type; }

	bool is_body() const { return object_type == OBJECT_TYPE_BODY; }
	bool is_soft_body() const { return object_type == OBJECT_TYPE_SOFT_BODY; }
	bool is_area() const { return object_type == OBJECT_TYPE_AREA; }
	bool is_shaped() const { return object_type != OBJECT_TYPE_SOFT_BODY; }

	JoltShapedObject3D *as_shaped() { return is_shaped() ? reinterpret_cast<JoltShapedObject3D *>(this) : nullptr; }
	const JoltShapedObject3D *as_shaped() const { return is_shaped() ? reinterpret_cast<const JoltShapedObject3D *>(this) : nullptr; }

	JoltBody3D *as_body() { return is_body() ? reinterpret_cast<JoltBody3D *>(this) : nullptr; }
	const JoltBody3D *as_body() const { return is_body() ? reinterpret_cast<const JoltBody3D *>(this) : nullptr; }

	JoltSoftBody3D *as_soft_body() { return is_soft_body() ? reinterpret_cast<JoltSoftBody3D *>(this) : nullptr; }
	const JoltSoftBody3D *as_soft_body() const { return is_soft_body() ? reinterpret_cast<const JoltSoftBody3D *>(this) : nullptr; }

	JoltArea3D *as_area() { return is_area() ? reinterpret_cast<JoltArea3D *>(this) : nullptr; }
	const JoltArea3D *as_area() const { return is_area() ? reinterpret_cast<const JoltArea3D *>(this) : nullptr; }

	RID get_rid() const { return rid; }
	void set_rid(const RID &p_rid) { rid = p_rid; }

	ObjectID get_instance_id() const { return instance_id; }
	void set_instance_id(ObjectID p_id) { instance_id = p_id; }
	Object *get_instance() const;

	JPH::BodyID get_jolt_id() const { return jolt_id; }

	JoltSpace3D *get_space() const { return space; }
	void set_space(JoltSpace3D *p_space);
	bool in_space() const { return space != nullptr && !jolt_id.IsInvalid(); }

	uint32_t get_collision_layer() const { return collision_layer; }
	void set_collision_layer(uint32_t p_layer);

	uint32_t get_collision_mask() const { return collision_mask; }
	void set_collision_mask(uint32_t p_mask);

	virtual Vector3 get_velocity_at_position(const Vector3 &p_position) const = 0;

	bool is_pickable() const { return pickable; }
	void set_pickable(bool p_enabled) { pickable = p_enabled; }

	bool can_collide_with(const JoltObject3D &p_other) const;
	bool can_interact_with(const JoltObject3D &p_other) const;

	virtual bool can_interact_with(const JoltBody3D &p_other) const = 0;
	virtual bool can_interact_with(const JoltSoftBody3D &p_other) const = 0;
	virtual bool can_interact_with(const JoltArea3D &p_other) const = 0;

	virtual bool reports_contacts() const = 0;

	virtual void pre_step(float p_step, JPH::Body &p_jolt_body);
	virtual void post_step(float p_step, JPH::Body &p_jolt_body);

	String to_string() const;
};

#endif // JOLT_OBJECT_3D_H

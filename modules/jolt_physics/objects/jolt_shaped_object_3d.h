/**************************************************************************/
/*  jolt_shaped_object_3d.h                                               */
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

#ifndef JOLT_SHAPED_OBJECT_3D_H
#define JOLT_SHAPED_OBJECT_3D_H

#include "jolt_object_3d.h"

#include "Jolt/Jolt.h"

#include "Jolt/Physics/Body/Body.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"

class JoltShapedObject3D : public JoltObject3D {
	friend class JoltShape3D;

protected:
	Vector3 scale = Vector3(1, 1, 1);

	JPH::ShapeRefC jolt_shape;
	JPH::ShapeRefC previous_jolt_shape;

	JPH::BodyCreationSettings *jolt_settings = new JPH::BodyCreationSettings();

	virtual JPH::EMotionType _get_motion_type() const = 0;

	bool _is_big() const;

	JPH::ShapeRefC _try_build_shape();
	JPH::ShapeRefC _try_build_single_shape();
	JPH::ShapeRefC _try_build_compound_shape();

	virtual void _shapes_changed();
	virtual void _shapes_built() {}
	virtual void _space_changing() override;

	void _update_shape();

public:
	explicit JoltShapedObject3D(ObjectType p_object_type);
	virtual ~JoltShapedObject3D() override;

	Transform3D get_transform_unscaled() const;
	Transform3D get_transform_scaled() const;

	Vector3 get_scale() const { return scale; }
	Basis get_basis() const;
	Vector3 get_position() const;

	Vector3 get_center_of_mass() const;
	Vector3 get_center_of_mass_relative() const;
	Vector3 get_center_of_mass_local() const;

	Vector3 get_linear_velocity() const;
	Vector3 get_angular_velocity() const;

	AABB get_aabb() const;

	virtual bool has_custom_center_of_mass() const = 0;
	virtual Vector3 get_center_of_mass_custom() const = 0;

	JPH::ShapeRefC build_shape();

	const JPH::Shape *get_jolt_shape() const { return jolt_shape; }
	const JPH::Shape *get_previous_jolt_shape() const { return previous_jolt_shape; }

	void add_shape(JoltShape3D *p_shape, Transform3D p_transform, bool p_disabled);
	void remove_shape(const JoltShape3D *p_shape);
	void remove_shape(int p_index);

	JoltShape3D *get_shape(int p_index) const;
	void set_shape(int p_index, JoltShape3D *p_shape);

	void clear_shapes();

	int get_shape_count() const { return shapes.size(); }

	int find_shape_index(uint32_t p_shape_instance_id) const;
	int find_shape_index(const JPH::SubShapeID &p_sub_shape_id) const;

	JoltShape3D *find_shape(uint32_t p_shape_instance_id) const;
	JoltShape3D *find_shape(const JPH::SubShapeID &p_sub_shape_id) const;

	Transform3D get_shape_transform_unscaled(int p_index) const;
	Transform3D get_shape_transform_scaled(int p_index) const;
	void set_shape_transform(int p_index, Transform3D p_transform);

	Vector3 get_shape_scale(int p_index) const;

	bool is_shape_disabled(int p_index) const;
	void set_shape_disabled(int p_index, bool p_disabled);

	virtual void post_step(float p_step, JPH::Body &p_jolt_body) override;
};

#endif // JOLT_SHAPED_OBJECT_3D_H

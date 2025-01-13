/**************************************************************************/
/*  jolt_motion_filter_3d.h                                               */
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

#ifndef JOLT_MOTION_FILTER_3D_H
#define JOLT_MOTION_FILTER_3D_H

#include "core/object/object_id.h"
#include "core/templates/hash_set.h"
#include "core/templates/rid.h"

#include "Jolt/Jolt.h"

#include "Jolt/Physics/Body/Body.h"
#include "Jolt/Physics/Body/BodyFilter.h"
#include "Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h"
#include "Jolt/Physics/Collision/ObjectLayer.h"
#include "Jolt/Physics/Collision/ShapeFilter.h"

class JoltBody3D;
class JoltPhysicsServer3D;
class JoltSpace3D;

class JoltMotionFilter3D final
		: public JPH::BroadPhaseLayerFilter,
		  public JPH::ObjectLayerFilter,
		  public JPH::BodyFilter,
		  public JPH::ShapeFilter {
	const JoltBody3D &body_self;
	const JoltSpace3D &space;
	const HashSet<RID> &excluded_bodies;
	const HashSet<ObjectID> &excluded_objects;
	bool collide_separation_ray = false;

public:
	explicit JoltMotionFilter3D(const JoltBody3D &p_body, const HashSet<RID> &p_excluded_bodies, const HashSet<ObjectID> &p_excluded_objects, bool p_collide_separation_ray = true);

	virtual bool ShouldCollide(JPH::BroadPhaseLayer p_broad_phase_layer) const override;
	virtual bool ShouldCollide(JPH::ObjectLayer p_object_layer) const override;
	virtual bool ShouldCollide(const JPH::BodyID &p_jolt_id) const override;
	virtual bool ShouldCollideLocked(const JPH::Body &p_jolt_body) const override;
	virtual bool ShouldCollide(const JPH::Shape *p_jolt_shape, const JPH::SubShapeID &p_jolt_shape_id) const override;
	virtual bool ShouldCollide(const JPH::Shape *p_jolt_shape_self, const JPH::SubShapeID &p_jolt_shape_id_self, const JPH::Shape *p_jolt_shape_other, const JPH::SubShapeID &p_jolt_shape_id_other) const override;
};

#endif // JOLT_MOTION_FILTER_3D_H

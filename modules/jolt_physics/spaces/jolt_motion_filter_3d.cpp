/**************************************************************************/
/*  jolt_motion_filter_3d.cpp                                             */
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

#include "jolt_motion_filter_3d.h"

#include "../objects/jolt_body_3d.h"
#include "../objects/jolt_object_3d.h"
#include "../shapes/jolt_custom_motion_shape.h"
#include "../shapes/jolt_custom_ray_shape.h"
#include "../shapes/jolt_custom_shape_type.h"
#include "../shapes/jolt_shape_3d.h"
#include "jolt_broad_phase_layer.h"
#include "jolt_space_3d.h"

JoltMotionFilter3D::JoltMotionFilter3D(const JoltBody3D &p_body, const HashSet<RID> &p_excluded_bodies, const HashSet<ObjectID> &p_excluded_objects, bool p_collide_separation_ray) :
		body_self(p_body),
		space(*body_self.get_space()),
		excluded_bodies(p_excluded_bodies),
		excluded_objects(p_excluded_objects),
		collide_separation_ray(p_collide_separation_ray) {
}

bool JoltMotionFilter3D::ShouldCollide(JPH::BroadPhaseLayer p_broad_phase_layer) const {
	const JPH::BroadPhaseLayer::Type broad_phase_layer = (JPH::BroadPhaseLayer::Type)p_broad_phase_layer;

	switch (broad_phase_layer) {
		case (JPH::BroadPhaseLayer::Type)JoltBroadPhaseLayer::BODY_STATIC:
		case (JPH::BroadPhaseLayer::Type)JoltBroadPhaseLayer::BODY_STATIC_BIG:
		case (JPH::BroadPhaseLayer::Type)JoltBroadPhaseLayer::BODY_DYNAMIC: {
			return true;
		} break;
		case (JPH::BroadPhaseLayer::Type)JoltBroadPhaseLayer::AREA_DETECTABLE:
		case (JPH::BroadPhaseLayer::Type)JoltBroadPhaseLayer::AREA_UNDETECTABLE: {
			return false;
		} break;
		default: {
			ERR_FAIL_V_MSG(false, vformat("Unhandled broad phase layer: '%d'. This should not happen. Please report this.", broad_phase_layer));
		}
	}
}

bool JoltMotionFilter3D::ShouldCollide(JPH::ObjectLayer p_object_layer) const {
	JPH::BroadPhaseLayer object_broad_phase_layer = JoltBroadPhaseLayer::BODY_STATIC;
	uint32_t object_collision_layer = 0;
	uint32_t object_collision_mask = 0;

	space.map_from_object_layer(p_object_layer, object_broad_phase_layer, object_collision_layer, object_collision_mask);

	return (body_self.get_collision_mask() & object_collision_layer) != 0;
}

bool JoltMotionFilter3D::ShouldCollide(const JPH::BodyID &p_jolt_id) const {
	return p_jolt_id != body_self.get_jolt_id();
}

bool JoltMotionFilter3D::ShouldCollideLocked(const JPH::Body &p_jolt_body) const {
	if (p_jolt_body.IsSoftBody()) {
		return false;
	}

	const JoltObject3D *object = reinterpret_cast<const JoltObject3D *>(p_jolt_body.GetUserData());
	if (excluded_objects.has(object->get_instance_id()) || excluded_bodies.has(object->get_rid())) {
		return false;
	}

	const JoltReadableBody3D jolt_body_self = space.read_body(body_self);
	return jolt_body_self->GetCollisionGroup().CanCollide(p_jolt_body.GetCollisionGroup());
}

bool JoltMotionFilter3D::ShouldCollide(const JPH::Shape *p_jolt_shape, const JPH::SubShapeID &p_jolt_shape_id) const {
	return true;
}

bool JoltMotionFilter3D::ShouldCollide(const JPH::Shape *p_jolt_shape_self, const JPH::SubShapeID &p_jolt_shape_id_self, const JPH::Shape *p_jolt_shape_other, const JPH::SubShapeID &p_jolt_shape_id_other) const {
	if (collide_separation_ray) {
		return true;
	}

	const JoltCustomMotionShape *motion_shape = static_cast<const JoltCustomMotionShape *>(p_jolt_shape_self);
	const JPH::ConvexShape &actual_shape_self = motion_shape->get_inner_shape();
	if (actual_shape_self.GetSubType() == JoltCustomShapeSubType::RAY) {
		// When `slide_on_slope` is enabled the ray shape acts as a regular shape.
		return static_cast<const JoltCustomRayShape &>(actual_shape_self).slide_on_slope;
	}

	return true;
}

/**************************************************************************/
/*  jolt_physics_direct_space_state_3d.h                                  */
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

#ifndef JOLT_PHYSICS_DIRECT_SPACE_STATE_3D_H
#define JOLT_PHYSICS_DIRECT_SPACE_STATE_3D_H

#include "servers/physics_server_3d.h"

#include "Jolt/Jolt.h"

#include "Jolt/Physics/Body/Body.h"
#include "Jolt/Physics/Body/BodyFilter.h"
#include "Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h"
#include "Jolt/Physics/Collision/ContactListener.h"
#include "Jolt/Physics/Collision/ObjectLayer.h"
#include "Jolt/Physics/Collision/Shape/Shape.h"
#include "Jolt/Physics/Collision/ShapeFilter.h"

class JoltBody3D;
class JoltShape3D;
class JoltSpace3D;

class JoltPhysicsDirectSpaceState3D final : public PhysicsDirectSpaceState3D {
	GDCLASS(JoltPhysicsDirectSpaceState3D, PhysicsDirectSpaceState3D)

	JoltSpace3D *space = nullptr;

	static void _bind_methods() {}

	bool _cast_motion_impl(const JPH::Shape &p_jolt_shape, const Transform3D &p_transform_com, const Vector3 &p_scale, const Vector3 &p_motion, bool p_use_edge_removal, bool p_ignore_overlaps, const JPH::CollideShapeSettings &p_settings, const JPH::BroadPhaseLayerFilter &p_broad_phase_layer_filter, const JPH::ObjectLayerFilter &p_object_layer_filter, const JPH::BodyFilter &p_body_filter, const JPH::ShapeFilter &p_shape_filter, real_t &r_closest_safe, real_t &r_closest_unsafe) const;

	bool _body_motion_recover(const JoltBody3D &p_body, const Transform3D &p_transform, float p_margin, const HashSet<RID> &p_excluded_bodies, const HashSet<ObjectID> &p_excluded_objects, Vector3 &r_recovery) const;
	bool _body_motion_cast(const JoltBody3D &p_body, const Transform3D &p_transform, const Vector3 &p_scale, const Vector3 &p_motion, bool p_collide_separation_ray, const HashSet<RID> &p_excluded_bodies, const HashSet<ObjectID> &p_excluded_objects, real_t &r_safe_fraction, real_t &r_unsafe_fraction) const;
	bool _body_motion_collide(const JoltBody3D &p_body, const Transform3D &p_transform, const Vector3 &p_motion, float p_margin, int p_max_collisions, const HashSet<RID> &p_excluded_bodies, const HashSet<ObjectID> &p_excluded_objects, PhysicsServer3D::MotionResult *r_result) const;

	int _try_get_face_index(const JPH::Body &p_body, const JPH::SubShapeID &p_sub_shape_id);

	void _generate_manifold(const JPH::CollideShapeResult &p_hit, JPH::ContactPoints &r_contact_points1, JPH::ContactPoints &r_contact_points2 JPH_IF_DEBUG_RENDERER(, JPH::RVec3Arg p_center_of_mass)) const;

	void _collide_shape_queries(const JPH::Shape *p_shape, JPH::Vec3Arg p_scale, JPH::RMat44Arg p_transform_com, const JPH::CollideShapeSettings &p_settings, JPH::RVec3Arg p_base_offset, JPH::CollideShapeCollector &p_collector, const JPH::BroadPhaseLayerFilter &p_broad_phase_layer_filter = JPH::BroadPhaseLayerFilter(), const JPH::ObjectLayerFilter &p_object_layer_filter = JPH::ObjectLayerFilter(), const JPH::BodyFilter &p_body_filter = JPH::BodyFilter(), const JPH::ShapeFilter &p_shape_filter = JPH::ShapeFilter()) const;
	void _collide_shape_kinematics(const JPH::Shape *p_shape, JPH::Vec3Arg p_scale, JPH::RMat44Arg p_transform_com, const JPH::CollideShapeSettings &p_settings, JPH::RVec3Arg p_base_offset, JPH::CollideShapeCollector &p_collector, const JPH::BroadPhaseLayerFilter &p_broad_phase_layer_filter = JPH::BroadPhaseLayerFilter(), const JPH::ObjectLayerFilter &p_object_layer_filter = JPH::ObjectLayerFilter(), const JPH::BodyFilter &p_body_filter = JPH::BodyFilter(), const JPH::ShapeFilter &p_shape_filter = JPH::ShapeFilter()) const;

public:
	JoltPhysicsDirectSpaceState3D() = default;
	explicit JoltPhysicsDirectSpaceState3D(JoltSpace3D *p_space);

	virtual bool intersect_ray(const RayParameters &p_parameters, RayResult &r_result) override;
	virtual int intersect_point(const PointParameters &p_parameters, ShapeResult *r_results, int p_result_max) override;
	virtual int intersect_shape(const ShapeParameters &p_parameters, ShapeResult *r_results, int p_result_max) override;
	virtual bool cast_motion(const ShapeParameters &p_parameters, real_t &r_closest_safe, real_t &r_closest_unsafe, ShapeRestInfo *r_info = nullptr) override;
	virtual bool collide_shape(const ShapeParameters &p_parameters, Vector3 *r_results, int p_result_max, int &r_result_count) override;
	virtual bool rest_info(const ShapeParameters &p_parameters, ShapeRestInfo *r_info) override;
	virtual Vector3 get_closest_point_to_object_volume(RID p_object, Vector3 p_point) const override;

	bool body_test_motion(const JoltBody3D &p_body, const PhysicsServer3D::MotionParameters &p_parameters, PhysicsServer3D::MotionResult *r_result) const;

	JoltSpace3D &get_space() const { return *space; }
};

#endif // JOLT_PHYSICS_DIRECT_SPACE_STATE_3D_H

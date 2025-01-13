/**************************************************************************/
/*  jolt_physics_direct_space_state_3d.cpp                                */
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

#include "jolt_physics_direct_space_state_3d.h"

#include "../jolt_physics_server_3d.h"
#include "../jolt_project_settings.h"
#include "../misc/jolt_type_conversions.h"
#include "../objects/jolt_area_3d.h"
#include "../objects/jolt_body_3d.h"
#include "../objects/jolt_object_3d.h"
#include "../shapes/jolt_custom_motion_shape.h"
#include "../shapes/jolt_shape_3d.h"
#include "jolt_motion_filter_3d.h"
#include "jolt_query_collectors.h"
#include "jolt_query_filter_3d.h"
#include "jolt_space_3d.h"

#include "Jolt/Geometry/GJKClosestPoint.h"
#include "Jolt/Physics/Body/Body.h"
#include "Jolt/Physics/Body/BodyFilter.h"
#include "Jolt/Physics/Collision/BroadPhase/BroadPhaseQuery.h"
#include "Jolt/Physics/Collision/CastResult.h"
#include "Jolt/Physics/Collision/CollidePointResult.h"
#include "Jolt/Physics/Collision/NarrowPhaseQuery.h"
#include "Jolt/Physics/Collision/RayCast.h"
#include "Jolt/Physics/Collision/Shape/MeshShape.h"
#include "Jolt/Physics/PhysicsSystem.h"

bool JoltPhysicsDirectSpaceState3D::_cast_motion_impl(const JPH::Shape &p_jolt_shape, const Transform3D &p_transform_com, const Vector3 &p_scale, const Vector3 &p_motion, bool p_use_edge_removal, bool p_ignore_overlaps, const JPH::CollideShapeSettings &p_settings, const JPH::BroadPhaseLayerFilter &p_broad_phase_layer_filter, const JPH::ObjectLayerFilter &p_object_layer_filter, const JPH::BodyFilter &p_body_filter, const JPH::ShapeFilter &p_shape_filter, real_t &r_closest_safe, real_t &r_closest_unsafe) const {
	r_closest_safe = 1.0f;
	r_closest_unsafe = 1.0f;

	ERR_FAIL_COND_V_MSG(p_jolt_shape.GetType() != JPH::EShapeType::Convex, false, "Shape-casting with non-convex shapes is not supported.");

	const float motion_length = (float)p_motion.length();

	if (p_ignore_overlaps && motion_length == 0.0f) {
		return false;
	}

	const JPH::RMat44 transform_com = to_jolt_r(p_transform_com);
	const JPH::Vec3 scale = to_jolt(p_scale);
	const JPH::Vec3 motion = to_jolt(p_motion);
	const JPH::Vec3 motion_local = transform_com.Multiply3x3Transposed(motion);

	JPH::AABox aabb = p_jolt_shape.GetWorldSpaceBounds(transform_com, scale);
	JPH::AABox aabb_translated = aabb;
	aabb_translated.Translate(motion);
	aabb.Encapsulate(aabb_translated);

	JoltQueryCollectorAnyMulti<JPH::CollideShapeBodyCollector, 2048> aabb_collector;
	space->get_broad_phase_query().CollideAABox(aabb, aabb_collector, p_broad_phase_layer_filter, p_object_layer_filter);

	if (!aabb_collector.had_hit()) {
		return false;
	}

	const JPH::RVec3 base_offset = transform_com.GetTranslation();

	JoltCustomMotionShape motion_shape(static_cast<const JPH::ConvexShape &>(p_jolt_shape));

	auto collides = [&](const JPH::Body &p_other_body, float p_fraction) {
		motion_shape.set_motion(motion_local * p_fraction);

		const JPH::TransformedShape other_shape = p_other_body.GetTransformedShape();

		JoltQueryCollectorAny<JPH::CollideShapeCollector> collector;

		if (p_use_edge_removal) {
			JPH::CollideShapeSettings eier_settings = p_settings;
			eier_settings.mActiveEdgeMode = JPH::EActiveEdgeMode::CollideWithAll;
			eier_settings.mCollectFacesMode = JPH::ECollectFacesMode::CollectFaces;

			JPH::InternalEdgeRemovingCollector eier_collector(collector);
			other_shape.CollideShape(&motion_shape, scale, transform_com, eier_settings, base_offset, eier_collector, p_shape_filter);
			eier_collector.Flush();
		} else {
			other_shape.CollideShape(&motion_shape, scale, transform_com, p_settings, base_offset, collector, p_shape_filter);
		}

		return collector.had_hit();
	};

	// Figure out the number of steps we need in our binary search in order to achieve millimeter precision, within reason.
	const int step_count = CLAMP(int(logf(1000.0f * motion_length) / (float)Math_LN2), 4, 16);

	bool collided = false;

	for (int i = 0; i < aabb_collector.get_hit_count(); ++i) {
		const JPH::BodyID other_jolt_id = aabb_collector.get_hit(i);
		if (!p_body_filter.ShouldCollide(other_jolt_id)) {
			continue;
		}

		const JoltReadableBody3D other_jolt_body = space->read_body(other_jolt_id);
		if (!p_body_filter.ShouldCollideLocked(*other_jolt_body)) {
			continue;
		}

		if (!collides(*other_jolt_body, 1.0f)) {
			continue;
		}

		if (p_ignore_overlaps && collides(*other_jolt_body, 0.0f)) {
			continue;
		}

		float lo = 0.0f;
		float hi = 1.0f;
		float coeff = 0.5f;

		for (int j = 0; j < step_count; ++j) {
			const float fraction = lo + (hi - lo) * coeff;

			if (collides(*other_jolt_body, fraction)) {
				collided = true;

				hi = fraction;

				if (j == 0 || lo > 0.0f) {
					coeff = 0.5f;
				} else {
					coeff = 0.25f;
				}
			} else {
				lo = fraction;

				if (j == 0 || hi < 1.0f) {
					coeff = 0.5f;
				} else {
					coeff = 0.75f;
				}
			}
		}

		if (lo < r_closest_safe) {
			r_closest_safe = lo;
			r_closest_unsafe = hi;
		}
	}

	return collided;
}

bool JoltPhysicsDirectSpaceState3D::_body_motion_recover(const JoltBody3D &p_body, const Transform3D &p_transform, float p_margin, const HashSet<RID> &p_excluded_bodies, const HashSet<ObjectID> &p_excluded_objects, Vector3 &r_recovery) const {
	const int recovery_iterations = JoltProjectSettings::get_motion_query_recovery_iterations();
	const float recovery_amount = JoltProjectSettings::get_motion_query_recovery_amount();

	const JPH::Shape *jolt_shape = p_body.get_jolt_shape();

	const Vector3 com_scaled = to_godot(jolt_shape->GetCenterOfMass());
	Transform3D transform_com = p_transform.translated_local(com_scaled);

	JPH::CollideShapeSettings settings;
	settings.mMaxSeparationDistance = p_margin;

	const Vector3 &base_offset = transform_com.origin;

	const JoltMotionFilter3D motion_filter(p_body, p_excluded_bodies, p_excluded_objects);
	JoltQueryCollectorAnyMulti<JPH::CollideShapeCollector, 32> collector;

	bool recovered = false;

	for (int i = 0; i < recovery_iterations; ++i) {
		collector.reset();

		_collide_shape_kinematics(jolt_shape, JPH::Vec3::sReplicate(1.0f), to_jolt_r(transform_com), settings, to_jolt_r(base_offset), collector, motion_filter, motion_filter, motion_filter, motion_filter);

		if (!collector.had_hit()) {
			break;
		}

		const int hit_count = collector.get_hit_count();

		float combined_priority = 0.0;

		for (int j = 0; j < hit_count; j++) {
			const JPH::CollideShapeResult &hit = collector.get_hit(j);

			const JoltReadableBody3D other_jolt_body = space->read_body(hit.mBodyID2);
			const JoltBody3D *other_body = other_jolt_body.as_body();
			ERR_CONTINUE(other_body == nullptr);

			combined_priority += other_body->get_collision_priority();
		}

		const float average_priority = MAX(combined_priority / (float)hit_count, (float)CMP_EPSILON);

		recovered = true;

		Vector3 recovery;

		for (int j = 0; j < hit_count; ++j) {
			const JPH::CollideShapeResult &hit = collector.get_hit(j);

			const Vector3 penetration_axis = to_godot(hit.mPenetrationAxis.Normalized());
			const Vector3 margin_offset = penetration_axis * p_margin;

			const Vector3 point_on_1 = base_offset + to_godot(hit.mContactPointOn1) + margin_offset;
			const Vector3 point_on_2 = base_offset + to_godot(hit.mContactPointOn2);

			const real_t distance_to_1 = penetration_axis.dot(point_on_1 + recovery);
			const real_t distance_to_2 = penetration_axis.dot(point_on_2);

			const float penetration_depth = float(distance_to_1 - distance_to_2);

			if (penetration_depth <= 0.0f) {
				continue;
			}

			const JoltReadableBody3D other_jolt_body = space->read_body(hit.mBodyID2);
			const JoltBody3D *other_body = other_jolt_body.as_body();
			ERR_CONTINUE(other_body == nullptr);

			const float recovery_distance = penetration_depth * recovery_amount;
			const float other_priority = other_body->get_collision_priority();
			const float other_priority_normalized = other_priority / average_priority;
			const float scaled_recovery_distance = recovery_distance * other_priority_normalized;

			recovery -= penetration_axis * scaled_recovery_distance;
		}

		if (recovery == Vector3()) {
			break;
		}

		r_recovery += recovery;
		transform_com.origin += recovery;
	}

	return recovered;
}

bool JoltPhysicsDirectSpaceState3D::_body_motion_cast(const JoltBody3D &p_body, const Transform3D &p_transform, const Vector3 &p_scale, const Vector3 &p_motion, bool p_collide_separation_ray, const HashSet<RID> &p_excluded_bodies, const HashSet<ObjectID> &p_excluded_objects, real_t &r_safe_fraction, real_t &r_unsafe_fraction) const {
	const Transform3D body_transform = p_transform.scaled_local(p_scale);

	const JPH::CollideShapeSettings settings;
	const JoltMotionFilter3D motion_filter(p_body, p_excluded_bodies, p_excluded_objects, p_collide_separation_ray);

	bool collided = false;

	for (int i = 0; i < p_body.get_shape_count(); ++i) {
		if (p_body.is_shape_disabled(i)) {
			continue;
		}

		JoltShape3D *shape = p_body.get_shape(i);

		if (!shape->is_convex()) {
			continue;
		}

		const JPH::ShapeRefC jolt_shape = shape->try_build();
		if (unlikely(jolt_shape == nullptr)) {
			return false;
		}

		const Vector3 com_scaled = to_godot(jolt_shape->GetCenterOfMass());
		const Transform3D transform_local = p_body.get_shape_transform_scaled(i);
		const Transform3D transform_com_local = transform_local.translated_local(com_scaled);
		Transform3D transform_com = body_transform * transform_com_local;

		Vector3 scale = transform_com.basis.get_scale();
		JOLT_ENSURE_SCALE_VALID(jolt_shape, scale, "body_test_motion was passed an invalid transform along with body '%s'. This results in invalid scaling for shape at index %d.");

		transform_com.basis.orthonormalize();

		real_t shape_safe_fraction = 1.0;
		real_t shape_unsafe_fraction = 1.0;

		collided |= _cast_motion_impl(*jolt_shape, transform_com, scale, p_motion, JoltProjectSettings::use_enhanced_internal_edge_removal_for_motion_queries(), false, settings, motion_filter, motion_filter, motion_filter, motion_filter, shape_safe_fraction, shape_unsafe_fraction);

		r_safe_fraction = MIN(r_safe_fraction, shape_safe_fraction);
		r_unsafe_fraction = MIN(r_unsafe_fraction, shape_unsafe_fraction);
	}

	return collided;
}

bool JoltPhysicsDirectSpaceState3D::_body_motion_collide(const JoltBody3D &p_body, const Transform3D &p_transform, const Vector3 &p_motion, float p_margin, int p_max_collisions, const HashSet<RID> &p_excluded_bodies, const HashSet<ObjectID> &p_excluded_objects, PhysicsServer3D::MotionResult *p_result) const {
	if (p_max_collisions == 0) {
		return false;
	}

	const JPH::Shape *jolt_shape = p_body.get_jolt_shape();

	const Vector3 com_scaled = to_godot(jolt_shape->GetCenterOfMass());
	const Transform3D transform_com = p_transform.translated_local(com_scaled);

	JPH::CollideShapeSettings settings;
	settings.mCollectFacesMode = JPH::ECollectFacesMode::CollectFaces;
	settings.mMaxSeparationDistance = p_margin;

	const Vector3 &base_offset = transform_com.origin;

	const JoltMotionFilter3D motion_filter(p_body, p_excluded_bodies, p_excluded_objects);
	JoltQueryCollectorClosestMulti<JPH::CollideShapeCollector, 32> collector(p_max_collisions);
	_collide_shape_kinematics(jolt_shape, JPH::Vec3::sReplicate(1.0f), to_jolt_r(transform_com), settings, to_jolt_r(base_offset), collector, motion_filter, motion_filter, motion_filter, motion_filter);

	if (!collector.had_hit() || p_result == nullptr) {
		return collector.had_hit();
	}

	int count = 0;

	for (int i = 0; i < collector.get_hit_count(); ++i) {
		const JPH::CollideShapeResult &hit = collector.get_hit(i);

		const float penetration_depth = hit.mPenetrationDepth + p_margin;

		if (penetration_depth <= 0.0f) {
			continue;
		}

		const Vector3 normal = to_godot(-hit.mPenetrationAxis.Normalized());

		if (p_motion.length_squared() > 0) {
			const Vector3 direction = p_motion.normalized();

			if (direction.dot(normal) >= -CMP_EPSILON) {
				continue;
			}
		}

		JPH::ContactPoints contact_points1;
		JPH::ContactPoints contact_points2;

		if (p_max_collisions > 1) {
			_generate_manifold(hit, contact_points1, contact_points2 JPH_IF_DEBUG_RENDERER(, to_jolt_r(base_offset)));
		} else {
			contact_points2.push_back(hit.mContactPointOn2);
		}

		const JoltReadableBody3D collider_jolt_body = space->read_body(hit.mBodyID2);
		const JoltShapedObject3D *collider = collider_jolt_body.as_shaped();
		ERR_FAIL_NULL_V(collider, false);

		const int local_shape = p_body.find_shape_index(hit.mSubShapeID1);
		ERR_FAIL_COND_V(local_shape == -1, false);

		const int collider_shape = collider->find_shape_index(hit.mSubShapeID2);
		ERR_FAIL_COND_V(collider_shape == -1, false);

		for (JPH::Vec3 contact_point : contact_points2) {
			const Vector3 position = base_offset + to_godot(contact_point);

			PhysicsServer3D::MotionCollision &collision = p_result->collisions[count++];

			collision.position = position;
			collision.normal = normal;
			collision.collider_velocity = collider->get_velocity_at_position(position);
			collision.collider_angular_velocity = collider->get_angular_velocity();
			collision.depth = penetration_depth;
			collision.local_shape = local_shape;
			collision.collider_id = collider->get_instance_id();
			collision.collider = collider->get_rid();
			collision.collider_shape = collider_shape;

			if (count == p_max_collisions) {
				break;
			}
		}

		if (count == p_max_collisions) {
			break;
		}
	}

	p_result->collision_count = count;

	return count > 0;
}

int JoltPhysicsDirectSpaceState3D::_try_get_face_index(const JPH::Body &p_body, const JPH::SubShapeID &p_sub_shape_id) {
	if (!JoltProjectSettings::enable_ray_cast_face_index()) {
		return -1;
	}

	const JPH::Shape *root_shape = p_body.GetShape();
	JPH::SubShapeID sub_shape_id_remainder;
	const JPH::Shape *leaf_shape = root_shape->GetLeafShape(p_sub_shape_id, sub_shape_id_remainder);

	if (leaf_shape->GetType() != JPH::EShapeType::Mesh) {
		return -1;
	}

	const JPH::MeshShape *mesh_shape = static_cast<const JPH::MeshShape *>(leaf_shape);
	return (int)mesh_shape->GetTriangleUserData(sub_shape_id_remainder);
}

void JoltPhysicsDirectSpaceState3D::_generate_manifold(const JPH::CollideShapeResult &p_hit, JPH::ContactPoints &r_contact_points1, JPH::ContactPoints &r_contact_points2 JPH_IF_DEBUG_RENDERER(, JPH::RVec3Arg p_center_of_mass)) const {
	const JPH::PhysicsSystem &physics_system = space->get_physics_system();
	const JPH::PhysicsSettings &physics_settings = physics_system.GetPhysicsSettings();
	const JPH::Vec3 penetration_axis = p_hit.mPenetrationAxis.Normalized();

	JPH::ManifoldBetweenTwoFaces(p_hit.mContactPointOn1, p_hit.mContactPointOn2, penetration_axis, physics_settings.mManifoldToleranceSq, p_hit.mShape1Face, p_hit.mShape2Face, r_contact_points1, r_contact_points2 JPH_IF_DEBUG_RENDERER(, p_center_of_mass));

	if (r_contact_points1.size() > 4) {
		JPH::PruneContactPoints(penetration_axis, r_contact_points1, r_contact_points2 JPH_IF_DEBUG_RENDERER(, p_center_of_mass));
	}
}

void JoltPhysicsDirectSpaceState3D::_collide_shape_queries(
		const JPH::Shape *p_shape,
		JPH::Vec3Arg p_scale,
		JPH::RMat44Arg p_transform_com,
		const JPH::CollideShapeSettings &p_settings,
		JPH::RVec3Arg p_base_offset,
		JPH::CollideShapeCollector &p_collector,
		const JPH::BroadPhaseLayerFilter &p_broad_phase_layer_filter,
		const JPH::ObjectLayerFilter &p_object_layer_filter,
		const JPH::BodyFilter &p_body_filter,
		const JPH::ShapeFilter &p_shape_filter) const {
	if (JoltProjectSettings::use_enhanced_internal_edge_removal_for_queries()) {
		space->get_narrow_phase_query().CollideShapeWithInternalEdgeRemoval(p_shape, p_scale, p_transform_com, p_settings, p_base_offset, p_collector, p_broad_phase_layer_filter, p_object_layer_filter, p_body_filter, p_shape_filter);
	} else {
		space->get_narrow_phase_query().CollideShape(p_shape, p_scale, p_transform_com, p_settings, p_base_offset, p_collector, p_broad_phase_layer_filter, p_object_layer_filter, p_body_filter, p_shape_filter);
	}
}

void JoltPhysicsDirectSpaceState3D::_collide_shape_kinematics(
		const JPH::Shape *p_shape,
		JPH::Vec3Arg p_scale,
		JPH::RMat44Arg p_transform_com,
		const JPH::CollideShapeSettings &p_settings,
		JPH::RVec3Arg p_base_offset,
		JPH::CollideShapeCollector &p_collector,
		const JPH::BroadPhaseLayerFilter &p_broad_phase_layer_filter,
		const JPH::ObjectLayerFilter &p_object_layer_filter,
		const JPH::BodyFilter &p_body_filter,
		const JPH::ShapeFilter &p_shape_filter) const {
	if (JoltProjectSettings::use_enhanced_internal_edge_removal_for_motion_queries()) {
		space->get_narrow_phase_query().CollideShapeWithInternalEdgeRemoval(p_shape, p_scale, p_transform_com, p_settings, p_base_offset, p_collector, p_broad_phase_layer_filter, p_object_layer_filter, p_body_filter, p_shape_filter);
	} else {
		space->get_narrow_phase_query().CollideShape(p_shape, p_scale, p_transform_com, p_settings, p_base_offset, p_collector, p_broad_phase_layer_filter, p_object_layer_filter, p_body_filter, p_shape_filter);
	}
}

JoltPhysicsDirectSpaceState3D::JoltPhysicsDirectSpaceState3D(JoltSpace3D *p_space) :
		space(p_space) {
}

bool JoltPhysicsDirectSpaceState3D::intersect_ray(const RayParameters &p_parameters, RayResult &r_result) {
	ERR_FAIL_COND_V_MSG(space->is_stepping(), false, "intersect_ray must not be called while the physics space is being stepped.");

	space->try_optimize();

	const JoltQueryFilter3D query_filter(*this, p_parameters.collision_mask, p_parameters.collide_with_bodies, p_parameters.collide_with_areas, p_parameters.exclude, p_parameters.pick_ray);

	const JPH::RVec3 from = to_jolt_r(p_parameters.from);
	const JPH::RVec3 to = to_jolt_r(p_parameters.to);
	const JPH::Vec3 vector = JPH::Vec3(to - from);
	const JPH::RRayCast ray(from, vector);

	const JPH::EBackFaceMode back_face_mode = p_parameters.hit_back_faces ? JPH::EBackFaceMode::CollideWithBackFaces : JPH::EBackFaceMode::IgnoreBackFaces;

	JPH::RayCastSettings settings;
	settings.mTreatConvexAsSolid = p_parameters.hit_from_inside;
	settings.mBackFaceModeTriangles = back_face_mode;

	JoltQueryCollectorClosest<JPH::CastRayCollector> collector;
	space->get_narrow_phase_query().CastRay(ray, settings, collector, query_filter, query_filter, query_filter);

	if (!collector.had_hit()) {
		return false;
	}

	const JPH::RayCastResult &hit = collector.get_hit();

	const JPH::BodyID &body_id = hit.mBodyID;
	const JPH::SubShapeID &sub_shape_id = hit.mSubShapeID2;

	const JoltReadableBody3D body = space->read_body(body_id);
	const JoltObject3D *object = body.as_object();
	ERR_FAIL_NULL_V(object, false);

	const JPH::RVec3 position = ray.GetPointOnRay(hit.mFraction);

	JPH::Vec3 normal = JPH::Vec3::sZero();

	if (!p_parameters.hit_from_inside || hit.mFraction > 0.0f) {
		normal = body->GetWorldSpaceSurfaceNormal(sub_shape_id, position);

		// If we got a back-face normal we need to flip it.
		if (normal.Dot(vector) > 0) {
			normal = -normal;
		}
	}

	r_result.position = to_godot(position);
	r_result.normal = to_godot(normal);
	r_result.rid = object->get_rid();
	r_result.collider_id = object->get_instance_id();
	r_result.collider = object->get_instance();
	r_result.shape = 0;

	if (const JoltShapedObject3D *shaped_object = object->as_shaped()) {
		const int shape_index = shaped_object->find_shape_index(sub_shape_id);
		ERR_FAIL_COND_V(shape_index == -1, false);
		r_result.shape = shape_index;
		r_result.face_index = _try_get_face_index(*body, sub_shape_id);
	}

	return true;
}

int JoltPhysicsDirectSpaceState3D::intersect_point(const PointParameters &p_parameters, ShapeResult *r_results, int p_result_max) {
	ERR_FAIL_COND_V_MSG(space->is_stepping(), false, "intersect_point must not be called while the physics space is being stepped.");

	if (p_result_max == 0) {
		return 0;
	}

	space->try_optimize();

	const JoltQueryFilter3D query_filter(*this, p_parameters.collision_mask, p_parameters.collide_with_bodies, p_parameters.collide_with_areas, p_parameters.exclude);
	JoltQueryCollectorAnyMulti<JPH::CollidePointCollector, 32> collector(p_result_max);
	space->get_narrow_phase_query().CollidePoint(to_jolt_r(p_parameters.position), collector, query_filter, query_filter, query_filter);

	const int hit_count = collector.get_hit_count();

	for (int i = 0; i < hit_count; ++i) {
		const JPH::CollidePointResult &hit = collector.get_hit(i);

		const JoltReadableBody3D body = space->read_body(hit.mBodyID);
		const JoltObject3D *object = body.as_object();
		ERR_FAIL_NULL_V(object, 0);

		ShapeResult &result = *r_results++;

		result.shape = 0;

		if (const JoltShapedObject3D *shaped_object = object->as_shaped()) {
			const int shape_index = shaped_object->find_shape_index(hit.mSubShapeID2);
			ERR_FAIL_COND_V(shape_index == -1, 0);
			result.shape = shape_index;
		}

		result.rid = object->get_rid();
		result.collider_id = object->get_instance_id();
		result.collider = object->get_instance();
	}

	return hit_count;
}

int JoltPhysicsDirectSpaceState3D::intersect_shape(const ShapeParameters &p_parameters, ShapeResult *r_results, int p_result_max) {
	ERR_FAIL_COND_V_MSG(space->is_stepping(), false, "intersect_shape must not be called while the physics space is being stepped.");

	if (p_result_max == 0) {
		return 0;
	}

	space->try_optimize();

	JoltShape3D *shape = JoltPhysicsServer3D::get_singleton()->get_shape(p_parameters.shape_rid);
	ERR_FAIL_NULL_V(shape, 0);

	const JPH::ShapeRefC jolt_shape = shape->try_build();
	ERR_FAIL_NULL_V(jolt_shape, 0);

	Transform3D transform = p_parameters.transform;
	JOLT_ENSURE_SCALE_NOT_ZERO(transform, "intersect_shape was passed an invalid transform.");

	Vector3 scale = transform.basis.get_scale();
	JOLT_ENSURE_SCALE_VALID(jolt_shape, scale, "intersect_shape was passed an invalid transform.");

	transform.basis.orthonormalize();

	const Vector3 com_scaled = to_godot(jolt_shape->GetCenterOfMass());
	const Transform3D transform_com = transform.translated_local(com_scaled);

	JPH::CollideShapeSettings settings;
	settings.mMaxSeparationDistance = (float)p_parameters.margin;

	const JoltQueryFilter3D query_filter(*this, p_parameters.collision_mask, p_parameters.collide_with_bodies, p_parameters.collide_with_areas, p_parameters.exclude);
	JoltQueryCollectorAnyMulti<JPH::CollideShapeCollector, 32> collector(p_result_max);
	_collide_shape_queries(jolt_shape, to_jolt(scale), to_jolt_r(transform_com), settings, to_jolt_r(transform_com.origin), collector, query_filter, query_filter, query_filter);

	const int hit_count = collector.get_hit_count();

	for (int i = 0; i < hit_count; ++i) {
		const JPH::CollideShapeResult &hit = collector.get_hit(i);

		const JoltReadableBody3D body = space->read_body(hit.mBodyID2);
		const JoltObject3D *object = body.as_object();
		ERR_FAIL_NULL_V(object, 0);

		ShapeResult &result = *r_results++;

		result.shape = 0;

		if (const JoltShapedObject3D *shaped_object = object->as_shaped()) {
			const int shape_index = shaped_object->find_shape_index(hit.mSubShapeID2);
			ERR_FAIL_COND_V(shape_index == -1, 0);
			result.shape = shape_index;
		}

		result.rid = object->get_rid();
		result.collider_id = object->get_instance_id();
		result.collider = object->get_instance();
	}

	return hit_count;
}

bool JoltPhysicsDirectSpaceState3D::cast_motion(const ShapeParameters &p_parameters, real_t &r_closest_safe, real_t &r_closest_unsafe, ShapeRestInfo *r_info) {
	ERR_FAIL_COND_V_MSG(space->is_stepping(), false, "cast_motion must not be called while the physics space is being stepped.");
	ERR_FAIL_COND_V_MSG(r_info != nullptr, false, "Providing rest info as part of cast_motion is not supported when using Jolt Physics.");

	space->try_optimize();

	JoltShape3D *shape = JoltPhysicsServer3D::get_singleton()->get_shape(p_parameters.shape_rid);
	ERR_FAIL_NULL_V(shape, false);

	const JPH::ShapeRefC jolt_shape = shape->try_build();
	ERR_FAIL_NULL_V(jolt_shape, false);

	Transform3D transform = p_parameters.transform;
	JOLT_ENSURE_SCALE_NOT_ZERO(transform, "cast_motion (maybe from ShapeCast3D?) was passed an invalid transform.");

	Vector3 scale = transform.basis.get_scale();
	JOLT_ENSURE_SCALE_VALID(jolt_shape, scale, "cast_motion (maybe from ShapeCast3D?) was passed an invalid transform.");

	transform.basis.orthonormalize();

	const Vector3 com_scaled = to_godot(jolt_shape->GetCenterOfMass());
	Transform3D transform_com = transform.translated_local(com_scaled);

	JPH::CollideShapeSettings settings;
	settings.mMaxSeparationDistance = (float)p_parameters.margin;

	const JoltQueryFilter3D query_filter(*this, p_parameters.collision_mask, p_parameters.collide_with_bodies, p_parameters.collide_with_areas, p_parameters.exclude);
	_cast_motion_impl(*jolt_shape, transform_com, scale, p_parameters.motion, JoltProjectSettings::use_enhanced_internal_edge_removal_for_queries(), true, settings, query_filter, query_filter, query_filter, JPH::ShapeFilter(), r_closest_safe, r_closest_unsafe);

	return true;
}

bool JoltPhysicsDirectSpaceState3D::collide_shape(const ShapeParameters &p_parameters, Vector3 *r_results, int p_result_max, int &r_result_count) {
	r_result_count = 0;

	ERR_FAIL_COND_V_MSG(space->is_stepping(), false, "collide_shape must not be called while the physics space is being stepped.");

	if (p_result_max == 0) {
		return false;
	}

	space->try_optimize();

	JoltShape3D *shape = JoltPhysicsServer3D::get_singleton()->get_shape(p_parameters.shape_rid);
	ERR_FAIL_NULL_V(shape, false);

	const JPH::ShapeRefC jolt_shape = shape->try_build();
	ERR_FAIL_NULL_V(jolt_shape, false);

	Transform3D transform = p_parameters.transform;
	JOLT_ENSURE_SCALE_NOT_ZERO(transform, "collide_shape was passed an invalid transform.");

	Vector3 scale = transform.basis.get_scale();
	JOLT_ENSURE_SCALE_VALID(jolt_shape, scale, "collide_shape was passed an invalid transform.");

	transform.basis.orthonormalize();

	const Vector3 com_scaled = to_godot(jolt_shape->GetCenterOfMass());
	const Transform3D transform_com = transform.translated_local(com_scaled);

	JPH::CollideShapeSettings settings;
	settings.mCollectFacesMode = JPH::ECollectFacesMode::CollectFaces;
	settings.mMaxSeparationDistance = (float)p_parameters.margin;

	const Vector3 &base_offset = transform_com.origin;

	const JoltQueryFilter3D query_filter(*this, p_parameters.collision_mask, p_parameters.collide_with_bodies, p_parameters.collide_with_areas, p_parameters.exclude);
	JoltQueryCollectorAnyMulti<JPH::CollideShapeCollector, 32> collector(p_result_max);
	_collide_shape_queries(jolt_shape, to_jolt(scale), to_jolt_r(transform_com), settings, to_jolt_r(base_offset), collector, query_filter, query_filter, query_filter);

	if (!collector.had_hit()) {
		return false;
	}

	const int max_points = p_result_max * 2;

	int point_count = 0;

	for (int i = 0; i < collector.get_hit_count(); ++i) {
		const JPH::CollideShapeResult &hit = collector.get_hit(i);

		const Vector3 penetration_axis = to_godot(hit.mPenetrationAxis.Normalized());
		const Vector3 margin_offset = penetration_axis * (float)p_parameters.margin;

		JPH::ContactPoints contact_points1;
		JPH::ContactPoints contact_points2;

		_generate_manifold(hit, contact_points1, contact_points2 JPH_IF_DEBUG_RENDERER(, to_jolt_r(base_offset)));

		for (JPH::uint j = 0; j < contact_points1.size(); ++j) {
			r_results[point_count++] = base_offset + to_godot(contact_points1[j]) + margin_offset;
			r_results[point_count++] = base_offset + to_godot(contact_points2[j]);

			if (point_count >= max_points) {
				break;
			}
		}

		if (point_count >= max_points) {
			break;
		}
	}

	r_result_count = point_count / 2;

	return true;
}

bool JoltPhysicsDirectSpaceState3D::rest_info(const ShapeParameters &p_parameters, ShapeRestInfo *r_info) {
	ERR_FAIL_COND_V_MSG(space->is_stepping(), false, "get_rest_info must not be called while the physics space is being stepped.");

	space->try_optimize();

	JoltShape3D *shape = JoltPhysicsServer3D::get_singleton()->get_shape(p_parameters.shape_rid);
	ERR_FAIL_NULL_V(shape, false);

	const JPH::ShapeRefC jolt_shape = shape->try_build();
	ERR_FAIL_NULL_V(jolt_shape, false);

	Transform3D transform = p_parameters.transform;
	JOLT_ENSURE_SCALE_NOT_ZERO(transform, "get_rest_info (maybe from ShapeCast3D?) was passed an invalid transform.");

	Vector3 scale = transform.basis.get_scale();
	JOLT_ENSURE_SCALE_VALID(jolt_shape, scale, "get_rest_info (maybe from ShapeCast3D?) was passed an invalid transform.");

	transform.basis.orthonormalize();

	const Vector3 com_scaled = to_godot(jolt_shape->GetCenterOfMass());
	const Transform3D transform_com = transform.translated_local(com_scaled);

	JPH::CollideShapeSettings settings;
	settings.mMaxSeparationDistance = (float)p_parameters.margin;

	const Vector3 &base_offset = transform_com.origin;

	const JoltQueryFilter3D query_filter(*this, p_parameters.collision_mask, p_parameters.collide_with_bodies, p_parameters.collide_with_areas, p_parameters.exclude);
	JoltQueryCollectorClosest<JPH::CollideShapeCollector> collector;
	_collide_shape_queries(jolt_shape, to_jolt(scale), to_jolt_r(transform_com), settings, to_jolt_r(base_offset), collector, query_filter, query_filter, query_filter);

	if (!collector.had_hit()) {
		return false;
	}

	const JPH::CollideShapeResult &hit = collector.get_hit();
	const JoltReadableBody3D body = space->read_body(hit.mBodyID2);
	const JoltObject3D *object = body.as_object();
	ERR_FAIL_NULL_V(object, false);

	r_info->shape = 0;

	if (const JoltShapedObject3D *shaped_object = object->as_shaped()) {
		const int shape_index = shaped_object->find_shape_index(hit.mSubShapeID2);
		ERR_FAIL_COND_V(shape_index == -1, false);
		r_info->shape = shape_index;
	}

	const Vector3 hit_point = base_offset + to_godot(hit.mContactPointOn2);

	r_info->point = hit_point;
	r_info->normal = to_godot(-hit.mPenetrationAxis.Normalized());
	r_info->rid = object->get_rid();
	r_info->collider_id = object->get_instance_id();
	r_info->shape = 0;
	r_info->linear_velocity = object->get_velocity_at_position(hit_point);

	return true;
}

Vector3 JoltPhysicsDirectSpaceState3D::get_closest_point_to_object_volume(RID p_object, Vector3 p_point) const {
	ERR_FAIL_COND_V_MSG(space->is_stepping(), Vector3(), "get_closest_point_to_object_volume must not be called while the physics space is being stepped.");

	space->try_optimize();

	JoltPhysicsServer3D *physics_server = JoltPhysicsServer3D::get_singleton();
	JoltObject3D *object = physics_server->get_area(p_object);

	if (object == nullptr) {
		object = physics_server->get_body(p_object);
	}

	ERR_FAIL_NULL_V(object, Vector3());
	ERR_FAIL_COND_V(object->get_space() != space, Vector3());

	const JoltReadableBody3D body = space->read_body(*object);
	const JPH::TransformedShape root_shape = body->GetTransformedShape();

	JoltQueryCollectorAll<JPH::TransformedShapeCollector, 32> collector;
	root_shape.CollectTransformedShapes(body->GetWorldSpaceBounds(), collector);

	const JPH::RVec3 point = to_jolt_r(p_point);

	float closest_distance_sq = FLT_MAX;
	JPH::RVec3 closest_point = JPH::RVec3::sZero();

	bool found_point = false;

	for (int i = 0; i < collector.get_hit_count(); ++i) {
		const JPH::TransformedShape &shape_transformed = collector.get_hit(i);
		const JPH::Shape &shape = *shape_transformed.mShape;

		if (shape.GetType() != JPH::EShapeType::Convex) {
			continue;
		}

		const JPH::ConvexShape &shape_convex = static_cast<const JPH::ConvexShape &>(shape);

		JPH::GJKClosestPoint gjk;

		JPH::ConvexShape::SupportBuffer shape_support_buffer;
		const JPH::ConvexShape::Support *shape_support = shape_convex.GetSupportFunction(JPH::ConvexShape::ESupportMode::IncludeConvexRadius, shape_support_buffer, shape_transformed.GetShapeScale());

		const JPH::Quat &shape_rotation = shape_transformed.mShapeRotation;
		const JPH::RVec3 &shape_pos_com = shape_transformed.mShapePositionCOM;
		const JPH::RMat44 shape_3x3 = JPH::RMat44::sRotation(shape_rotation);
		const JPH::Vec3 shape_com_local = shape.GetCenterOfMass();
		const JPH::Vec3 shape_com = shape_3x3.Multiply3x3(shape_com_local);
		const JPH::RVec3 shape_pos = shape_pos_com - JPH::RVec3(shape_com);
		const JPH::RMat44 shape_4x4 = shape_3x3.PostTranslated(shape_pos);
		const JPH::RMat44 shape_4x4_inv = shape_4x4.InversedRotationTranslation();

		JPH::PointConvexSupport point_support;
		point_support.mPoint = JPH::Vec3(shape_4x4_inv * point);

		JPH::Vec3 separating_axis = JPH::Vec3::sAxisX();
		JPH::Vec3 point_on_a = JPH::Vec3::sZero();
		JPH::Vec3 point_on_b = JPH::Vec3::sZero();

		const float distance_sq = gjk.GetClosestPoints(*shape_support, point_support, JPH::cDefaultCollisionTolerance, FLT_MAX, separating_axis, point_on_a, point_on_b);

		if (distance_sq == 0.0f) {
			closest_point = point;
			found_point = true;
			break;
		}

		if (distance_sq < closest_distance_sq) {
			closest_distance_sq = distance_sq;
			closest_point = shape_4x4 * point_on_a;
			found_point = true;
		}
	}

	if (found_point) {
		return to_godot(closest_point);
	} else {
		return to_godot(body->GetPosition());
	}
}

bool JoltPhysicsDirectSpaceState3D::body_test_motion(const JoltBody3D &p_body, const PhysicsServer3D::MotionParameters &p_parameters, PhysicsServer3D::MotionResult *r_result) const {
	ERR_FAIL_COND_V_MSG(space->is_stepping(), false, "body_test_motion (maybe from move_and_slide?) must not be called while the physics space is being stepped.");

	const float margin = MAX((float)p_parameters.margin, 0.0001f);
	const int max_collisions = MIN(p_parameters.max_collisions, 32);

	Transform3D transform = p_parameters.from;
	JOLT_ENSURE_SCALE_NOT_ZERO(transform, vformat("body_test_motion (maybe from move_and_slide?) was passed an invalid transform along with body '%s'.", p_body.to_string()));

	Vector3 scale = transform.basis.get_scale();
	transform.basis.orthonormalize();

	space->try_optimize();

	Vector3 recovery;
	const bool recovered = _body_motion_recover(p_body, transform, margin, p_parameters.exclude_bodies, p_parameters.exclude_objects, recovery);

	transform.origin += recovery;

	real_t safe_fraction = 1.0;
	real_t unsafe_fraction = 1.0;

	const bool hit = _body_motion_cast(p_body, transform, scale, p_parameters.motion, p_parameters.collide_separation_ray, p_parameters.exclude_bodies, p_parameters.exclude_objects, safe_fraction, unsafe_fraction);

	bool collided = false;

	if (hit || (recovered && p_parameters.recovery_as_collision)) {
		collided = _body_motion_collide(p_body, transform.translated(p_parameters.motion * unsafe_fraction), p_parameters.motion, margin, max_collisions, p_parameters.exclude_bodies, p_parameters.exclude_objects, r_result);
	}

	if (r_result == nullptr) {
		return collided;
	}

	if (collided) {
		const PhysicsServer3D::MotionCollision &deepest = r_result->collisions[0];

		r_result->travel = recovery + p_parameters.motion * safe_fraction;
		r_result->remainder = p_parameters.motion - p_parameters.motion * safe_fraction;
		r_result->collision_depth = deepest.depth;
		r_result->collision_safe_fraction = safe_fraction;
		r_result->collision_unsafe_fraction = unsafe_fraction;
	} else {
		r_result->travel = recovery + p_parameters.motion;
		r_result->remainder = Vector3();
		r_result->collision_depth = 0.0f;
		r_result->collision_safe_fraction = 1.0f;
		r_result->collision_unsafe_fraction = 1.0f;
		r_result->collision_count = 0;
	}

	return collided;
}

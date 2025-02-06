/**************************************************************************/
/*  jolt_custom_motion_shape.h                                            */
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

#include "jolt_custom_shape_type.h"

#include "core/error/error_macros.h"

#include "Jolt/Jolt.h"

#include "Jolt/Physics/Collision/Shape/ConvexShape.h"
#include "Jolt/Physics/Collision/TransformedShape.h"

class JoltCustomMotionShape final : public JPH::ConvexShape {
	mutable JPH::ConvexShape::SupportBuffer inner_support_buffer;

	JPH::Vec3 motion = JPH::Vec3::sZero();

	const JPH::ConvexShape &inner_shape;

public:
	explicit JoltCustomMotionShape(const JPH::ConvexShape &p_shape) :
			JPH::ConvexShape(JoltCustomShapeSubType::MOTION), inner_shape(p_shape) {}

	virtual bool MustBeStatic() const override { return false; }

	virtual JPH::Vec3 GetCenterOfMass() const override { ERR_FAIL_V_MSG(JPH::Vec3::sZero(), "Not implemented."); }

	virtual JPH::AABox GetLocalBounds() const override;

	virtual JPH::uint GetSubShapeIDBitsRecursive() const override { ERR_FAIL_V_MSG(0, "Not implemented."); }

	virtual JPH::AABox GetWorldSpaceBounds(JPH::Mat44Arg p_center_of_mass_transform, JPH::Vec3Arg p_scale) const override { ERR_FAIL_V_MSG(JPH::AABox(), "Not implemented."); }

	virtual float GetInnerRadius() const override { ERR_FAIL_V_MSG(0.0f, "Not implemented."); }

	virtual JPH::MassProperties GetMassProperties() const override { ERR_FAIL_V_MSG(JPH::MassProperties(), "Not implemented."); }

	virtual const JPH::PhysicsMaterial *GetMaterial(const JPH::SubShapeID &p_sub_shape_id) const override { ERR_FAIL_V_MSG(nullptr, "Not implemented."); }

	virtual JPH::Vec3 GetSurfaceNormal(const JPH::SubShapeID &p_sub_shape_id, JPH::Vec3Arg p_local_surface_position) const override { ERR_FAIL_V_MSG(JPH::Vec3::sZero(), "Not implemented."); }

	virtual void GetSupportingFace(const JPH::SubShapeID &p_sub_shape_id, JPH::Vec3Arg p_direction, JPH::Vec3Arg p_scale, JPH::Mat44Arg p_center_of_mass_transform, JPH::Shape::SupportingFace &p_vertices) const override;

	virtual JPH::uint64 GetSubShapeUserData(const JPH::SubShapeID &p_sub_shape_id) const override { ERR_FAIL_V_MSG(0, "Not implemented."); }

	virtual JPH::TransformedShape GetSubShapeTransformedShape(const JPH::SubShapeID &p_sub_shape_id, JPH::Vec3Arg p_position_com, JPH::QuatArg p_rotation, JPH::Vec3Arg p_scale, JPH::SubShapeID &p_remainder) const override { ERR_FAIL_V_MSG(JPH::TransformedShape(), "Not implemented."); }

	virtual void GetSubmergedVolume(JPH::Mat44Arg p_center_of_mass_transform, JPH::Vec3Arg p_scale, const JPH::Plane &p_surface, float &p_total_volume, float &p_submerged_volume, JPH::Vec3 &p_center_of_buoyancy JPH_IF_DEBUG_RENDERER(, JPH::RVec3Arg p_base_offset)) const override { ERR_FAIL_MSG("Not implemented."); }

	virtual const JPH::ConvexShape::Support *GetSupportFunction(JPH::ConvexShape::ESupportMode p_mode, JPH::ConvexShape::SupportBuffer &p_buffer, JPH::Vec3Arg p_scale) const override;

#ifdef JPH_DEBUG_RENDERER
	virtual void Draw(JPH::DebugRenderer *p_renderer, JPH::RMat44Arg p_center_of_mass_transform, JPH::Vec3Arg p_scale, JPH::ColorArg p_color, bool p_use_material_colors, bool p_draw_wireframe) const override { ERR_FAIL_MSG("Not implemented."); }

	virtual void DrawGetSupportFunction(JPH::DebugRenderer *p_renderer, JPH::RMat44Arg p_center_of_mass_transform, JPH::Vec3Arg p_scale, JPH::ColorArg p_color, bool p_draw_support_direction) const override { ERR_FAIL_MSG("Not implemented."); }

	virtual void DrawGetSupportingFace(JPH::DebugRenderer *p_renderer, JPH::RMat44Arg p_center_of_mass_transform, JPH::Vec3Arg p_scale) const override { ERR_FAIL_MSG("Not implemented."); }
#endif

	virtual bool CastRay(const JPH::RayCast &p_ray, const JPH::SubShapeIDCreator &p_sub_shape_id_creator, JPH::RayCastResult &p_hit) const override { ERR_FAIL_V_MSG(false, "Not implemented."); }

	virtual void CastRay(const JPH::RayCast &p_ray, const JPH::RayCastSettings &p_ray_cast_settings, const JPH::SubShapeIDCreator &p_sub_shape_id_creator, JPH::CastRayCollector &p_collector, const JPH::ShapeFilter &p_shape_filter = JPH::ShapeFilter()) const override { ERR_FAIL_MSG("Not implemented."); }

	virtual void CollidePoint(JPH::Vec3Arg p_point, const JPH::SubShapeIDCreator &p_sub_shape_id_creator, JPH::CollidePointCollector &p_collector, const JPH::ShapeFilter &p_shape_filter = JPH::ShapeFilter()) const override { ERR_FAIL_MSG("Not implemented."); }

	virtual void CollideSoftBodyVertices(JPH::Mat44Arg p_center_of_mass_transform, JPH::Vec3Arg p_scale, const JPH::CollideSoftBodyVertexIterator &p_vertices, JPH::uint p_num_vertices, int p_colliding_shape_index) const override { ERR_FAIL_MSG("Not implemented."); }

	virtual void CollectTransformedShapes(const JPH::AABox &p_box, JPH::Vec3Arg p_position_com, JPH::QuatArg p_rotation, JPH::Vec3Arg p_scale, const JPH::SubShapeIDCreator &p_sub_shape_id_creator, JPH::TransformedShapeCollector &p_collector, const JPH::ShapeFilter &p_shape_filter = JPH::ShapeFilter()) const override { ERR_FAIL_MSG("Not implemented."); }

	virtual void TransformShape(JPH::Mat44Arg p_center_of_mass_transform, JPH::TransformedShapeCollector &p_collector) const override { ERR_FAIL_MSG("Not implemented."); }

	virtual void GetTrianglesStart(JPH::Shape::GetTrianglesContext &p_context, const JPH::AABox &p_box, JPH::Vec3Arg p_position_com, JPH::QuatArg p_rotation, JPH::Vec3Arg p_scale) const override { ERR_FAIL_MSG("Not implemented."); }

	virtual int GetTrianglesNext(JPH::Shape::GetTrianglesContext &p_context, int p_max_triangles_requested, JPH::Float3 *p_triangle_vertices, const JPH::PhysicsMaterial **p_materials = nullptr) const override { ERR_FAIL_V_MSG(0, "Not implemented."); }

	virtual JPH::Shape::Stats GetStats() const override { return JPH::Shape::Stats(sizeof(*this), 0); }

	virtual float GetVolume() const override { ERR_FAIL_V_MSG(0.0f, "Not implemented."); }

	virtual bool IsValidScale(JPH::Vec3Arg p_scale) const override { ERR_FAIL_V_MSG(false, "Not implemented."); }

	const JPH::ConvexShape &get_inner_shape() const { return inner_shape; }

	void set_motion(JPH::Vec3Arg p_motion) { motion = p_motion; }
};

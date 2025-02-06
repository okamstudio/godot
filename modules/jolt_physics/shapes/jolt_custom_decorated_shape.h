/**************************************************************************/
/*  jolt_custom_decorated_shape.h                                         */
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

#include "Jolt/Jolt.h"

#include "Jolt/Physics/Collision/Shape/DecoratedShape.h"
#include "Jolt/Physics/Collision/TransformedShape.h"

class JoltCustomDecoratedShapeSettings : public JPH::DecoratedShapeSettings {
public:
	using JPH::DecoratedShapeSettings::DecoratedShapeSettings;
};

class JoltCustomDecoratedShape : public JPH::DecoratedShape {
public:
	using JPH::DecoratedShape::DecoratedShape;

	virtual JPH::AABox GetLocalBounds() const override { return mInnerShape->GetLocalBounds(); }

	virtual JPH::AABox GetWorldSpaceBounds(JPH::Mat44Arg p_center_of_mass_transform, JPH::Vec3Arg p_scale) const override { return mInnerShape->GetWorldSpaceBounds(p_center_of_mass_transform, p_scale); }

	virtual float GetInnerRadius() const override { return mInnerShape->GetInnerRadius(); }

	virtual JPH::MassProperties GetMassProperties() const override { return mInnerShape->GetMassProperties(); }

	virtual JPH::Vec3 GetSurfaceNormal(const JPH::SubShapeID &p_sub_shape_id, JPH::Vec3Arg p_local_surface_position) const override { return mInnerShape->GetSurfaceNormal(p_sub_shape_id, p_local_surface_position); }

	virtual JPH::uint64 GetSubShapeUserData(const JPH::SubShapeID &p_sub_shape_id) const override { return mInnerShape->GetSubShapeUserData(p_sub_shape_id); }

	virtual JPH::TransformedShape GetSubShapeTransformedShape(const JPH::SubShapeID &p_sub_shape_id, JPH::Vec3Arg p_position_com, JPH::QuatArg p_rotation, JPH::Vec3Arg p_scale, JPH::SubShapeID &p_remainder) const override { return mInnerShape->GetSubShapeTransformedShape(p_sub_shape_id, p_position_com, p_rotation, p_scale, p_remainder); }

	virtual void GetSubmergedVolume(JPH::Mat44Arg p_center_of_mass_transform, JPH::Vec3Arg p_scale, const JPH::Plane &p_surface, float &p_total_volume, float &p_submerged_volume, JPH::Vec3 &p_center_of_buoyancy JPH_IF_DEBUG_RENDERER(, JPH::RVec3Arg p_base_offset)) const override { mInnerShape->GetSubmergedVolume(p_center_of_mass_transform, p_scale, p_surface, p_total_volume, p_submerged_volume, p_center_of_buoyancy JPH_IF_DEBUG_RENDERER(, p_base_offset)); }

#ifdef JPH_DEBUG_RENDERER
	virtual void Draw(JPH::DebugRenderer *p_renderer, JPH::RMat44Arg p_center_of_mass_transform, JPH::Vec3Arg p_scale, JPH::ColorArg p_color, bool p_use_material_colors, bool p_draw_wireframe) const override { mInnerShape->Draw(p_renderer, p_center_of_mass_transform, p_scale, p_color, p_use_material_colors, p_draw_wireframe); }

	virtual void DrawGetSupportFunction(JPH::DebugRenderer *p_renderer, JPH::RMat44Arg p_center_of_mass_transform, JPH::Vec3Arg p_scale, JPH::ColorArg p_color, bool p_draw_support_direction) const override { mInnerShape->DrawGetSupportFunction(p_renderer, p_center_of_mass_transform, p_scale, p_color, p_draw_support_direction); }

	virtual void DrawGetSupportingFace(JPH::DebugRenderer *p_renderer, JPH::RMat44Arg p_center_of_mass_transform, JPH::Vec3Arg p_scale) const override { mInnerShape->DrawGetSupportingFace(p_renderer, p_center_of_mass_transform, p_scale); }
#endif

	virtual bool CastRay(const JPH::RayCast &p_ray, const JPH::SubShapeIDCreator &p_sub_shape_id_creator, JPH::RayCastResult &p_hit) const override { return mInnerShape->CastRay(p_ray, p_sub_shape_id_creator, p_hit); }

	virtual void CastRay(const JPH::RayCast &p_ray, const JPH::RayCastSettings &p_ray_cast_settings, const JPH::SubShapeIDCreator &p_sub_shape_id_creator, JPH::CastRayCollector &p_collector, const JPH::ShapeFilter &p_shape_filter = JPH::ShapeFilter()) const override { return mInnerShape->CastRay(p_ray, p_ray_cast_settings, p_sub_shape_id_creator, p_collector, p_shape_filter); }

	virtual void CollidePoint(JPH::Vec3Arg p_point, const JPH::SubShapeIDCreator &p_sub_shape_id_creator, JPH::CollidePointCollector &p_collector, const JPH::ShapeFilter &p_shape_filter = JPH::ShapeFilter()) const override { mInnerShape->CollidePoint(p_point, p_sub_shape_id_creator, p_collector, p_shape_filter); }

	virtual void CollideSoftBodyVertices(JPH::Mat44Arg p_center_of_mass_transform, JPH::Vec3Arg p_scale, const JPH::CollideSoftBodyVertexIterator &p_vertices, JPH::uint p_num_vertices, int p_colliding_shape_index) const override { mInnerShape->CollideSoftBodyVertices(p_center_of_mass_transform, p_scale, p_vertices, p_num_vertices, p_colliding_shape_index); }

	virtual void CollectTransformedShapes(const JPH::AABox &p_box, JPH::Vec3Arg p_position_com, JPH::QuatArg p_rotation, JPH::Vec3Arg p_scale, const JPH::SubShapeIDCreator &p_sub_shape_id_creator, JPH::TransformedShapeCollector &p_collector, const JPH::ShapeFilter &p_shape_filter = JPH::ShapeFilter()) const override { mInnerShape->CollectTransformedShapes(p_box, p_position_com, p_rotation, p_scale, p_sub_shape_id_creator, p_collector, p_shape_filter); }

	virtual void TransformShape(JPH::Mat44Arg p_center_of_mass_transform, JPH::TransformedShapeCollector &p_collector) const override { mInnerShape->TransformShape(p_center_of_mass_transform, p_collector); }

	virtual void GetTrianglesStart(JPH::Shape::GetTrianglesContext &p_context, const JPH::AABox &p_box, JPH::Vec3Arg p_position_com, JPH::QuatArg p_rotation, JPH::Vec3Arg p_scale) const override { mInnerShape->GetTrianglesStart(p_context, p_box, p_position_com, p_rotation, p_scale); }

	virtual int GetTrianglesNext(JPH::Shape::GetTrianglesContext &p_context, int p_max_triangles_requested, JPH::Float3 *p_triangle_vertices, const JPH::PhysicsMaterial **p_materials = nullptr) const override { return mInnerShape->GetTrianglesNext(p_context, p_max_triangles_requested, p_triangle_vertices, p_materials); }

	virtual JPH::Shape::Stats GetStats() const override { return JPH::Shape::Stats(sizeof(*this), 0); }

	virtual float GetVolume() const override { return mInnerShape->GetVolume(); }
};

/**************************************************************************/
/*  jolt_custom_ray_shape.h                                               */
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

#include "Jolt/Physics/Collision/Shape/ConvexShape.h"

class JoltCustomRayShapeSettings final : public JPH::ConvexShapeSettings {
public:
	JPH::RefConst<JPH::PhysicsMaterial> material;
	float length = 1.0f;
	bool slide_on_slope = false;

	JoltCustomRayShapeSettings() = default;
	JoltCustomRayShapeSettings(float p_length, bool p_slide_on_slope, const JPH::PhysicsMaterial *p_material = nullptr) :
			material(p_material), length(p_length), slide_on_slope(p_slide_on_slope) {}

	virtual JPH::ShapeSettings::ShapeResult Create() const override;
};

class JoltCustomRayShape final : public JPH::ConvexShape {
public:
	JPH::RefConst<JPH::PhysicsMaterial> material;
	float length = 0.0f;
	bool slide_on_slope = false;

	static void register_type();

	JoltCustomRayShape() :
			JPH::ConvexShape(JoltCustomShapeSubType::RAY) {}

	JoltCustomRayShape(const JoltCustomRayShapeSettings &p_settings, JPH::Shape::ShapeResult &p_result) :
			JPH::ConvexShape(JoltCustomShapeSubType::RAY, p_settings, p_result), material(p_settings.material), length(p_settings.length), slide_on_slope(p_settings.slide_on_slope) {
		if (!p_result.HasError()) {
			p_result.Set(this);
		}
	}

	JoltCustomRayShape(float p_length, bool p_slide_on_slope, const JPH::PhysicsMaterial *p_material = nullptr) :
			JPH::ConvexShape(JoltCustomShapeSubType::RAY), material(p_material), length(p_length), slide_on_slope(p_slide_on_slope) {}

	virtual JPH::AABox GetLocalBounds() const override;

	virtual float GetInnerRadius() const override;

	virtual JPH::MassProperties GetMassProperties() const override;

	virtual JPH::Vec3 GetSurfaceNormal(const JPH::SubShapeID &p_sub_shape_id, JPH::Vec3Arg p_local_surface_position) const override { return JPH::Vec3::sAxisZ(); }

	virtual void GetSubmergedVolume(JPH::Mat44Arg p_center_of_mass_transform, JPH::Vec3Arg p_scale, const JPH::Plane &p_surface, float &p_total_volume, float &p_submerged_volume, JPH::Vec3 &p_center_of_buoyancy JPH_IF_DEBUG_RENDERER(, JPH::RVec3Arg p_base_offset)) const override {
		p_total_volume = 0.0f;
		p_submerged_volume = 0.0f;
		p_center_of_buoyancy = JPH::Vec3::sZero();
	}

#ifdef JPH_DEBUG_RENDERER
	virtual void Draw(JPH::DebugRenderer *p_renderer, JPH::RMat44Arg p_center_of_mass_transform, JPH::Vec3Arg p_scale, JPH::ColorArg p_color, bool p_use_material_colors, bool p_draw_wireframe) const override;
#endif

	virtual bool CastRay(const JPH::RayCast &p_ray, const JPH::SubShapeIDCreator &p_sub_shape_id_creator, JPH::RayCastResult &p_hit) const override { return false; }

	virtual void CastRay(const JPH::RayCast &p_ray, const JPH::RayCastSettings &p_ray_cast_settings, const JPH::SubShapeIDCreator &p_sub_shape_id_creator, JPH::CastRayCollector &p_collector, const JPH::ShapeFilter &p_shape_filter = JPH::ShapeFilter()) const override {}

	virtual void CollidePoint(JPH::Vec3Arg p_point, const JPH::SubShapeIDCreator &p_sub_shape_id_creator, JPH::CollidePointCollector &p_collector, const JPH::ShapeFilter &p_shape_filter = JPH::ShapeFilter()) const override {}

	virtual void CollideSoftBodyVertices(JPH::Mat44Arg p_center_of_mass_transform, JPH::Vec3Arg p_scale, const JPH::CollideSoftBodyVertexIterator &p_vertices, JPH::uint p_num_vertices, int p_colliding_shape_index) const override {}

	virtual JPH::Shape::Stats GetStats() const override { return JPH::Shape::Stats(sizeof(*this), 0); }

	virtual float GetVolume() const override { return 0.0f; }

	virtual const JPH::ConvexShape::Support *GetSupportFunction(JPH::ConvexShape::ESupportMode p_mode, JPH::ConvexShape::SupportBuffer &p_buffer, JPH::Vec3Arg p_scale) const override;
};

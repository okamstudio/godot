/**************************************************************************/
/*  jolt_custom_double_sided_shape.h                                      */
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

#ifndef JOLT_CUSTOM_DOUBLE_SIDED_SHAPE_H
#define JOLT_CUSTOM_DOUBLE_SIDED_SHAPE_H

#include "jolt_custom_decorated_shape.h"
#include "jolt_custom_shape_type.h"

class JoltCustomDoubleSidedShapeSettings final : public JoltCustomDecoratedShapeSettings {
public:
	bool back_face_collision = false;

	JoltCustomDoubleSidedShapeSettings() = default;

	JoltCustomDoubleSidedShapeSettings(const ShapeSettings *p_inner_settings, bool p_back_face_collision) :
			JoltCustomDecoratedShapeSettings(p_inner_settings), back_face_collision(p_back_face_collision) {}

	JoltCustomDoubleSidedShapeSettings(const JPH::Shape *p_inner_shape, bool p_back_face_collision) :
			JoltCustomDecoratedShapeSettings(p_inner_shape), back_face_collision(p_back_face_collision) {}

	virtual JPH::Shape::ShapeResult Create() const override;
};

class JoltCustomDoubleSidedShape final : public JoltCustomDecoratedShape {
	bool back_face_collision = false;

public:
	static void register_type();

	JoltCustomDoubleSidedShape() :
			JoltCustomDecoratedShape(JoltCustomShapeSubType::DOUBLE_SIDED) {}

	JoltCustomDoubleSidedShape(const JoltCustomDoubleSidedShapeSettings &p_settings, JPH::Shape::ShapeResult &p_result) :
			JoltCustomDecoratedShape(JoltCustomShapeSubType::DOUBLE_SIDED, p_settings, p_result), back_face_collision(p_settings.back_face_collision) {
		if (!p_result.HasError()) {
			p_result.Set(this);
		}
	}

	JoltCustomDoubleSidedShape(const JPH::Shape *p_inner_shape, bool p_back_face_collision) :
			JoltCustomDecoratedShape(JoltCustomShapeSubType::DOUBLE_SIDED, p_inner_shape), back_face_collision(p_back_face_collision) {}

	virtual void CastRay(const JPH::RayCast &p_ray, const JPH::RayCastSettings &p_ray_cast_settings, const JPH::SubShapeIDCreator &p_sub_shape_id_creator, JPH::CastRayCollector &p_collector, const JPH::ShapeFilter &p_shape_filter = JPH::ShapeFilter()) const override;
};

#endif // JOLT_CUSTOM_DOUBLE_SIDED_SHAPE_H

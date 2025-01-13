/**************************************************************************/
/*  jolt_height_map_shape_3d.h                                            */
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

#ifndef JOLT_HEIGHT_MAP_SHAPE_3D_H
#define JOLT_HEIGHT_MAP_SHAPE_3D_H

#include "jolt_shape_3d.h"

class JoltHeightMapShape3D final : public JoltShape3D {
	AABB aabb;

#ifdef REAL_T_IS_DOUBLE
	PackedFloat64Array heights;
#else
	PackedFloat32Array heights;
#endif

	int width = 0;
	int depth = 0;

	virtual JPH::ShapeRefC _build() const override;
	JPH::ShapeRefC _build_height_field() const;
	JPH::ShapeRefC _build_mesh() const;

	AABB _calculate_aabb() const;

public:
	virtual ShapeType get_type() const override { return ShapeType::SHAPE_HEIGHTMAP; }
	virtual bool is_convex() const override { return false; }

	virtual Variant get_data() const override;
	virtual void set_data(const Variant &p_data) override;

	virtual float get_margin() const override { return 0.0f; }
	virtual void set_margin(float p_margin) override {}

	virtual AABB get_aabb() const override { return aabb; }

	String to_string() const;
};

#endif // JOLT_HEIGHT_MAP_SHAPE_3D_H

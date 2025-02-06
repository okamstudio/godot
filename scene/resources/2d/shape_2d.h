/**************************************************************************/
/*  shape_2d.h                                                            */
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

#include "core/io/resource.h"

class Shape2D : public Resource {
	GDCLASS(Shape2D, Resource);
	OBJ_SAVE_TYPE(Shape2D);

	RID shape;
	real_t custom_bias = 0.0;

protected:
	static void _bind_methods();
	Shape2D(const RID &p_rid);

public:
	virtual bool _edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const { return get_rect().has_point(p_point); }

	void set_custom_solver_bias(real_t p_bias);
	real_t get_custom_solver_bias() const;

	bool collide_with_motion(const Transform2D &p_local_xform, const Vector2 &p_local_motion, const Ref<Shape2D> &p_shape, const Transform2D &p_shape_xform, const Vector2 &p_shape_motion);
	bool collide(const Transform2D &p_local_xform, const Ref<Shape2D> &p_shape, const Transform2D &p_shape_xform);

	PackedVector2Array collide_with_motion_and_get_contacts(const Transform2D &p_local_xform, const Vector2 &p_local_motion, const Ref<Shape2D> &p_shape, const Transform2D &p_shape_xform, const Vector2 &p_shape_motion);
	PackedVector2Array collide_and_get_contacts(const Transform2D &p_local_xform, const Ref<Shape2D> &p_shape, const Transform2D &p_shape_xform);

	virtual void draw(const RID &p_to_rid, const Color &p_color) {}
	virtual Rect2 get_rect() const { return Rect2(); }
	/// Returns the radius of a circle that fully enclose this shape
	virtual real_t get_enclosing_radius() const = 0;
	virtual RID get_rid() const override;

	static bool is_collision_outline_enabled();

	~Shape2D();
};

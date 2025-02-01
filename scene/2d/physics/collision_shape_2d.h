/**************************************************************************/
/*  collision_shape_2d.h                                                  */
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

#include "scene/2d/node_2d.h"
#include "scene/resources/2d/shape_2d.h"

class CollisionObject2D;

class CollisionShape2D : public Node2D {
	GDCLASS(CollisionShape2D, Node2D);
	Ref<Shape2D> shape;
	Rect2 rect = Rect2(-Point2(10, 10), Point2(20, 20));
	uint32_t owner_id = 0;
	CollisionObject2D *collision_object = nullptr;
	bool disabled = false;
	bool one_way_collision = false;
	real_t one_way_collision_margin = 1.0;

	void _shape_changed();
	void _update_in_shape_owner(bool p_xform_only = false);

	// Not wrapped in `#ifdef DEBUG_ENABLED` as it is used for rendering.
	Color debug_color = Color(0.0, 0.0, 0.0, 0.0);

	Color _get_default_debug_color() const;

protected:
	void _notification(int p_what);

#ifdef DEBUG_ENABLED
	bool _property_can_revert(const StringName &p_name) const;
	bool _property_get_revert(const StringName &p_name, Variant &r_property) const;
	void _validate_property(PropertyInfo &p_property) const;
#endif // DEBUG_ENABLED

	static void _bind_methods();

public:
#ifdef DEBUG_ENABLED
	virtual bool _edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const override;
#else
	virtual bool _edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const;
#endif // DEBUG_ENABLED

	void set_shape(const Ref<Shape2D> &p_shape);
	Ref<Shape2D> get_shape() const;

	void set_disabled(bool p_disabled);
	bool is_disabled() const;

	void set_one_way_collision(bool p_enable);
	bool is_one_way_collision_enabled() const;

	void set_one_way_collision_margin(real_t p_margin);
	real_t get_one_way_collision_margin() const;

	void set_debug_color(const Color &p_color);
	Color get_debug_color() const;

	PackedStringArray get_configuration_warnings() const override;

	CollisionShape2D();
};

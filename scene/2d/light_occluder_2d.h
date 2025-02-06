/**************************************************************************/
/*  light_occluder_2d.h                                                   */
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

class OccluderPolygon2D : public Resource {
	GDCLASS(OccluderPolygon2D, Resource);

public:
	enum CullMode {
		CULL_DISABLED,
		CULL_CLOCKWISE,
		CULL_COUNTER_CLOCKWISE
	};

private:
	RID occ_polygon;
	Vector<Vector2> polygon;
	bool closed = true;
	CullMode cull = CULL_DISABLED;

	mutable Rect2 item_rect;
	mutable bool rect_cache_dirty = true;

protected:
	static void _bind_methods();

public:
#ifdef DEBUG_ENABLED
	virtual Rect2 _edit_get_rect() const;
	virtual bool _edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const;
#endif // DEBUG_ENABLED
	void set_polygon(const Vector<Vector2> &p_polygon);
	Vector<Vector2> get_polygon() const;

	void set_closed(bool p_closed);
	bool is_closed() const;

	void set_cull_mode(CullMode p_mode);
	CullMode get_cull_mode() const;

	virtual RID get_rid() const override;
	OccluderPolygon2D();
	~OccluderPolygon2D();
};

VARIANT_ENUM_CAST(OccluderPolygon2D::CullMode);

class LightOccluder2D : public Node2D {
	GDCLASS(LightOccluder2D, Node2D);

	RID occluder;
	int mask = 1;
	Ref<OccluderPolygon2D> occluder_polygon;
	bool sdf_collision = false;
	void _poly_changed();

	virtual void _physics_interpolated_changed() override;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
#ifdef DEBUG_ENABLED
	virtual Rect2 _edit_get_rect() const override;
	virtual bool _edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const override;
#endif // DEBUG_ENABLED

	void set_occluder_polygon(const Ref<OccluderPolygon2D> &p_polygon);
	Ref<OccluderPolygon2D> get_occluder_polygon() const;

	void set_occluder_light_mask(int p_mask);
	int get_occluder_light_mask() const;

	void set_as_sdf_collision(bool p_enable);
	bool is_set_as_sdf_collision() const;

	PackedStringArray get_configuration_warnings() const override;

	LightOccluder2D();
	~LightOccluder2D();
};

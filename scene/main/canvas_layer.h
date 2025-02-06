/**************************************************************************/
/*  canvas_layer.h                                                        */
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

#include "scene/main/node.h"

class Viewport;
class CanvasLayer : public Node {
	GDCLASS(CanvasLayer, Node);

	mutable bool locrotscale_dirty = false;
	mutable Vector2 ofs;
	mutable Size2 scale = Vector2(1, 1);
	mutable real_t rot = 0.0;
	int layer = 1;
	Transform2D transform;
	RID canvas;

	ObjectID custom_viewport_id; // to check validity
	Viewport *custom_viewport = nullptr;

	RID viewport;
	Viewport *vp = nullptr;

	int sort_index = 0;
	bool visible = true;

	bool follow_viewport = false;
	float follow_viewport_scale = 1.0;

	void _update_xform();
	void _update_locrotscale() const;
	void _update_follow_viewport(bool p_force_exit = false);

protected:
	void _notification(int p_what);
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	void update_draw_order();

	void set_layer(int p_xform);
	int get_layer() const;

	void set_visible(bool p_visible);
	bool is_visible() const;
	void show();
	void hide();

	void set_transform(const Transform2D &p_xform);
	Transform2D get_transform() const;
	Transform2D get_final_transform() const;

	void set_offset(const Vector2 &p_offset);
	Vector2 get_offset() const;

	void set_rotation(real_t p_radians);
	real_t get_rotation() const;

	void set_scale(const Size2 &p_scale);
	Size2 get_scale() const;

	Size2 get_viewport_size() const;

	RID get_viewport() const;

	void set_custom_viewport(Node *p_viewport);
	Node *get_custom_viewport() const;

	void reset_sort_index();
	int get_sort_index();

	void set_follow_viewport(bool p_enable);
	bool is_following_viewport() const;

	void set_follow_viewport_scale(float p_ratio);
	float get_follow_viewport_scale() const;

	RID get_canvas() const;

	CanvasLayer();
	~CanvasLayer();
};

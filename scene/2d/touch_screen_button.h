/**************************************************************************/
/*  touch_screen_button.h                                                 */
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
#include "scene/resources/2d/rectangle_shape_2d.h"
#include "scene/resources/bit_map.h"
#include "scene/resources/texture.h"

class TouchScreenButton : public Node2D {
	GDCLASS(TouchScreenButton, Node2D);

public:
	enum VisibilityMode {
		VISIBILITY_ALWAYS,
		VISIBILITY_TOUCHSCREEN_ONLY
	};

private:
	Ref<Texture2D> texture_normal;
	Ref<Texture2D> texture_pressed;
	Ref<BitMap> bitmask;
	Ref<Shape2D> shape;
	bool shape_centered = true;
	bool shape_visible = true;

	Ref<RectangleShape2D> unit_rect;

	StringName action;
	bool passby_press = false;
	int finger_pressed = -1;

	VisibilityMode visibility = VISIBILITY_ALWAYS;

	virtual void input(const Ref<InputEvent> &p_event) override;

	bool _is_point_inside(const Point2 &p_point);

	void _press(int p_finger_pressed);
	void _release(bool p_exiting_tree = false);

protected:
	void _notification(int p_what);
	static void _bind_methods();
#ifndef DISABLE_DEPRECATED
	bool _set(const StringName &p_name, const Variant &p_value);
#endif // DISABLE_DEPRECATED

public:
#ifdef DEBUG_ENABLED
	virtual Rect2 _edit_get_rect() const override;
	virtual bool _edit_use_rect() const override;
#endif // DEBUG_ENABLED

	void set_texture_normal(const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_texture_normal() const;

	void set_texture_pressed(const Ref<Texture2D> &p_texture_pressed);
	Ref<Texture2D> get_texture_pressed() const;

	void set_bitmask(const Ref<BitMap> &p_bitmask);
	Ref<BitMap> get_bitmask() const;

	void set_shape(const Ref<Shape2D> &p_shape);
	Ref<Shape2D> get_shape() const;

	void set_shape_centered(bool p_shape_centered);
	bool is_shape_centered() const;

	void set_shape_visible(bool p_shape_visible);
	bool is_shape_visible() const;

	void set_action(const String &p_action);
	String get_action() const;

	void set_passby_press(bool p_enable);
	bool is_passby_press_enabled() const;

	void set_visibility_mode(VisibilityMode p_mode);
	VisibilityMode get_visibility_mode() const;

	bool is_pressed() const;

	virtual Rect2 get_anchorable_rect() const override;

	TouchScreenButton();
};

VARIANT_ENUM_CAST(TouchScreenButton::VisibilityMode);

/**************************************************************************/
/*  scroll_container.h                                                    */
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

#ifndef SCROLL_CONTAINER_H
#define SCROLL_CONTAINER_H

#include "container.h"

#include "scroll_bar.h"

class ScrollContainer : public Container {
	GDCLASS(ScrollContainer, Container);

public:
	enum ScrollMode {
		SCROLL_MODE_DISABLED = 0,
		SCROLL_MODE_AUTO,
		SCROLL_MODE_SHOW_ALWAYS,
		SCROLL_MODE_SHOW_NEVER,
		SCROLL_MODE_RESERVE,
	};

private:
	HScrollBar *h_scroll = nullptr;
	VScrollBar *v_scroll = nullptr;

	mutable Size2 largest_child_min_size; // The largest one among the min sizes of all available child controls.

	void update_scrollbars();

	Vector2 drag_speed;
	Vector2 drag_accum;
	Vector2 drag_from;
	Vector2 last_drag_accum;
	float time_since_motion = 0.0f;
	bool drag_touching = false;
	bool drag_touching_deaccel = false;
	bool beyond_deadzone = false;

	ScrollMode horizontal_scroll_mode = SCROLL_MODE_AUTO;
	ScrollMode vertical_scroll_mode = SCROLL_MODE_AUTO;

	int deadzone = 0;
	bool follow_focus = false;

	struct ThemeCache {
		Ref<StyleBox> panel_style;
		Ref<StyleBox> focus_style;
	} theme_cache;

	void _cancel_drag();

	bool _is_h_scroll_visible() const;
	bool _is_v_scroll_visible() const;

	bool draw_focus_border = false;
	bool focus_border_is_drawn = false;
	bool child_has_focus();

protected:
	Size2 get_minimum_size() const override;

	void _gui_focus_changed(Control *p_control);
	void _reposition_children();

	void _notification(int p_what);
	static void _bind_methods();

	bool _updating_scrollbars = false;
	void _update_scrollbar_position();
	void _scroll_moved(float);

public:
	virtual void gui_input(const Ref<InputEvent> &p_gui_input) override;

	void set_h_scroll(int p_pos);
	int get_h_scroll() const;

	void set_v_scroll(int p_pos);
	int get_v_scroll() const;

	void set_horizontal_custom_step(float p_custom_step);
	float get_horizontal_custom_step() const;

	void set_vertical_custom_step(float p_custom_step);
	float get_vertical_custom_step() const;

	void set_horizontal_scroll_mode(ScrollMode p_mode);
	ScrollMode get_horizontal_scroll_mode() const;

	void set_vertical_scroll_mode(ScrollMode p_mode);
	ScrollMode get_vertical_scroll_mode() const;

	int get_deadzone() const;
	void set_deadzone(int p_deadzone);

	bool is_following_focus() const;
	void set_follow_focus(bool p_follow);

	HScrollBar *get_h_scroll_bar();
	VScrollBar *get_v_scroll_bar();
	void ensure_control_visible(Control *p_control);

	PackedStringArray get_configuration_warnings() const override;

	void set_draw_focus_border(bool p_draw);
	bool get_draw_focus_border();

	ScrollContainer();
};

VARIANT_ENUM_CAST(ScrollContainer::ScrollMode);

#endif // SCROLL_CONTAINER_H

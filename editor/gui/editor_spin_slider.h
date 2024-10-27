/**************************************************************************/
/*  editor_spin_slider.h                                                  */
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

#ifndef EDITOR_SPIN_SLIDER_H
#define EDITOR_SPIN_SLIDER_H

#include "scene/gui/line_edit.h"
#include "scene/gui/range.h"
#include "scene/gui/texture_rect.h"

class EditorSpinSlider : public Range {
	GDCLASS(EditorSpinSlider, Range);

	String label;
	String suffix;
	int updown_offset = -1;
	bool hover_updown = false;
	bool mouse_hover = false;

	TextureRect *grabber = nullptr;
	int grabber_range = 1;

	bool mouse_over_spin = false;
	bool mouse_over_grabber = false;
	bool mousewheel_over_grabber = false;

	bool grabbing_grabber = false;
	int grabbing_from = 0;
	float grabbing_ratio = 0.0f;

	bool grabbing_spinner_attempt = false;
	bool grabbing_spinner = false;

	bool read_only = false;
	float grabbing_spinner_dist_cache = 0.0f;
	float grabbing_spinner_speed = 0.0f;
	Vector2 grabbing_spinner_mouse_pos;
	double pre_grab_value = 0.0;

	Control *value_input_popup = nullptr;
	LineEdit *value_input = nullptr;
	uint64_t value_input_closed_frame = 0;
	bool value_input_dirty = false;

	bool hide_slider = false;
	bool flat = false;

	void _grab_start();
	void _grab_end();

	void _grabber_gui_input(const Ref<InputEvent> &p_event);
	void _value_input_closed();
	void _value_input_submitted(const String &);
	void _value_focus_exited();
	void _value_input_gui_input(const Ref<InputEvent> &p_event);

	void _evaluate_input_text();

	void _update_value_input_stylebox();
	void _ensure_input_popup();
	void _draw_spin_slider();

	struct ThemeCache {
		Ref<Texture2D> updown_icon;
		Ref<Texture2D> updown_disabled_icon;
	} theme_cache;

protected:
	void _notification(int p_what);
	virtual void gui_input(const Ref<InputEvent> &p_event) override;
	static void _bind_methods();
	void _grabber_mouse_entered();
	void _grabber_mouse_exited();
	void _focus_entered();

public:
	String get_tooltip(const Point2 &p_pos) const override;

	String get_text_value() const;
	void set_label(const String &p_label);
	String get_label() const;

	void set_suffix(const String &p_suffix);
	String get_suffix() const;

	void set_hide_slider(bool p_hide);
	bool is_hiding_slider() const;

	void set_read_only(bool p_enable);
	bool is_read_only() const;

	void set_flat(bool p_enable);
	bool is_flat() const;

	bool is_grabbing() const;

	void setup_and_show() { _focus_entered(); }
	LineEdit *get_line_edit();

	virtual Size2 get_minimum_size() const override;
	EditorSpinSlider();
};

#endif // EDITOR_SPIN_SLIDER_H

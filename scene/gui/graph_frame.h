/**************************************************************************/
/*  graph_frame.h                                                         */
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

#include "scene/gui/graph_element.h"

class HBoxContainer;

class GraphFrame : public GraphElement {
	GDCLASS(GraphFrame, GraphElement);

	struct _MinSizeCache {
		int min_size = 0;
		bool will_stretch = false;
		int final_size = 0;
	};

	struct ThemeCache {
		Ref<StyleBox> panel;
		Ref<StyleBox> panel_selected;
		Ref<StyleBox> titlebar;
		Ref<StyleBox> titlebar_selected;

		Ref<Texture2D> resizer;
		Color resizer_color;
	} theme_cache;

private:
	String title;

	HBoxContainer *titlebar_hbox = nullptr;
	Label *title_label = nullptr;

	bool autoshrink_enabled = true;
	int autoshrink_margin = 40;
	int drag_margin = 16;

	bool tint_color_enabled = false;
	Color tint_color = Color(0.3, 0.3, 0.3, 0.75);

protected:
	virtual void gui_input(const Ref<InputEvent> &p_event) override;
	virtual CursorShape get_cursor_shape(const Point2 &p_pos = Point2i()) const override;

	void _notification(int p_what);
	static void _bind_methods();

	void _validate_property(PropertyInfo &p_property) const;

	virtual void _resort() override;

public:
	void set_title(const String &p_title);
	String get_title() const;

	void set_autoshrink_enabled(bool p_enable);
	bool is_autoshrink_enabled() const;

	void set_autoshrink_margin(const int &p_margin);
	int get_autoshrink_margin() const;

	HBoxContainer *get_titlebar_hbox();
	Size2 get_titlebar_size() const;

	void set_drag_margin(int p_margin);
	int get_drag_margin() const;

	void set_tint_color_enabled(bool p_enable);
	bool is_tint_color_enabled() const;

	void set_tint_color(const Color &p_tint_color);
	Color get_tint_color() const;

	virtual bool has_point(const Point2 &p_point) const override;
	virtual Size2 get_minimum_size() const override;

	GraphFrame();
};

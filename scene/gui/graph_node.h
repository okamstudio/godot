/**************************************************************************/
/*  graph_node.h                                                          */
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

class GraphNode : public GraphElement {
	GDCLASS(GraphNode, GraphElement);

	friend class GraphEdit;

	struct Slot {
		bool enable_left = false;
		int type_left = 0;
		Color color_left = Color(1, 1, 1, 1);
		Ref<Texture2D> custom_port_icon_left;

		bool enable_right = false;
		int type_right = 0;
		Color color_right = Color(1, 1, 1, 1);
		Ref<Texture2D> custom_port_icon_right;

		bool draw_stylebox = true;
	};

	struct PortCache {
		Vector2 pos;
		int slot_index;
		int type = 0;
		Color color;
	};

	struct _MinSizeCache {
		int min_size = 0;
		bool will_stretch = false;
		int final_size = 0;
	};

	HBoxContainer *titlebar_hbox = nullptr;
	Label *title_label = nullptr;

	String title;

	Vector<PortCache> left_port_cache;
	Vector<PortCache> right_port_cache;

	HashMap<int, Slot> slot_table;
	Vector<int> slot_y_cache;

	struct ThemeCache {
		Ref<StyleBox> panel;
		Ref<StyleBox> panel_selected;
		Ref<StyleBox> titlebar;
		Ref<StyleBox> titlebar_selected;
		Ref<StyleBox> slot;

		int separation = 0;
		int port_h_offset = 0;

		Ref<Texture2D> port;
		Ref<Texture2D> resizer;
		Color resizer_color;
	} theme_cache;

	bool port_pos_dirty = true;

	bool ignore_invalid_connection_type = false;

	void _port_pos_update();

protected:
	void _notification(int p_what);
	static void _bind_methods();

	virtual void _resort() override;

	virtual void draw_port(int p_slot_index, Point2i p_pos, bool p_left, const Color &p_color);
	GDVIRTUAL4(_draw_port, int, Point2i, bool, const Color &);

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:
	void set_title(const String &p_title);
	String get_title() const;

	HBoxContainer *get_titlebar_hbox();

	void set_slot(int p_slot_index, bool p_enable_left, int p_type_left, const Color &p_color_left, bool p_enable_right, int p_type_right, const Color &p_color_right, const Ref<Texture2D> &p_custom_left = Ref<Texture2D>(), const Ref<Texture2D> &p_custom_right = Ref<Texture2D>(), bool p_draw_stylebox = true);
	void clear_slot(int p_slot_index);
	void clear_all_slots();

	bool is_slot_enabled_left(int p_slot_index) const;
	void set_slot_enabled_left(int p_slot_index, bool p_enable);

	void set_slot_type_left(int p_slot_index, int p_type);
	int get_slot_type_left(int p_slot_index) const;

	void set_slot_color_left(int p_slot_index, const Color &p_color);
	Color get_slot_color_left(int p_slot_index) const;

	void set_slot_custom_icon_left(int p_slot_index, const Ref<Texture2D> &p_custom_icon);
	Ref<Texture2D> get_slot_custom_icon_left(int p_slot_index) const;

	bool is_slot_enabled_right(int p_slot_index) const;
	void set_slot_enabled_right(int p_slot_index, bool p_enable);

	void set_slot_type_right(int p_slot_index, int p_type);
	int get_slot_type_right(int p_slot_index) const;

	void set_slot_color_right(int p_slot_index, const Color &p_color);
	Color get_slot_color_right(int p_slot_index) const;

	void set_slot_custom_icon_right(int p_slot_index, const Ref<Texture2D> &p_custom_icon);
	Ref<Texture2D> get_slot_custom_icon_right(int p_slot_index) const;

	bool is_slot_draw_stylebox(int p_slot_index) const;
	void set_slot_draw_stylebox(int p_slot_index, bool p_enable);

	void set_ignore_invalid_connection_type(bool p_ignore);
	bool is_ignoring_valid_connection_type() const;

	int get_input_port_count();
	Vector2 get_input_port_position(int p_port_idx);
	int get_input_port_type(int p_port_idx);
	Color get_input_port_color(int p_port_idx);
	int get_input_port_slot(int p_port_idx);

	int get_output_port_count();
	Vector2 get_output_port_position(int p_port_idx);
	int get_output_port_type(int p_port_idx);
	Color get_output_port_color(int p_port_idx);
	int get_output_port_slot(int p_port_idx);

	virtual Size2 get_minimum_size() const override;

	virtual CursorShape get_cursor_shape(const Point2 &p_pos = Point2i()) const override;

	virtual Vector<int> get_allowed_size_flags_horizontal() const override;
	virtual Vector<int> get_allowed_size_flags_vertical() const override;

	GraphNode();
};

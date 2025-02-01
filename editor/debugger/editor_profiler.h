/**************************************************************************/
/*  editor_profiler.h                                                     */
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

#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_button.h"
#include "scene/gui/label.h"
#include "scene/gui/option_button.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/split_container.h"
#include "scene/gui/texture_rect.h"
#include "scene/gui/tree.h"

class ImageTexture;

class EditorProfiler : public VBoxContainer {
	GDCLASS(EditorProfiler, VBoxContainer);

public:
	struct Metric {
		bool valid = false;

		int frame_number = 0;
		float frame_time = 0;
		float process_time = 0;
		float physics_time = 0;
		float physics_frame_time = 0;

		struct Category {
			StringName signature;
			String name;
			float total_time = 0; //total for category

			struct Item {
				StringName signature;
				String name;
				String script;
				int line = 0;
				float self = 0;
				float total = 0;
				float internal = 0;
				int calls = 0;
			};

			Vector<Item> items;
		};

		Vector<Category> categories;

		HashMap<StringName, Category *> category_ptrs;
		HashMap<StringName, Category::Item *> item_ptrs;
	};

	enum DisplayMode {
		DISPLAY_FRAME_TIME,
		DISPLAY_AVERAGE_TIME,
		DISPLAY_FRAME_PERCENT,
		DISPLAY_PHYSICS_FRAME_PERCENT,
	};

	enum DisplayTime {
		DISPLAY_TOTAL_TIME,
		DISPLAY_SELF_TIME,
	};

private:
	struct ThemeCache {
		Color seek_line_color;
		Color seek_line_hover_color;
	} theme_cache;

	Button *activate = nullptr;
	Button *clear_button = nullptr;
	TextureRect *graph = nullptr;
	Ref<ImageTexture> graph_texture;
	Vector<uint8_t> graph_image;

	float graph_zoom = 0.0f;
	float pan_accumulator = 0.0f;
	int zoom_center = -1;

	Tree *variables = nullptr;
	HSplitContainer *h_split = nullptr;

	HashSet<StringName> plot_sigs;

	OptionButton *display_mode = nullptr;
	OptionButton *display_time = nullptr;

	CheckButton *display_internal_profiles = nullptr;

	SpinBox *cursor_metric_edit = nullptr;

	Vector<Metric> frame_metrics;
	int total_metrics = 0;
	int last_metric = -1;

	int max_functions = 0;

	bool updating_frame = false;

	int hover_metric = -1;

	float graph_height = 1.0f;

	bool seeking = false;

	Timer *frame_delay = nullptr;
	Timer *plot_delay = nullptr;

	void _update_button_text();
	void _update_frame();

	void _activate_pressed();
	void _clear_pressed();
	void _autostart_toggled(bool p_toggled_on);

	void _internal_profiles_pressed();

	String _get_time_as_text(const Metric &m, float p_time, int p_calls);

	void _make_metric_ptrs(Metric &m);
	void _item_edited();

	void _update_plot();

	void _graph_tex_mouse_exit();

	void _graph_tex_draw();
	void _graph_tex_input(const Ref<InputEvent> &p_ev);

	Color _get_color_from_signature(const StringName &p_signature) const;
	int _get_zoom_left_border() const;

	void _cursor_metric_changed(double);

	void _combo_changed(int);

	Metric _get_frame_metric(int index);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void add_frame_metric(const Metric &p_metric, bool p_final = false);
	void set_enabled(bool p_enable, bool p_clear = true);
	void set_profiling(bool p_pressed);
	bool is_profiling();
	bool is_seeking() { return seeking; }
	void disable_seeking();

	void clear();

	Vector<Vector<String>> get_data_as_csv() const;

	EditorProfiler();
};

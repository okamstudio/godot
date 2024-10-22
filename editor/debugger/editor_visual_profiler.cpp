/**************************************************************************/
/*  editor_visual_profiler.cpp                                            */
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

#include "editor_visual_profiler.h"

#include "core/io/image.h"
#include "core/os/os.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/resources/image_texture.h"

void EditorVisualProfiler::add_frame_metric(const Metric &p_metric) {
	++last_metric;
	if (last_metric >= frame_metrics.size()) {
		last_metric = 0;
	}

	frame_metrics.write[last_metric] = p_metric;

	List<String> stack;
	for (int i = 0; i < frame_metrics[last_metric].areas.size(); i++) {
		String name = frame_metrics[last_metric].areas[i].name;
		frame_metrics.write[last_metric].areas.write[i].color_cache = _get_color_from_signature(name);
		String full_name;

		if (name[0] == '<') {
			stack.pop_back();
		}

		if (stack.size()) {
			full_name = stack.back()->get() + name;
		} else {
			full_name = name;
		}

		if (name[0] == '>') {
			stack.push_back(full_name + "/");
		}

		frame_metrics.write[last_metric].areas.write[i].fullpath_cache = full_name;
	}

	updating_frame = true;
	clear_button->set_disabled(false);
	cursor_metric_edit->set_max(frame_metrics[last_metric].frame_number);
	cursor_metric_edit->set_min(MAX(int64_t(frame_metrics[last_metric].frame_number) - frame_metrics.size(), 0));

	if (!seeking) {
		cursor_metric_edit->set_value(frame_metrics[last_metric].frame_number);
		if (hover_metric != -1) {
			hover_metric++;
			if (hover_metric >= frame_metrics.size()) {
				hover_metric = 0;
			}
		}
	}
	updating_frame = false;

	if (frame_delay->is_stopped()) {
		frame_delay->set_wait_time(0.1);
		frame_delay->start();
	}

	if (plot_delay->is_stopped()) {
		plot_delay->set_wait_time(0.1);
		plot_delay->start();
	}
}

void EditorVisualProfiler::clear() {
	int metric_size = EDITOR_GET("debugger/profiler_frame_history_size");
	metric_size = CLAMP(metric_size, 60, 10000);
	frame_metrics.clear();
	frame_metrics.resize(metric_size);
	last_metric = -1;
	variables->clear();
	//activate->set_pressed(false);

	graph_limit = 1000.0f / CLAMP(int(EDITOR_GET("debugger/profiler_target_fps")), 1, 1000);

	updating_frame = true;
	cursor_metric_edit->set_min(0);
	cursor_metric_edit->set_max(0);
	cursor_metric_edit->set_value(0);
	updating_frame = false;
	hover_metric = -1;
	seeking = false;
}

String EditorVisualProfiler::_get_time_as_text(float p_time) {
	int dmode = display_mode->get_selected();

	if (dmode == DISPLAY_FRAME_TIME) {
		return TS->format_number(String::num(p_time, 2)) + " " + TTR("ms");
	} else if (dmode == DISPLAY_FRAME_PERCENT) {
		return TS->format_number(String::num(p_time * 100 / graph_limit, 2)) + " " + TS->percent_sign();
	}

	return "err";
}

Color EditorVisualProfiler::_get_color_from_signature(const StringName &p_signature) const {
	Color bc = get_theme_color(SNAME("error_color"), EditorStringName(Editor));
	double rot = ABS(double(p_signature.hash()) / double(0x7FFFFFFF));
	Color c;
	c.set_hsv(rot, bc.get_s(), bc.get_v());
	return c.lerp(get_theme_color(SNAME("base_color"), EditorStringName(Editor)), 0.07);
}

void EditorVisualProfiler::_item_selected() {
	if (updating_frame) {
		return;
	}

	TreeItem *item = variables->get_selected();
	if (!item) {
		return;
	}
	selected_area = item->get_metadata(0);
	_update_plot();
}

void EditorVisualProfiler::_update_plot() {
	const int w = graph->get_size().width + 1; // `+1` is to prevent from crashing when visual profiler is auto started.
	const int h = graph->get_size().height + 1;

	bool reset_texture = false;

	const int desired_len = w * h * 4;

	if (graph_image.size() != desired_len) {
		reset_texture = true;
		graph_image.resize(desired_len);
	}

	uint8_t *wr = graph_image.ptrw();
	const Color background_color = get_theme_color("dark_color_2", EditorStringName(Editor));

	// Clear the previous frame and set the background color.
	for (int i = 0; i < desired_len; i += 4) {
		wr[i + 0] = Math::fast_ftoi(background_color.r * 255);
		wr[i + 1] = Math::fast_ftoi(background_color.g * 255);
		wr[i + 2] = Math::fast_ftoi(background_color.b * 255);
		wr[i + 3] = 255;
	}

	//find highest value

	float highest_cpu = 0;
	float highest_gpu = 0;

	for (int i = 0; i < frame_metrics.size(); i++) {
		const Metric &m = frame_metrics[i];
		if (!m.valid) {
			continue;
		}

		if (m.areas.size()) {
			highest_cpu = MAX(highest_cpu, m.areas[m.areas.size() - 1].cpu_time);
			highest_gpu = MAX(highest_gpu, m.areas[m.areas.size() - 1].gpu_time);
		}
	}

	if (highest_cpu > 0 || highest_gpu > 0) {
		if (frame_relative->is_pressed()) {
			highest_cpu = MAX(graph_limit, highest_cpu);
			highest_gpu = MAX(graph_limit, highest_gpu);
		}

		if (linked->is_pressed()) {
			float highest = MAX(highest_cpu, highest_gpu);
			highest_cpu = highest_gpu = highest;
		}

		//means some data exists..
		highest_cpu *= 1.2; //leave some upper room
		highest_gpu *= 1.2; //leave some upper room
		graph_height_cpu = highest_cpu;
		graph_height_gpu = highest_gpu;

		Vector<Color> columnv_cpu;
		columnv_cpu.resize(h);
		Color *column_cpu = columnv_cpu.ptrw();

		Vector<Color> columnv_gpu;
		columnv_gpu.resize(h);
		Color *column_gpu = columnv_gpu.ptrw();

		int half_w = w / 2;
		for (int i = 0; i < half_w; i++) {
			for (int j = 0; j < h; j++) {
				column_cpu[j] = Color(0, 0, 0, 0);
				column_gpu[j] = Color(0, 0, 0, 0);
			}

			int current = i * frame_metrics.size() / half_w;
			int next = (i + 1) * frame_metrics.size() / half_w;
			if (next > frame_metrics.size()) {
				next = frame_metrics.size();
			}
			if (next == current) {
				next = current + 1; //just because for loop must work
			}

			for (int j = current; j < next; j++) {
				//wrap
				int idx = last_metric + 1 + j;
				while (idx >= frame_metrics.size()) {
					idx -= frame_metrics.size();
				}

				int area_count = frame_metrics[idx].areas.size();
				const Metric::Area *areas = frame_metrics[idx].areas.ptr();
				int prev_cpu = 0;
				int prev_gpu = 0;
				for (int k = 1; k < area_count; k++) {
					int ofs_cpu = int(areas[k].cpu_time * h / highest_cpu);
					ofs_cpu = CLAMP(ofs_cpu, 0, h - 1);
					Color color = selected_area == areas[k - 1].fullpath_cache ? Color(1, 1, 1, 1) : areas[k - 1].color_cache;

					for (int l = prev_cpu; l < ofs_cpu; l++) {
						column_cpu[h - l - 1] += color;
					}
					prev_cpu = ofs_cpu;

					int ofs_gpu = int(areas[k].gpu_time * h / highest_gpu);
					ofs_gpu = CLAMP(ofs_gpu, 0, h - 1);
					for (int l = prev_gpu; l < ofs_gpu; l++) {
						column_gpu[h - l - 1] += color;
					}

					prev_gpu = ofs_gpu;
				}
			}

			//plot CPU
			for (int j = 0; j < h; j++) {
				uint8_t r, g, b;

				if (column_cpu[j].a == 0) {
					r = Math::fast_ftoi(background_color.r * 255);
					g = Math::fast_ftoi(background_color.g * 255);
					b = Math::fast_ftoi(background_color.b * 255);
				} else {
					r = CLAMP((column_cpu[j].r / column_cpu[j].a) * 255.0, 0, 255);
					g = CLAMP((column_cpu[j].g / column_cpu[j].a) * 255.0, 0, 255);
					b = CLAMP((column_cpu[j].b / column_cpu[j].a) * 255.0, 0, 255);
				}

				int widx = (j * w + i) * 4;
				wr[widx + 0] = r;
				wr[widx + 1] = g;
				wr[widx + 2] = b;
				wr[widx + 3] = 255;
			}
			//plot GPU
			for (int j = 0; j < h; j++) {
				uint8_t r, g, b;

				if (column_gpu[j].a == 0) {
					r = Math::fast_ftoi(background_color.r * 255);
					g = Math::fast_ftoi(background_color.g * 255);
					b = Math::fast_ftoi(background_color.b * 255);
				} else {
					r = CLAMP((column_gpu[j].r / column_gpu[j].a) * 255.0, 0, 255);
					g = CLAMP((column_gpu[j].g / column_gpu[j].a) * 255.0, 0, 255);
					b = CLAMP((column_gpu[j].b / column_gpu[j].a) * 255.0, 0, 255);
				}

				int widx = (j * w + w / 2 + i) * 4;
				wr[widx + 0] = r;
				wr[widx + 1] = g;
				wr[widx + 2] = b;
				wr[widx + 3] = 255;
			}
		}
	}

	Ref<Image> img = Image::create_from_data(w, h, false, Image::FORMAT_RGBA8, graph_image);

	if (reset_texture) {
		if (graph_texture.is_null()) {
			graph_texture.instantiate();
		}
		graph_texture->set_image(img);
	}

	graph_texture->update(img);

	graph->set_texture(graph_texture);
	graph->queue_redraw();
}

void EditorVisualProfiler::_update_frame(bool p_focus_selected) {
	int cursor_metric = _get_cursor_index();

	Ref<Texture> track_icon = get_editor_theme_icon(SNAME("TrackColor"));

	ERR_FAIL_INDEX(cursor_metric, frame_metrics.size());

	updating_frame = true;
	variables->clear();

	TreeItem *root = variables->create_item();
	const Metric &m = frame_metrics[cursor_metric];

	List<TreeItem *> stack;
	List<TreeItem *> categories;

	TreeItem *ensure_selected = nullptr;

	for (int i = 1; i < m.areas.size() - 1; i++) {
		TreeItem *parent = stack.size() ? stack.back()->get() : root;

		String name = m.areas[i].name;

		float cpu_time = m.areas[i].cpu_time;
		float gpu_time = m.areas[i].gpu_time;
		if (i < m.areas.size() - 1) {
			cpu_time = m.areas[i + 1].cpu_time - cpu_time;
			gpu_time = m.areas[i + 1].gpu_time - gpu_time;
		}

		if (name.begins_with(">")) {
			TreeItem *category = variables->create_item(parent);

			stack.push_back(category);
			categories.push_back(category);

			name = name.substr(1, name.length());

			category->set_text(0, name);
			category->set_metadata(1, cpu_time);
			category->set_metadata(2, gpu_time);
			continue;
		}

		if (name.begins_with("<")) {
			stack.pop_back();
			continue;
		}
		TreeItem *category = variables->create_item(parent);

		for (TreeItem *E : stack) {
			float total_cpu = E->get_metadata(1);
			float total_gpu = E->get_metadata(2);
			total_cpu += cpu_time;
			total_gpu += gpu_time;
			E->set_metadata(1, total_cpu);
			E->set_metadata(2, total_gpu);
		}

		category->set_icon(0, track_icon);
		category->set_icon_modulate(0, m.areas[i].color_cache);
		category->set_selectable(0, true);
		category->set_metadata(0, m.areas[i].fullpath_cache);
		category->set_text(0, m.areas[i].name);
		category->set_text(1, _get_time_as_text(cpu_time));
		category->set_metadata(1, m.areas[i].cpu_time);
		category->set_text(2, _get_time_as_text(gpu_time));
		category->set_metadata(2, m.areas[i].gpu_time);

		if (selected_area == m.areas[i].fullpath_cache) {
			category->select(0);
			if (p_focus_selected) {
				ensure_selected = category;
			}
		}
	}

	for (TreeItem *E : categories) {
		float total_cpu = E->get_metadata(1);
		float total_gpu = E->get_metadata(2);
		E->set_text(1, _get_time_as_text(total_cpu));
		E->set_text(2, _get_time_as_text(total_gpu));
	}

	if (ensure_selected) {
		variables->ensure_cursor_is_visible();
	}
	updating_frame = false;
}

void EditorVisualProfiler::_activate_pressed() {
	if (activate->is_pressed()) {
		activate->set_icon(get_editor_theme_icon(SNAME("Stop")));
		activate->set_text(TTR("Stop"));
		_clear_pressed(); //always clear on start
		clear_button->set_disabled(false);
	} else {
		activate->set_icon(get_editor_theme_icon(SNAME("Play")));
		activate->set_text(TTR("Start"));
	}
	emit_signal(SNAME("enable_profiling"), activate->is_pressed());
}

void EditorVisualProfiler::_clear_pressed() {
	clear_button->set_disabled(true);
	clear();
	_update_plot();
}

void EditorVisualProfiler::_autostart_toggled(bool p_toggled_on) {
	EditorSettings::get_singleton()->set_project_metadata("debug_options", "autostart_visual_profiler", p_toggled_on);
}

void EditorVisualProfiler::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
		case NOTIFICATION_THEME_CHANGED:
		case NOTIFICATION_TRANSLATION_CHANGED: {
			activate->set_icon(get_editor_theme_icon(SNAME("Play")));
			clear_button->set_icon(get_editor_theme_icon(SNAME("Clear")));
		} break;
	}
}

void EditorVisualProfiler::_graph_tex_draw() {
	if (last_metric < 0) {
		return;
	}

	Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
	int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
	const Color color = get_theme_color(SceneStringName(font_color), EditorStringName(Editor));

	if (seeking) {
		int max_frames = frame_metrics.size();
		int frame = cursor_metric_edit->get_value() - (frame_metrics[last_metric].frame_number - max_frames + 1);
		if (frame < 0) {
			frame = 0;
		}

		int half_width = graph->get_size().x / 2;
		int cur_x = frame * half_width / max_frames;

		graph->draw_line(Vector2(cur_x, 0), Vector2(cur_x, graph->get_size().y), color * Color(1, 1, 1));
		graph->draw_line(Vector2(cur_x + half_width, 0), Vector2(cur_x + half_width, graph->get_size().y), color * Color(1, 1, 1));
	}

	if (graph_height_cpu > 0) {
		int frame_y = graph->get_size().y - graph_limit * graph->get_size().y / graph_height_cpu - 1;

		int half_width = graph->get_size().x / 2;

		graph->draw_line(Vector2(0, frame_y), Vector2(half_width, frame_y), color * Color(1, 1, 1, 0.5));

		const String limit_str = String::num(graph_limit, 2) + " ms";
		graph->draw_string(font, Vector2(half_width - font->get_string_size(limit_str, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x - 2, frame_y - 2), limit_str, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, color * Color(1, 1, 1, 0.75));
	}

	if (graph_height_gpu > 0) {
		int frame_y = graph->get_size().y - graph_limit * graph->get_size().y / graph_height_gpu - 1;

		int half_width = graph->get_size().x / 2;

		graph->draw_line(Vector2(half_width, frame_y), Vector2(graph->get_size().x, frame_y), color * Color(1, 1, 1, 0.5));

		const String limit_str = String::num(graph_limit, 2) + " ms";
		graph->draw_string(font, Vector2(half_width * 2 - font->get_string_size(limit_str, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x - 2, frame_y - 2), limit_str, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, color * Color(1, 1, 1, 0.75));
	}

	graph->draw_string(font, Vector2(font->get_string_size("X", HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x, font->get_ascent(font_size) + 2), "CPU:", HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, color * Color(1, 1, 1));
	graph->draw_string(font, Vector2(font->get_string_size("X", HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x + graph->get_size().width / 2, font->get_ascent(font_size) + 2), "GPU:", HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, color * Color(1, 1, 1));
}

void EditorVisualProfiler::_graph_tex_mouse_exit() {
	hover_metric = -1;
	graph->queue_redraw();
}

void EditorVisualProfiler::_cursor_metric_changed(double) {
	if (updating_frame) {
		return;
	}

	graph->queue_redraw();
	_update_frame();
}

void EditorVisualProfiler::_graph_tex_input(const Ref<InputEvent> &p_ev) {
	if (last_metric < 0) {
		return;
	}

	Ref<InputEventMouse> me = p_ev;
	Ref<InputEventMouseButton> mb = p_ev;
	Ref<InputEventMouseMotion> mm = p_ev;

	if (
			(mb.is_valid() && mb->get_button_index() == MouseButton::LEFT && mb->is_pressed()) ||
			(mm.is_valid())) {
		int half_w = graph->get_size().width / 2;
		int x = me->get_position().x;
		if (x > half_w) {
			x -= half_w;
		}
		x = x * frame_metrics.size() / half_w;

		bool show_hover = x >= 0 && x < frame_metrics.size();

		if (x < 0) {
			x = 0;
		}

		if (x >= frame_metrics.size()) {
			x = frame_metrics.size() - 1;
		}

		int metric = frame_metrics.size() - x - 1;
		metric = last_metric - metric;
		while (metric < 0) {
			metric += frame_metrics.size();
		}

		if (show_hover) {
			hover_metric = metric;

		} else {
			hover_metric = -1;
		}

		if (mb.is_valid() || mm->get_button_mask().has_flag(MouseButtonMask::LEFT)) {
			//cursor_metric=x;
			updating_frame = true;

			//metric may be invalid, so look for closest metric that is valid, this makes snap feel better
			bool valid = false;
			for (int i = 0; i < frame_metrics.size(); i++) {
				if (frame_metrics[metric].valid) {
					valid = true;
					break;
				}

				metric++;
				if (metric >= frame_metrics.size()) {
					metric = 0;
				}
			}

			if (!valid) {
				return;
			}

			cursor_metric_edit->set_value(frame_metrics[metric].frame_number);

			updating_frame = false;

			if (activate->is_pressed()) {
				if (!seeking) {
					// Break request is not required, just stop profiling
				}
			}

			seeking = true;

			if (!frame_delay->is_processing()) {
				frame_delay->set_wait_time(0.1);
				frame_delay->start();
			}

			bool touched_cpu = me->get_position().x < graph->get_size().width * 0.5;

			const Metric::Area *areas = frame_metrics[metric].areas.ptr();
			int area_count = frame_metrics[metric].areas.size();
			float posy = (1.0 - (me->get_position().y / graph->get_size().height)) * (touched_cpu ? graph_height_cpu : graph_height_gpu);
			int last_valid = -1;
			bool found = false;
			for (int i = 0; i < area_count - 1; i++) {
				if (areas[i].name[0] != '<' && areas[i].name[0] != '>') {
					last_valid = i;
				}
				float h = touched_cpu ? areas[i + 1].cpu_time : areas[i + 1].gpu_time;

				if (h > posy) {
					found = true;
					break;
				}
			}

			StringName area_found;
			if (found && last_valid != -1) {
				area_found = areas[last_valid].fullpath_cache;
			}

			if (area_found != selected_area) {
				selected_area = area_found;
				_update_frame(true);
				_update_plot();
			}
		}

		graph->queue_redraw();
	}
}

int EditorVisualProfiler::_get_cursor_index() const {
	if (last_metric < 0) {
		return 0;
	}
	if (!frame_metrics[last_metric].valid) {
		return 0;
	}

	int diff = (frame_metrics[last_metric].frame_number - cursor_metric_edit->get_value());

	int idx = last_metric - diff;
	while (idx < 0) {
		idx += frame_metrics.size();
	}

	return idx;
}

void EditorVisualProfiler::disable_seeking() {
	seeking = false;
	graph->queue_redraw();
}

void EditorVisualProfiler::_combo_changed(int) {
	_update_frame();
	_update_plot();
}

void EditorVisualProfiler::_bind_methods() {
	ADD_SIGNAL(MethodInfo("enable_profiling", PropertyInfo(Variant::BOOL, "enable")));
}

void EditorVisualProfiler::_update_button_text() {
	if (activate->is_pressed()) {
		activate->set_icon(get_editor_theme_icon(SNAME("Stop")));
		activate->set_text(TTR("Stop"));
	} else {
		activate->set_icon(get_editor_theme_icon(SNAME("Play")));
		activate->set_text(TTR("Start"));
	}
}

void EditorVisualProfiler::set_enabled(bool p_enable) {
	activate->set_disabled(!p_enable);
}

void EditorVisualProfiler::set_profiling(bool p_profiling) {
	activate->set_pressed(p_profiling);
	_update_button_text();
	emit_signal(SNAME("enable_profiling"), activate->is_pressed());
}

bool EditorVisualProfiler::is_profiling() {
	return activate->is_pressed();
}

Vector<Vector<String>> EditorVisualProfiler::get_data_as_csv() const {
	Vector<Vector<String>> res;
#if 0
	if (frame_metrics.is_empty()) {
		return res;
	}

	// signatures
	Vector<String> signatures;
	const Vector<EditorFrameProfiler::Metric::Category> &categories = frame_metrics[0].categories;

	for (int j = 0; j < categories.size(); j++) {
		const EditorFrameProfiler::Metric::Category &c = categories[j];
		signatures.push_back(c.signature);

		for (int k = 0; k < c.items.size(); k++) {
			signatures.push_back(c.items[k].signature);
		}
	}
	res.push_back(signatures);

	// values
	Vector<String> values;
	values.resize(signatures.size());

	int index = last_metric;

	for (int i = 0; i < frame_metrics.size(); i++) {
		++index;

		if (index >= frame_metrics.size()) {
			index = 0;
		}

		if (!frame_metrics[index].valid) {
			continue;
		}
		int it = 0;
		const Vector<EditorFrameProfiler::Metric::Category> &frame_cat = frame_metrics[index].categories;

		for (int j = 0; j < frame_cat.size(); j++) {
			const EditorFrameProfiler::Metric::Category &c = frame_cat[j];
			values.write[it++] = String::num_real(c.total_time);

			for (int k = 0; k < c.items.size(); k++) {
				values.write[it++] = String::num_real(c.items[k].total);
			}
		}
		res.push_back(values);
	}
#endif
	return res;
}

EditorVisualProfiler::EditorVisualProfiler() {
	HBoxContainer *hb = memnew(HBoxContainer);
	add_child(hb);
	activate = memnew(Button);
	activate->set_toggle_mode(true);
	activate->set_disabled(true);
	activate->set_text(TTR("Start"));
	activate->connect(SceneStringName(pressed), callable_mp(this, &EditorVisualProfiler::_activate_pressed));
	hb->add_child(activate);

	clear_button = memnew(Button);
	clear_button->set_text(TTR("Clear"));
	clear_button->set_disabled(true);
	clear_button->connect(SceneStringName(pressed), callable_mp(this, &EditorVisualProfiler::_clear_pressed));
	hb->add_child(clear_button);

	CheckBox *autostart_checkbox = memnew(CheckBox);
	autostart_checkbox->set_text(TTR("Autostart"));
	autostart_checkbox->set_pressed(EditorSettings::get_singleton()->get_project_metadata("debug_options", "autostart_visual_profiler", false));
	autostart_checkbox->connect(SceneStringName(toggled), callable_mp(this, &EditorVisualProfiler::_autostart_toggled));
	hb->add_child(autostart_checkbox);

	hb->add_child(memnew(Label(TTR("Measure:"))));

	display_mode = memnew(OptionButton);
	display_mode->add_item(TTR("Frame Time (ms)"));
	display_mode->add_item(TTR("Frame %"));
	display_mode->connect(SceneStringName(item_selected), callable_mp(this, &EditorVisualProfiler::_combo_changed));

	hb->add_child(display_mode);

	frame_relative = memnew(CheckBox(TTR("Fit to Frame")));
	frame_relative->set_pressed(true);
	hb->add_child(frame_relative);
	frame_relative->connect(SceneStringName(pressed), callable_mp(this, &EditorVisualProfiler::_update_plot));
	linked = memnew(CheckBox(TTR("Linked")));
	linked->set_pressed(true);
	hb->add_child(linked);
	linked->connect(SceneStringName(pressed), callable_mp(this, &EditorVisualProfiler::_update_plot));

	hb->add_spacer();

	hb->add_child(memnew(Label(TTR("Frame #:"))));

	cursor_metric_edit = memnew(SpinBox);
	cursor_metric_edit->set_h_size_flags(SIZE_FILL);
	hb->add_child(cursor_metric_edit);
	cursor_metric_edit->connect(SceneStringName(value_changed), callable_mp(this, &EditorVisualProfiler::_cursor_metric_changed));

	hb->add_theme_constant_override("separation", 8 * EDSCALE);

	h_split = memnew(HSplitContainer);
	add_child(h_split);
	h_split->set_v_size_flags(SIZE_EXPAND_FILL);

	variables = memnew(Tree);
	variables->set_custom_minimum_size(Size2(300, 0) * EDSCALE);
	variables->set_hide_folding(true);
	h_split->add_child(variables);
	variables->set_hide_root(true);
	variables->set_columns(3);
	variables->set_column_titles_visible(true);
	variables->set_column_title(0, TTR("Name"));
	variables->set_column_expand(0, true);
	variables->set_column_clip_content(0, true);
	variables->set_column_custom_minimum_width(0, 60);
	variables->set_column_title(1, TTR("CPU"));
	variables->set_column_expand(1, false);
	variables->set_column_clip_content(1, true);
	variables->set_column_custom_minimum_width(1, 75 * EDSCALE);
	variables->set_column_title(2, TTR("GPU"));
	variables->set_column_expand(2, false);
	variables->set_column_clip_content(2, true);
	variables->set_column_custom_minimum_width(2, 75 * EDSCALE);
	variables->connect("cell_selected", callable_mp(this, &EditorVisualProfiler::_item_selected));

	graph = memnew(TextureRect);
	graph->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	graph->set_mouse_filter(MOUSE_FILTER_STOP);
	graph->connect(SceneStringName(draw), callable_mp(this, &EditorVisualProfiler::_graph_tex_draw));
	graph->connect(SceneStringName(gui_input), callable_mp(this, &EditorVisualProfiler::_graph_tex_input));
	graph->connect(SceneStringName(mouse_exited), callable_mp(this, &EditorVisualProfiler::_graph_tex_mouse_exit));

	h_split->add_child(graph);
	graph->set_h_size_flags(SIZE_EXPAND_FILL);

	int metric_size = CLAMP(int(EDITOR_GET("debugger/profiler_frame_history_size")), 60, 10000);
	frame_metrics.resize(metric_size);

	graph_limit = 1000.0f / CLAMP(int(EDITOR_GET("debugger/profiler_target_fps")), 1, 1000);

	frame_delay = memnew(Timer);
	frame_delay->set_wait_time(0.1);
	frame_delay->set_one_shot(true);
	add_child(frame_delay);
	frame_delay->connect("timeout", callable_mp(this, &EditorVisualProfiler::_update_frame).bind(false));

	plot_delay = memnew(Timer);
	plot_delay->set_wait_time(0.1);
	plot_delay->set_one_shot(true);
	add_child(plot_delay);
	plot_delay->connect("timeout", callable_mp(this, &EditorVisualProfiler::_update_plot));
}

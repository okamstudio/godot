/**************************************************************************/
/*  editor_performance_profiler.cpp                                       */
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

#include "editor_performance_profiler.h"

#include "editor/editor_property_name_processor.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "editor/themes/editor_theme_manager.h"
#include "main/performance.h"

EditorPerformanceProfiler::Monitor::Monitor() {}

EditorPerformanceProfiler::Monitor::Monitor(const String &p_name, const String &p_base, int p_frame_index, Performance::MonitorType p_type, TreeItem *p_item) {
	type = p_type;
	item = p_item;
	frame_index = p_frame_index;
	name = p_name;
	base = p_base;
}

void EditorPerformanceProfiler::Monitor::update_value(float p_value) {
	ERR_FAIL_NULL(item);
	String label = EditorPerformanceProfiler::_create_label(p_value, type);
	String tooltip = label;
	switch (type) {
		case Performance::MONITOR_TYPE_MEMORY: {
			tooltip = label;
		} break;
		case Performance::MONITOR_TYPE_TIME: {
			tooltip = label;
		} break;
		default: {
			tooltip += " " + item->get_text(0);
		} break;
	}
	item->set_text(1, label);
	item->set_tooltip_text(1, tooltip);

	if (p_value > max) {
		max = p_value;
	}
}

void EditorPerformanceProfiler::Monitor::reset() {
	history.clear();
	max = 0.0f;
	if (item) {
		item->set_text(1, "");
		item->set_tooltip_text(1, "");
	}
}

String EditorPerformanceProfiler::_create_label(float p_value, Performance::MonitorType p_type) {
	switch (p_type) {
		case Performance::MONITOR_TYPE_QUANTITY: {
			return TS->format_number(itos(p_value));
		}
		case Performance::MONITOR_TYPE_MEMORY: {
			return String::humanize_size(p_value);
		}
		case Performance::MONITOR_TYPE_TIME: {
			return TS->format_number(rtos(p_value * 1000).pad_decimals(2)) + " " + TTR("ms");
		}
		default: {
			return TS->format_number(rtos(p_value));
		}
	}
}

void EditorPerformanceProfiler::_monitor_select() {
	monitor_draw->queue_redraw();
}

void EditorPerformanceProfiler::_monitor_draw() {
	Vector<StringName> active;
	for (const KeyValue<StringName, Monitor> &E : monitors) {
		if (E.value.item->is_checked(0)) {
			active.push_back(E.key);
		}
	}

	if (active.is_empty()) {
		info_message->show();
		return;
	}

	info_message->hide();

	Ref<StyleBox> graph_style_box = get_theme_stylebox(CoreStringName(normal), SNAME("TextEdit"));
	Ref<Font> graph_font = get_theme_font(SceneStringName(font), SNAME("TextEdit"));
	int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("TextEdit"));

	int columns = int(Math::ceil(Math::sqrt(float(active.size()))));
	int rows = int(Math::ceil(float(active.size()) / float(columns)));
	if (active.size() == 1) {
		rows = 1;
	}
	Size2i cell_size = Size2i(monitor_draw->get_size()) / Size2i(columns, rows);
	float spacing = float(POINT_SEPARATION) / float(columns);
	float value_multiplier = EditorThemeManager::is_dark_theme() ? 1.4f : 0.55f;
	float hue_shift = 1.0f / float(monitors.size());

	for (int i = 0; i < active.size(); i++) {
		Monitor &current = monitors[active[i]];
		Rect2i rect(Point2i(i % columns, i / columns) * cell_size + Point2i(MARGIN, MARGIN), cell_size - Point2i(MARGIN, MARGIN) * 2);
		monitor_draw->draw_style_box(graph_style_box, rect);

		rect.position += graph_style_box->get_offset();
		rect.size -= graph_style_box->get_minimum_size();
		Color draw_color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
		draw_color.set_hsv(Math::fmod(hue_shift * float(current.frame_index), 0.9f), draw_color.get_s() * 0.9f, draw_color.get_v() * value_multiplier, 0.6f);
		monitor_draw->draw_string(graph_font, rect.position + Point2(0, graph_font->get_ascent(font_size)), current.item->get_text(0), HORIZONTAL_ALIGNMENT_LEFT, rect.size.x, font_size, draw_color);

		draw_color.a = 0.9f;
		float value_position = rect.size.width - graph_font->get_string_size(current.item->get_text(1), HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).width;
		if (value_position < 0) {
			value_position = 0;
		}
		monitor_draw->draw_string(graph_font, rect.position + Point2(value_position, graph_font->get_ascent(font_size)), current.item->get_text(1), HORIZONTAL_ALIGNMENT_LEFT, rect.size.x, font_size, draw_color);

		rect.position.y += graph_font->get_height(font_size);
		rect.size.height -= graph_font->get_height(font_size);

		int line_count = rect.size.height / (graph_font->get_height(font_size) * 2);
		if (line_count > 5) {
			line_count = 5;
		}
		if (line_count > 0) {
			Color horizontal_line_color;
			horizontal_line_color.set_hsv(draw_color.get_h(), draw_color.get_s() * 0.5f, draw_color.get_v() * 0.5f, 0.3f);
			monitor_draw->draw_line(rect.position, rect.position + Vector2(rect.size.width, 0), horizontal_line_color, Math::round(EDSCALE));
			monitor_draw->draw_string(graph_font, rect.position + Vector2(0, graph_font->get_ascent(font_size)), _create_label(current.max, current.type), HORIZONTAL_ALIGNMENT_LEFT, rect.size.width, font_size, horizontal_line_color);

			for (int j = 0; j < line_count; j++) {
				Vector2 y_offset = Vector2(0, rect.size.height * (1.0f - float(j) / float(line_count)));
				monitor_draw->draw_line(rect.position + y_offset, rect.position + Vector2(rect.size.width, 0) + y_offset, horizontal_line_color, Math::round(EDSCALE));
				monitor_draw->draw_string(graph_font, rect.position - Vector2(0, graph_font->get_descent(font_size)) + y_offset, _create_label(current.max * float(j) / float(line_count), current.type), HORIZONTAL_ALIGNMENT_LEFT, rect.size.width, font_size, horizontal_line_color);
			}
		}

		float from = rect.size.width;
		float prev = -1.0f;
		int count = 0;
		List<float>::Element *e = current.history.front();

		while (from >= 0 && e) {
			float m = current.max;
			float h2 = 0;
			if (m != 0) {
				h2 = (e->get() / m);
			}
			h2 = (1.0f - h2) * float(rect.size.y);
			if (e != current.history.front()) {
				monitor_draw->draw_line(rect.position + Point2(from, h2), rect.position + Point2(from + spacing, prev), draw_color, Math::round(EDSCALE));
			}

			if (marker_key == active[i] && count == marker_frame) {
				Color line_color;
				line_color.set_hsv(draw_color.get_h(), draw_color.get_s() * 0.8f, draw_color.get_v(), 0.5f);
				monitor_draw->draw_line(rect.position + Point2(from, 0), rect.position + Point2(from, rect.size.y), line_color, Math::round(EDSCALE));

				String label = _create_label(e->get(), current.type);
				Size2 size = graph_font->get_string_size(label, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size);
				Vector2 text_top_left_position = Vector2(from, h2) - (size + Vector2(MARKER_MARGIN, MARKER_MARGIN));
				if (text_top_left_position.x < 0) {
					text_top_left_position.x = from + MARKER_MARGIN;
				}
				if (text_top_left_position.y < 0) {
					text_top_left_position.y = h2 + MARKER_MARGIN;
				}
				monitor_draw->draw_string(graph_font, rect.position + text_top_left_position + Point2(0, graph_font->get_ascent(font_size)), label, HORIZONTAL_ALIGNMENT_LEFT, rect.size.x, font_size, line_color);
			}
			prev = h2;
			e = e->next();
			from -= spacing;
			count++;
		}
	}
}

void EditorPerformanceProfiler::_build_monitor_tree() {
	HashSet<StringName> monitor_checked;
	for (KeyValue<StringName, Monitor> &E : monitors) {
		if (E.value.item && E.value.item->is_checked(0)) {
			monitor_checked.insert(E.key);
		}
	}

	base_map.clear();
	monitor_tree->get_root()->clear_children();

	for (KeyValue<StringName, Monitor> &E : monitors) {
		TreeItem *base = _get_monitor_base(E.value.base);
		TreeItem *item = _create_monitor_item(E.value.name, base);
		item->set_checked(0, monitor_checked.has(E.key));
		E.value.item = item;
		if (!E.value.history.is_empty()) {
			E.value.update_value(E.value.history.front()->get());
		}
	}
}

TreeItem *EditorPerformanceProfiler::_get_monitor_base(const StringName &p_base_name) {
	if (base_map.has(p_base_name)) {
		return base_map[p_base_name];
	}

	TreeItem *base = monitor_tree->create_item(monitor_tree->get_root());
	base->set_text(0, EditorPropertyNameProcessor::get_singleton()->process_name(p_base_name, EditorPropertyNameProcessor::get_settings_style()));
	base->set_editable(0, false);
	base->set_selectable(0, false);
	base->set_expand_right(0, true);
	if (is_inside_tree()) {
		base->set_custom_font(0, get_theme_font(SNAME("bold"), EditorStringName(EditorFonts)));
	}
	base_map.insert(p_base_name, base);
	return base;
}

TreeItem *EditorPerformanceProfiler::_create_monitor_item(const StringName &p_monitor_name, TreeItem *p_base) {
	TreeItem *item = monitor_tree->create_item(p_base);
	item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
	item->set_editable(0, true);
	item->set_selectable(0, false);
	item->set_selectable(1, false);
	item->set_text(0, EditorPropertyNameProcessor::get_singleton()->process_name(p_monitor_name, EditorPropertyNameProcessor::get_settings_style()));
	return item;
}

void EditorPerformanceProfiler::_marker_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		Vector<StringName> active;
		for (KeyValue<StringName, Monitor> &E : monitors) {
			if (E.value.item->is_checked(0)) {
				active.push_back(E.key);
			}
		}
		if (active.size() > 0) {
			int columns = int(Math::ceil(Math::sqrt(float(active.size()))));
			int rows = int(Math::ceil(float(active.size()) / float(columns)));
			if (active.size() == 1) {
				rows = 1;
			}
			Size2i cell_size = Size2i(monitor_draw->get_size()) / Size2i(columns, rows);
			Vector2i index = mb->get_position() / cell_size;
			Rect2i rect(index * cell_size + Point2i(MARGIN, MARGIN), cell_size - Point2i(MARGIN, MARGIN) * 2);
			if (rect.has_point(mb->get_position())) {
				if (index.x + index.y * columns < active.size()) {
					marker_key = active[index.x + index.y * columns];
				} else {
					marker_key = "";
				}
				Ref<StyleBox> graph_style_box = get_theme_stylebox(CoreStringName(normal), SNAME("TextEdit"));
				rect.position += graph_style_box->get_offset();
				rect.size -= graph_style_box->get_minimum_size();
				Vector2 point = mb->get_position() - rect.position;
				if (point.x >= rect.size.x) {
					marker_frame = 0;
				} else {
					int point_sep = 5;
					float spacing = float(point_sep) / float(columns);
					marker_frame = (rect.size.x - point.x) / spacing;
				}
				monitor_draw->queue_redraw();
				return;
			}
		}
		marker_key = "";
		monitor_draw->queue_redraw();
	}
}

void EditorPerformanceProfiler::reset() {
	HashMap<StringName, Monitor>::Iterator E = monitors.begin();
	while (E != monitors.end()) {
		HashMap<StringName, Monitor>::Iterator N = E;
		++N;
		if (String(E->key).begins_with("custom:")) {
			monitors.remove(E);
		} else {
			E->value.reset();
		}
		E = N;
	}

	_build_monitor_tree();
	marker_key = "";
	marker_frame = 0;
	monitor_draw->queue_redraw();
}

void EditorPerformanceProfiler::update_monitors(const Vector<StringName> &p_names) {
	HashMap<StringName, int> names;
	for (int i = 0; i < p_names.size(); i++) {
		names.insert("custom:" + p_names[i], Performance::MONITOR_MAX + i);
	}

	{
		HashMap<StringName, Monitor>::Iterator E = monitors.begin();
		while (E != monitors.end()) {
			HashMap<StringName, Monitor>::Iterator N = E;
			++N;
			if (String(E->key).begins_with("custom:")) {
				if (!names.has(E->key)) {
					monitors.remove(E);
				} else {
					E->value.frame_index = names[E->key];
					names.erase(E->key);
				}
			}
			E = N;
		}
	}

	for (const KeyValue<StringName, int> &E : names) {
		String name = String(E.key).replace_first("custom:", "");
		String base = "Custom";
		if (name.get_slice_count("/") == 2) {
			base = name.get_slicec('/', 0);
			name = name.get_slicec('/', 1);
		}
		monitors.insert(E.key, Monitor(name, base, E.value, Performance::MONITOR_TYPE_QUANTITY, nullptr));
	}

	_build_monitor_tree();
}

void EditorPerformanceProfiler::add_profile_frame(const Vector<float> &p_values) {
	for (KeyValue<StringName, Monitor> &E : monitors) {
		float value = 0.0f;
		if (E.value.frame_index >= 0 && E.value.frame_index < p_values.size()) {
			value = p_values[E.value.frame_index];
		}
		E.value.history.push_front(value);
		E.value.update_value(value);
	}
	marker_frame++;
	monitor_draw->queue_redraw();
}

List<float> *EditorPerformanceProfiler::get_monitor_data(const StringName &p_name) {
	if (monitors.has(p_name)) {
		return &monitors[p_name].history;
	}
	return nullptr;
}

void EditorPerformanceProfiler::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			for (KeyValue<StringName, TreeItem *> &E : base_map) {
				E.value->set_custom_font(0, get_theme_font(SNAME("bold"), EditorStringName(EditorFonts)));
			}
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (EditorSettings::get_singleton()->check_changed_settings_in_group("interface/editor/localize_settings")) {
				_build_monitor_tree();
			}
		} break;
	}
}

EditorPerformanceProfiler::EditorPerformanceProfiler() {
	set_name(TTR("Monitors"));
	set_split_offset(340 * EDSCALE);

	monitor_tree = memnew(Tree);
	monitor_tree->set_custom_minimum_size(Size2(300, 0) * EDSCALE);
	monitor_tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	monitor_tree->set_columns(2);
	monitor_tree->set_column_title(0, TTR("Monitor"));
	monitor_tree->set_column_expand(0, true);
	monitor_tree->set_column_title(1, TTR("Value"));
	monitor_tree->set_column_custom_minimum_width(1, 100 * EDSCALE);
	monitor_tree->set_column_expand(1, false);
	monitor_tree->set_column_titles_visible(true);
	monitor_tree->connect("item_edited", callable_mp(this, &EditorPerformanceProfiler::_monitor_select));
	monitor_tree->create_item();
	monitor_tree->set_hide_root(true);
	monitor_tree->set_theme_type_variation("TreeSecondary");
	add_child(monitor_tree);

	monitor_draw = memnew(Control);
	monitor_draw->set_custom_minimum_size(Size2(300, 0) * EDSCALE);
	monitor_draw->set_clip_contents(true);
	monitor_draw->connect(SceneStringName(draw), callable_mp(this, &EditorPerformanceProfiler::_monitor_draw));
	monitor_draw->connect(SceneStringName(gui_input), callable_mp(this, &EditorPerformanceProfiler::_marker_input));
	add_child(monitor_draw);

	info_message = memnew(Label);
	info_message->set_text(TTR("Pick one or more items from the list to display the graph."));
	info_message->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	info_message->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	info_message->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	info_message->set_custom_minimum_size(Size2(100 * EDSCALE, 0));
	info_message->set_anchors_and_offsets_preset(PRESET_FULL_RECT, PRESET_MODE_KEEP_SIZE, 8 * EDSCALE);
	monitor_draw->add_child(info_message);

	for (int i = 0; i < Performance::MONITOR_MAX; i++) {
		const Performance::Monitor monitor = Performance::Monitor(i);
		const String path = Performance::get_singleton()->get_monitor_name(monitor);
		const String base = path.get_slicec('/', 0);
		const String name = path.get_slicec('/', 1);
		monitors.insert(path, Monitor(name, base, i, Performance::get_singleton()->get_monitor_type(monitor), nullptr));
	}

	_build_monitor_tree();
}

/**************************************************************************/
/*  editor_quick_open_dialog.cpp                                          */
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

#include "editor_quick_open_dialog.h"

#include "core/config/project_settings.h"
#include "core/string/fuzzy_search.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/editor_paths.h"
#include "editor/editor_resource_preview.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/filesystem_dock.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/center_container.h"
#include "scene/gui/check_button.h"
#include "scene/gui/flow_container.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/texture_rect.h"
#include "scene/gui/tree.h"

void HighlightedLabel::draw_substr_rects(const Vector2i &p_substr, Vector2 p_offset, int p_line_limit, int line_spacing) {
	for (int i = get_lines_skipped(); i < p_line_limit; i++) {
		RID line = get_line_rid(i);
		Vector<Vector2> ranges = TS->shaped_text_get_selection(line, p_substr.x, p_substr.x + p_substr.y);
		Rect2 line_rect = get_line_rect(i);
		for (const Vector2 &range : ranges) {
			Rect2 rect = Rect2(Point2(range.x, 0) + line_rect.position, Size2(range.y - range.x, line_rect.size.y));
			rect.position = p_offset + line_rect.position;
			rect.position.x += range.x;
			rect.size = Size2(range.y - range.x, line_rect.size.y);
			rect.size.x = MIN(rect.size.x, line_rect.size.x - range.x);
			if (rect.size.x > 0) {
				draw_rect(rect, Color(1, 1, 1, 0.07), true);
				draw_rect(rect, Color(0.5, 0.7, 1.0, 0.4), false, 1);
			}
		}
		p_offset.y += line_spacing + TS->shaped_text_get_ascent(line) + TS->shaped_text_get_descent(line);
	}
}

void HighlightedLabel::add_highlight(const Vector2i &p_interval) {
	if (p_interval.y > 0) {
		highlights.append(p_interval);
		queue_redraw();
	}
}

void HighlightedLabel::reset_highlights() {
	highlights.clear();
	queue_redraw();
}

void HighlightedLabel::_notification(int p_notification) {
	if (p_notification == NOTIFICATION_DRAW) {
		if (highlights.is_empty()) {
			return;
		}

		Vector2 offset;
		int line_limit;
		int line_spacing;
		get_layout_data(offset, line_limit, line_spacing);

		for (const Vector2i &substr : highlights) {
			draw_substr_rects(substr, offset, line_limit, line_spacing);
		}
	}
}

EditorQuickOpenDialog::EditorQuickOpenDialog() {
	VBoxContainer *vbc = memnew(VBoxContainer);
	vbc->add_theme_constant_override("separation", 0);
	add_child(vbc);

	{
		// Search bar
		MarginContainer *mc = memnew(MarginContainer);
		mc->add_theme_constant_override("margin_top", 6);
		mc->add_theme_constant_override("margin_bottom", 6);
		mc->add_theme_constant_override("margin_left", 1);
		mc->add_theme_constant_override("margin_right", 1);
		vbc->add_child(mc);

		search_box = memnew(LineEdit);
		search_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		search_box->set_placeholder(TTR("Search files..."));
		search_box->set_clear_button_enabled(true);
		mc->add_child(search_box);
	}

	{
		container = memnew(QuickOpenResultContainer);
		container->connect("result_clicked", callable_mp(this, &EditorQuickOpenDialog::ok_pressed));
		vbc->add_child(container);
	}

	search_box->connect(SceneStringName(text_changed), callable_mp(this, &EditorQuickOpenDialog::_search_box_text_changed));
	search_box->connect(SceneStringName(gui_input), callable_mp(container, &QuickOpenResultContainer::handle_search_box_input));
	register_text_enter(search_box);
	get_ok_button()->hide();
}

String EditorQuickOpenDialog::get_dialog_title(const Vector<StringName> &p_base_types) {
	if (p_base_types.size() > 1) {
		return TTR("Select Resource");
	}

	if (p_base_types[0] == SNAME("PackedScene")) {
		return TTR("Select Scene");
	}

	return TTR("Select") + " " + p_base_types[0];
}

void EditorQuickOpenDialog::popup_dialog(const Vector<StringName> &p_base_types, const Callable &p_item_selected_callback) {
	ERR_FAIL_COND(p_base_types.is_empty());
	ERR_FAIL_COND(!p_item_selected_callback.is_valid());

	item_selected_callback = p_item_selected_callback;

	container->init(p_base_types);
	get_ok_button()->set_disabled(container->has_nothing_selected());

	set_title(get_dialog_title(p_base_types));
	popup_centered_clamped(Size2(780, 650) * EDSCALE, 0.8f);
	search_box->grab_focus();
}

void EditorQuickOpenDialog::ok_pressed() {
	item_selected_callback.call(container->get_selected());

	container->save_selected_item();
	container->cleanup();
	search_box->clear();
	hide();
}

void EditorQuickOpenDialog::cancel_pressed() {
	container->cleanup();
	search_box->clear();
}

void EditorQuickOpenDialog::_search_box_text_changed(const String &p_query) {
	container->set_query_and_update(p_query);
	get_ok_button()->set_disabled(container->has_nothing_selected());
}

//------------------------- Result Container

void style_button(Button *p_button) {
	p_button->set_flat(true);
	p_button->set_focus_mode(Control::FOCUS_NONE);
	p_button->set_default_cursor_shape(Control::CURSOR_POINTING_HAND);
}

QuickOpenResultContainer::QuickOpenResultContainer() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_theme_constant_override("separation", 0);
	history_file.instantiate();

	{
		// Results section
		panel_container = memnew(PanelContainer);
		panel_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		add_child(panel_container);

		{
			// No search results
			no_results_container = memnew(CenterContainer);
			no_results_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			no_results_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
			panel_container->add_child(no_results_container);

			no_results_label = memnew(Label);
			no_results_label->add_theme_font_size_override(SceneStringName(font_size), 24 * EDSCALE);
			no_results_container->add_child(no_results_label);
			no_results_container->hide();
		}

		{
			// Search results
			scroll_container = memnew(ScrollContainer);
			scroll_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			scroll_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
			scroll_container->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
			scroll_container->hide();
			panel_container->add_child(scroll_container);

			list = memnew(VBoxContainer);
			list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			list->hide();
			scroll_container->add_child(list);

			grid = memnew(HFlowContainer);
			grid->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			grid->set_v_size_flags(Control::SIZE_EXPAND_FILL);
			grid->add_theme_constant_override("v_separation", 18);
			grid->add_theme_constant_override("h_separation", 4);
			grid->hide();
			scroll_container->add_child(grid);

			file_context_menu = memnew(PopupMenu);
			file_context_menu->add_item(TTR("Show in FileSystem"), FILE_SHOW_IN_FILESYSTEM);
			file_context_menu->add_item(TTR("Show in File Manager"), FILE_SHOW_IN_FILE_MANAGER);
			file_context_menu->connect(SceneStringName(id_pressed), callable_mp(this, &QuickOpenResultContainer::_menu_option));
			file_context_menu->hide();
			scroll_container->add_child(file_context_menu);
		}
	}

	{
		// Selected filepath
		file_details_path = memnew(Label);
		file_details_path->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		file_details_path->set_horizontal_alignment(HorizontalAlignment::HORIZONTAL_ALIGNMENT_CENTER);
		file_details_path->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		add_child(file_details_path);
	}

	{
		// Bottom bar
		HBoxContainer *bottom_bar = memnew(HBoxContainer);
		bottom_bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		bottom_bar->set_alignment(ALIGNMENT_END);
		bottom_bar->add_theme_constant_override("separation", 3);
		add_child(bottom_bar);

		fuzzy_search_toggle = memnew(CheckButton);
		style_button(fuzzy_search_toggle);
		fuzzy_search_toggle->set_text(TTR("Fuzzy Search"));
		fuzzy_search_toggle->set_tooltip_text(TTR("Enable fuzzy matching"));
		fuzzy_search_toggle->connect(SceneStringName(toggled), callable_mp(this, &QuickOpenResultContainer::_toggle_fuzzy_search));
		bottom_bar->add_child(fuzzy_search_toggle);

		include_addons_toggle = memnew(CheckButton);
		style_button(include_addons_toggle);
		include_addons_toggle->set_text(TTR("Addons"));
		include_addons_toggle->set_tooltip_text(TTR("Include files from addons"));
		include_addons_toggle->connect(SceneStringName(toggled), callable_mp(this, &QuickOpenResultContainer::_toggle_include_addons));
		bottom_bar->add_child(include_addons_toggle);

		VSeparator *vsep = memnew(VSeparator);
		vsep->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
		vsep->set_custom_minimum_size(Size2i(0, 14 * EDSCALE));
		bottom_bar->add_child(vsep);

		display_mode_toggle = memnew(Button);
		style_button(display_mode_toggle);
		display_mode_toggle->connect(SceneStringName(pressed), callable_mp(this, &QuickOpenResultContainer::_toggle_display_mode));
		bottom_bar->add_child(display_mode_toggle);
	}
}

void QuickOpenResultContainer::_menu_option(int p_option) {
	switch (p_option) {
		case FILE_SHOW_IN_FILESYSTEM: {
			FileSystemDock::get_singleton()->navigate_to_path(get_selected());
		} break;
		case FILE_SHOW_IN_FILE_MANAGER: {
			String dir = ProjectSettings::get_singleton()->globalize_path(get_selected());
			OS::get_singleton()->shell_show_in_file_manager(dir, true);
		} break;
	}
}

void QuickOpenResultContainer::_ensure_result_vector_capacity() {
	int target_size = EDITOR_GET("filesystem/quick_open_dialog/max_results");
	int initial_size = result_items.size();
	for (int i = target_size; i < initial_size; i++) {
		result_items[i]->queue_free();
	}
	result_items.resize(target_size);
	for (int i = initial_size; i < target_size; i++) {
		QuickOpenResultItem *item = memnew(QuickOpenResultItem);
		item->connect(SceneStringName(gui_input), callable_mp(this, &QuickOpenResultContainer::_item_input).bind(i));
		result_items.write[i] = item;
		if (!never_opened) {
			_layout_result_item(item);
		}
	}
}

void QuickOpenResultContainer::init(const Vector<StringName> &p_base_types) {
	_ensure_result_vector_capacity();
	base_types = p_base_types;

	const int display_mode_behavior = EDITOR_GET("filesystem/quick_open_dialog/default_display_mode");
	const bool adaptive_display_mode = (display_mode_behavior == 0);
	const bool first_open = never_opened;

	if (adaptive_display_mode) {
		_set_display_mode(get_adaptive_display_mode(p_base_types));
	} else if (never_opened) {
		int last = EditorSettings::get_singleton()->get_project_metadata("quick_open_dialog", "last_mode", (int)QuickOpenDisplayMode::LIST);
		_set_display_mode((QuickOpenDisplayMode)last);
	}

	const bool fuzzy_matching = EDITOR_GET("filesystem/quick_open_dialog/enable_fuzzy_matching");
	const bool include_addons = EDITOR_GET("filesystem/quick_open_dialog/include_addons");
	fuzzy_search_toggle->set_pressed_no_signal(fuzzy_matching);
	include_addons_toggle->set_pressed_no_signal(include_addons);
	never_opened = false;

	const bool enable_highlights = EDITOR_GET("filesystem/quick_open_dialog/show_search_highlight");
	for (QuickOpenResultItem *E : result_items) {
		E->enable_highlights = enable_highlights;
	}

	if (first_open && history_file->load(_get_cache_file_path()) == OK) {
		// Load history when opening for the first time.
		file_type_icons.insert(SNAME("__default_icon"), get_editor_theme_icon(SNAME("Object")));

		List<String> history_keys;
		history_file->get_section_keys("selected_history", &history_keys);
		for (const String &type : history_keys) {
			const StringName type_name = type;
			const PackedStringArray paths = history_file->get_value("selected_history", type);

			Vector<QuickOpenResultCandidate> loaded_candidates;
			loaded_candidates.resize(paths.size());
			{
				QuickOpenResultCandidate *candidates_write = loaded_candidates.ptrw();
				int i = 0;
				for (const String &path : paths) {
					filetypes.insert(path, type_name);
					QuickOpenResultCandidate candidate;
					_setup_candidate(candidate, path);
					candidates_write[i] = candidate;
					i++;
				}
				selected_history.insert(type, loaded_candidates);
			}
		}
	}
	_create_initial_results();
}

void QuickOpenResultContainer::_create_initial_results() {
	file_type_icons.clear();
	file_type_icons.insert(SNAME("__default_icon"), get_editor_theme_icon(SNAME("Object")));
	filepaths.clear();
	filetypes.clear();
	_find_filepaths_in_folder(EditorFileSystem::get_singleton()->get_filesystem(), include_addons_toggle->is_pressed());
	max_total_results = MIN(filepaths.size(), result_items.size());
	update_results();
}

void QuickOpenResultContainer::_find_filepaths_in_folder(EditorFileSystemDirectory *p_directory, bool p_include_addons) {
	for (int i = 0; i < p_directory->get_subdir_count(); i++) {
		if (p_include_addons || p_directory->get_name() != "addons") {
			_find_filepaths_in_folder(p_directory->get_subdir(i), p_include_addons);
		}
	}

	for (int i = 0; i < p_directory->get_file_count(); i++) {
		String file_path = p_directory->get_file_path(i);

		const StringName engine_type = p_directory->get_file_type(i);
		const StringName script_type = p_directory->get_file_resource_script_class(i);

		const bool is_engine_type = script_type == StringName();
		const StringName &actual_type = is_engine_type ? engine_type : script_type;

		for (const StringName &parent_type : base_types) {
			bool is_valid = ClassDB::is_parent_class(engine_type, parent_type) || (!is_engine_type && EditorNode::get_editor_data().script_class_is_parent(script_type, parent_type));

			if (is_valid) {
				filepaths.append(file_path);
				filetypes.insert(file_path, actual_type);
				break; // Stop testing base types as soon as we get a match.
			}
		}
	}
}

void QuickOpenResultContainer::set_query_and_update(const String &p_query) {
	query = p_query;
	update_results();
}

void QuickOpenResultContainer::_setup_candidate(QuickOpenResultCandidate &p_candidate, const String &p_filepath) {
	p_candidate.file_path = p_filepath;
	p_candidate.result = nullptr;

	StringName actual_type;
	{
		StringName *actual_type_ptr = filetypes.lookup_ptr(p_filepath);
		if (actual_type_ptr) {
			actual_type = *actual_type_ptr;
		} else {
			ERR_PRINT(vformat("EditorQuickOpenDialog: No type for path %s.", p_filepath));
		}
	}
	EditorResourcePreview::PreviewItem item = EditorResourcePreview::get_singleton()->get_resource_preview_if_available(p_filepath);
	if (item.preview.is_valid()) {
		p_candidate.thumbnail = item.preview;
	} else if (file_type_icons.has(actual_type)) {
		p_candidate.thumbnail = *file_type_icons.lookup_ptr(actual_type);
	} else if (has_theme_icon(actual_type, EditorStringName(EditorIcons))) {
		p_candidate.thumbnail = get_editor_theme_icon(actual_type);
		file_type_icons.insert(actual_type, p_candidate.thumbnail);
	} else {
		p_candidate.thumbnail = *file_type_icons.lookup_ptr(SNAME("__default_icon"));
	}
}

void QuickOpenResultContainer::_setup_candidate(QuickOpenResultCandidate &p_candidate, const FuzzySearchResult &p_result) {
	_setup_candidate(p_candidate, p_result.target);
	p_candidate.result = &p_result;
}

void QuickOpenResultContainer::update_results() {
	showing_history = false;
	candidates.clear();
	if (query.is_empty()) {
		_use_default_candidates();
	} else {
		_score_and_sort_candidates();
	}
	_update_result_items(MIN(candidates.size(), max_total_results), 0);
}

void QuickOpenResultContainer::_use_default_candidates() {
	if (filepaths.size() <= SHOW_ALL_FILES_THRESHOLD) {
		candidates.resize(filepaths.size());
		QuickOpenResultCandidate *candidates_write = candidates.ptrw();
		for (const String &filepath : filepaths) {
			_setup_candidate(*candidates_write++, filepath);
		}
	} else if (base_types.size() == 1) {
		Vector<QuickOpenResultCandidate> *history = selected_history.lookup_ptr(base_types[0]);
		if (history) {
			showing_history = true;
			candidates.append_array(*history);
		}
	}
}

void QuickOpenResultContainer::_update_fuzzy_search_results() {
	FuzzySearch fuzzy_search;
	fuzzy_search.start_offset = 6; // Don't match against "res://" at the start of each filepath.
	fuzzy_search.set_query(query);
	fuzzy_search.max_results = max_total_results;
	bool fuzzy_matching = EDITOR_GET("filesystem/quick_open_dialog/enable_fuzzy_matching");
	int max_misses = EDITOR_GET("filesystem/quick_open_dialog/max_fuzzy_misses");
	fuzzy_search.allow_subsequences = fuzzy_matching;
	fuzzy_search.max_misses = fuzzy_matching ? max_misses : 0;
	fuzzy_search.search_all(filepaths, search_results);
}

void QuickOpenResultContainer::_score_and_sort_candidates() {
	_update_fuzzy_search_results();
	candidates.resize(search_results.size());
	QuickOpenResultCandidate *candidates_write = candidates.ptrw();
	for (const FuzzySearchResult &result : search_results) {
		_setup_candidate(*candidates_write++, result);
	}
}

void QuickOpenResultContainer::_update_result_items(int p_new_visible_results_count, int p_new_selection_index) {
	// Only need to update items that were not hidden in previous update.
	int num_items_needing_updates = MAX(num_visible_results, p_new_visible_results_count);
	num_visible_results = p_new_visible_results_count;

	for (int i = 0; i < num_items_needing_updates; i++) {
		QuickOpenResultItem *item = result_items[i];

		if (i < num_visible_results) {
			item->set_content(candidates[i]);
		} else {
			item->reset();
		}
	};

	const bool any_results = num_visible_results > 0;
	_select_item(any_results ? p_new_selection_index : -1);

	scroll_container->set_visible(any_results);
	no_results_container->set_visible(!any_results);

	if (!any_results) {
		if (filepaths.is_empty()) {
			no_results_label->set_text(TTR("No files found for this type"));
		} else if (query.is_empty()) {
			no_results_label->set_text(TTR("Start searching to find files..."));
		} else {
			no_results_label->set_text(TTR("No results found"));
		}
	}
}

void QuickOpenResultContainer::handle_search_box_input(const Ref<InputEvent> &p_ie) {
	if (num_visible_results < 0) {
		return;
	}

	Ref<InputEventKey> key_event = p_ie;
	if (key_event.is_valid() && key_event->is_pressed()) {
		bool move_selection = false;

		switch (key_event->get_keycode()) {
			case Key::UP:
			case Key::DOWN:
			case Key::PAGEUP:
			case Key::PAGEDOWN: {
				move_selection = true;
			} break;
			case Key::LEFT:
			case Key::RIGHT: {
				if (content_display_mode == QuickOpenDisplayMode::GRID) {
					// Maybe strip off the shift modifier to allow non-selecting navigation by character?
					if (key_event->get_modifiers_mask() == 0) {
						move_selection = true;
					}
				}
			} break;
			default:
				break; // Let the event through so it will reach the search box.
		}

		if (move_selection) {
			_move_selection_index(key_event->get_keycode());
			queue_redraw();
			accept_event();
		}
	}
}

void QuickOpenResultContainer::_move_selection_index(Key p_key) {
	// Don't move selection if there are no results.
	if (num_visible_results <= 0) {
		return;
	}
	const int max_index = num_visible_results - 1;

	int idx = selection_index;
	if (content_display_mode == QuickOpenDisplayMode::LIST) {
		if (p_key == Key::UP) {
			idx = (idx == 0) ? max_index : (idx - 1);
		} else if (p_key == Key::DOWN) {
			idx = (idx == max_index) ? 0 : (idx + 1);
		} else if (p_key == Key::PAGEUP) {
			idx = (idx == 0) ? idx : MAX(idx - 10, 0);
		} else if (p_key == Key::PAGEDOWN) {
			idx = (idx == max_index) ? idx : MIN(idx + 10, max_index);
		}
	} else {
		int column_count = grid->get_line_max_child_count();

		if (p_key == Key::LEFT) {
			idx = (idx == 0) ? max_index : (idx - 1);
		} else if (p_key == Key::RIGHT) {
			idx = (idx == max_index) ? 0 : (idx + 1);
		} else if (p_key == Key::UP) {
			idx = (idx == 0) ? max_index : MAX(idx - column_count, 0);
		} else if (p_key == Key::DOWN) {
			idx = (idx == max_index) ? 0 : MIN(idx + column_count, max_index);
		} else if (p_key == Key::PAGEUP) {
			idx = (idx == 0) ? idx : MAX(idx - (3 * column_count), 0);
		} else if (p_key == Key::PAGEDOWN) {
			idx = (idx == max_index) ? idx : MIN(idx + (3 * column_count), max_index);
		}
	}

	_select_item(idx);
}

void QuickOpenResultContainer::_select_item(int p_index) {
	if (!has_nothing_selected()) {
		result_items[selection_index]->highlight_item(false);
	}

	selection_index = p_index;

	if (has_nothing_selected()) {
		file_details_path->set_text("");
		return;
	}

	result_items[selection_index]->highlight_item(true);
	file_details_path->set_text(get_selected() + (showing_history ? TTR(" (recently opened)") : ""));

	const QuickOpenResultItem *item = result_items[selection_index];

	// Copied from Tree.
	const int selected_position = item->get_position().y;
	const int selected_size = item->get_size().y;
	const int scroll_window_size = scroll_container->get_size().y;
	const int scroll_position = scroll_container->get_v_scroll();

	if (selected_position <= scroll_position) {
		scroll_container->set_v_scroll(selected_position);
	} else if (selected_position + selected_size > scroll_position + scroll_window_size) {
		scroll_container->set_v_scroll(selected_position + selected_size - scroll_window_size);
	}
}

void QuickOpenResultContainer::_item_input(const Ref<InputEvent> &p_ev, int p_index) {
	Ref<InputEventMouseButton> mb = p_ev;

	if (mb.is_valid() && mb->is_pressed()) {
		if (mb->get_button_index() == MouseButton::LEFT) {
			_select_item(p_index);
			emit_signal(SNAME("result_clicked"));
		} else if (mb->get_button_index() == MouseButton::RIGHT) {
			_select_item(p_index);
			file_context_menu->set_position(result_items[p_index]->get_screen_position() + mb->get_position());
			file_context_menu->reset_size();
			file_context_menu->popup();
		}
	}
}

void QuickOpenResultContainer::_toggle_fuzzy_search(bool p_pressed) {
	EditorSettings::get_singleton()->set("filesystem/quick_open_dialog/enable_fuzzy_matching", p_pressed);
	update_results();
}

String QuickOpenResultContainer::_get_cache_file_path() const {
	return EditorPaths::get_singleton()->get_project_settings_dir().path_join("quick_open_dialog_cache.cfg");
}

void QuickOpenResultContainer::_toggle_include_addons(bool p_pressed) {
	EditorSettings::get_singleton()->set("filesystem/quick_open_dialog/include_addons", p_pressed);
	cleanup();
	_create_initial_results();
}

void QuickOpenResultContainer::_toggle_display_mode() {
	QuickOpenDisplayMode new_display_mode = (content_display_mode == QuickOpenDisplayMode::LIST) ? QuickOpenDisplayMode::GRID : QuickOpenDisplayMode::LIST;
	_set_display_mode(new_display_mode);
}

CanvasItem *QuickOpenResultContainer::_get_result_root() {
	if (content_display_mode == QuickOpenDisplayMode::LIST) {
		return list;
	} else {
		return grid;
	}
}

void QuickOpenResultContainer::_layout_result_item(QuickOpenResultItem *item) {
	item->set_display_mode(content_display_mode);
	Node *parent = item->get_parent();
	if (parent) {
		parent->remove_child(item);
	}
	_get_result_root()->add_child(item);
}

void QuickOpenResultContainer::_set_display_mode(QuickOpenDisplayMode p_display_mode) {
	CanvasItem *prev_root = _get_result_root();

	if (prev_root->is_visible() && content_display_mode == p_display_mode) {
		return;
	}

	content_display_mode = p_display_mode;
	CanvasItem *next_root = _get_result_root();

	EditorSettings::get_singleton()->set_project_metadata("quick_open_dialog", "last_mode", (int)content_display_mode);

	prev_root->hide();
	next_root->show();

	for (QuickOpenResultItem *item : result_items) {
		_layout_result_item(item);
	}

	_update_result_items(num_visible_results, selection_index);

	if (content_display_mode == QuickOpenDisplayMode::LIST) {
		display_mode_toggle->set_button_icon(get_editor_theme_icon(SNAME("FileThumbnail")));
		display_mode_toggle->set_tooltip_text(TTR("Grid view"));
	} else {
		display_mode_toggle->set_button_icon(get_editor_theme_icon(SNAME("FileList")));
		display_mode_toggle->set_tooltip_text(TTR("List view"));
	}
}

bool QuickOpenResultContainer::has_nothing_selected() const {
	return selection_index < 0;
}

String QuickOpenResultContainer::get_selected() const {
	ERR_FAIL_COND_V_MSG(has_nothing_selected(), String(), "Tried to get selected file, but nothing was selected.");
	return candidates[selection_index].file_path;
}

QuickOpenDisplayMode QuickOpenResultContainer::get_adaptive_display_mode(const Vector<StringName> &p_base_types) {
	static const Vector<StringName> grid_preferred_types = {
		"Font",
		"Texture2D",
		"Material",
		"Mesh"
	};

	for (const StringName &type : grid_preferred_types) {
		for (const StringName &base_type : p_base_types) {
			if (base_type == type || ClassDB::is_parent_class(base_type, type)) {
				return QuickOpenDisplayMode::GRID;
			}
		}
	}

	return QuickOpenDisplayMode::LIST;
}

void QuickOpenResultContainer::save_selected_item() {
	if (base_types.size() > 1) {
		// Getting the type of the file and checking which base type it belongs to should be possible.
		// However, for now these are not supported, and we don't record this.
		return;
	}

	const StringName &base_type = base_types[0];
	QuickOpenResultCandidate &selected = candidates.write[selection_index];
	Vector<QuickOpenResultCandidate> *type_history = selected_history.lookup_ptr(base_type);

	if (!type_history) {
		selected_history.insert(base_type, Vector<QuickOpenResultCandidate>());
		type_history = selected_history.lookup_ptr(base_type);
	} else {
		for (int i = 0; i < type_history->size(); i++) {
			if (selected.file_path == type_history->get(i).file_path) {
				type_history->remove_at(i);
				break;
			}
		}
	}

	selected.result = nullptr;
	type_history->insert(0, selected);
	if (type_history->size() > MAX_HISTORY_SIZE) {
		type_history->resize(MAX_HISTORY_SIZE);
	}

	PackedStringArray paths;
	paths.resize(type_history->size());
	{
		String *paths_write = paths.ptrw();

		int i = 0;
		for (const QuickOpenResultCandidate &candidate : *type_history) {
			paths_write[i] = candidate.file_path;
			i++;
		}
	}
	history_file->set_value("selected_history", base_type, paths);
	history_file->save(_get_cache_file_path());
}

void QuickOpenResultContainer::cleanup() {
	num_visible_results = 0;
	candidates.clear();
	_select_item(-1);

	for (QuickOpenResultItem *item : result_items) {
		item->reset();
	}
}

void QuickOpenResultContainer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			Color text_color = get_theme_color("font_readonly_color", EditorStringName(Editor));
			file_details_path->add_theme_color_override(SceneStringName(font_color), text_color);
			no_results_label->add_theme_color_override(SceneStringName(font_color), text_color);

			panel_container->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SceneStringName(panel), SNAME("Tree")));

			if (content_display_mode == QuickOpenDisplayMode::LIST) {
				display_mode_toggle->set_button_icon(get_editor_theme_icon(SNAME("FileThumbnail")));
			} else {
				display_mode_toggle->set_button_icon(get_editor_theme_icon(SNAME("FileList")));
			}
		} break;
	}
}

void QuickOpenResultContainer::_bind_methods() {
	ADD_SIGNAL(MethodInfo("result_clicked"));
}

//------------------------- Result Item

QuickOpenResultItem::QuickOpenResultItem() {
	set_focus_mode(FocusMode::FOCUS_ALL);
	_set_enabled(false);
	set_default_cursor_shape(CURSOR_POINTING_HAND);

	list_item = memnew(QuickOpenResultListItem);
	list_item->hide();
	add_child(list_item);

	grid_item = memnew(QuickOpenResultGridItem);
	grid_item->hide();
	add_child(grid_item);
}

void QuickOpenResultItem::set_display_mode(QuickOpenDisplayMode p_display_mode) {
	if (p_display_mode == QuickOpenDisplayMode::LIST) {
		grid_item->hide();
		grid_item->reset();
		list_item->show();
	} else {
		list_item->hide();
		list_item->reset();
		grid_item->show();
	}

	queue_redraw();
}

void QuickOpenResultItem::set_content(const QuickOpenResultCandidate &p_candidate) {
	_set_enabled(true);

	if (list_item->is_visible()) {
		list_item->set_content(p_candidate, enable_highlights);
	} else {
		grid_item->set_content(p_candidate, enable_highlights);
	}

	queue_redraw();
}

void QuickOpenResultItem::reset() {
	_set_enabled(false);
	is_hovering = false;
	is_selected = false;
	list_item->reset();
	grid_item->reset();
}

void QuickOpenResultItem::highlight_item(bool p_enabled) {
	is_selected = p_enabled;

	if (list_item->is_visible()) {
		if (p_enabled) {
			list_item->highlight_item(highlighted_font_color);
		} else {
			list_item->remove_highlight();
		}
	} else {
		if (p_enabled) {
			grid_item->highlight_item(highlighted_font_color);
		} else {
			grid_item->remove_highlight();
		}
	}

	queue_redraw();
}

void QuickOpenResultItem::_set_enabled(bool p_enabled) {
	set_visible(p_enabled);
	set_process(p_enabled);
	set_process_input(p_enabled);
}

void QuickOpenResultItem::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_MOUSE_ENTER:
		case NOTIFICATION_MOUSE_EXIT: {
			is_hovering = is_visible() && p_what == NOTIFICATION_MOUSE_ENTER;
			queue_redraw();
		} break;
		case NOTIFICATION_THEME_CHANGED: {
			selected_stylebox = get_theme_stylebox("selected", "Tree");
			hovering_stylebox = get_theme_stylebox(SceneStringName(hover), "Tree");
			highlighted_font_color = get_theme_color("font_focus_color", EditorStringName(Editor));
		} break;
		case NOTIFICATION_DRAW: {
			if (is_selected) {
				draw_style_box(selected_stylebox, Rect2(Point2(), get_size()));
			} else if (is_hovering) {
				draw_style_box(hovering_stylebox, Rect2(Point2(), get_size()));
			}
		} break;
	}
}

//----------------- List item

static Vector2i _get_path_interval(const Vector2i &p_interval, int p_dir_index) {
	if (p_interval.x >= p_dir_index || p_interval.y < 1) {
		return { -1, -1 };
	}
	return { p_interval.x, MIN(p_interval.x + p_interval.y, p_dir_index) - p_interval.x };
}

static Vector2i _get_name_interval(const Vector2i &p_interval, int p_dir_index) {
	if (p_interval.x + p_interval.y <= p_dir_index || p_interval.y < 1) {
		return { -1, -1 };
	}
	int first_name_idx = p_dir_index + 1;
	int start = MAX(p_interval.x, first_name_idx);
	return { start - first_name_idx, p_interval.y - start + p_interval.x };
}

QuickOpenResultListItem::QuickOpenResultListItem() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_theme_constant_override("separation", 4 * EDSCALE);

	{
		image_container = memnew(MarginContainer);
		image_container->add_theme_constant_override("margin_top", 2 * EDSCALE);
		image_container->add_theme_constant_override("margin_bottom", 2 * EDSCALE);
		image_container->add_theme_constant_override("margin_left", CONTAINER_MARGIN * EDSCALE);
		image_container->add_theme_constant_override("margin_right", 0);
		add_child(image_container);

		thumbnail = memnew(TextureRect);
		thumbnail->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
		thumbnail->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
		thumbnail->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
		thumbnail->set_stretch_mode(TextureRect::StretchMode::STRETCH_SCALE);
		image_container->add_child(thumbnail);
	}

	{
		text_container = memnew(VBoxContainer);
		text_container->add_theme_constant_override("separation", -6 * EDSCALE);
		text_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		text_container->set_v_size_flags(Control::SIZE_FILL);
		add_child(text_container);

		name = memnew(HighlightedLabel);
		name->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		name->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		name->set_horizontal_alignment(HorizontalAlignment::HORIZONTAL_ALIGNMENT_LEFT);
		text_container->add_child(name);

		path = memnew(HighlightedLabel);
		path->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		path->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		path->add_theme_font_size_override(SceneStringName(font_size), 12 * EDSCALE);
		text_container->add_child(path);
	}
}

void QuickOpenResultListItem::set_content(const QuickOpenResultCandidate &p_candidate, bool p_highlight) {
	thumbnail->set_texture(p_candidate.thumbnail);
	name->set_text(p_candidate.file_path.get_file());
	path->set_text(p_candidate.file_path.get_base_dir());
	name->reset_highlights();
	path->reset_highlights();

	if (p_highlight && p_candidate.result != nullptr) {
		for (const FuzzyTokenMatch &match : p_candidate.result->token_matches) {
			for (const Vector2i &interval : match.substrings) {
				path->add_highlight(_get_path_interval(interval, p_candidate.result->dir_index));
				name->add_highlight(_get_name_interval(interval, p_candidate.result->dir_index));
			}
		}
	}

	const int max_size = 32 * EDSCALE;
	bool uses_icon = p_candidate.thumbnail->get_width() < max_size;

	if (uses_icon) {
		thumbnail->set_custom_minimum_size(p_candidate.thumbnail->get_size());

		int margin_needed = (max_size - p_candidate.thumbnail->get_width()) / 2;
		image_container->add_theme_constant_override("margin_left", CONTAINER_MARGIN + margin_needed);
		image_container->add_theme_constant_override("margin_right", margin_needed);
	} else {
		thumbnail->set_custom_minimum_size(Size2i(max_size, max_size));
		image_container->add_theme_constant_override("margin_left", CONTAINER_MARGIN);
		image_container->add_theme_constant_override("margin_right", 0);
	}
}

void QuickOpenResultListItem::reset() {
	thumbnail->set_texture(nullptr);
	name->set_text("");
	path->set_text("");
	name->reset_highlights();
	path->reset_highlights();
}

void QuickOpenResultListItem::highlight_item(const Color &p_color) {
	name->add_theme_color_override(SceneStringName(font_color), p_color);
}

void QuickOpenResultListItem::remove_highlight() {
	name->remove_theme_color_override(SceneStringName(font_color));
}

void QuickOpenResultListItem::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			path->add_theme_color_override(SceneStringName(font_color), get_theme_color("font_disabled_color", EditorStringName(Editor)));
		} break;
	}
}

//--------------- Grid Item

QuickOpenResultGridItem::QuickOpenResultGridItem() {
	set_h_size_flags(Control::SIZE_FILL);
	set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_theme_constant_override("separation", -2 * EDSCALE);

	thumbnail = memnew(TextureRect);
	thumbnail->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
	thumbnail->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	thumbnail->set_custom_minimum_size(Size2i(120 * EDSCALE, 64 * EDSCALE));
	add_child(thumbnail);

	name = memnew(HighlightedLabel);
	name->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	name->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	name->set_horizontal_alignment(HorizontalAlignment::HORIZONTAL_ALIGNMENT_CENTER);
	name->add_theme_font_size_override(SceneStringName(font_size), 13 * EDSCALE);
	add_child(name);
}

void QuickOpenResultGridItem::set_content(const QuickOpenResultCandidate &p_candidate, bool p_highlight) {
	thumbnail->set_texture(p_candidate.thumbnail);
	name->set_text(p_candidate.file_path.get_file());
	name->set_tooltip_text(p_candidate.file_path);
	name->reset_highlights();

	if (p_highlight && p_candidate.result != nullptr) {
		for (const FuzzyTokenMatch &match : p_candidate.result->token_matches) {
			for (const Vector2i &interval : match.substrings) {
				name->add_highlight(_get_name_interval(interval, p_candidate.result->dir_index));
			}
		}
	}

	bool uses_icon = p_candidate.thumbnail->get_width() < (32 * EDSCALE);

	if (uses_icon || p_candidate.thumbnail->get_height() <= thumbnail->get_custom_minimum_size().y) {
		thumbnail->set_expand_mode(TextureRect::EXPAND_KEEP_SIZE);
		thumbnail->set_stretch_mode(TextureRect::StretchMode::STRETCH_KEEP_CENTERED);
	} else {
		thumbnail->set_expand_mode(TextureRect::EXPAND_FIT_WIDTH_PROPORTIONAL);
		thumbnail->set_stretch_mode(TextureRect::StretchMode::STRETCH_SCALE);
	}
}

void QuickOpenResultGridItem::reset() {
	thumbnail->set_texture(nullptr);
	name->set_text("");
	name->reset_highlights();
}

void QuickOpenResultGridItem::highlight_item(const Color &p_color) {
	name->add_theme_color_override(SceneStringName(font_color), p_color);
}

void QuickOpenResultGridItem::remove_highlight() {
	name->remove_theme_color_override(SceneStringName(font_color));
}

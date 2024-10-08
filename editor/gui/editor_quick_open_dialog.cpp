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

#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/editor_resource_preview.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/center_container.h"
#include "scene/gui/check_button.h"
#include "scene/gui/flow_container.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/texture_rect.h"
#include "scene/gui/tree.h"

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
	popup_centered_clamped(Size2(710, 650) * EDSCALE, 0.8f);
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
	container->update_results(p_query.to_lower());

	get_ok_button()->set_disabled(container->has_nothing_selected());
}

//------------------------- Result Container

QuickOpenResultContainer::QuickOpenResultContainer() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_theme_constant_override("separation", 0);

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
		}
	}

	{
		// Bottom bar
		HBoxContainer *bottom_bar = memnew(HBoxContainer);
		add_child(bottom_bar);

		file_details_path = memnew(Label);
		file_details_path->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		file_details_path->set_horizontal_alignment(HorizontalAlignment::HORIZONTAL_ALIGNMENT_CENTER);
		file_details_path->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		bottom_bar->add_child(file_details_path);

		{
			HBoxContainer *hbc = memnew(HBoxContainer);
			hbc->add_theme_constant_override("separation", 3);
			bottom_bar->add_child(hbc);

			include_addons_toggle = memnew(CheckButton);
			include_addons_toggle->set_flat(true);
			include_addons_toggle->set_focus_mode(Control::FOCUS_NONE);
			include_addons_toggle->set_default_cursor_shape(CURSOR_POINTING_HAND);
			include_addons_toggle->set_tooltip_text(TTR("Include files from addons"));
			include_addons_toggle->connect(SceneStringName(toggled), callable_mp(this, &QuickOpenResultContainer::_toggle_include_addons));
			hbc->add_child(include_addons_toggle);

			VSeparator *vsep = memnew(VSeparator);
			vsep->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
			vsep->set_custom_minimum_size(Size2i(0, 14 * EDSCALE));
			hbc->add_child(vsep);

			display_mode_toggle = memnew(Button);
			display_mode_toggle->set_flat(true);
			display_mode_toggle->set_focus_mode(Control::FOCUS_NONE);
			display_mode_toggle->set_default_cursor_shape(CURSOR_POINTING_HAND);
			display_mode_toggle->connect(SceneStringName(pressed), callable_mp(this, &QuickOpenResultContainer::_toggle_display_mode));
			hbc->add_child(display_mode_toggle);
		}
	}

	// Creating and deleting nodes while searching is slow, so we allocate
	// a bunch of result nodes and fill in the content based on result ranking.
	result_items.resize(TOTAL_ALLOCATED_RESULT_ITEMS);
	for (int i = 0; i < TOTAL_ALLOCATED_RESULT_ITEMS; i++) {
		QuickOpenResultItem *item = memnew(QuickOpenResultItem);
		item->connect(SceneStringName(gui_input), callable_mp(this, &QuickOpenResultContainer::_item_input).bind(i));
		result_items.write[i] = item;
	}
}

QuickOpenResultContainer::~QuickOpenResultContainer() {
	if (never_opened) {
		for (QuickOpenResultItem *E : result_items) {
			memdelete(E);
		}
	}
}

void QuickOpenResultContainer::init(const Vector<StringName> &p_base_types) {
	base_types = p_base_types;
	never_opened = false;

	const int display_mode_behavior = EDITOR_GET("filesystem/quick_open_dialog/default_display_mode");
	const bool adaptive_display_mode = (display_mode_behavior == 0);

	if (adaptive_display_mode) {
		_set_display_mode(get_adaptive_display_mode(p_base_types));
	}

	const bool include_addons = EDITOR_GET("filesystem/quick_open_dialog/include_addons");
	include_addons_toggle->set_pressed_no_signal(include_addons);

	_create_initial_results(include_addons);
}

void QuickOpenResultContainer::_create_initial_results(bool p_include_addons) {
	file_type_icons.insert("__default_icon", get_editor_theme_icon(SNAME("Object")));
	_find_candidates_in_folder(EditorFileSystem::get_singleton()->get_filesystem(), p_include_addons);
	max_total_results = MIN(candidates.size(), TOTAL_ALLOCATED_RESULT_ITEMS);
	file_type_icons.clear();

	update_results(query);
}

void QuickOpenResultContainer::_find_candidates_in_folder(EditorFileSystemDirectory *p_directory, bool p_include_addons) {
	for (int i = 0; i < p_directory->get_subdir_count(); i++) {
		if (p_include_addons || p_directory->get_name() != "addons") {
			_find_candidates_in_folder(p_directory->get_subdir(i), p_include_addons);
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
				Candidate c;
				c.file_name = file_path.get_file();
				c.file_directory = file_path.get_base_dir();

				EditorResourcePreview::PreviewItem item = EditorResourcePreview::get_singleton()->get_resource_preview_if_available(file_path);
				if (item.preview.is_valid()) {
					c.thumbnail = item.preview;
				} else if (file_type_icons.has(actual_type)) {
					c.thumbnail = *file_type_icons.lookup_ptr(actual_type);
				} else if (has_theme_icon(actual_type, EditorStringName(EditorIcons))) {
					c.thumbnail = get_editor_theme_icon(actual_type);
					file_type_icons.insert(actual_type, c.thumbnail);
				} else {
					c.thumbnail = *file_type_icons.lookup_ptr("__default_icon");
				}

				candidates.push_back(c);

				break; // Stop testing base types as soon as we get a match.
			}
		}
	}
}

void QuickOpenResultContainer::update_results(const String &p_query) {
	query = p_query;

	int relevant_candidates = _sort_candidates(p_query);
	_update_result_items(MIN(relevant_candidates, max_total_results), 0);
}

int QuickOpenResultContainer::_sort_candidates(const String &p_query) {
	if (p_query.is_empty()) {
		return 0;
	}

	const PackedStringArray search_tokens = p_query.to_lower().replace("/", " ").split(" ", false);

	if (search_tokens.is_empty()) {
		return 0;
	}

	// First, we assign a score to each candidate.
	int num_relevant_candidates = 0;
	for (Candidate &c : candidates) {
		c.score = 0;
		int prev_token_match_pos = -1;

		for (const String &token : search_tokens) {
			const int file_pos = c.file_name.findn(token);
			const int dir_pos = c.file_directory.findn(token);

			const bool file_match = file_pos > -1;
			const bool dir_match = dir_pos > -1;
			if (!file_match && !dir_match) {
				c.score = -1.0f;
				break;
			}

			float token_score = file_match ? 0.6f : 0.1999f;

			// Add bias for shorter filenames/paths: they resemble the query more.
			const String &matched_string = file_match ? c.file_name : c.file_directory;
			int matched_string_token_pos = file_match ? file_pos : dir_pos;
			token_score += 0.1f * (1.0f - ((float)matched_string_token_pos / (float)matched_string.length()));

			// Add bias if the match happened in the file name, not the extension.
			if (file_match) {
				int ext_pos = matched_string.rfind(".");
				if (ext_pos == -1 || ext_pos > matched_string_token_pos) {
					token_score += 0.1f;
				}
			}

			// Add bias if token is in order.
			{
				int candidate_string_token_pos = file_match ? (c.file_directory.length() + file_pos) : dir_pos;

				if (prev_token_match_pos != -1 && candidate_string_token_pos > prev_token_match_pos) {
					token_score += 0.2f;
				}

				prev_token_match_pos = candidate_string_token_pos;
			}

			c.score += token_score;
		}

		if (c.score > 0.0f) {
			num_relevant_candidates++;
		}
	}

	// Now we will sort the candidates based on score, resolving ties by favoring:
	// 1. Shorter file length.
	// 2. Shorter directory length.
	// 3. Lower alphabetic order.
	struct CandidateComparator {
		_FORCE_INLINE_ bool operator()(const Candidate &p_a, const Candidate &p_b) const {
			if (!Math::is_equal_approx(p_a.score, p_b.score)) {
				return p_a.score > p_b.score;
			}

			if (p_a.file_name.length() != p_b.file_name.length()) {
				return p_a.file_name.length() < p_b.file_name.length();
			}

			if (p_a.file_directory.length() != p_b.file_directory.length()) {
				return p_a.file_directory.length() < p_b.file_directory.length();
			}

			return p_a.file_name < p_b.file_name;
		}
	};
	candidates.sort_custom<CandidateComparator>();

	return num_relevant_candidates;
}

void QuickOpenResultContainer::_update_result_items(int p_new_visible_results_count, int p_new_selection_index) {
	List<Candidate> *type_history = nullptr;

	showing_history = false;

	if (query.is_empty()) {
		if (candidates.size() <= SHOW_ALL_FILES_THRESHOLD) {
			p_new_visible_results_count = candidates.size();
		} else {
			p_new_visible_results_count = 0;

			if (base_types.size() == 1) {
				type_history = selected_history.lookup_ptr(base_types[0]);
				if (type_history) {
					p_new_visible_results_count = type_history->size();
					showing_history = true;
				}
			}
		}
	}

	// Only need to update items that were not hidden in previous update.
	int num_items_needing_updates = MAX(num_visible_results, p_new_visible_results_count);
	num_visible_results = p_new_visible_results_count;

	for (int i = 0; i < num_items_needing_updates; i++) {
		QuickOpenResultItem *item = result_items[i];

		if (i < num_visible_results) {
			if (type_history) {
				const Candidate &c = type_history->get(i);
				item->set_content(c.thumbnail, c.file_name, c.file_directory);
			} else {
				const Candidate &c = candidates[i];
				item->set_content(c.thumbnail, c.file_name, c.file_directory);
			}
		} else {
			item->reset();
		}
	};

	const bool any_results = num_visible_results > 0;
	_select_item(any_results ? p_new_selection_index : -1);

	scroll_container->set_visible(any_results);
	no_results_container->set_visible(!any_results);

	if (!any_results) {
		if (candidates.is_empty()) {
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
				// Both grid and the search box use left/right keys. By default, grid will take it.
				// It would be nice if we could check for ALT to give the event to the searchbox cursor.
				// However, if you press ALT, the searchbox also denies the input.
				move_selection = (content_display_mode == QuickOpenDisplayMode::GRID);
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

	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		_select_item(p_index);
		emit_signal(SNAME("result_clicked"));
	}
}

void QuickOpenResultContainer::_toggle_include_addons(bool p_pressed) {
	EditorSettings::get_singleton()->set("filesystem/quick_open_dialog/include_addons", p_pressed);

	cleanup();
	_create_initial_results(p_pressed);
}

void QuickOpenResultContainer::_toggle_display_mode() {
	QuickOpenDisplayMode new_display_mode = (content_display_mode == QuickOpenDisplayMode::LIST) ? QuickOpenDisplayMode::GRID : QuickOpenDisplayMode::LIST;
	_set_display_mode(new_display_mode);
}

void QuickOpenResultContainer::_set_display_mode(QuickOpenDisplayMode p_display_mode) {
	content_display_mode = p_display_mode;

	const bool show_list = (content_display_mode == QuickOpenDisplayMode::LIST);
	if ((show_list && list->is_visible()) || (!show_list && grid->is_visible())) {
		return;
	}

	hide();

	// Move result item nodes from one container to the other.
	CanvasItem *prev_root;
	CanvasItem *next_root;
	if (content_display_mode == QuickOpenDisplayMode::LIST) {
		prev_root = Object::cast_to<CanvasItem>(grid);
		next_root = Object::cast_to<CanvasItem>(list);
	} else {
		prev_root = Object::cast_to<CanvasItem>(list);
		next_root = Object::cast_to<CanvasItem>(grid);
	}

	const bool first_time = !list->is_visible() && !grid->is_visible();

	prev_root->hide();
	for (QuickOpenResultItem *item : result_items) {
		item->set_display_mode(content_display_mode);

		if (!first_time) {
			prev_root->remove_child(item);
		}

		next_root->add_child(item);
	}
	next_root->show();
	show();

	_update_result_items(num_visible_results, selection_index);

	if (content_display_mode == QuickOpenDisplayMode::LIST) {
		display_mode_toggle->set_icon(get_editor_theme_icon(SNAME("FileThumbnail")));
		display_mode_toggle->set_tooltip_text(TTR("Grid view"));
	} else {
		display_mode_toggle->set_icon(get_editor_theme_icon(SNAME("FileList")));
		display_mode_toggle->set_tooltip_text(TTR("List view"));
	}
}

bool QuickOpenResultContainer::has_nothing_selected() const {
	return selection_index < 0;
}

String QuickOpenResultContainer::get_selected() const {
	ERR_FAIL_COND_V_MSG(has_nothing_selected(), String(), "Tried to get selected file, but nothing was selected.");

	if (showing_history) {
		const List<Candidate> *type_history = selected_history.lookup_ptr(base_types[0]);

		const Candidate &c = type_history->get(selection_index);
		return c.file_directory.path_join(c.file_name);
	} else {
		const Candidate &c = candidates[selection_index];
		return c.file_directory.path_join(c.file_name);
	}
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
			if (base_type == type || ClassDB::is_parent_class(base_type, type))
				return QuickOpenDisplayMode::GRID;
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

	if (showing_history) {
		// Selecting from history, so already added.
		return;
	}

	const StringName &base_type = base_types[0];

	List<Candidate> *type_history = selected_history.lookup_ptr(base_type);
	if (!type_history) {
		selected_history.insert(base_type, List<Candidate>());
		type_history = selected_history.lookup_ptr(base_type);
	} else {
		const Candidate &selected = candidates[selection_index];

		for (const Candidate &candidate : *type_history) {
			if (candidate.file_directory == selected.file_directory && candidate.file_name == selected.file_name) {
				return;
			}
		}

		if (type_history->size() > 8) {
			type_history->pop_back();
		}
	}

	type_history->push_front(candidates[selection_index]);
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
				display_mode_toggle->set_icon(get_editor_theme_icon(SNAME("FileThumbnail")));
			} else {
				display_mode_toggle->set_icon(get_editor_theme_icon(SNAME("FileList")));
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
		list_item->show();
	} else {
		list_item->hide();
		grid_item->show();
	}

	queue_redraw();
}

void QuickOpenResultItem::set_content(const Ref<Texture2D> &p_thumbnail, const String &p_file, const String &p_file_directory) {
	_set_enabled(true);

	if (list_item->is_visible()) {
		list_item->set_content(p_thumbnail, p_file, p_file_directory);
	} else {
		grid_item->set_content(p_thumbnail, p_file);
	}
}

void QuickOpenResultItem::reset() {
	_set_enabled(false);

	is_hovering = false;
	is_selected = false;

	if (list_item->is_visible()) {
		list_item->reset();
	} else {
		grid_item->reset();
	}
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
			hovering_stylebox = get_theme_stylebox("hover", "Tree");
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

		name = memnew(Label);
		name->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		name->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		name->set_horizontal_alignment(HorizontalAlignment::HORIZONTAL_ALIGNMENT_LEFT);
		text_container->add_child(name);

		path = memnew(Label);
		path->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		path->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		path->add_theme_font_size_override(SceneStringName(font_size), 12 * EDSCALE);
		text_container->add_child(path);
	}
}

void QuickOpenResultListItem::set_content(const Ref<Texture2D> &p_thumbnail, const String &p_file, const String &p_file_directory) {
	thumbnail->set_texture(p_thumbnail);
	name->set_text(p_file);
	path->set_text(p_file_directory);

	const int max_size = 32 * EDSCALE;
	bool uses_icon = p_thumbnail->get_width() < max_size;

	if (uses_icon) {
		thumbnail->set_custom_minimum_size(p_thumbnail->get_size());

		int margin_needed = (max_size - p_thumbnail->get_width()) / 2;
		image_container->add_theme_constant_override("margin_left", CONTAINER_MARGIN + margin_needed);
		image_container->add_theme_constant_override("margin_right", margin_needed);
	} else {
		thumbnail->set_custom_minimum_size(Size2i(max_size, max_size));
		image_container->add_theme_constant_override("margin_left", CONTAINER_MARGIN);
		image_container->add_theme_constant_override("margin_right", 0);
	}
}

void QuickOpenResultListItem::reset() {
	name->set_text("");
	thumbnail->set_texture(nullptr);
	path->set_text("");
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
	thumbnail->set_custom_minimum_size(Size2i(80 * EDSCALE, 64 * EDSCALE));
	add_child(thumbnail);

	name = memnew(Label);
	name->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	name->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	name->set_horizontal_alignment(HorizontalAlignment::HORIZONTAL_ALIGNMENT_CENTER);
	name->add_theme_font_size_override(SceneStringName(font_size), 13 * EDSCALE);
	add_child(name);
}

void QuickOpenResultGridItem::set_content(const Ref<Texture2D> &p_thumbnail, const String &p_file) {
	thumbnail->set_texture(p_thumbnail);

	const String &file_name = p_file.get_basename();
	name->set_text(file_name);
	name->set_tooltip_text(file_name);

	bool uses_icon = p_thumbnail->get_width() < (32 * EDSCALE);

	if (uses_icon || p_thumbnail->get_height() <= thumbnail->get_custom_minimum_size().y) {
		thumbnail->set_expand_mode(TextureRect::EXPAND_KEEP_SIZE);
		thumbnail->set_stretch_mode(TextureRect::StretchMode::STRETCH_KEEP_CENTERED);
	} else {
		thumbnail->set_expand_mode(TextureRect::EXPAND_FIT_WIDTH_PROPORTIONAL);
		thumbnail->set_stretch_mode(TextureRect::StretchMode::STRETCH_SCALE);
	}
}

void QuickOpenResultGridItem::reset() {
	name->set_text("");
	thumbnail->set_texture(nullptr);
}

void QuickOpenResultGridItem::highlight_item(const Color &p_color) {
	name->add_theme_color_override(SceneStringName(font_color), p_color);
}

void QuickOpenResultGridItem::remove_highlight() {
	name->remove_theme_color_override(SceneStringName(font_color));
}

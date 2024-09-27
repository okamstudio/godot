/**************************************************************************/
/*  editor_command_palette.cpp                                            */
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

#include "editor/editor_command_palette.h"
#include "core/os/keyboard.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_toaster.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/control.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/tree.h"

EditorCommandPalette *EditorCommandPalette::singleton = nullptr;

static Rect2i prev_rect = Rect2i();
static bool was_showed = false;

float EditorCommandPalette::_score_path(const String &p_search, const String &p_path) {
	float score = 0.9f + .1f * (p_search.length() / (float)p_path.length());

	// Positive bias for matches close to the beginning of the file name.
	int pos = p_path.findn(p_search);
	if (pos != -1) {
		return score * (1.0f - 0.1f * (float(pos) / p_path.length()));
	}

	// Positive bias for matches close to the end of the path.
	pos = p_path.rfindn(p_search);
	if (pos != -1) {
		return score * (0.8f - 0.1f * (float(p_path.length() - pos) / p_path.length()));
	}

	// Remaining results belong to the same class of results.
	return score * 0.69f;
}

void EditorCommandPalette::_update_command_search(const String &search_text) {
	ERR_FAIL_COND(commands.is_empty());

	HashMap<String, TreeItem *> sections;
	TreeItem *first_section = nullptr;

	// Filter possible candidates.
	Vector<CommandEntry> entries;
	for (const KeyValue<String, Command> &E : commands) {
		CommandEntry r;
		r.key_name = E.key;
		r.display_name = E.value.name;
		r.shortcut_text = E.value.shortcut_text;
		r.last_used = E.value.last_used;

		bool is_subsequence_of_key_name = search_text.is_subsequence_ofn(r.key_name);
		bool is_subsequence_of_display_name = search_text.is_subsequence_ofn(r.display_name);

		if (is_subsequence_of_key_name || is_subsequence_of_display_name) {
			if (!search_text.is_empty()) {
				float key_name_score = is_subsequence_of_key_name ? _score_path(search_text, r.key_name.to_lower()) : .0f;
				float display_name_score = is_subsequence_of_display_name ? _score_path(search_text, r.display_name.to_lower()) : .0f;

				r.score = MAX(key_name_score, display_name_score);
			}

			entries.push_back(r);
		}
	}

	TreeItem *root = search_options->get_root();
	root->clear_children();

	if (entries.is_empty()) {
		get_ok_button()->set_disabled(true);

		return;
	}

	if (!search_text.is_empty()) {
		SortArray<CommandEntry, CommandEntryComparator> sorter;
		sorter.sort(entries.ptrw(), entries.size());
	} else {
		SortArray<CommandEntry, CommandHistoryComparator> sorter;
		sorter.sort(entries.ptrw(), entries.size());
	}

	const int entry_limit = MIN(entries.size(), 300);
	for (int i = 0; i < entry_limit; i++) {
		String section_name = entries[i].key_name.get_slice("/", 0);
		TreeItem *section;

		if (sections.has(section_name)) {
			section = sections[section_name];
		} else {
			section = search_options->create_item(root);

			if (!first_section) {
				first_section = section;
			}

			String item_name = section_name.capitalize();
			section->set_text(0, item_name);
			section->set_selectable(0, false);
			section->set_selectable(1, false);
			section->set_custom_bg_color(0, get_theme_color(SNAME("prop_subsection"), EditorStringName(Editor)));
			section->set_custom_bg_color(1, get_theme_color(SNAME("prop_subsection"), EditorStringName(Editor)));

			sections[section_name] = section;
		}

		TreeItem *ti = search_options->create_item(section);
		String shortcut_text = entries[i].shortcut_text == "None" ? "" : entries[i].shortcut_text;
		ti->set_text(0, entries[i].display_name);
		ti->set_metadata(0, entries[i].key_name);
		ti->set_text_alignment(1, HORIZONTAL_ALIGNMENT_RIGHT);
		ti->set_text(1, shortcut_text);
		Color c = get_theme_color(SceneStringName(font_color), EditorStringName(Editor)) * Color(1, 1, 1, 0.5);
		ti->set_custom_color(1, c);
	}

	TreeItem *to_select = first_section->get_first_child();
	to_select->select(0);
	to_select->set_as_cursor(0);
	search_options->ensure_cursor_is_visible();
}

void EditorCommandPalette::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_command", "command_name", "key_name", "binded_callable", "shortcut_text"), &EditorCommandPalette::_add_command, DEFVAL("None"));
	ClassDB::bind_method(D_METHOD("remove_command", "key_name"), &EditorCommandPalette::remove_command);
}

void EditorCommandPalette::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_visible()) {
				prev_rect = Rect2i(get_position(), get_size());
				was_showed = true;
			}
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			command_search_box->set_right_icon(get_editor_theme_icon(SNAME("Search")));
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (!EditorSettings::get_singleton()->check_changed_settings_in_group("shortcuts")) {
				break;
			}

			for (KeyValue<String, Command> &kv : commands) {
				Command &c = kv.value;
				if (c.shortcut.is_valid()) {
					c.shortcut_text = c.shortcut->get_as_text();
				}
			}
		} break;
	}
}

void EditorCommandPalette::_sbox_input(const Ref<InputEvent> &p_event) {
	// Redirect navigational key events to the tree.
	Ref<InputEventKey> key = p_event;
	if (key.is_valid()) {
		if (key->is_action("ui_up", true) || key->is_action("ui_down", true) || key->is_action("ui_page_up") || key->is_action("ui_page_down")) {
			search_options->gui_input(key);
			command_search_box->accept_event();
		}
	}
}

void EditorCommandPalette::_confirmed() {
	TreeItem *selected_option = search_options->get_selected();
	const String command_key = selected_option != nullptr ? selected_option->get_metadata(0) : "";
	if (!command_key.is_empty()) {
		hide();
		callable_mp(this, &EditorCommandPalette::execute_command).call_deferred(command_key);
	}
}

void EditorCommandPalette::open_popup() {
	if (was_showed) {
		popup(prev_rect);
	} else {
		popup_centered_clamped(Size2(600, 440) * EDSCALE, 0.8f);
	}

	command_search_box->clear();
	command_search_box->grab_focus();

	search_options->scroll_to_item(search_options->get_root());
}

void EditorCommandPalette::get_actions_list(List<String> *p_list) const {
	for (const KeyValue<String, Command> &E : commands) {
		p_list->push_back(E.key);
	}
}

void EditorCommandPalette::remove_command(String p_key_name) {
	ERR_FAIL_COND_MSG(!commands.has(p_key_name), "The Command '" + String(p_key_name) + "' doesn't exists. Unable to remove it.");

	commands.erase(p_key_name);
}

void EditorCommandPalette::add_command(String p_command_name, String p_key_name, Callable p_action, Vector<Variant> arguments, const Ref<Shortcut> &p_shortcut) {
	ERR_FAIL_COND_MSG(commands.has(p_key_name), "The Command '" + String(p_command_name) + "' already exists. Unable to add it.");

	const Variant **argptrs = (const Variant **)alloca(sizeof(Variant *) * arguments.size());
	for (int i = 0; i < arguments.size(); i++) {
		argptrs[i] = &arguments[i];
	}
	Command command;
	command.name = p_command_name;
	command.callable = p_action.bindp(argptrs, arguments.size());
	if (p_shortcut.is_null()) {
		command.shortcut_text = "None";
	} else {
		command.shortcut = p_shortcut;
		command.shortcut_text = p_shortcut->get_as_text();
	}

	commands[p_key_name] = command;
}

void EditorCommandPalette::_add_command(String p_command_name, String p_key_name, Callable p_binded_action, String p_shortcut_text) {
	ERR_FAIL_COND_MSG(commands.has(p_key_name), "The Command '" + String(p_command_name) + "' already exists. Unable to add it.");

	Command command;
	command.name = p_command_name;
	command.callable = p_binded_action;
	command.shortcut_text = p_shortcut_text;

	// Commands added from plugins don't exist yet when the history is loaded, so we assign the last use time here if it was recorded.
	Dictionary command_history = EditorSettings::get_singleton()->get_project_metadata("command_palette", "command_history", Dictionary());
	if (command_history.has(p_key_name)) {
		command.last_used = command_history[p_key_name];
	}

	commands[p_key_name] = command;
}

void EditorCommandPalette::execute_command(const String &p_command_key) {
	ERR_FAIL_COND_MSG(!commands.has(p_command_key), p_command_key + " not found.");
	commands[p_command_key].last_used = OS::get_singleton()->get_unix_time();
	_save_history();

	Variant ret;
	Callable::CallError ce;
	const Callable &callable = commands[p_command_key].callable;
	callable.callp(nullptr, 0, ret, ce);

	if (ce.error != Callable::CallError::CALL_OK) {
		EditorToaster::get_singleton()->popup_str(vformat(TTR("Failed to execute command \"%s\":\n%s."), p_command_key, Variant::get_callable_error_text(callable, nullptr, 0, ce)), EditorToaster::SEVERITY_ERROR);
	}
}

void EditorCommandPalette::register_shortcuts_as_command() {
	for (const KeyValue<String, Pair<String, Ref<Shortcut>>> &E : unregistered_shortcuts) {
		String command_name = E.value.first;
		Ref<Shortcut> shortcut = E.value.second;
		Ref<InputEventShortcut> ev;
		ev.instantiate();
		ev->set_shortcut(shortcut);
		add_command(command_name, E.key, callable_mp(EditorNode::get_singleton()->get_viewport(), &Viewport::push_input), varray(ev, false), shortcut);
	}
	unregistered_shortcuts.clear();

	// Load command use history.
	Dictionary command_history = EditorSettings::get_singleton()->get_project_metadata("command_palette", "command_history", Dictionary());
	List<Variant> history_entries;
	command_history.get_key_list(&history_entries);
	for (const String history_key : history_entries) {
		if (commands.has(history_key)) {
			commands[history_key].last_used = command_history[history_key];
		}
	}
}

Ref<Shortcut> EditorCommandPalette::add_shortcut_command(const String &p_command, const String &p_key, Ref<Shortcut> p_shortcut) {
	if (is_inside_tree()) {
		Ref<InputEventShortcut> ev;
		ev.instantiate();
		ev->set_shortcut(p_shortcut);
		add_command(p_command, p_key, callable_mp(EditorNode::get_singleton()->get_viewport(), &Viewport::push_input), varray(ev, false), p_shortcut);
	} else {
		const String key_name = String(p_key);
		const String command_name = String(p_command);
		Pair pair = Pair(command_name, p_shortcut);
		unregistered_shortcuts[key_name] = pair;
	}
	return p_shortcut;
}

void EditorCommandPalette::_save_history() const {
	Dictionary command_history;

	for (const KeyValue<String, Command> &E : commands) {
		if (E.value.last_used > 0) {
			command_history[E.key] = E.value.last_used;
		}
	}
	EditorSettings::get_singleton()->set_project_metadata("command_palette", "command_history", command_history);
}

EditorCommandPalette *EditorCommandPalette::get_singleton() {
	if (singleton == nullptr) {
		singleton = memnew(EditorCommandPalette);
	}
	return singleton;
}

EditorCommandPalette::EditorCommandPalette() {
	set_hide_on_ok(false);
	connect(SceneStringName(confirmed), callable_mp(this, &EditorCommandPalette::_confirmed));

	VBoxContainer *vbc = memnew(VBoxContainer);
	add_child(vbc);

	command_search_box = memnew(LineEdit);
	command_search_box->set_placeholder(TTR("Filter Commands"));
	command_search_box->connect(SceneStringName(gui_input), callable_mp(this, &EditorCommandPalette::_sbox_input));
	command_search_box->connect(SceneStringName(text_changed), callable_mp(this, &EditorCommandPalette::_update_command_search));
	command_search_box->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	command_search_box->set_clear_button_enabled(true);
	MarginContainer *margin_container_csb = memnew(MarginContainer);
	margin_container_csb->add_child(command_search_box);
	vbc->add_child(margin_container_csb);
	register_text_enter(command_search_box);

	search_options = memnew(Tree);
	search_options->connect("item_activated", callable_mp(this, &EditorCommandPalette::_confirmed));
	search_options->connect(SceneStringName(item_selected), callable_mp((BaseButton *)get_ok_button(), &BaseButton::set_disabled).bind(false));
	search_options->connect("nothing_selected", callable_mp((BaseButton *)get_ok_button(), &BaseButton::set_disabled).bind(true));
	search_options->create_item();
	search_options->set_hide_root(true);
	search_options->set_columns(2);
	search_options->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	search_options->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	search_options->set_column_custom_minimum_width(0, int(8 * EDSCALE));
	vbc->add_child(search_options, true);
}

Ref<Shortcut> ED_SHORTCUT_AND_COMMAND(const String &p_path, const String &p_name, Key p_keycode, String p_command_name) {
	if (p_command_name.is_empty()) {
		p_command_name = p_name;
	}

	Ref<Shortcut> shortcut = ED_SHORTCUT(p_path, p_name, p_keycode);
	EditorCommandPalette::get_singleton()->add_shortcut_command(p_command_name, p_path, shortcut);
	return shortcut;
}

Ref<Shortcut> ED_SHORTCUT_ARRAY_AND_COMMAND(const String &p_path, const String &p_name, const PackedInt32Array &p_keycodes, String p_command_name) {
	if (p_command_name.is_empty()) {
		p_command_name = p_name;
	}

	Ref<Shortcut> shortcut = ED_SHORTCUT_ARRAY(p_path, p_name, p_keycodes);
	EditorCommandPalette::get_singleton()->add_shortcut_command(p_command_name, p_path, shortcut);
	return shortcut;
}

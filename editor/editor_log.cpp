/**************************************************************************/
/*  editor_log.cpp                                                        */
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

#include "editor_log.h"

#include "core/object/undo_redo.h"
#include "core/os/keyboard.h"
#include "core/version.h"
#include "editor/editor_node.h"
#include "editor/editor_paths.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/center_container.h"
#include "scene/gui/separator.h"
#include "scene/resources/font.h"

void EditorLog::_error_handler(void *p_self, const char *p_func, const char *p_file, int p_line, const char *p_error, const char *p_errorexp, bool p_editor_notify, ErrorHandlerType p_type) {
	EditorLog *self = static_cast<EditorLog *>(p_self);

	String err_str;
	if (p_errorexp && p_errorexp[0]) {
		err_str = String::utf8(p_errorexp);
	} else {
		err_str = String::utf8(p_file) + ":" + itos(p_line) + " - " + String::utf8(p_error);
	}

	MessageType message_type = p_type == ERR_HANDLER_WARNING ? MSG_TYPE_WARNING : MSG_TYPE_ERROR;

	if (!Thread::is_main_thread()) {
		MessageQueue::get_main_singleton()->push_callable(callable_mp(self, &EditorLog::add_message), err_str, message_type);
	} else {
		self->add_message(err_str, message_type);
	}
}

void EditorLog::_update_theme() {
	const Ref<Font> normal_font = get_theme_font(SNAME("output_source"), EditorStringName(EditorFonts));
	if (normal_font.is_valid()) {
		log->add_theme_font_override("normal_font", normal_font);
	}

	const Ref<Font> bold_font = get_theme_font(SNAME("output_source_bold"), EditorStringName(EditorFonts));
	if (bold_font.is_valid()) {
		log->add_theme_font_override("bold_font", bold_font);
	}

	const Ref<Font> italics_font = get_theme_font(SNAME("output_source_italic"), EditorStringName(EditorFonts));
	if (italics_font.is_valid()) {
		log->add_theme_font_override("italics_font", italics_font);
	}

	const Ref<Font> bold_italics_font = get_theme_font(SNAME("output_source_bold_italic"), EditorStringName(EditorFonts));
	if (bold_italics_font.is_valid()) {
		log->add_theme_font_override("bold_italics_font", bold_italics_font);
	}

	const Ref<Font> mono_font = get_theme_font(SNAME("output_source_mono"), EditorStringName(EditorFonts));
	if (mono_font.is_valid()) {
		log->add_theme_font_override("mono_font", mono_font);
	}

	// Disable padding for highlighted background/foreground to prevent highlights from overlapping on close lines.
	// This also better matches terminal output, which does not use any form of padding.
	log->add_theme_constant_override("text_highlight_h_padding", 0);
	log->add_theme_constant_override("text_highlight_v_padding", 0);

	const int font_size = get_theme_font_size(SNAME("output_source_size"), EditorStringName(EditorFonts));
	log->begin_bulk_theme_override();
	log->add_theme_font_size_override("normal_font_size", font_size);
	log->add_theme_font_size_override("bold_font_size", font_size);
	log->add_theme_font_size_override("italics_font_size", font_size);
	log->add_theme_font_size_override("mono_font_size", font_size);
	log->end_bulk_theme_override();

	type_filter_map[MSG_TYPE_STD]->toggle_button->set_button_icon(get_editor_theme_icon(SNAME("Popup")));
	type_filter_map[MSG_TYPE_ERROR]->toggle_button->set_button_icon(get_editor_theme_icon(SNAME("StatusError")));
	type_filter_map[MSG_TYPE_WARNING]->toggle_button->set_button_icon(get_editor_theme_icon(SNAME("StatusWarning")));
	type_filter_map[MSG_TYPE_EDITOR]->toggle_button->set_button_icon(get_editor_theme_icon(SNAME("Edit")));

	type_filter_map[MSG_TYPE_STD]->toggle_button->set_theme_type_variation("EditorLogFilterButton");
	type_filter_map[MSG_TYPE_ERROR]->toggle_button->set_theme_type_variation("EditorLogFilterButton");
	type_filter_map[MSG_TYPE_WARNING]->toggle_button->set_theme_type_variation("EditorLogFilterButton");
	type_filter_map[MSG_TYPE_EDITOR]->toggle_button->set_theme_type_variation("EditorLogFilterButton");

	clear_button->set_button_icon(get_editor_theme_icon(SNAME("Clear")));
	copy_button->set_button_icon(get_editor_theme_icon(SNAME("ActionCopy")));
	collapse_button->set_button_icon(get_editor_theme_icon(SNAME("CombineLines")));
	show_search_button->set_button_icon(get_editor_theme_icon(SNAME("Search")));
	search_box->set_right_icon(get_editor_theme_icon(SNAME("Search")));

	theme_cache.error_color = get_theme_color(SNAME("error_color"), EditorStringName(Editor));
	theme_cache.error_icon = get_editor_theme_icon(SNAME("Error"));
	theme_cache.warning_color = get_theme_color(SNAME("warning_color"), EditorStringName(Editor));
	theme_cache.warning_icon = get_editor_theme_icon(SNAME("Warning"));
	theme_cache.message_color = get_theme_color(SceneStringName(font_color), EditorStringName(Editor)) * Color(1, 1, 1, 0.6);
}

void EditorLog::_editor_settings_changed() {
	int new_line_limit = int(EDITOR_GET("run/output/max_lines"));
	if (new_line_limit != line_limit) {
		line_limit = new_line_limit;
		_rebuild_log();
	}
}

void EditorLog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_update_theme();
			_load_state();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			_update_theme();
			_rebuild_log();
		} break;
	}
}

void EditorLog::_set_collapse(bool p_collapse) {
	collapse = p_collapse;
	_start_state_save_timer();
	_rebuild_log();
}

void EditorLog::_start_state_save_timer() {
	if (!is_loading_state) {
		save_state_timer->start();
	}
}

void EditorLog::_save_state() {
	Ref<ConfigFile> config;
	config.instantiate();
	// Load and amend existing config if it exists.
	config->load(EditorPaths::get_singleton()->get_project_settings_dir().path_join("editor_layout.cfg"));

	const String section = "editor_log";
	for (const KeyValue<MessageType, LogFilter *> &E : type_filter_map) {
		config->set_value(section, "log_filter_" + itos(E.key), E.value->is_active());
	}

	config->set_value(section, "collapse", collapse);
	config->set_value(section, "show_search", search_box->is_visible());

	config->save(EditorPaths::get_singleton()->get_project_settings_dir().path_join("editor_layout.cfg"));
}

void EditorLog::_load_state() {
	is_loading_state = true;

	Ref<ConfigFile> config;
	config.instantiate();
	config->load(EditorPaths::get_singleton()->get_project_settings_dir().path_join("editor_layout.cfg"));

	// Run the below code even if config->load returns an error, since we want the defaults to be set even if the file does not exist yet.
	const String section = "editor_log";
	for (const KeyValue<MessageType, LogFilter *> &E : type_filter_map) {
		E.value->set_active(config->get_value(section, "log_filter_" + itos(E.key), true));
	}

	collapse = config->get_value(section, "collapse", false);
	collapse_button->set_pressed(collapse);
	bool show_search = config->get_value(section, "show_search", true);
	search_box->set_visible(show_search);
	show_search_button->set_pressed(show_search);

	is_loading_state = false;
}

void EditorLog::_meta_clicked(const String &p_meta) {
	OS::get_singleton()->shell_open(p_meta);
}

void EditorLog::_clear_request() {
	log->clear();
	messages.clear();
	_reset_message_counts();
	tool_button->set_button_icon(Ref<Texture2D>());
}

void EditorLog::_copy_request() {
	String text = log->get_selected_text();

	if (text.is_empty()) {
		text = log->get_parsed_text();
	}

	if (!text.is_empty()) {
		DisplayServer::get_singleton()->clipboard_set(text);
	}
}

void EditorLog::clear() {
	_clear_request();
}

void EditorLog::_process_message(const String &p_msg, MessageType p_type, bool p_clear) {
	if (messages.size() > 0 && messages[messages.size() - 1].text == p_msg && messages[messages.size() - 1].type == p_type) {
		// If previous message is the same as the new one, increase previous count rather than adding another
		// instance to the messages list.
		LogMessage &previous = messages.write[messages.size() - 1];
		previous.count++;

		_add_log_line(previous, collapse);
	} else {
		// Different message to the previous one received.
		LogMessage message(p_msg, p_type, p_clear);
		_add_log_line(message);
		messages.push_back(message);
	}

	type_filter_map[p_type]->set_message_count(type_filter_map[p_type]->get_message_count() + 1);
}

void EditorLog::add_message(const String &p_msg, MessageType p_type) {
	// Make text split by new lines their own message.
	// See #41321 for reasoning. At time of writing, multiple print()'s in running projects
	// get grouped together and sent to the editor log as one message. This can mess with the
	// search functionality (see the comments on the PR above for more details). This behavior
	// also matches that of other IDE's.
	Vector<String> lines = p_msg.split("\n", true);
	int line_count = lines.size();

	for (int i = 0; i < line_count; i++) {
		_process_message(lines[i], p_type, i == line_count - 1);
	}
}

void EditorLog::set_tool_button(Button *p_tool_button) {
	tool_button = p_tool_button;
}

void EditorLog::register_undo_redo(UndoRedo *p_undo_redo) {
	p_undo_redo->set_commit_notify_callback(_undo_redo_cbk, this);
}

void EditorLog::_undo_redo_cbk(void *p_self, const String &p_name) {
	EditorLog *self = static_cast<EditorLog *>(p_self);
	self->add_message(p_name, EditorLog::MSG_TYPE_EDITOR);
}

void EditorLog::_rebuild_log() {
	if (messages.is_empty()) {
		return;
	}

	log->clear();

	int line_count = 0;
	int start_message_index = 0;
	int initial_skip = 0;

	// Search backward for starting place.
	for (start_message_index = messages.size() - 1; start_message_index >= 0; start_message_index--) {
		LogMessage msg = messages[start_message_index];
		if (collapse) {
			if (_check_display_message(msg)) {
				line_count++;
			}
		} else {
			// If not collapsing, log each instance on a line.
			for (int i = 0; i < msg.count; i++) {
				if (_check_display_message(msg)) {
					line_count++;
				}
			}
		}
		if (line_count >= line_limit) {
			initial_skip = line_count - line_limit;
			break;
		}
		if (start_message_index == 0) {
			break;
		}
	}

	for (int msg_idx = start_message_index; msg_idx < messages.size(); msg_idx++) {
		LogMessage msg = messages[msg_idx];

		if (collapse) {
			// If collapsing, only log one instance of the message.
			_add_log_line(msg);
		} else {
			// If not collapsing, log each instance on a line.
			for (int i = initial_skip; i < msg.count; i++) {
				initial_skip = 0;
				_add_log_line(msg);
			}
		}
	}
}

bool EditorLog::_check_display_message(LogMessage &p_message) {
	bool filter_active = type_filter_map[p_message.type]->is_active();
	String search_text = search_box->get_text();
	bool search_match = search_text.is_empty() || p_message.text.containsn(search_text);
	return filter_active && search_match;
}

void EditorLog::_add_log_line(LogMessage &p_message, bool p_replace_previous) {
	if (!is_inside_tree()) {
		// The log will be built all at once when it enters the tree and has its theme items.
		return;
	}

	if (unlikely(log->is_updating())) {
		// The new message arrived during log RTL text processing/redraw (invalid BiDi control characters / font error), ignore it to avoid RTL data corruption.
		return;
	}

	// Only add the message to the log if it passes the filters.
	if (!_check_display_message(p_message)) {
		return;
	}

	if (p_replace_previous) {
		// Remove last line if replacing, as it will be replace by the next added line.
		// Why "- 2"? RichTextLabel is weird. When you add a line with add_newline(), it also adds an element to the list of lines which is null/blank,
		// but it still counts as a line. So if you remove the last line (count - 1) you are actually removing nothing...
		log->remove_paragraph(log->get_paragraph_count() - 2);
	}

	switch (p_message.type) {
		case MSG_TYPE_STD: {
		} break;
		case MSG_TYPE_STD_RICH: {
		} break;
		case MSG_TYPE_ERROR: {
			log->push_color(theme_cache.error_color);
			Ref<Texture2D> icon = theme_cache.error_icon;
			log->add_image(icon);
			log->push_bold();
			log->add_text(" ERROR: ");
			log->pop(); // bold
			tool_button->set_button_icon(icon);
		} break;
		case MSG_TYPE_WARNING: {
			log->push_color(theme_cache.warning_color);
			Ref<Texture2D> icon = theme_cache.warning_icon;
			log->add_image(icon);
			log->push_bold();
			log->add_text(" WARNING: ");
			log->pop(); // bold
			tool_button->set_button_icon(icon);
		} break;
		case MSG_TYPE_EDITOR: {
			// Distinguish editor messages from messages printed by the project
			log->push_color(theme_cache.message_color);
		} break;
	}

	// If collapsing, add the count of this message in bold at the start of the line.
	if (collapse && p_message.count > 1) {
		log->push_bold();
		log->add_text(vformat("(%s) ", itos(p_message.count)));
		log->pop();
	}

	if (p_message.type == MSG_TYPE_STD_RICH) {
		log->append_text(p_message.text);
	} else {
		log->add_text(p_message.text);
	}
	if (p_message.clear || p_message.type != MSG_TYPE_STD_RICH) {
		log->pop_all(); // Pop all unclosed tags.
	}
	log->add_newline();

	if (p_replace_previous) {
		// Force sync last line update (skip if number of unprocessed log messages is too large to avoid editor lag).
		if (log->get_pending_paragraphs() < 100) {
			while (!log->is_finished()) {
				::OS::get_singleton()->delay_usec(1);
			}
		}
	}

	while (log->get_paragraph_count() > line_limit + 1) {
		log->remove_paragraph(0, true);
	}
}

void EditorLog::_set_filter_active(bool p_active, MessageType p_message_type) {
	type_filter_map[p_message_type]->set_active(p_active);
	_start_state_save_timer();
	_rebuild_log();
}

void EditorLog::_set_search_visible(bool p_visible) {
	search_box->set_visible(p_visible);
	if (p_visible) {
		search_box->grab_focus();
	}
	_start_state_save_timer();
}

void EditorLog::_search_changed(const String &p_text) {
	_rebuild_log();
}

void EditorLog::_reset_message_counts() {
	for (const KeyValue<MessageType, LogFilter *> &E : type_filter_map) {
		E.value->set_message_count(0);
	}
}

EditorLog::EditorLog() {
	save_state_timer = memnew(Timer);
	save_state_timer->set_wait_time(2);
	save_state_timer->set_one_shot(true);
	save_state_timer->connect("timeout", callable_mp(this, &EditorLog::_save_state));
	add_child(save_state_timer);

	line_limit = int(EDITOR_GET("run/output/max_lines"));
	EditorSettings::get_singleton()->connect("settings_changed", callable_mp(this, &EditorLog::_editor_settings_changed));

	HBoxContainer *hb = this;

	VBoxContainer *vb_left = memnew(VBoxContainer);
	vb_left->set_custom_minimum_size(Size2(0, 180) * EDSCALE);
	vb_left->set_v_size_flags(SIZE_EXPAND_FILL);
	vb_left->set_h_size_flags(SIZE_EXPAND_FILL);
	hb->add_child(vb_left);

	// Log - Rich Text Label.
	log = memnew(RichTextLabel);
	log->set_threaded(true);
	log->set_use_bbcode(true);
	log->set_scroll_follow(true);
	log->set_selection_enabled(true);
	log->set_context_menu_enabled(true);
	log->set_focus_mode(FOCUS_CLICK);
	log->set_v_size_flags(SIZE_EXPAND_FILL);
	log->set_h_size_flags(SIZE_EXPAND_FILL);
	log->set_deselect_on_focus_loss_enabled(false);
	log->connect("meta_clicked", callable_mp(this, &EditorLog::_meta_clicked));
	vb_left->add_child(log);

	// Search box
	search_box = memnew(LineEdit);
	search_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	search_box->set_placeholder(TTR("Filter Messages"));
	search_box->set_clear_button_enabled(true);
	search_box->set_visible(true);
	search_box->connect(SceneStringName(text_changed), callable_mp(this, &EditorLog::_search_changed));
	vb_left->add_child(search_box);

	VBoxContainer *vb_right = memnew(VBoxContainer);
	hb->add_child(vb_right);

	// Tools grid
	HBoxContainer *hb_tools = memnew(HBoxContainer);
	hb_tools->set_h_size_flags(SIZE_SHRINK_CENTER);
	vb_right->add_child(hb_tools);

	// Clear.
	clear_button = memnew(Button);
	clear_button->set_theme_type_variation(SceneStringName(FlatButton));
	clear_button->set_focus_mode(FOCUS_NONE);
	clear_button->set_shortcut(ED_SHORTCUT("editor/clear_output", TTR("Clear Output"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::ALT | Key::K));
	clear_button->connect(SceneStringName(pressed), callable_mp(this, &EditorLog::_clear_request));
	hb_tools->add_child(clear_button);

	// Copy.
	copy_button = memnew(Button);
	copy_button->set_theme_type_variation(SceneStringName(FlatButton));
	copy_button->set_focus_mode(FOCUS_NONE);
	copy_button->set_shortcut(ED_SHORTCUT("editor/copy_output", TTR("Copy Selection"), KeyModifierMask::CMD_OR_CTRL | Key::C));
	copy_button->set_shortcut_context(this);
	copy_button->connect(SceneStringName(pressed), callable_mp(this, &EditorLog::_copy_request));
	hb_tools->add_child(copy_button);

	// Separate toggle buttons from normal buttons.
	vb_right->add_child(memnew(HSeparator));

	// A second hbox to make a 2x2 grid of buttons.
	HBoxContainer *hb_tools2 = memnew(HBoxContainer);
	hb_tools2->set_h_size_flags(SIZE_SHRINK_CENTER);
	vb_right->add_child(hb_tools2);

	// Collapse.
	collapse_button = memnew(Button);
	collapse_button->set_theme_type_variation(SceneStringName(FlatButton));
	collapse_button->set_focus_mode(FOCUS_NONE);
	collapse_button->set_tooltip_text(TTR("Collapse duplicate messages into one log entry. Shows number of occurrences."));
	collapse_button->set_toggle_mode(true);
	collapse_button->set_pressed(false);
	collapse_button->connect(SceneStringName(toggled), callable_mp(this, &EditorLog::_set_collapse));
	hb_tools2->add_child(collapse_button);

	// Show Search.
	show_search_button = memnew(Button);
	show_search_button->set_theme_type_variation(SceneStringName(FlatButton));
	show_search_button->set_focus_mode(FOCUS_NONE);
	show_search_button->set_toggle_mode(true);
	show_search_button->set_pressed(true);
	show_search_button->set_shortcut(ED_SHORTCUT("editor/open_search", TTR("Focus Search/Filter Bar"), KeyModifierMask::CMD_OR_CTRL | Key::F));
	show_search_button->set_shortcut_context(this);
	show_search_button->connect(SceneStringName(toggled), callable_mp(this, &EditorLog::_set_search_visible));
	hb_tools2->add_child(show_search_button);

	// Message Type Filters.
	vb_right->add_child(memnew(HSeparator));

	LogFilter *std_filter = memnew(LogFilter(MSG_TYPE_STD));
	std_filter->initialize_button(TTR("Toggle visibility of standard output messages."), callable_mp(this, &EditorLog::_set_filter_active));
	vb_right->add_child(std_filter->toggle_button);
	type_filter_map.insert(MSG_TYPE_STD, std_filter);
	type_filter_map.insert(MSG_TYPE_STD_RICH, std_filter);

	LogFilter *error_filter = memnew(LogFilter(MSG_TYPE_ERROR));
	error_filter->initialize_button(TTR("Toggle visibility of errors."), callable_mp(this, &EditorLog::_set_filter_active));
	vb_right->add_child(error_filter->toggle_button);
	type_filter_map.insert(MSG_TYPE_ERROR, error_filter);

	LogFilter *warning_filter = memnew(LogFilter(MSG_TYPE_WARNING));
	warning_filter->initialize_button(TTR("Toggle visibility of warnings."), callable_mp(this, &EditorLog::_set_filter_active));
	vb_right->add_child(warning_filter->toggle_button);
	type_filter_map.insert(MSG_TYPE_WARNING, warning_filter);

	LogFilter *editor_filter = memnew(LogFilter(MSG_TYPE_EDITOR));
	editor_filter->initialize_button(TTR("Toggle visibility of editor messages."), callable_mp(this, &EditorLog::_set_filter_active));
	vb_right->add_child(editor_filter->toggle_button);
	type_filter_map.insert(MSG_TYPE_EDITOR, editor_filter);

	add_message(VERSION_FULL_NAME " (c) 2007-present Juan Linietsky, Ariel Manzur & Godot Contributors.");

	eh.errfunc = _error_handler;
	eh.userdata = this;
	add_error_handler(&eh);
}

void EditorLog::deinit() {
	remove_error_handler(&eh);
}

EditorLog::~EditorLog() {
	for (const KeyValue<MessageType, LogFilter *> &E : type_filter_map) {
		// MSG_TYPE_STD_RICH is connected to the std_filter button, so we do this
		// to avoid it from being deleted twice, causing a crash on closing.
		if (E.key != MSG_TYPE_STD_RICH) {
			memdelete(E.value);
		}
	}
}

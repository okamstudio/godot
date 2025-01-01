/**************************************************************************/
/*  script_editor_plugin.h                                                */
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

#ifndef SCRIPT_EDITOR_PLUGIN_H
#define SCRIPT_EDITOR_PLUGIN_H

#include "core/object/script_language.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/panel_container.h"
#include "scene/resources/syntax_highlighter.h"
#include "scene/resources/text_file.h"

class CodeTextEditor;
class EditorFileDialog;
class EditorHelpSearch;
class FindReplaceBar;
class HSplitContainer;
class ItemList;
class MenuButton;
class TabContainer;
class TextureRect;
class Tree;
class VSplitContainer;
class WindowWrapper;

class EditorSyntaxHighlighter : public SyntaxHighlighter {
	GDCLASS(EditorSyntaxHighlighter, SyntaxHighlighter)

private:
	Ref<RefCounted> edited_resource;

protected:
	static void _bind_methods();

	GDVIRTUAL0RC(String, _get_name)
	GDVIRTUAL0RC(PackedStringArray, _get_supported_languages)

public:
	virtual String _get_name() const;
	virtual PackedStringArray _get_supported_languages() const;

	void _set_edited_resource(const Ref<Resource> &p_res) { edited_resource = p_res; }
	Ref<RefCounted> _get_edited_resource() { return edited_resource; }

	virtual Ref<EditorSyntaxHighlighter> _create() const;
};

class EditorStandardSyntaxHighlighter : public EditorSyntaxHighlighter {
	GDCLASS(EditorStandardSyntaxHighlighter, EditorSyntaxHighlighter)

private:
	Ref<CodeHighlighter> highlighter;
	ScriptLanguage *script_language = nullptr; // See GH-89610.

public:
	virtual void _update_cache() override;
	virtual Dictionary _get_line_syntax_highlighting_impl(int p_line) override { return highlighter->get_line_syntax_highlighting(p_line); }

	virtual String _get_name() const override { return TTR("Standard"); }

	virtual Ref<EditorSyntaxHighlighter> _create() const override;

	void _set_script_language(ScriptLanguage *p_script_language) { script_language = p_script_language; }

	EditorStandardSyntaxHighlighter() { highlighter.instantiate(); }
};

class EditorPlainTextSyntaxHighlighter : public EditorSyntaxHighlighter {
	GDCLASS(EditorPlainTextSyntaxHighlighter, EditorSyntaxHighlighter)

public:
	virtual String _get_name() const override { return TTR("Plain Text"); }

	virtual Ref<EditorSyntaxHighlighter> _create() const override;
};

class EditorJSONSyntaxHighlighter : public EditorSyntaxHighlighter {
	GDCLASS(EditorJSONSyntaxHighlighter, EditorSyntaxHighlighter)

private:
	Ref<CodeHighlighter> highlighter;

public:
	virtual void _update_cache() override;
	virtual Dictionary _get_line_syntax_highlighting_impl(int p_line) override { return highlighter->get_line_syntax_highlighting(p_line); }

	virtual PackedStringArray _get_supported_languages() const override { return PackedStringArray{ "json" }; }
	virtual String _get_name() const override { return TTR("JSON"); }

	virtual Ref<EditorSyntaxHighlighter> _create() const override;

	EditorJSONSyntaxHighlighter() { highlighter.instantiate(); }
};

class EditorMarkdownSyntaxHighlighter : public EditorSyntaxHighlighter {
	GDCLASS(EditorMarkdownSyntaxHighlighter, EditorSyntaxHighlighter)

private:
	Ref<CodeHighlighter> highlighter;

public:
	virtual void _update_cache() override;
	virtual Dictionary _get_line_syntax_highlighting_impl(int p_line) override { return highlighter->get_line_syntax_highlighting(p_line); }

	virtual PackedStringArray _get_supported_languages() const override { return PackedStringArray{ "md", "markdown" }; }
	virtual String _get_name() const override { return TTR("Markdown"); }

	virtual Ref<EditorSyntaxHighlighter> _create() const override;

	EditorMarkdownSyntaxHighlighter() { highlighter.instantiate(); }
};

///////////////////////////////////////////////////////////////////////////////

class ScriptEditorQuickOpen : public ConfirmationDialog {
	GDCLASS(ScriptEditorQuickOpen, ConfirmationDialog);

	LineEdit *search_box = nullptr;
	Tree *search_options = nullptr;
	String function;

	void _update_search();

	void _sbox_input(const Ref<InputEvent> &p_event);
	Vector<String> functions;

	void _confirmed();
	void _text_changed(const String &p_newtext);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void popup_dialog(const Vector<String> &p_functions, bool p_dontclear = false);
	ScriptEditorQuickOpen();
};

class EditorDebuggerNode;

class ScriptEditorBase : public VBoxContainer {
	GDCLASS(ScriptEditorBase, VBoxContainer);

protected:
	static void _bind_methods();

public:
	virtual void add_syntax_highlighter(Ref<EditorSyntaxHighlighter> p_highlighter) = 0;
	virtual void set_syntax_highlighter(Ref<EditorSyntaxHighlighter> p_highlighter) = 0;

	virtual void apply_code() = 0;
	virtual Ref<Resource> get_edited_resource() const = 0;
	virtual Vector<String> get_functions() = 0;
	virtual void set_edited_resource(const Ref<Resource> &p_res) = 0;
	virtual void enable_editor(Control *p_shortcut_context = nullptr) = 0;
	virtual void reload_text() = 0;
	virtual String get_name() = 0;
	virtual Ref<Texture2D> get_theme_icon() = 0;
	virtual bool is_unsaved() = 0;
	virtual Variant get_edit_state() = 0;
	virtual void set_edit_state(const Variant &p_state) = 0;
	virtual Variant get_navigation_state() = 0;
	virtual void goto_line(int p_line, int p_column = 0) = 0;
	virtual void set_executing_line(int p_line) = 0;
	virtual void clear_executing_line() = 0;
	virtual void trim_trailing_whitespace() = 0;
	virtual void trim_final_newlines() = 0;
	virtual void insert_final_newline() = 0;
	virtual void convert_indent() = 0;
	virtual void ensure_focus() = 0;
	virtual void tag_saved_version() = 0;
	virtual void reload(bool p_soft) {}
	virtual PackedInt32Array get_breakpoints() = 0;
	virtual void set_breakpoint(int p_line, bool p_enabled) = 0;
	virtual void clear_breakpoints() = 0;
	virtual void add_callback(const String &p_function, const PackedStringArray &p_args) = 0;
	virtual void update_settings() = 0;
	virtual void set_debugger_active(bool p_active) = 0;
	virtual bool can_lose_focus_on_node_selection() { return true; }
	virtual void update_toggle_scripts_button() {}

	virtual bool show_members_overview() = 0;

	virtual void set_tooltip_request_func(const Callable &p_toolip_callback) = 0;
	virtual Control *get_edit_menu() = 0;
	virtual void clear_edit_menu() = 0;
	virtual void set_find_replace_bar(FindReplaceBar *p_bar) = 0;

	virtual Control *get_base_editor() const = 0;
	virtual CodeTextEditor *get_code_editor() const = 0;

	virtual void validate() = 0;

	ScriptEditorBase() {}
};

typedef ScriptEditorBase *(*CreateScriptEditorFunc)(const Ref<Resource> &p_resource);

class EditorScriptCodeCompletionCache;
class FindInFilesDialog;
class FindInFilesPanel;

#ifdef MINGW_ENABLED
#undef FILE_OPEN
#endif

class ScriptEditor : public PanelContainer {
	GDCLASS(ScriptEditor, PanelContainer);

	enum {
		FILE_NEW,
		FILE_NEW_TEXTFILE,
		FILE_OPEN,
		FILE_REOPEN_CLOSED,
		FILE_OPEN_RECENT,
		FILE_SAVE,
		FILE_SAVE_AS,
		FILE_SAVE_ALL,
		FILE_THEME,
		FILE_RUN,
		FILE_CLOSE,
		CLOSE_DOCS,
		CLOSE_ALL,
		CLOSE_OTHER_TABS,
		TOGGLE_SCRIPTS_PANEL,
		SHOW_IN_FILE_SYSTEM,
		FILE_COPY_PATH,
		FILE_TOOL_RELOAD_SOFT,
		SEARCH_IN_FILES,
		REPLACE_IN_FILES,
		SEARCH_HELP,
		SEARCH_WEBSITE,
		HELP_SEARCH_FIND,
		HELP_SEARCH_FIND_NEXT,
		HELP_SEARCH_FIND_PREVIOUS,
		WINDOW_MOVE_UP,
		WINDOW_MOVE_DOWN,
		WINDOW_NEXT,
		WINDOW_PREV,
		WINDOW_SORT,
		WINDOW_SELECT_BASE = 100,
	};

	enum {
		THEME_IMPORT,
		THEME_RELOAD,
		THEME_SAVE,
		THEME_SAVE_AS
	};

	enum ScriptSortBy {
		SORT_BY_NAME,
		SORT_BY_PATH,
		SORT_BY_NONE
	};

	enum ScriptListName {
		DISPLAY_NAME,
		DISPLAY_DIR_AND_NAME,
		DISPLAY_FULL_PATH,
	};

	HBoxContainer *menu_hb = nullptr;
	MenuButton *file_menu = nullptr;
	MenuButton *edit_menu = nullptr;
	MenuButton *script_search_menu = nullptr;
	MenuButton *debug_menu = nullptr;
	PopupMenu *context_menu = nullptr;
	Timer *autosave_timer = nullptr;
	uint64_t idle = 0;

	PopupMenu *recent_scripts = nullptr;
	PopupMenu *theme_submenu = nullptr;

	Button *help_search = nullptr;
	Button *site_search = nullptr;
	Button *make_floating = nullptr;
	bool is_floating = false;
	EditorHelpSearch *help_search_dialog = nullptr;

	ItemList *script_list = nullptr;
	HSplitContainer *script_split = nullptr;
	ItemList *members_overview = nullptr;
	LineEdit *filter_scripts = nullptr;
	LineEdit *filter_methods = nullptr;
	VBoxContainer *scripts_vbox = nullptr;
	VBoxContainer *overview_vbox = nullptr;
	HBoxContainer *buttons_hbox = nullptr;
	Label *filename = nullptr;
	Button *members_overview_alphabeta_sort_button = nullptr;
	bool members_overview_enabled;
	ItemList *help_overview = nullptr;
	bool help_overview_enabled;
	VSplitContainer *list_split = nullptr;
	TabContainer *tab_container = nullptr;
	EditorFileDialog *file_dialog = nullptr;
	AcceptDialog *error_dialog = nullptr;
	ConfirmationDialog *erase_tab_confirm = nullptr;
	ScriptCreateDialog *script_create_dialog = nullptr;
	Button *scripts_visible = nullptr;
	FindReplaceBar *find_replace_bar = nullptr;

	String current_theme;

	float zoom_factor = 1.0f;

	TextureRect *script_icon = nullptr;
	Label *script_name_label = nullptr;

	Button *script_back = nullptr;
	Button *script_forward = nullptr;

	FindInFilesDialog *find_in_files_dialog = nullptr;
	FindInFilesPanel *find_in_files = nullptr;
	Button *find_in_files_button = nullptr;

	WindowWrapper *window_wrapper = nullptr;

	enum {
		SCRIPT_EDITOR_FUNC_MAX = 32,
	};

	static int script_editor_func_count;
	static CreateScriptEditorFunc script_editor_funcs[SCRIPT_EDITOR_FUNC_MAX];

	Vector<Ref<EditorSyntaxHighlighter>> syntax_highlighters;

	struct ScriptHistory {
		Control *control = nullptr;
		Variant state;
	};

	Vector<ScriptHistory> history;
	int history_pos;

	List<String> previous_scripts;
	List<int> script_close_queue;

	void _tab_changed(int p_which);
	void _menu_option(int p_option);
	void _theme_option(int p_option);
	void _show_save_theme_as_dialog();
	bool _has_docs_tab() const;
	bool _has_script_tab() const;
	void _prepare_file_menu();
	void _file_menu_closed();

	Tree *disk_changed_list = nullptr;
	ConfirmationDialog *disk_changed = nullptr;

	bool restoring_layout;

	String _get_debug_tooltip(const String &p_text, Node *_se);

	void _resave_scripts(const String &p_str);

	bool _test_script_times_on_disk(Ref<Resource> p_for_script = Ref<Resource>());

	void _add_recent_script(const String &p_path);
	void _update_recent_scripts();
	void _open_recent_script(int p_idx);

	void _show_error_dialog(const String &p_path);

	void _close_tab(int p_idx, bool p_save = true, bool p_history_back = true);
	void _update_find_replace_bar();

	void _close_current_tab(bool p_save = true, bool p_history_back = true);
	void _close_discard_current_tab(const String &p_str);
	void _close_docs_tab();
	void _close_other_tabs();
	void _close_all_tabs();
	void _queue_close_tabs();

	void _copy_script_path();

	void _ask_close_current_unsaved_tab(ScriptEditorBase *current);

	bool grab_focus_block;

	bool pending_auto_reload;
	bool auto_reload_running_scripts;
	bool reload_all_scripts = false;
	Vector<String> script_paths_to_reload;
	void _live_auto_reload_running_scripts();

	void _update_selected_editor_menu();

	EditorScriptCodeCompletionCache *completion_cache = nullptr;

	void _editor_stop();

	int edit_pass;

	void _add_callback(Object *p_obj, const String &p_function, const PackedStringArray &p_args);
	void _res_saved_callback(const Ref<Resource> &p_res);
	void _scene_saved_callback(const String &p_path);
	void _mark_built_in_scripts_as_saved(const String &p_parent_path);

	bool open_textfile_after_create = true;
	bool trim_trailing_whitespace_on_save;
	bool trim_final_newlines_on_save;
	bool convert_indent_on_save;
	bool external_editor_active;

	void _goto_script_line2(int p_line);
	void _goto_script_line(Ref<RefCounted> p_script, int p_line);
	void _set_execution(Ref<RefCounted> p_script, int p_line);
	void _clear_execution(Ref<RefCounted> p_script);
	void _breaked(bool p_breaked, bool p_can_debug);
	void _script_created(Ref<Script> p_script);
	void _set_breakpoint(Ref<RefCounted> p_scrpt, int p_line, bool p_enabled);
	void _clear_breakpoints();
	Array _get_cached_breakpoints_for_script(const String &p_path) const;

	ScriptEditorBase *_get_current_editor() const;
	TypedArray<ScriptEditorBase> _get_open_script_editors() const;

	Ref<ConfigFile> script_editor_cache;
	void _save_editor_state(ScriptEditorBase *p_editor);
	void _save_layout();
	void _editor_settings_changed();
	void _apply_editor_settings();
	void _filesystem_changed();
	void _files_moved(const String &p_old_file, const String &p_new_file);
	void _file_removed(const String &p_file);
	void _autosave_scripts();
	void _update_autosave_timer();
	void _reload_scripts(bool p_refresh_only = false);

	void _update_members_overview_visibility();
	void _update_members_overview();
	void _toggle_members_overview_alpha_sort(bool p_alphabetic_sort);
	void _filter_scripts_text_changed(const String &p_newtext);
	void _filter_methods_text_changed(const String &p_newtext);
	void _update_script_names();
	bool _sort_list_on_update;

	void _members_overview_selected(int p_idx);
	void _script_selected(int p_idx);

	void _update_help_overview_visibility();
	void _update_help_overview();
	void _help_overview_selected(int p_idx);

	void _update_online_doc();

	void _find_scripts(Node *p_base, Node *p_current, HashSet<Ref<Script>> &used);

	void _tree_changed();

	void _split_dragged(float);

	Variant get_drag_data_fw(const Point2 &p_point, Control *p_from);
	bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);

	virtual void input(const Ref<InputEvent> &p_event) override;
	virtual void shortcut_input(const Ref<InputEvent> &p_event) override;

	void _script_list_clicked(int p_item, Vector2 p_local_mouse_pos, MouseButton p_mouse_button_index);
	void _make_script_list_context_menu();

	void _help_search(const String &p_text);

	void _history_forward();
	void _history_back();

	bool waiting_update_names;
	bool lock_history = false;

	void _help_class_open(const String &p_class);
	void _help_class_goto(const String &p_desc);
	bool _help_tab_goto(const String &p_name, const String &p_desc);
	void _update_history_arrows();
	void _save_history();
	void _save_previous_state(Dictionary p_state);
	void _go_to_tab(int p_idx);
	void _update_history_pos(int p_new_pos);
	void _update_script_colors();
	void _update_modified_scripts_for_external_editor(Ref<Script> p_for_script = Ref<Script>());

	void _script_changed();
	int file_dialog_option;
	void _file_dialog_action(const String &p_file);

	Ref<Script> _get_current_script();
	TypedArray<Script> _get_open_scripts() const;

	HashSet<String> textfile_extensions;
	Ref<TextFile> _load_text_file(const String &p_path, Error *r_error) const;
	Error _save_text_file(Ref<TextFile> p_text_file, const String &p_path);

	void _on_find_in_files_requested(const String &text);
	void _on_replace_in_files_requested(const String &text);
	void _on_find_in_files_result_selected(const String &fpath, int line_number, int begin, int end);
	void _start_find_in_files(bool with_replace);
	void _on_find_in_files_modified_files(const PackedStringArray &paths);
	void _on_find_in_files_close_button_clicked();

	void _set_zoom_factor(float p_zoom_factor);

	void _window_changed(bool p_visible);

	static void _open_script_request(const String &p_path);
	void _close_builtin_scripts_from_scene(const String &p_scene);

	static ScriptEditor *script_editor;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	static ScriptEditor *get_singleton() { return script_editor; }

	bool toggle_scripts_panel();
	bool is_scripts_panel_toggled();
	void apply_scripts() const;
	void reload_scripts(bool p_refresh_only = false);
	void open_script_create_dialog(const String &p_base_name, const String &p_base_path);
	void open_text_file_create_dialog(const String &p_base_path, const String &p_base_name = "");
	Ref<Resource> open_file(const String &p_file);

	void ensure_select_current();

	bool is_editor_floating();

	_FORCE_INLINE_ bool edit(const Ref<Resource> &p_resource, bool p_grab_focus = true) { return edit(p_resource, -1, 0, p_grab_focus); }
	bool edit(const Ref<Resource> &p_resource, int p_line, int p_col, bool p_grab_focus = true);

	Vector<String> _get_breakpoints();
	void get_breakpoints(List<String> *p_breakpoints);

	PackedStringArray get_unsaved_scripts() const;
	void save_current_script();
	void save_all_scripts();

	void set_window_layout(Ref<ConfigFile> p_layout);
	void get_window_layout(Ref<ConfigFile> p_layout);

	void set_scene_root_script(Ref<Script> p_script);
	Vector<Ref<Script>> get_open_scripts() const;

	bool script_goto_method(Ref<Script> p_script, const String &p_method);

	virtual void edited_scene_changed();

	void notify_script_close(const Ref<Script> &p_script);
	void notify_script_changed(const Ref<Script> &p_script);

	void goto_help(const String &p_desc) { _help_class_goto(p_desc); }
	void update_doc(const String &p_name);
	void clear_docs_from_script(const Ref<Script> &p_script);
	void update_docs_from_script(const Ref<Script> &p_script);

	void trigger_live_script_reload(const String &p_script_path);
	void trigger_live_script_reload_all();

	bool can_take_away_focus() const;

	VSplitContainer *get_left_list_split() { return list_split; }

	void set_live_auto_reload_running_scripts(bool p_enabled);

	void register_syntax_highlighter(const Ref<EditorSyntaxHighlighter> &p_syntax_highlighter);
	void unregister_syntax_highlighter(const Ref<EditorSyntaxHighlighter> &p_syntax_highlighter);

	static void register_create_script_editor_function(CreateScriptEditorFunc p_func);

	ScriptEditor(WindowWrapper *p_wrapper);
	~ScriptEditor();
};

class ScriptEditorPlugin : public EditorPlugin {
	GDCLASS(ScriptEditorPlugin, EditorPlugin);

	ScriptEditor *script_editor = nullptr;
	WindowWrapper *window_wrapper = nullptr;

	String last_editor;

	void _focus_another_editor();

	void _save_last_editor(const String &p_editor);
	void _window_visibility_changed(bool p_visible);

protected:
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "Script"; }
	bool has_main_screen() const override { return true; }
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;
	virtual void selected_notify() override;

	virtual String get_unsaved_status(const String &p_for_scene) const override;
	virtual void save_external_data() override;
	virtual void apply_changes() override;

	virtual void set_window_layout(Ref<ConfigFile> p_layout) override;
	virtual void get_window_layout(Ref<ConfigFile> p_layout) override;

	virtual void get_breakpoints(List<String> *p_breakpoints) override;

	virtual void edited_scene_changed() override;

	ScriptEditorPlugin();
	~ScriptEditorPlugin();
};

#endif // SCRIPT_EDITOR_PLUGIN_H

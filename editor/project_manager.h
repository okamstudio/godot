/**************************************************************************/
/*  project_manager.h                                                     */
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

#ifndef PROJECT_MANAGER_H
#define PROJECT_MANAGER_H

#include "editor/editor_about.h"
#include "editor/plugins/asset_library_editor_plugin.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/file_dialog.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/tool_button.h"
#include "scene/gui/tree.h"

class ProjectDialog;
class ProjectList;
class ProjectListFilter;

class ProjectManager : public Control {
	GDCLASS(ProjectManager, Control);

	Button *open_btn;
	Button *run_btn;
	Button *scan_btn;
	Button *create_btn;
	Button *import_btn;
	Button *rename_btn;
	Button *erase_btn;
	Button *erase_missing_btn;
	Button *about_btn;

	EditorAssetLibrary *asset_library = nullptr;

	ProjectListFilter *project_filter;
	ProjectListFilter *project_order_filter;
	Label *loading_label;

	FileDialog *scan_dir;
	ConfirmationDialog *language_restart_ask;

	ConfirmationDialog *erase_ask;
	Label *erase_ask_label;
	CheckBox *delete_project_contents;

	ConfirmationDialog *erase_missing_ask;
	ConfirmationDialog *multi_open_ask;
	ConfirmationDialog *multi_run_ask;
	ConfirmationDialog *multi_scan_ask;
	ConfirmationDialog *ask_update_settings;
	ConfirmationDialog *open_templates = nullptr;
	EditorAbout *about;
	AcceptDialog *run_error_diag;
	AcceptDialog *dialog_error;
	ProjectDialog *npdialog;

	HBoxContainer *local_projects_hb;
	TabContainer *tabs;
	ProjectList *_project_list;

	LinkButton *version_btn;
	OptionButton *language_btn;
	Control *gui_base;

	bool importing;

	void _open_asset_library();
	void _scan_projects();
	void _run_project();
	void _run_project_confirm();
	void _open_selected_projects();
	void _open_selected_projects_ask();
	void _import_project();
	void _new_project();
	void _rename_project();
	void _erase_project();
	void _erase_missing_projects();
	void _erase_project_confirm();
	void _erase_missing_projects_confirm();
	void _show_about();
	void _update_project_buttons();
	void _language_selected(int p_id);
	void _restart_confirm();
	void _scan_begin(const String &p_base);
	void _global_menu_action(const Variant &p_id, const Variant &p_meta);

	void _confirm_update_settings();

	void _load_recent_projects();
	void _on_project_created(const String &dir);
	void _on_projects_updated();
	void _scan_dir(const String &path, List<String> *r_projects);

	void _install_project(const String &p_zip_path, const String &p_title);

	void _dim_window();
	void _unhandled_input(const Ref<InputEvent> &p_ev);
	void _files_dropped(PoolStringArray p_files, int p_screen);
	void _scan_multiple_folders(PoolStringArray p_files);

	void _version_button_pressed();
	void _on_order_option_changed();
	void _on_filter_option_changed();
	void _on_tab_changed(int p_tab);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	ProjectManager();
	~ProjectManager();
};

class ProjectListFilter : public HBoxContainer {
	GDCLASS(ProjectListFilter, HBoxContainer);

public:
	enum FilterOption {
		FILTER_NAME,
		FILTER_PATH,
		FILTER_MODIFIED,
	};

private:
	friend class ProjectManager;

	OptionButton *filter_option;
	LineEdit *search_box;
	bool has_search_box;
	FilterOption _current_filter;

	void _search_text_changed(const String &p_newtext);
	void _filter_option_selected(int p_idx);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void _setup_filters(Vector<String> options);
	void add_filter_option();

	void add_search_box();
	// May return `nullptr` if the search box wasn't created yet, so check for validity
	// before using the returned value.
	LineEdit *get_search_box() const;

	void set_filter_size(int h_size);
	String get_search_term();
	FilterOption get_filter_option();
	void set_filter_option(FilterOption);
	ProjectListFilter();
	void clear();
};

#endif // PROJECT_MANAGER_H

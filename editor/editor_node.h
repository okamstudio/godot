/**************************************************************************/
/*  editor_node.h                                                         */
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

#ifndef EDITOR_NODE_H
#define EDITOR_NODE_H

#include "core/safe_refcount.h"
#include "editor/editor_data.h"
#include "editor/editor_folding.h"
#include "editor/editor_run.h"
#include "editor/inspector_dock.h"
#include "editor/property_editor.h"
#include "editor/scene_tree_dock.h"
#include "scene/gui/link_button.h"

typedef void (*EditorNodeInitCallback)();
typedef void (*EditorPluginInitializeCallback)();
typedef bool (*EditorBuildCallback)();

class AcceptDialog;
class AudioStreamPreviewGenerator;
class BackgroundProgress;
class CenterContainer;
class ConfirmationDialog;
class Control;
class DependencyEditor;
class DependencyErrorDialog;
class EditorAbout;
class EditorExport;
class EditorFeatureProfileManager;
class EditorFileServer;
class EditorInspector;
class EditorLayoutsDialog;
class EditorLog;
class EditorPlugin;
class EditorPluginList;
class EditorQuickOpen;
class EditorResourcePreview;
class EditorRunNative;
class EditorSettingsDialog;
class ExportTemplateManager;
class FileSystemDock;
class HSplitContainer;
class ImportDock;
class MenuButton;
class NodeDock;
class OrphanResourcesDialog;
class PaneDrag;
class Panel;
class PanelContainer;
class PluginConfigDialog;
class ProgressDialog;
class ProjectExportDialog;
class ProjectSettingsEditor;
class RunSettingsDialog;
class ScriptCreateDialog;
class TabContainer;
class Tabs;
class TextureProgress;
class ToolButton;
class VSplitContainer;

class EditorNode : public Node {
	GDCLASS(EditorNode, Node);

public:
	enum DockSlot {
		DOCK_SLOT_LEFT_UL,
		DOCK_SLOT_LEFT_BL,
		DOCK_SLOT_LEFT_UR,
		DOCK_SLOT_LEFT_BR,
		DOCK_SLOT_RIGHT_UL,
		DOCK_SLOT_RIGHT_BL,
		DOCK_SLOT_RIGHT_UR,
		DOCK_SLOT_RIGHT_BR,
		DOCK_SLOT_MAX
	};

	struct ExecuteThreadArgs {
		String path;
		List<String> args;
		String output;
		Thread execute_output_thread;
		Mutex execute_output_mutex;
		int exitcode;
		SafeFlag done;
	};

private:
	enum {
		HISTORY_SIZE = 64
	};

	enum MenuOptions {
		FILE_NEW_SCENE,
		FILE_NEW_INHERITED_SCENE,
		FILE_OPEN_SCENE,
		FILE_SAVE_SCENE,
		FILE_SAVE_AS_SCENE,
		FILE_SAVE_ALL_SCENES,
		FILE_SAVE_AND_RUN,
		FILE_SHOW_IN_FILESYSTEM,
		FILE_IMPORT_SUBSCENE,
		FILE_EXPORT_PROJECT,
		FILE_EXPORT_MESH_LIBRARY,
		FILE_INSTALL_ANDROID_SOURCE,
		FILE_EXPLORE_ANDROID_BUILD_TEMPLATES,
		FILE_EXPORT_TILESET,
		FILE_SAVE_OPTIMIZED,
		FILE_OPEN_RECENT,
		FILE_OPEN_OLD_SCENE,
		FILE_QUICK_OPEN,
		FILE_QUICK_OPEN_SCENE,
		FILE_QUICK_OPEN_SCRIPT,
		FILE_OPEN_PREV,
		FILE_CLOSE,
		FILE_CLOSE_OTHERS,
		FILE_CLOSE_RIGHT,
		FILE_CLOSE_ALL,
		FILE_CLOSE_ALL_AND_QUIT,
		FILE_CLOSE_ALL_AND_RUN_PROJECT_MANAGER,
		FILE_CLOSE_ALL_AND_RELOAD_CURRENT_PROJECT,
		FILE_QUIT,
		FILE_EXTERNAL_OPEN_SCENE,
		EDIT_UNDO,
		EDIT_REDO,
		EDIT_RELOAD_SAVED_SCENE,
		TOOLS_ORPHAN_RESOURCES,
		TOOLS_CUSTOM,
		RESOURCE_SAVE,
		RESOURCE_SAVE_AS,
		RUN_PLAY,

		RUN_STOP,
		RUN_PLAY_SCENE,
		RUN_PLAY_NATIVE,
		RUN_PLAY_CUSTOM_SCENE,
		RUN_SCENE_SETTINGS,
		RUN_SETTINGS,
		RUN_USER_DATA_FOLDER,
		RELOAD_CURRENT_PROJECT,
		RUN_PROJECT_MANAGER,
		RUN_FILE_SERVER,
		RUN_LIVE_DEBUG,
		RUN_DEBUG_COLLISONS,
		RUN_DEBUG_NAVIGATION,
		RUN_DEBUG_SHADER_FALLBACKS,
		RUN_DEPLOY_REMOTE_DEBUG,
		RUN_RELOAD_SCRIPTS,
		RUN_VCS_SETTINGS,
		RUN_VCS_SHUT_DOWN,
		SETTINGS_UPDATE_CONTINUOUSLY,
		SETTINGS_UPDATE_VITAL_ONLY,
		SETTINGS_UPDATE_WHEN_CHANGED,
		SETTINGS_UPDATE_ALWAYS,
		SETTINGS_UPDATE_CHANGES,
		SETTINGS_UPDATE_SPINNER_HIDE,
		SETTINGS_PREFERENCES,
		SETTINGS_LAYOUT_SAVE,
		SETTINGS_LAYOUT_DELETE,
		SETTINGS_LAYOUT_DEFAULT,
		SETTINGS_EDITOR_DATA_FOLDER,
		SETTINGS_EDITOR_CONFIG_FOLDER,
		SETTINGS_MANAGE_EXPORT_TEMPLATES,
		SETTINGS_MANAGE_FEATURE_PROFILES,
		SETTINGS_INSTALL_ANDROID_BUILD_TEMPLATE,
		SETTINGS_PICK_MAIN_SCENE,
		SETTINGS_TOGGLE_FULLSCREEN,
		SETTINGS_HELP,
		SCENE_TAB_CLOSE,

		EDITOR_SCREENSHOT,
		EDITOR_OPEN_SCREENSHOT,

		HELP_SEARCH,
		HELP_DOCS,
		HELP_QA,
		HELP_REPORT_A_BUG,
		HELP_SUGGEST_A_FEATURE,
		HELP_SEND_DOCS_FEEDBACK,
		HELP_COMMUNITY,
		HELP_ABOUT,
		HELP_SUPPORT_GODOT_DEVELOPMENT,

		SET_VIDEO_DRIVER_SAVE_AND_RESTART,

		GLOBAL_NEW_WINDOW,
		GLOBAL_SCENE,

		IMPORT_PLUGIN_BASE = 100,

		TOOL_MENU_BASE = 1000
	};

	enum ScriptNameCasing {
		SCENE_NAME_CASING_AUTO,
		SCENE_NAME_CASING_PASCAL_CASE,
		SCENE_NAME_CASING_SNAKE_CASE
	};

	Viewport *scene_root; //root of the scene being edited

	PanelContainer *scene_root_parent;
	Control *theme_base;
	Control *gui_base;
	VBoxContainer *main_vbox;
	OptionButton *video_driver;

	ConfirmationDialog *video_restart_dialog;

	int video_driver_current;
	String video_driver_request;
	void _video_driver_selected(int);
	void _update_video_driver_color();

	// Split containers

	HSplitContainer *left_l_hsplit;
	VSplitContainer *left_l_vsplit;
	HSplitContainer *left_r_hsplit;
	VSplitContainer *left_r_vsplit;
	HSplitContainer *main_hsplit;
	HSplitContainer *right_hsplit;
	VSplitContainer *right_l_vsplit;
	VSplitContainer *right_r_vsplit;

	VSplitContainer *center_split;

	// To access those easily by index
	Vector<VSplitContainer *> vsplits;
	Vector<HSplitContainer *> hsplits;

	// Main tabs

	Tabs *scene_tabs;
	PopupMenu *scene_tabs_context_menu;
	Panel *tab_preview_panel;
	TextureRect *tab_preview;
	int tab_closing;

	bool exiting;
	bool dimmed;

	int old_split_ofs;
	VSplitContainer *top_split;
	HBoxContainer *bottom_hb;
	Control *vp_base;
	PaneDrag *pd;

	HBoxContainer *menu_hb;
	Control *viewport;
	MenuButton *file_menu;
	MenuButton *project_menu;
	MenuButton *debug_menu;
	MenuButton *settings_menu;
	MenuButton *help_menu;
	PopupMenu *tool_menu;
	ToolButton *export_button;
	ToolButton *prev_scene;
	ToolButton *play_button;
	ToolButton *pause_button;
	ToolButton *stop_button;
	ToolButton *run_settings_button;
	ToolButton *play_scene_button;
	ToolButton *play_custom_scene_button;
	ToolButton *search_button;
	TextureProgress *audio_vu;

	Timer *screenshot_timer;

	PluginConfigDialog *plugin_config_dialog;

	RichTextLabel *load_errors;
	AcceptDialog *load_error_dialog;

	RichTextLabel *execute_outputs;
	AcceptDialog *execute_output_dialog;

	Ref<Theme> theme;

	PopupMenu *recent_scenes;
	SceneTreeDock *scene_tree_dock;
	InspectorDock *inspector_dock;
	NodeDock *node_dock;
	ImportDock *import_dock;
	FileSystemDock *filesystem_dock;
	EditorRunNative *run_native;

	ConfirmationDialog *confirmation;
	ConfirmationDialog *save_confirmation;
	ConfirmationDialog *import_confirmation;
	ConfirmationDialog *pick_main_scene;
	Button *select_current_scene_button;
	AcceptDialog *accept;
	AcceptDialog *save_accept;
	EditorAbout *about;
	AcceptDialog *warning;

	int overridden_default_layout;
	Ref<ConfigFile> default_layout;
	PopupMenu *editor_layouts;
	EditorLayoutsDialog *layout_dialog;

	ConfirmationDialog *custom_build_manage_templates;
	ConfirmationDialog *install_android_build_template;
	ConfirmationDialog *remove_android_build_template;

	EditorSettingsDialog *settings_config_dialog;
	RunSettingsDialog *run_settings_dialog;
	ProjectSettingsEditor *project_settings;
	PopupMenu *vcs_actions_menu;
	EditorFileDialog *file;
	ExportTemplateManager *export_template_manager;
	EditorFeatureProfileManager *feature_profile_manager;
	EditorFileDialog *file_templates;
	EditorFileDialog *file_export_lib;
	EditorFileDialog *file_script;
	EditorFileDialog *file_android_build_source;
	CheckBox *file_export_lib_merge;
	CheckBox *file_export_lib_apply_xforms;
	String current_path;
	MenuButton *update_spinner;

	String defer_load_scene;
	Node *_last_instanced_scene;

	EditorLog *log;
	CenterContainer *tabs_center;
	EditorQuickOpen *quick_open;
	EditorQuickOpen *quick_run;

	HBoxContainer *main_editor_button_vb;
	Vector<ToolButton *> main_editor_buttons;
	Vector<EditorPlugin *> editor_table;

	AudioStreamPreviewGenerator *preview_gen;
	ProgressDialog *progress_dialog;
	BackgroundProgress *progress_hb;

	DependencyErrorDialog *dependency_error;
	DependencyEditor *dependency_fixer;
	OrphanResourcesDialog *orphan_resources;
	ConfirmationDialog *open_imported;
	Button *new_inherited_button;
	String open_import_request;

	TabContainer *dock_slot[DOCK_SLOT_MAX];
	Rect2 dock_select_rect[DOCK_SLOT_MAX];
	int dock_select_rect_over;
	PopupPanel *dock_select_popup;
	Control *dock_select;
	ToolButton *dock_tab_move_left;
	ToolButton *dock_tab_move_right;
	int dock_popup_selected;
	Timer *dock_drag_timer;
	bool docks_visible;

	HBoxContainer *tabbar_container;
	ToolButton *distraction_free;
	ToolButton *scene_tab_add;

	bool scene_distraction;
	bool script_distraction;

	String _tmp_import_path;

	EditorExport *editor_export;

	Object *current;
	Ref<Resource> saving_resource;

	bool _playing_edited;
	String run_custom_filename;
	bool reference_resource_mem;
	bool save_external_resources_mem;
	uint64_t saved_version;
	uint64_t last_checked_version;
	bool unsaved_cache;
	String open_navigate;
	bool changing_scene;
	bool waiting_for_first_scan;

	bool waiting_for_sources_changed;

	uint64_t update_spinner_step_msec;
	uint64_t update_spinner_step_frame;
	int update_spinner_step;

	Vector<EditorPlugin *> editor_plugins;
	EditorPlugin *editor_plugin_screen;
	EditorPluginList *editor_plugins_over;
	EditorPluginList *editor_plugins_force_over;
	EditorPluginList *editor_plugins_force_input_forwarding;

	EditorHistory editor_history;
	EditorData editor_data;
	EditorRun editor_run;
	EditorSelection *editor_selection;
	ProjectExportDialog *project_export;
	EditorResourcePreview *resource_preview;
	EditorFolding editor_folding;

	EditorFileServer *file_server;

	struct BottomPanelItem {
		String name;
		Control *control;
		ToolButton *button;
	};

	Vector<BottomPanelItem> bottom_panel_items;

	PanelContainer *bottom_panel;
	HBoxContainer *bottom_panel_hb;
	HBoxContainer *bottom_panel_hb_editors;
	VBoxContainer *bottom_panel_vb;
	LinkButton *version_btn;
	ToolButton *bottom_panel_raise;
	ToolButton *bottom_panel_move;

	Tree *disk_changed_list;
	ConfirmationDialog *disk_changed;

	void _bottom_panel_raise_toggled(bool);
	void _bottom_panel_move_pressed();
	void _init_center_split(bool p_on_right);

	EditorInterface *editor_interface;

	void _bottom_panel_switch(bool p_enable, int p_idx);

	String external_file;
	List<String> previous_scenes;
	bool opening_prev;

	void _dialog_action(String p_file);

	void _edit_current(bool p_skip_foreign = false);
	void _dialog_display_save_error(String p_file, Error p_error);
	void _dialog_display_load_error(String p_file, Error p_error);

	int current_option;
	void _menu_option(int p_option);
	void _menu_confirm_current();
	void _menu_option_confirm(int p_option, bool p_confirmed);

	void _android_build_source_selected(const String &p_file);

	void _request_screenshot();
	void _screenshot(bool p_use_utc = false);
	void _save_screenshot(NodePath p_path);

	void _tool_menu_option(int p_idx);
	void _update_debug_options();
	void _update_file_menu_opened();
	void _update_file_menu_closed();

	void _on_plugin_ready(Object *p_script, const String &p_activate_name);
	void _remove_plugin_from_enabled(const String &p_name);

	void _fs_changed();
	void _resources_reimported(const Vector<String> &p_resources);
	void _sources_changed(bool p_exist);

	void _node_renamed();
	void _editor_select_next();
	void _editor_select_prev();
	void _editor_select(int p_which);
	void _set_scene_metadata(const String &p_file, int p_idx = -1);
	void _get_scene_metadata(const String &p_file);
	void _update_title();
	void _update_scene_tabs();
	void _version_control_menu_option(int p_idx);
	void _close_messages();
	void _show_messages();
	void _vp_resized();
	void _version_button_pressed();

	int _save_external_resources();

	bool _validate_scene_recursive(const String &p_filename, Node *p_node);
	void _save_scene(String p_file, int idx = -1);
	void _save_all_scenes();
	int _next_unsaved_scene(bool p_valid_filename, int p_start = 0);
	void _discard_changes(const String &p_str = String());

	void _inherit_request(String p_file);
	void _instance_request(const Vector<String> &p_files);

	void _display_top_editors(bool p_display);
	void _set_top_editors(Vector<EditorPlugin *> p_editor_plugins_over);
	void _set_editing_top_editors(Object *p_current_object);

	void _quick_opened();
	void _quick_run();

	void _run(bool p_current = false, const String &p_custom = "");

	void _add_to_recent_scenes(const String &p_scene);
	void _update_recent_scenes();
	void _open_recent_scene(int p_idx);
	void _global_menu_action(const Variant &p_id, const Variant &p_meta);
	void _dropped_files(const Vector<String> &p_files, int p_screen);
	void _add_dropped_files_recursive(const Vector<String> &p_files, String to_path);
	String _recent_scene;

	void _exit_editor();

	bool convert_old;

	void _unhandled_input(const Ref<InputEvent> &p_event);

	static void _load_error_notify(void *p_ud, const String &p_text);

	bool has_main_screen() const { return true; }

	String import_reload_fn;

	Set<FileDialog *> file_dialogs;
	Set<EditorFileDialog *> editor_file_dialogs;

	Map<String, Ref<Texture>> icon_type_cache;
	Map<Ref<Script>, Ref<Texture>> script_icon_cache;
	void _build_icon_type_cache();

	bool _initializing_addons;
	Map<String, EditorPlugin *> plugin_addons;

	static Ref<Texture> _file_dialog_get_icon(const String &p_path);
	static void _file_dialog_register(FileDialog *p_dialog);
	static void _file_dialog_unregister(FileDialog *p_dialog);
	static void _editor_file_dialog_register(EditorFileDialog *p_dialog);
	static void _editor_file_dialog_unregister(EditorFileDialog *p_dialog);

	void _remove_edited_scene(bool p_change_tab = true);
	void _remove_scene(int index, bool p_change_tab = true);
	bool _find_and_save_resource(RES p_res, Map<RES, bool> &processed, int32_t flags);
	bool _find_and_save_edited_subresources(Object *obj, Map<RES, bool> &processed, int32_t flags);
	void _save_edited_subresources(Node *scene, Map<RES, bool> &processed, int32_t flags);
	void _mark_unsaved_scenes();

	void _find_node_types(Node *p_node, int &count_2d, int &count_3d);
	void _save_scene_with_preview(String p_file, int p_idx = -1);

	Map<String, Set<String>> dependency_errors;

	static void _dependency_error_report(void *ud, const String &p_path, const String &p_dep, const String &p_type) {
		EditorNode *en = (EditorNode *)ud;
		if (!en->dependency_errors.has(p_path)) {
			en->dependency_errors[p_path] = Set<String>();
		}
		en->dependency_errors[p_path].insert(p_dep + "::" + p_type);
	}

	struct ExportDefer {
		String preset;
		String path;
		bool debug;
		bool pack_only;
	} export_defer;

	bool cmdline_export_mode;

	static EditorNode *singleton;

	static Vector<EditorNodeInitCallback> _init_callbacks;

	bool _find_scene_in_use(Node *p_node, const String &p_path) const;

	void _dock_select_input(const Ref<InputEvent> &p_input);
	void _dock_move_left();
	void _dock_move_right();
	void _dock_select_draw();
	void _dock_pre_popup(int p_which);
	void _dock_split_dragged(int ofs);
	void _dock_popup_exit();
	void _scene_tab_changed(int p_tab);
	void _scene_tab_closed(int p_tab, int option = SCENE_TAB_CLOSE);
	void _scene_tab_hover(int p_tab);
	void _scene_tab_exit();
	void _scene_tab_input(const Ref<InputEvent> &p_input);
	void _reposition_active_tab(int idx_to);
	void _thumbnail_done(const String &p_path, const Ref<Texture> &p_preview, const Ref<Texture> &p_small_preview, const Variant &p_udata);
	void _scene_tab_script_edited(int p_tab);

	Dictionary _get_main_scene_state();
	void _set_main_scene_state(Dictionary p_state, Node *p_for_scene);

	int _get_current_main_editor();

	void _save_docks();
	void _load_docks();
	void _save_docks_to_config(Ref<ConfigFile> p_layout, const String &p_section);
	void _load_docks_from_config(Ref<ConfigFile> p_layout, const String &p_section);
	void _update_dock_slots_visibility(bool p_keep_selected_tabs = false);
	void _dock_tab_changed(int p_tab);

	bool restoring_scenes;
	void _save_open_scenes_to_config(Ref<ConfigFile> p_layout, const String &p_section);
	void _load_open_scenes_from_config(Ref<ConfigFile> p_layout, const String &p_section);

	void _update_layouts_menu();
	void _layout_menu_option(int p_id);

	void _clear_undo_history();

	void _update_addon_config();

	static void _file_access_close_error_notify(const String &p_str);

	void _toggle_distraction_free_mode();

	enum {
		MAX_INIT_CALLBACKS = 128,
		MAX_BUILD_CALLBACKS = 128
	};

	void _inherit_imported(const String &p_action);
	void _open_imported();

	static int plugin_init_callback_count;
	static EditorPluginInitializeCallback plugin_init_callbacks[MAX_INIT_CALLBACKS];
	void _save_default_environment();

	static int build_callback_count;
	static EditorBuildCallback build_callbacks[MAX_BUILD_CALLBACKS];

	void _update_update_spinner();

	Vector<Ref<EditorResourceConversionPlugin>> resource_conversion_plugins;

	PrintHandlerList print_handler;
	static void _print_handler(void *p_this, const String &p_string, bool p_error);

	static void _resource_saved(RES p_resource, const String &p_path);
	static void _resource_loaded(RES p_resource, const String &p_path);

	void _resources_changed(const PoolVector<String> &p_resources);
	void _scan_external_changes();
	void _reload_modified_scenes();
	void _reload_project_settings();
	void _resave_scenes(String p_str);

	void _project_settings_changed();
	void _feature_profile_changed();
	bool _is_class_editor_disabled_by_feature_profile(const StringName &p_class);
	Ref<ImageTexture> _load_custom_class_icon(const String &p_path) const;

	static String _to_absolute_plugin_path(const String &p_path);

	void _pick_main_scene_custom_action(const String &p_custom_action_name);

protected:
	void _notification(int p_what);

	static void _bind_methods();

protected:
	friend class FileSystemDock;

	int get_current_tab();
	void set_current_tab(int p_tab);

public:
	bool call_build();

	static void add_plugin_init_callback(EditorPluginInitializeCallback p_callback);

	enum EditorTable {
		EDITOR_2D = 0,
		EDITOR_3D,
		EDITOR_SCRIPT,
		EDITOR_ASSETLIB
	};

	void set_visible_editor(EditorTable p_table) { _editor_select(p_table); }
	static EditorNode *get_singleton() { return singleton; }

	EditorPlugin *get_editor_plugin_screen() { return editor_plugin_screen; }
	EditorPluginList *get_editor_plugins_over() { return editor_plugins_over; }
	EditorPluginList *get_editor_plugins_force_over() { return editor_plugins_force_over; }
	EditorPluginList *get_editor_plugins_force_input_forwarding() { return editor_plugins_force_input_forwarding; }
	EditorInspector *get_inspector() { return inspector_dock->get_inspector(); }
	Container *get_inspector_dock_addon_area() { return inspector_dock->get_addon_area(); }
	ScriptCreateDialog *get_script_create_dialog() { return scene_tree_dock->get_script_create_dialog(); }

	ProjectSettingsEditor *get_project_settings() { return project_settings; }

	static void add_editor_plugin(EditorPlugin *p_editor, bool p_config_changed = false);
	static void remove_editor_plugin(EditorPlugin *p_editor, bool p_config_changed = false);

	static void disambiguate_filenames(const Vector<String> p_full_paths, Vector<String> &r_filenames);

	void new_inherited_scene() { _menu_option_confirm(FILE_NEW_INHERITED_SCENE, false); }

	void set_docks_visible(bool p_show);
	bool get_docks_visible() const;

	void set_distraction_free_mode(bool p_enter);
	bool is_distraction_free_mode_enabled() const;

	void add_control_to_dock(DockSlot p_slot, Control *p_control);
	void remove_control_from_dock(Control *p_control);

	void set_addon_plugin_enabled(String p_addon, bool p_enabled, bool p_config_changed = false);
	bool is_addon_plugin_enabled(const String &p_addon) const;

	void edit_node(Node *p_node);
	void edit_resource(const Ref<Resource> &p_resource) { inspector_dock->edit_resource(p_resource); };
	void open_resource(const String &p_type) { inspector_dock->open_resource(p_type); };

	void save_resource_in_path(const Ref<Resource> &p_resource, const String &p_path);
	void save_resource(const Ref<Resource> &p_resource);
	void save_resource_as(const Ref<Resource> &p_resource, const String &p_at_path = String());

	void merge_from_scene() { _menu_option_confirm(FILE_IMPORT_SUBSCENE, false); }

	void show_about() { _menu_option_confirm(HELP_ABOUT, false); }

	static bool has_unsaved_changes() { return singleton->unsaved_cache; }

	static HBoxContainer *get_menu_hb() { return singleton->menu_hb; }

	void push_item(Object *p_object, const String &p_property = "", bool p_inspector_only = false);
	void edit_item(Object *p_object);
	void edit_item_resource(RES p_resource);
	bool item_has_editor(Object *p_object);
	void hide_top_editors();

	void select_editor_by_name(const String &p_name);

	void open_request(const String &p_path);

	bool is_changing_scene() const;

	static EditorLog *get_log() { return singleton->log; }
	Control *get_viewport();

	void set_edited_scene(Node *p_scene);

	Node *get_edited_scene() { return editor_data.get_edited_scene_root(); }

	Viewport *get_scene_root() { return scene_root; } //root of the scene being edited

	void fix_dependencies(const String &p_for_file);
	int new_scene();
	Error load_scene(const String &p_scene, bool p_ignore_broken_deps = false, bool p_set_inherited = false, bool p_clear_errors = true, bool p_force_open_imported = false, bool p_silent_change_tab = false);
	Error load_resource(const String &p_resource, bool p_ignore_broken_deps = false);

	bool is_scene_open(const String &p_path);

	void set_current_version(uint64_t p_version);
	void set_current_scene(int p_idx);

	static EditorData &get_editor_data() { return singleton->editor_data; }
	static EditorFolding &get_editor_folding() { return singleton->editor_folding; }
	EditorHistory *get_editor_history() { return &editor_history; }

	static VSplitContainer *get_top_split() { return singleton->top_split; }

	void request_instance_scene(const String &p_path);
	void request_instance_scenes(const Vector<String> &p_files);
	FileSystemDock *get_filesystem_dock();
	ImportDock *get_import_dock();
	SceneTreeDock *get_scene_tree_dock();
	InspectorDock *get_inspector_dock();
	static UndoRedo *get_undo_redo() { return &singleton->editor_data.get_undo_redo(); }

	EditorSelection *get_editor_selection() { return editor_selection; }

	void set_convert_old_scene(bool p_old) { convert_old = p_old; }

	void notify_child_process_exited();

	OS::ProcessID get_child_process_id() const { return editor_run.get_pid(); }
	void stop_child_process();

	Ref<Theme> get_editor_theme() const { return theme; }
	Ref<Script> get_object_custom_type_base(const Object *p_object) const;
	StringName get_object_custom_type_name(const Object *p_object) const;
	Ref<Texture> get_object_icon(const Object *p_object, const String &p_fallback = "Object");
	Ref<Texture> get_class_icon(const String &p_class, const String &p_fallback = "Object") const;

	void show_accept(const String &p_text, const String &p_title);
	void show_save_accept(const String &p_text, const String &p_title);
	void show_warning(const String &p_text, const String &p_title = TTR("Warning!"));

	void _copy_warning(const String &p_str);

	Error export_preset(const String &p_preset, const String &p_path, bool p_debug, bool p_pack_only);

	static void register_editor_types();
	static void unregister_editor_types();

	Control *get_gui_base() { return gui_base; }
	Control *get_theme_base() { return gui_base->get_parent_control(); }

	static void add_io_error(const String &p_error);

	static void progress_add_task(const String &p_task, const String &p_label, int p_steps, bool p_can_cancel = false);
	static bool progress_task_step(const String &p_task, const String &p_state, int p_step = -1, bool p_force_refresh = true);
	static void progress_end_task(const String &p_task);

	static void progress_add_task_bg(const String &p_task, const String &p_label, int p_steps);
	static void progress_task_step_bg(const String &p_task, int p_step = -1);
	static void progress_end_task_bg(const String &p_task);

	void save_scene_to_path(String p_file, bool p_with_preview = true) {
		if (p_with_preview) {
			_save_scene_with_preview(p_file);
		} else {
			_save_scene(p_file);
		}
	}

	bool is_scene_in_use(const String &p_path);

	void save_layout();

	void open_export_template_manager();

	void reload_scene(const String &p_path);

	bool is_exiting() const { return exiting; }

	ToolButton *get_pause_button() { return pause_button; }

	ToolButton *add_bottom_panel_item(String p_text, Control *p_item);
	bool are_bottom_panels_hidden() const;
	void make_bottom_panel_item_visible(Control *p_item);
	void raise_bottom_panel_item(Control *p_item);
	void hide_bottom_panel();
	void remove_bottom_panel_item(Control *p_item);

	Variant drag_resource(const Ref<Resource> &p_res, Control *p_from);
	Variant drag_files_and_dirs(const Vector<String> &p_paths, Control *p_from);

	void add_tool_menu_item(const String &p_name, Object *p_handler, const String &p_callback, const Variant &p_ud = Variant());
	void add_tool_submenu_item(const String &p_name, PopupMenu *p_submenu);
	void remove_tool_menu_item(const String &p_name);

	void save_all_scenes();
	void save_scene_list(Vector<String> p_scene_filenames);
	void restart_editor();

	void dim_editor(bool p_dimming, bool p_force_dim = false);
	bool is_editor_dimmed() const;

	void edit_current() { _edit_current(); };

	void update_keying() const { inspector_dock->update_keying(); };
	bool has_scenes_in_session();

	int execute_and_show_output(const String &p_title, const String &p_path, const List<String> &p_arguments, bool p_close_on_ok = true, bool p_close_on_errors = false);

	EditorNode();
	~EditorNode();

	void add_resource_conversion_plugin(const Ref<EditorResourceConversionPlugin> &p_plugin);
	void remove_resource_conversion_plugin(const Ref<EditorResourceConversionPlugin> &p_plugin);
	Vector<Ref<EditorResourceConversionPlugin>> find_resource_conversion_plugin(const Ref<Resource> &p_for_resource);

	static void add_init_callback(EditorNodeInitCallback p_callback) { _init_callbacks.push_back(p_callback); }
	static void add_build_callback(EditorBuildCallback p_callback);

	bool ensure_main_scene(bool p_from_native);

	void run_play();
	void run_play_current();
	void run_play_custom(const String &p_custom);
	void run_stop();
	bool is_run_playing() const;
	String get_run_playing_scene() const;
};

struct EditorProgress {
	String task;
	bool step(const String &p_state, int p_step = -1, bool p_force_refresh = true) { return EditorNode::progress_task_step(task, p_state, p_step, p_force_refresh); }
	EditorProgress(const String &p_task, const String &p_label, int p_amount, bool p_can_cancel = false) {
		EditorNode::progress_add_task(p_task, p_label, p_amount, p_can_cancel);
		task = p_task;
	}
	~EditorProgress() { EditorNode::progress_end_task(task); }
};

class EditorPluginList : public Object {
private:
	Vector<EditorPlugin *> plugins_list;

public:
	void set_plugins_list(Vector<EditorPlugin *> p_plugins_list) {
		plugins_list = p_plugins_list;
	}

	Vector<EditorPlugin *> &get_plugins_list() {
		return plugins_list;
	}

	void make_visible(bool p_visible);
	void edit(Object *p_object);
	bool forward_gui_input(const Ref<InputEvent> &p_event);
	void forward_canvas_draw_over_viewport(Control *p_overlay);
	void forward_canvas_force_draw_over_viewport(Control *p_overlay);
	bool forward_spatial_gui_input(Camera *p_camera, const Ref<InputEvent> &p_event, bool serve_when_force_input_enabled);
	void forward_spatial_draw_over_viewport(Control *p_overlay);
	void forward_spatial_force_draw_over_viewport(Control *p_overlay);
	void add_plugin(EditorPlugin *p_plugin);
	void remove_plugin(EditorPlugin *p_plugin);
	void clear();
	bool empty();

	EditorPluginList();
	~EditorPluginList();
};

struct EditorProgressBG {
	String task;
	void step(int p_step = -1) { EditorNode::progress_task_step_bg(task, p_step); }
	EditorProgressBG(const String &p_task, const String &p_label, int p_amount) {
		EditorNode::progress_add_task_bg(p_task, p_label, p_amount);
		task = p_task;
	}
	~EditorProgressBG() { EditorNode::progress_end_task_bg(task); }
};

#endif // EDITOR_NODE_H

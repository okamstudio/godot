/**************************************************************************/
/*  editor_plugin.h                                                       */
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

#ifndef EDITOR_PLUGIN_H
#define EDITOR_PLUGIN_H

#include "core/io/config_file.h"
#include "editor/plugins/editor_context_menu_plugin.h"
#include "scene/3d/camera_3d.h"
#include "scene/gui/control.h"

class Node3D;
class Button;
class PopupMenu;
class EditorDebuggerPlugin;
class EditorExport;
class EditorExportPlugin;
class EditorExportPlatform;
class EditorImportPlugin;
class EditorInspectorPlugin;
class EditorInterface;
class EditorNode3DGizmoPlugin;
class EditorResourceConversionPlugin;
class EditorSceneFormatImporter;
class EditorScenePostImportPlugin;
class EditorToolAddons;
class EditorTranslationParserPlugin;
class EditorUndoRedoManager;
class ScriptCreateDialog;

class EditorPlugin : public Node {
	GDCLASS(EditorPlugin, Node);
	friend class EditorData;

	bool input_event_forwarding_always_enabled = false;
	bool force_draw_over_forwarding_enabled = false;

	String last_main_screen_name;
	String plugin_version;

#ifndef DISABLE_DEPRECATED
	void _editor_project_settings_changed();
#endif

public:
	enum CustomControlContainer {
		CONTAINER_TOOLBAR,
		CONTAINER_SPATIAL_EDITOR_MENU,
		CONTAINER_SPATIAL_EDITOR_SIDE_LEFT,
		CONTAINER_SPATIAL_EDITOR_SIDE_RIGHT,
		CONTAINER_SPATIAL_EDITOR_BOTTOM,
		CONTAINER_CANVAS_EDITOR_MENU,
		CONTAINER_CANVAS_EDITOR_SIDE_LEFT,
		CONTAINER_CANVAS_EDITOR_SIDE_RIGHT,
		CONTAINER_CANVAS_EDITOR_BOTTOM,
		CONTAINER_INSPECTOR_BOTTOM,
		CONTAINER_PROJECT_SETTING_TAB_LEFT,
		CONTAINER_PROJECT_SETTING_TAB_RIGHT,
	};

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

	enum AfterGUIInput {
		AFTER_GUI_INPUT_PASS,
		AFTER_GUI_INPUT_STOP,
		AFTER_GUI_INPUT_CUSTOM,
	};

protected:
	void _notification(int p_what);

	static void _bind_methods();
	EditorUndoRedoManager *get_undo_redo();

	void add_custom_type(const String &p_type, const String &p_base, const Ref<Script> &p_script, const Ref<Texture2D> &p_icon);
	void remove_custom_type(const String &p_type);

	GDVIRTUAL1R(bool, _forward_canvas_gui_input, Ref<InputEvent>)
	GDVIRTUAL1(_forward_canvas_draw_over_viewport, Control *)
	GDVIRTUAL1(_forward_canvas_force_draw_over_viewport, Control *)
	GDVIRTUAL2R(int, _forward_3d_gui_input, Camera3D *, Ref<InputEvent>)
	GDVIRTUAL1(_forward_3d_draw_over_viewport, Control *)
	GDVIRTUAL1(_forward_3d_force_draw_over_viewport, Control *)
	GDVIRTUAL0RC(String, _get_plugin_name)
	GDVIRTUAL0RC(Ref<Texture2D>, _get_plugin_icon)
	GDVIRTUAL0RC(bool, _has_main_screen)
	GDVIRTUAL1(_make_visible, bool)
	GDVIRTUAL1(_edit, Object *)
	GDVIRTUAL1RC(bool, _handles, Object *)
	GDVIRTUAL0RC(Dictionary, _get_state)
	GDVIRTUAL1(_set_state, Dictionary)
	GDVIRTUAL0(_clear)
	GDVIRTUAL1RC(String, _get_unsaved_status, String)
	GDVIRTUAL0(_save_external_data)
	GDVIRTUAL0(_apply_changes)
	GDVIRTUAL0RC(Vector<String>, _get_breakpoints)
	GDVIRTUAL1(_set_window_layout, Ref<ConfigFile>)
	GDVIRTUAL1(_get_window_layout, Ref<ConfigFile>)
	GDVIRTUAL0R(bool, _build)
	GDVIRTUAL0(_enable_plugin)
	GDVIRTUAL0(_disable_plugin)

#ifndef DISABLE_DEPRECATED
	Button *_add_control_to_bottom_panel_compat_88081(Control *p_control, const String &p_title);
	void _add_control_to_dock_compat_88081(DockSlot p_slot, Control *p_control);
	static void _bind_compatibility_methods();
#endif

public:
	//TODO: send a resource for editing to the editor node?

	void add_control_to_container(CustomControlContainer p_location, Control *p_control);
	void remove_control_from_container(CustomControlContainer p_location, Control *p_control);
	Button *add_control_to_bottom_panel(Control *p_control, const String &p_title, const Ref<Shortcut> &p_shortcut = nullptr);
	void add_control_to_dock(DockSlot p_slot, Control *p_control, const Ref<Shortcut> &p_shortcut = nullptr);
	void remove_control_from_docks(Control *p_control);
	void remove_control_from_bottom_panel(Control *p_control);

	void set_dock_tab_icon(Control *p_control, const Ref<Texture2D> &p_icon);

	void add_tool_menu_item(const String &p_name, const Callable &p_callable);
	void add_tool_submenu_item(const String &p_name, PopupMenu *p_submenu);
	void remove_tool_menu_item(const String &p_name);

	PopupMenu *get_export_as_menu();

	void set_input_event_forwarding_always_enabled();
	bool is_input_event_forwarding_always_enabled() { return input_event_forwarding_always_enabled; }

	void set_force_draw_over_forwarding_enabled();
	bool is_force_draw_over_forwarding_enabled() { return force_draw_over_forwarding_enabled; }

	void notify_main_screen_changed(const String &screen_name);
	void notify_scene_changed(const Node *scn_root);
	void notify_scene_closed(const String &scene_filepath);
	void notify_resource_saved(const Ref<Resource> &p_resource);
	void notify_scene_saved(const String &p_scene_filepath);

	virtual bool forward_canvas_gui_input(const Ref<InputEvent> &p_event);
	virtual void forward_canvas_draw_over_viewport(Control *p_overlay);
	virtual void forward_canvas_force_draw_over_viewport(Control *p_overlay);

	virtual EditorPlugin::AfterGUIInput forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event);
	virtual void forward_3d_draw_over_viewport(Control *p_overlay);
	virtual void forward_3d_force_draw_over_viewport(Control *p_overlay);

	virtual String get_plugin_name() const;
	virtual const Ref<Texture2D> get_plugin_icon() const;
	virtual String get_plugin_version() const;
	virtual void set_plugin_version(const String &p_version);
	virtual bool has_main_screen() const;
	virtual void make_visible(bool p_visible);
	virtual void selected_notify() {} //notify that it was raised by the user, not the editor
	virtual void edit(Object *p_object);
	virtual bool handles(Object *p_object) const;
	virtual bool can_auto_hide() const;
	virtual Dictionary get_state() const; //save editor state so it can't be reloaded when reloading scene
	virtual void set_state(const Dictionary &p_state); //restore editor state (likely was saved with the scene)
	virtual void clear(); // clear any temporary data in the editor, reset it (likely new scene or load another scene)
	virtual String get_unsaved_status(const String &p_for_scene = "") const;
	virtual void save_external_data(); // if editor references external resources/scenes, save them
	virtual void apply_changes(); // if changes are pending in editor, apply them
	virtual void get_breakpoints(List<String> *p_breakpoints);
	virtual bool get_remove_list(List<Node *> *p_list);
	virtual void set_window_layout(Ref<ConfigFile> p_layout);
	virtual void get_window_layout(Ref<ConfigFile> p_layout);
	virtual void edited_scene_changed() {} // if changes are pending in editor, apply them
	virtual bool build(); // builds with external tools. Returns true if safe to continue running scene.

	EditorInterface *get_editor_interface();
	ScriptCreateDialog *get_script_create_dialog();

	void add_undo_redo_inspector_hook_callback(Callable p_callable);
	void remove_undo_redo_inspector_hook_callback(Callable p_callable);

	int update_overlays() const;

	void queue_save_layout();

	void make_bottom_panel_item_visible(Control *p_item);
	void hide_bottom_panel();

	void add_translation_parser_plugin(const Ref<EditorTranslationParserPlugin> &p_parser);
	void remove_translation_parser_plugin(const Ref<EditorTranslationParserPlugin> &p_parser);

	void add_import_plugin(const Ref<EditorImportPlugin> &p_importer, bool p_first_priority = false);
	void remove_import_plugin(const Ref<EditorImportPlugin> &p_importer);

	void add_export_plugin(const Ref<EditorExportPlugin> &p_exporter);
	void remove_export_plugin(const Ref<EditorExportPlugin> &p_exporter);

	void add_export_platform(const Ref<EditorExportPlatform> &p_platform);
	void remove_export_platform(const Ref<EditorExportPlatform> &p_platform);

	void add_node_3d_gizmo_plugin(const Ref<EditorNode3DGizmoPlugin> &p_gizmo_plugin);
	void remove_node_3d_gizmo_plugin(const Ref<EditorNode3DGizmoPlugin> &p_gizmo_plugin);

	void add_inspector_plugin(const Ref<EditorInspectorPlugin> &p_plugin);
	void remove_inspector_plugin(const Ref<EditorInspectorPlugin> &p_plugin);

	void add_scene_format_importer_plugin(const Ref<EditorSceneFormatImporter> &p_importer, bool p_first_priority = false);
	void remove_scene_format_importer_plugin(const Ref<EditorSceneFormatImporter> &p_importer);

	void add_scene_post_import_plugin(const Ref<EditorScenePostImportPlugin> &p_importer, bool p_first_priority = false);
	void remove_scene_post_import_plugin(const Ref<EditorScenePostImportPlugin> &p_importer);

	void add_autoload_singleton(const String &p_name, const String &p_path);
	void remove_autoload_singleton(const String &p_name);

	void add_debugger_plugin(const Ref<EditorDebuggerPlugin> &p_plugin);
	void remove_debugger_plugin(const Ref<EditorDebuggerPlugin> &p_plugin);

	void add_resource_conversion_plugin(const Ref<EditorResourceConversionPlugin> &p_plugin);
	void remove_resource_conversion_plugin(const Ref<EditorResourceConversionPlugin> &p_plugin);

	void add_context_menu_plugin(EditorContextMenuPlugin::ContextMenuSlot p_slot, const Ref<EditorContextMenuPlugin> &p_plugin);
	void remove_context_menu_plugin(const Ref<EditorContextMenuPlugin> &p_plugin);

	void enable_plugin();
	void disable_plugin();

	EditorPlugin() {}
	virtual ~EditorPlugin() {}
};

VARIANT_ENUM_CAST(EditorPlugin::CustomControlContainer);
VARIANT_ENUM_CAST(EditorPlugin::DockSlot);
VARIANT_ENUM_CAST(EditorPlugin::AfterGUIInput);

typedef EditorPlugin *(*EditorPluginCreateFunc)();

class EditorPlugins {
	enum {
		MAX_CREATE_FUNCS = 128
	};

	static EditorPluginCreateFunc creation_funcs[MAX_CREATE_FUNCS];
	static int creation_func_count;

	template <typename T>
	static EditorPlugin *creator() {
		return memnew(T);
	}

public:
	static int get_plugin_count() { return creation_func_count; }
	static EditorPlugin *create(int p_idx) {
		ERR_FAIL_INDEX_V(p_idx, creation_func_count, nullptr);
		return creation_funcs[p_idx]();
	}

	template <typename T>
	static void add_by_type() {
		add_create_func(creator<T>);
	}

	static void add_create_func(EditorPluginCreateFunc p_func) {
		ERR_FAIL_COND(creation_func_count >= MAX_CREATE_FUNCS);
		creation_funcs[creation_func_count++] = p_func;
	}
};

#endif // EDITOR_PLUGIN_H

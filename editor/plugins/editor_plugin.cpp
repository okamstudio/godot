/**************************************************************************/
/*  editor_plugin.cpp                                                     */
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

#include "editor_plugin.h"
#include "editor_plugin.compat.inc"

#include "editor/debugger/editor_debugger_node.h"
#include "editor/editor_dock_manager.h"
#include "editor/editor_file_system.h"
#include "editor/editor_inspector.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_translation_parser.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/export/editor_export.h"
#include "editor/export/editor_export_platform.h"
#include "editor/gui/editor_bottom_panel.h"
#include "editor/gui/editor_title_bar.h"
#include "editor/import/3d/resource_importer_scene.h"
#include "editor/import/editor_import_plugin.h"
#include "editor/inspector_dock.h"
#include "editor/plugins/canvas_item_editor_plugin.h"
#include "editor/plugins/editor_debugger_plugin.h"
#include "editor/plugins/editor_resource_conversion_plugin.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor/project_settings_editor.h"
#include "editor/scene_tree_dock.h"
#include "scene/3d/camera_3d.h"
#include "scene/gui/popup_menu.h"

void EditorPlugin::add_custom_type(const String &p_type, const String &p_base, const Ref<Script> &p_script, const Ref<Texture2D> &p_icon) {
	EditorNode::get_editor_data().add_custom_type(p_type, p_base, p_script, p_icon);
}

void EditorPlugin::remove_custom_type(const String &p_type) {
	EditorNode::get_editor_data().remove_custom_type(p_type);
}

void EditorPlugin::add_autoload_singleton(const String &p_name, const String &p_path) {
	if (p_path.begins_with("res://")) {
		EditorNode::get_singleton()->get_project_settings()->get_autoload_settings()->autoload_add(p_name, p_path);
	} else {
		const Ref<Script> plugin_script = static_cast<Ref<Script>>(get_script());
		ERR_FAIL_COND(plugin_script.is_null());
		const String script_base_path = plugin_script->get_path().get_base_dir();
		EditorNode::get_singleton()->get_project_settings()->get_autoload_settings()->autoload_add(p_name, script_base_path.path_join(p_path));
	}
}

void EditorPlugin::remove_autoload_singleton(const String &p_name) {
	EditorNode::get_singleton()->get_project_settings()->get_autoload_settings()->autoload_remove(p_name);
}

Button *EditorPlugin::add_control_to_bottom_panel(Control *p_control, const String &p_title, const Ref<Shortcut> &p_shortcut) {
	ERR_FAIL_NULL_V(p_control, nullptr);
	return EditorNode::get_bottom_panel()->add_item(p_title, p_control, p_shortcut);
}

void EditorPlugin::add_control_to_dock(DockSlot p_slot, Control *p_control, const Ref<Shortcut> &p_shortcut) {
	ERR_FAIL_NULL(p_control);
	EditorDockManager::get_singleton()->add_dock(p_control, String(), EditorDockManager::DockSlot(p_slot), p_shortcut);
}

void EditorPlugin::remove_control_from_docks(Control *p_control) {
	ERR_FAIL_NULL(p_control);
	EditorDockManager::get_singleton()->remove_dock(p_control);
}

void EditorPlugin::remove_control_from_bottom_panel(Control *p_control) {
	ERR_FAIL_NULL(p_control);
	EditorNode::get_bottom_panel()->remove_item(p_control);
}

void EditorPlugin::set_dock_tab_icon(Control *p_control, const Ref<Texture2D> &p_icon) {
	ERR_FAIL_NULL(p_control);
	EditorDockManager::get_singleton()->set_dock_tab_icon(p_control, p_icon);
}

void EditorPlugin::add_control_to_container(CustomControlContainer p_location, Control *p_control) {
	ERR_FAIL_NULL(p_control);

	switch (p_location) {
		case CONTAINER_TOOLBAR: {
			EditorNode::get_title_bar()->add_child(p_control);
		} break;

		case CONTAINER_SPATIAL_EDITOR_MENU: {
			Node3DEditor::get_singleton()->add_control_to_menu_panel(p_control);

		} break;
		case CONTAINER_SPATIAL_EDITOR_SIDE_LEFT: {
			Node3DEditor::get_singleton()->add_control_to_left_panel(p_control);
		} break;
		case CONTAINER_SPATIAL_EDITOR_SIDE_RIGHT: {
			Node3DEditor::get_singleton()->add_control_to_right_panel(p_control);
		} break;
		case CONTAINER_SPATIAL_EDITOR_BOTTOM: {
			Node3DEditor::get_singleton()->get_shader_split()->add_child(p_control);

		} break;
		case CONTAINER_CANVAS_EDITOR_MENU: {
			CanvasItemEditor::get_singleton()->add_control_to_menu_panel(p_control);

		} break;
		case CONTAINER_CANVAS_EDITOR_SIDE_LEFT: {
			CanvasItemEditor::get_singleton()->add_control_to_left_panel(p_control);
		} break;
		case CONTAINER_CANVAS_EDITOR_SIDE_RIGHT: {
			CanvasItemEditor::get_singleton()->add_control_to_right_panel(p_control);
		} break;
		case CONTAINER_CANVAS_EDITOR_BOTTOM: {
			CanvasItemEditor::get_singleton()->get_bottom_split()->add_child(p_control);

		} break;
		case CONTAINER_INSPECTOR_BOTTOM: {
			InspectorDock::get_singleton()->get_addon_area()->add_child(p_control);

		} break;
		case CONTAINER_PROJECT_SETTING_TAB_LEFT: {
			ProjectSettingsEditor::get_singleton()->get_tabs()->add_child(p_control);
			ProjectSettingsEditor::get_singleton()->get_tabs()->move_child(p_control, 0);

		} break;
		case CONTAINER_PROJECT_SETTING_TAB_RIGHT: {
			ProjectSettingsEditor::get_singleton()->get_tabs()->add_child(p_control);

		} break;
	}
}

void EditorPlugin::remove_control_from_container(CustomControlContainer p_location, Control *p_control) {
	ERR_FAIL_NULL(p_control);

	switch (p_location) {
		case CONTAINER_TOOLBAR: {
			EditorNode::get_title_bar()->remove_child(p_control);
		} break;

		case CONTAINER_SPATIAL_EDITOR_MENU: {
			Node3DEditor::get_singleton()->remove_control_from_menu_panel(p_control);

		} break;
		case CONTAINER_SPATIAL_EDITOR_SIDE_LEFT: {
			Node3DEditor::get_singleton()->remove_control_from_left_panel(p_control);
		} break;
		case CONTAINER_SPATIAL_EDITOR_SIDE_RIGHT: {
			Node3DEditor::get_singleton()->remove_control_from_right_panel(p_control);
		} break;
		case CONTAINER_SPATIAL_EDITOR_BOTTOM: {
			Node3DEditor::get_singleton()->get_shader_split()->remove_child(p_control);

		} break;
		case CONTAINER_CANVAS_EDITOR_MENU: {
			CanvasItemEditor::get_singleton()->remove_control_from_menu_panel(p_control);

		} break;
		case CONTAINER_CANVAS_EDITOR_SIDE_LEFT: {
			CanvasItemEditor::get_singleton()->remove_control_from_left_panel(p_control);
		} break;
		case CONTAINER_CANVAS_EDITOR_SIDE_RIGHT: {
			CanvasItemEditor::get_singleton()->remove_control_from_right_panel(p_control);
		} break;
		case CONTAINER_CANVAS_EDITOR_BOTTOM: {
			CanvasItemEditor::get_singleton()->get_bottom_split()->remove_child(p_control);

		} break;
		case CONTAINER_INSPECTOR_BOTTOM: {
			InspectorDock::get_singleton()->get_addon_area()->remove_child(p_control);

		} break;
		case CONTAINER_PROJECT_SETTING_TAB_LEFT:
		case CONTAINER_PROJECT_SETTING_TAB_RIGHT: {
			ProjectSettingsEditor::get_singleton()->get_tabs()->remove_child(p_control);

		} break;
	}
}

void EditorPlugin::add_tool_menu_item(const String &p_name, const Callable &p_callable) {
	EditorNode::get_singleton()->add_tool_menu_item(p_name, p_callable);
}

void EditorPlugin::add_tool_submenu_item(const String &p_name, PopupMenu *p_submenu) {
	ERR_FAIL_NULL(p_submenu);
	EditorNode::get_singleton()->add_tool_submenu_item(p_name, p_submenu);
}

void EditorPlugin::remove_tool_menu_item(const String &p_name) {
	EditorNode::get_singleton()->remove_tool_menu_item(p_name);
}

PopupMenu *EditorPlugin::get_export_as_menu() {
	return EditorNode::get_singleton()->get_export_as_menu();
}

void EditorPlugin::set_input_event_forwarding_always_enabled() {
	input_event_forwarding_always_enabled = true;
	EditorPluginList *always_input_forwarding_list = EditorNode::get_singleton()->get_editor_plugins_force_input_forwarding();
	always_input_forwarding_list->add_plugin(this);
}

void EditorPlugin::set_force_draw_over_forwarding_enabled() {
	force_draw_over_forwarding_enabled = true;
	EditorPluginList *always_draw_over_forwarding_list = EditorNode::get_singleton()->get_editor_plugins_force_over();
	always_draw_over_forwarding_list->add_plugin(this);
}

void EditorPlugin::notify_scene_changed(const Node *scn_root) {
	emit_signal(SNAME("scene_changed"), scn_root);
}

void EditorPlugin::notify_main_screen_changed(const String &screen_name) {
	if (screen_name == last_main_screen_name) {
		return;
	}

	emit_signal(SNAME("main_screen_changed"), screen_name);
	last_main_screen_name = screen_name;
}

void EditorPlugin::notify_scene_closed(const String &scene_filepath) {
	emit_signal(SNAME("scene_closed"), scene_filepath);
}

void EditorPlugin::notify_resource_saved(const Ref<Resource> &p_resource) {
	emit_signal(SNAME("resource_saved"), p_resource);
}

void EditorPlugin::notify_scene_saved(const String &p_scene_filepath) {
	emit_signal(SNAME("scene_saved"), p_scene_filepath);
}

bool EditorPlugin::forward_canvas_gui_input(const Ref<InputEvent> &p_event) {
	bool success = false;
	GDVIRTUAL_CALL(_forward_canvas_gui_input, p_event, success);
	return success;
}

void EditorPlugin::forward_canvas_draw_over_viewport(Control *p_overlay) {
	GDVIRTUAL_CALL(_forward_canvas_draw_over_viewport, p_overlay);
}

void EditorPlugin::forward_canvas_force_draw_over_viewport(Control *p_overlay) {
	GDVIRTUAL_CALL(_forward_canvas_force_draw_over_viewport, p_overlay);
}

// Updates the overlays of the 2D viewport or, if in 3D mode, of every 3D viewport.
int EditorPlugin::update_overlays() const {
	if (Node3DEditor::get_singleton()->is_visible()) {
		int count = 0;
		for (uint32_t i = 0; i < Node3DEditor::VIEWPORTS_COUNT; i++) {
			Node3DEditorViewport *vp = Node3DEditor::get_singleton()->get_editor_viewport(i);
			if (vp->is_visible()) {
				vp->update_surface();
				count++;
			}
		}
		return count;
	} else {
		// This will update the normal viewport itself as well
		CanvasItemEditor::get_singleton()->get_viewport_control()->queue_redraw();
		return 1;
	}
}

EditorPlugin::AfterGUIInput EditorPlugin::forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) {
	int success = EditorPlugin::AFTER_GUI_INPUT_PASS;
	GDVIRTUAL_CALL(_forward_3d_gui_input, p_camera, p_event, success);
	return static_cast<EditorPlugin::AfterGUIInput>(success);
}

void EditorPlugin::forward_3d_draw_over_viewport(Control *p_overlay) {
	GDVIRTUAL_CALL(_forward_3d_draw_over_viewport, p_overlay);
}

void EditorPlugin::forward_3d_force_draw_over_viewport(Control *p_overlay) {
	GDVIRTUAL_CALL(_forward_3d_force_draw_over_viewport, p_overlay);
}

String EditorPlugin::get_plugin_name() const {
	String name;
	GDVIRTUAL_CALL(_get_plugin_name, name);
	return name;
}

const Ref<Texture2D> EditorPlugin::get_plugin_icon() const {
	Ref<Texture2D> icon;
	GDVIRTUAL_CALL(_get_plugin_icon, icon);
	return icon;
}

String EditorPlugin::get_plugin_version() const {
	return plugin_version;
}

void EditorPlugin::set_plugin_version(const String &p_version) {
	plugin_version = p_version;
}

bool EditorPlugin::has_main_screen() const {
	bool success = false;
	GDVIRTUAL_CALL(_has_main_screen, success);
	return success;
}

void EditorPlugin::make_visible(bool p_visible) {
	GDVIRTUAL_CALL(_make_visible, p_visible);
}

void EditorPlugin::edit(Object *p_object) {
	GDVIRTUAL_CALL(_edit, p_object);
}

bool EditorPlugin::handles(Object *p_object) const {
	bool success = false;
	GDVIRTUAL_CALL(_handles, p_object, success);
	return success;
}

bool EditorPlugin::can_auto_hide() const {
	return true;
}

Dictionary EditorPlugin::get_state() const {
	Dictionary state;
	GDVIRTUAL_CALL(_get_state, state);
	return state;
}

void EditorPlugin::set_state(const Dictionary &p_state) {
	GDVIRTUAL_CALL(_set_state, p_state);
}

void EditorPlugin::clear() {
	GDVIRTUAL_CALL(_clear);
}

String EditorPlugin::get_unsaved_status(const String &p_for_scene) const {
	String ret;
	GDVIRTUAL_CALL(_get_unsaved_status, p_for_scene, ret);
	return ret;
}

void EditorPlugin::save_external_data() {
	GDVIRTUAL_CALL(_save_external_data);
}

// if changes are pending in editor, apply them
void EditorPlugin::apply_changes() {
	GDVIRTUAL_CALL(_apply_changes);
}

void EditorPlugin::get_breakpoints(List<String> *p_breakpoints) {
	PackedStringArray arr;
	if (GDVIRTUAL_CALL(_get_breakpoints, arr)) {
		for (int i = 0; i < arr.size(); i++) {
			p_breakpoints->push_back(arr[i]);
		}
	}
}

bool EditorPlugin::get_remove_list(List<Node *> *p_list) {
	return false;
}

void EditorPlugin::add_undo_redo_inspector_hook_callback(Callable p_callable) {
	EditorNode::get_editor_data().add_undo_redo_inspector_hook_callback(p_callable);
}

void EditorPlugin::remove_undo_redo_inspector_hook_callback(Callable p_callable) {
	EditorNode::get_editor_data().remove_undo_redo_inspector_hook_callback(p_callable);
}

void EditorPlugin::add_translation_parser_plugin(const Ref<EditorTranslationParserPlugin> &p_parser) {
	ERR_FAIL_COND(p_parser.is_null());
	EditorTranslationParser::get_singleton()->add_parser(p_parser, EditorTranslationParser::CUSTOM);
}

void EditorPlugin::remove_translation_parser_plugin(const Ref<EditorTranslationParserPlugin> &p_parser) {
	ERR_FAIL_COND(p_parser.is_null());
	EditorTranslationParser::get_singleton()->remove_parser(p_parser, EditorTranslationParser::CUSTOM);
}

void EditorPlugin::add_import_plugin(const Ref<EditorImportPlugin> &p_importer, bool p_first_priority) {
	ERR_FAIL_COND(p_importer.is_null());
	ResourceFormatImporter::get_singleton()->add_importer(p_importer, p_first_priority);
	// Plugins are now loaded during the first scan. It's important not to start another scan,
	// even a deferred one, as it would cause a scan during a scan at the next main thread iteration.
	if (!EditorFileSystem::get_singleton()->doing_first_scan()) {
		callable_mp(EditorFileSystem::get_singleton(), &EditorFileSystem::scan).call_deferred();
	}
}

void EditorPlugin::remove_import_plugin(const Ref<EditorImportPlugin> &p_importer) {
	ERR_FAIL_COND(p_importer.is_null());
	ResourceFormatImporter::get_singleton()->remove_importer(p_importer);
	// Plugins are now loaded during the first scan. It's important not to start another scan,
	// even a deferred one, as it would cause a scan during a scan at the next main thread iteration.
	if (!EditorNode::get_singleton()->is_exiting() && !EditorFileSystem::get_singleton()->doing_first_scan()) {
		callable_mp(EditorFileSystem::get_singleton(), &EditorFileSystem::scan).call_deferred();
	}
}

void EditorPlugin::add_export_plugin(const Ref<EditorExportPlugin> &p_exporter) {
	ERR_FAIL_COND(p_exporter.is_null());
	EditorExport::get_singleton()->add_export_plugin(p_exporter);
}

void EditorPlugin::remove_export_plugin(const Ref<EditorExportPlugin> &p_exporter) {
	ERR_FAIL_COND(p_exporter.is_null());
	EditorExport::get_singleton()->remove_export_plugin(p_exporter);
}

void EditorPlugin::add_export_platform(const Ref<EditorExportPlatform> &p_platform) {
	ERR_FAIL_COND(p_platform.is_null());
	EditorExport::get_singleton()->add_export_platform(p_platform);
}

void EditorPlugin::remove_export_platform(const Ref<EditorExportPlatform> &p_platform) {
	ERR_FAIL_COND(p_platform.is_null());
	EditorExport::get_singleton()->remove_export_platform(p_platform);
}

void EditorPlugin::add_node_3d_gizmo_plugin(const Ref<EditorNode3DGizmoPlugin> &p_gizmo_plugin) {
	ERR_FAIL_COND(p_gizmo_plugin.is_null());
	Node3DEditor::get_singleton()->add_gizmo_plugin(p_gizmo_plugin);
}

void EditorPlugin::remove_node_3d_gizmo_plugin(const Ref<EditorNode3DGizmoPlugin> &p_gizmo_plugin) {
	ERR_FAIL_COND(p_gizmo_plugin.is_null());
	Node3DEditor::get_singleton()->remove_gizmo_plugin(p_gizmo_plugin);
}

void EditorPlugin::add_inspector_plugin(const Ref<EditorInspectorPlugin> &p_plugin) {
	ERR_FAIL_COND(p_plugin.is_null());
	EditorInspector::add_inspector_plugin(p_plugin);
}

void EditorPlugin::remove_inspector_plugin(const Ref<EditorInspectorPlugin> &p_plugin) {
	ERR_FAIL_COND(p_plugin.is_null());
	EditorInspector::remove_inspector_plugin(p_plugin);
}

void EditorPlugin::add_scene_format_importer_plugin(const Ref<EditorSceneFormatImporter> &p_importer, bool p_first_priority) {
	ERR_FAIL_COND(p_importer.is_null());
	ResourceImporterScene::add_scene_importer(p_importer, p_first_priority);
}

void EditorPlugin::remove_scene_format_importer_plugin(const Ref<EditorSceneFormatImporter> &p_importer) {
	ERR_FAIL_COND(p_importer.is_null());
	ResourceImporterScene::remove_scene_importer(p_importer);
}

void EditorPlugin::add_scene_post_import_plugin(const Ref<EditorScenePostImportPlugin> &p_plugin, bool p_first_priority) {
	ResourceImporterScene::add_post_importer_plugin(p_plugin, p_first_priority);
}

void EditorPlugin::remove_scene_post_import_plugin(const Ref<EditorScenePostImportPlugin> &p_plugin) {
	ResourceImporterScene::remove_post_importer_plugin(p_plugin);
}

void EditorPlugin::add_context_menu_plugin(EditorContextMenuPlugin::ContextMenuSlot p_slot, const Ref<EditorContextMenuPlugin> &p_plugin) {
	EditorContextMenuPluginManager::get_singleton()->add_plugin(p_slot, p_plugin);
}

void EditorPlugin::remove_context_menu_plugin(const Ref<EditorContextMenuPlugin> &p_plugin) {
	EditorContextMenuPluginManager::get_singleton()->remove_plugin(p_plugin);
}

int find(const PackedStringArray &a, const String &v) {
	const String *r = a.ptr();
	for (int j = 0; j < a.size(); ++j) {
		if (r[j] == v) {
			return j;
		}
	}
	return -1;
}

void EditorPlugin::enable_plugin() {
	// Called when the plugin gets enabled in project settings, after it's added to the tree.
	// You can implement it to register autoloads.
	GDVIRTUAL_CALL(_enable_plugin);
}

void EditorPlugin::disable_plugin() {
	// Last function called when the plugin gets disabled in project settings.
	// Implement it to cleanup things from the project, such as unregister autoloads.
	GDVIRTUAL_CALL(_disable_plugin);
}

void EditorPlugin::set_window_layout(Ref<ConfigFile> p_layout) {
	GDVIRTUAL_CALL(_set_window_layout, p_layout);
}

void EditorPlugin::get_window_layout(Ref<ConfigFile> p_layout) {
	GDVIRTUAL_CALL(_get_window_layout, p_layout);
}

bool EditorPlugin::build() {
	bool success = true;
	GDVIRTUAL_CALL(_build, success);
	return success;
}

void EditorPlugin::queue_save_layout() {
	EditorNode::get_singleton()->save_editor_layout_delayed();
}

void EditorPlugin::make_bottom_panel_item_visible(Control *p_item) {
	EditorNode::get_bottom_panel()->make_item_visible(p_item);
}

void EditorPlugin::hide_bottom_panel() {
	EditorNode::get_bottom_panel()->hide_bottom_panel();
}

EditorInterface *EditorPlugin::get_editor_interface() {
	return EditorInterface::get_singleton();
}

ScriptCreateDialog *EditorPlugin::get_script_create_dialog() {
	return SceneTreeDock::get_singleton()->get_script_create_dialog();
}

void EditorPlugin::add_debugger_plugin(const Ref<EditorDebuggerPlugin> &p_plugin) {
	EditorDebuggerNode::get_singleton()->add_debugger_plugin(p_plugin);
}

void EditorPlugin::remove_debugger_plugin(const Ref<EditorDebuggerPlugin> &p_plugin) {
	EditorDebuggerNode::get_singleton()->remove_debugger_plugin(p_plugin);
}

void EditorPlugin::add_resource_conversion_plugin(const Ref<EditorResourceConversionPlugin> &p_plugin) {
	EditorNode::get_singleton()->add_resource_conversion_plugin(p_plugin);
}

void EditorPlugin::remove_resource_conversion_plugin(const Ref<EditorResourceConversionPlugin> &p_plugin) {
	EditorNode::get_singleton()->remove_resource_conversion_plugin(p_plugin);
}

#ifndef DISABLE_DEPRECATED
void EditorPlugin::_editor_project_settings_changed() {
	emit_signal(SNAME("project_settings_changed"));
}
#endif

void EditorPlugin::_notification(int p_what) {
#ifndef DISABLE_DEPRECATED
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &EditorPlugin::_editor_project_settings_changed));
		} break;

		case NOTIFICATION_EXIT_TREE: {
			ProjectSettings::get_singleton()->disconnect("settings_changed", callable_mp(this, &EditorPlugin::_editor_project_settings_changed));
		} break;
	}
#endif
}

void EditorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_control_to_container", "container", "control"), &EditorPlugin::add_control_to_container);
	ClassDB::bind_method(D_METHOD("add_control_to_bottom_panel", "control", "title", "shortcut"), &EditorPlugin::add_control_to_bottom_panel, DEFVAL(Ref<Shortcut>()));
	ClassDB::bind_method(D_METHOD("add_control_to_dock", "slot", "control", "shortcut"), &EditorPlugin::add_control_to_dock, DEFVAL(Ref<Shortcut>()));
	ClassDB::bind_method(D_METHOD("remove_control_from_docks", "control"), &EditorPlugin::remove_control_from_docks);
	ClassDB::bind_method(D_METHOD("remove_control_from_bottom_panel", "control"), &EditorPlugin::remove_control_from_bottom_panel);
	ClassDB::bind_method(D_METHOD("remove_control_from_container", "container", "control"), &EditorPlugin::remove_control_from_container);
	ClassDB::bind_method(D_METHOD("set_dock_tab_icon", "control", "icon"), &EditorPlugin::set_dock_tab_icon);
	ClassDB::bind_method(D_METHOD("add_tool_menu_item", "name", "callable"), &EditorPlugin::add_tool_menu_item);
	ClassDB::bind_method(D_METHOD("add_tool_submenu_item", "name", "submenu"), &EditorPlugin::add_tool_submenu_item);
	ClassDB::bind_method(D_METHOD("remove_tool_menu_item", "name"), &EditorPlugin::remove_tool_menu_item);
	ClassDB::bind_method(D_METHOD("get_export_as_menu"), &EditorPlugin::get_export_as_menu);
	ClassDB::bind_method(D_METHOD("add_custom_type", "type", "base", "script", "icon"), &EditorPlugin::add_custom_type);
	ClassDB::bind_method(D_METHOD("remove_custom_type", "type"), &EditorPlugin::remove_custom_type);

	ClassDB::bind_method(D_METHOD("add_autoload_singleton", "name", "path"), &EditorPlugin::add_autoload_singleton);
	ClassDB::bind_method(D_METHOD("remove_autoload_singleton", "name"), &EditorPlugin::remove_autoload_singleton);

	ClassDB::bind_method(D_METHOD("update_overlays"), &EditorPlugin::update_overlays);

	ClassDB::bind_method(D_METHOD("make_bottom_panel_item_visible", "item"), &EditorPlugin::make_bottom_panel_item_visible);
	ClassDB::bind_method(D_METHOD("hide_bottom_panel"), &EditorPlugin::hide_bottom_panel);

	ClassDB::bind_method(D_METHOD("get_undo_redo"), &EditorPlugin::get_undo_redo);
	ClassDB::bind_method(D_METHOD("add_undo_redo_inspector_hook_callback", "callable"), &EditorPlugin::add_undo_redo_inspector_hook_callback);
	ClassDB::bind_method(D_METHOD("remove_undo_redo_inspector_hook_callback", "callable"), &EditorPlugin::remove_undo_redo_inspector_hook_callback);
	ClassDB::bind_method(D_METHOD("queue_save_layout"), &EditorPlugin::queue_save_layout);
	ClassDB::bind_method(D_METHOD("add_translation_parser_plugin", "parser"), &EditorPlugin::add_translation_parser_plugin);
	ClassDB::bind_method(D_METHOD("remove_translation_parser_plugin", "parser"), &EditorPlugin::remove_translation_parser_plugin);
	ClassDB::bind_method(D_METHOD("add_import_plugin", "importer", "first_priority"), &EditorPlugin::add_import_plugin, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("remove_import_plugin", "importer"), &EditorPlugin::remove_import_plugin);
	ClassDB::bind_method(D_METHOD("add_scene_format_importer_plugin", "scene_format_importer", "first_priority"), &EditorPlugin::add_scene_format_importer_plugin, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("remove_scene_format_importer_plugin", "scene_format_importer"), &EditorPlugin::remove_scene_format_importer_plugin);
	ClassDB::bind_method(D_METHOD("add_scene_post_import_plugin", "scene_import_plugin", "first_priority"), &EditorPlugin::add_scene_post_import_plugin, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("remove_scene_post_import_plugin", "scene_import_plugin"), &EditorPlugin::remove_scene_post_import_plugin);
	ClassDB::bind_method(D_METHOD("add_export_plugin", "plugin"), &EditorPlugin::add_export_plugin);
	ClassDB::bind_method(D_METHOD("remove_export_plugin", "plugin"), &EditorPlugin::remove_export_plugin);
	ClassDB::bind_method(D_METHOD("add_export_platform", "platform"), &EditorPlugin::add_export_platform);
	ClassDB::bind_method(D_METHOD("remove_export_platform", "platform"), &EditorPlugin::remove_export_platform);
	ClassDB::bind_method(D_METHOD("add_node_3d_gizmo_plugin", "plugin"), &EditorPlugin::add_node_3d_gizmo_plugin);
	ClassDB::bind_method(D_METHOD("remove_node_3d_gizmo_plugin", "plugin"), &EditorPlugin::remove_node_3d_gizmo_plugin);
	ClassDB::bind_method(D_METHOD("add_inspector_plugin", "plugin"), &EditorPlugin::add_inspector_plugin);
	ClassDB::bind_method(D_METHOD("remove_inspector_plugin", "plugin"), &EditorPlugin::remove_inspector_plugin);
	ClassDB::bind_method(D_METHOD("add_resource_conversion_plugin", "plugin"), &EditorPlugin::add_resource_conversion_plugin);
	ClassDB::bind_method(D_METHOD("remove_resource_conversion_plugin", "plugin"), &EditorPlugin::remove_resource_conversion_plugin);
	ClassDB::bind_method(D_METHOD("set_input_event_forwarding_always_enabled"), &EditorPlugin::set_input_event_forwarding_always_enabled);
	ClassDB::bind_method(D_METHOD("set_force_draw_over_forwarding_enabled"), &EditorPlugin::set_force_draw_over_forwarding_enabled);
	ClassDB::bind_method(D_METHOD("add_context_menu_plugin", "slot", "plugin"), &EditorPlugin::add_context_menu_plugin);
	ClassDB::bind_method(D_METHOD("remove_context_menu_plugin", "plugin"), &EditorPlugin::remove_context_menu_plugin);

	ClassDB::bind_method(D_METHOD("get_editor_interface"), &EditorPlugin::get_editor_interface);
	ClassDB::bind_method(D_METHOD("get_script_create_dialog"), &EditorPlugin::get_script_create_dialog);
	ClassDB::bind_method(D_METHOD("add_debugger_plugin", "script"), &EditorPlugin::add_debugger_plugin);
	ClassDB::bind_method(D_METHOD("remove_debugger_plugin", "script"), &EditorPlugin::remove_debugger_plugin);
	ClassDB::bind_method(D_METHOD("get_plugin_version"), &EditorPlugin::get_plugin_version);

	GDVIRTUAL_BIND(_forward_canvas_gui_input, "event");
	GDVIRTUAL_BIND(_forward_canvas_draw_over_viewport, "viewport_control");
	GDVIRTUAL_BIND(_forward_canvas_force_draw_over_viewport, "viewport_control");
	GDVIRTUAL_BIND(_forward_3d_gui_input, "viewport_camera", "event");
	GDVIRTUAL_BIND(_forward_3d_draw_over_viewport, "viewport_control");
	GDVIRTUAL_BIND(_forward_3d_force_draw_over_viewport, "viewport_control");
	GDVIRTUAL_BIND(_get_plugin_name);
	GDVIRTUAL_BIND(_get_plugin_icon);
	GDVIRTUAL_BIND(_has_main_screen);
	GDVIRTUAL_BIND(_make_visible, "visible");
	GDVIRTUAL_BIND(_edit, "object");
	GDVIRTUAL_BIND(_handles, "object");
	GDVIRTUAL_BIND(_get_state);
	GDVIRTUAL_BIND(_set_state, "state");
	GDVIRTUAL_BIND(_clear);
	GDVIRTUAL_BIND(_get_unsaved_status, "for_scene");
	GDVIRTUAL_BIND(_save_external_data);
	GDVIRTUAL_BIND(_apply_changes);
	GDVIRTUAL_BIND(_get_breakpoints);
	GDVIRTUAL_BIND(_set_window_layout, "configuration");
	GDVIRTUAL_BIND(_get_window_layout, "configuration");
	GDVIRTUAL_BIND(_build);
	GDVIRTUAL_BIND(_enable_plugin);
	GDVIRTUAL_BIND(_disable_plugin);

	ADD_SIGNAL(MethodInfo("scene_changed", PropertyInfo(Variant::OBJECT, "scene_root", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
	ADD_SIGNAL(MethodInfo("scene_closed", PropertyInfo(Variant::STRING, "filepath")));
	ADD_SIGNAL(MethodInfo("main_screen_changed", PropertyInfo(Variant::STRING, "screen_name")));
	ADD_SIGNAL(MethodInfo("resource_saved", PropertyInfo(Variant::OBJECT, "resource", PROPERTY_HINT_RESOURCE_TYPE, "Resource")));
	ADD_SIGNAL(MethodInfo("scene_saved", PropertyInfo(Variant::STRING, "filepath")));
	ADD_SIGNAL(MethodInfo("project_settings_changed"));

	BIND_ENUM_CONSTANT(CONTAINER_TOOLBAR);
	BIND_ENUM_CONSTANT(CONTAINER_SPATIAL_EDITOR_MENU);
	BIND_ENUM_CONSTANT(CONTAINER_SPATIAL_EDITOR_SIDE_LEFT);
	BIND_ENUM_CONSTANT(CONTAINER_SPATIAL_EDITOR_SIDE_RIGHT);
	BIND_ENUM_CONSTANT(CONTAINER_SPATIAL_EDITOR_BOTTOM);
	BIND_ENUM_CONSTANT(CONTAINER_CANVAS_EDITOR_MENU);
	BIND_ENUM_CONSTANT(CONTAINER_CANVAS_EDITOR_SIDE_LEFT);
	BIND_ENUM_CONSTANT(CONTAINER_CANVAS_EDITOR_SIDE_RIGHT);
	BIND_ENUM_CONSTANT(CONTAINER_CANVAS_EDITOR_BOTTOM);
	BIND_ENUM_CONSTANT(CONTAINER_INSPECTOR_BOTTOM);
	BIND_ENUM_CONSTANT(CONTAINER_PROJECT_SETTING_TAB_LEFT);
	BIND_ENUM_CONSTANT(CONTAINER_PROJECT_SETTING_TAB_RIGHT);

	BIND_ENUM_CONSTANT(DOCK_SLOT_LEFT_UL);
	BIND_ENUM_CONSTANT(DOCK_SLOT_LEFT_BL);
	BIND_ENUM_CONSTANT(DOCK_SLOT_LEFT_UR);
	BIND_ENUM_CONSTANT(DOCK_SLOT_LEFT_BR);
	BIND_ENUM_CONSTANT(DOCK_SLOT_RIGHT_UL);
	BIND_ENUM_CONSTANT(DOCK_SLOT_RIGHT_BL);
	BIND_ENUM_CONSTANT(DOCK_SLOT_RIGHT_UR);
	BIND_ENUM_CONSTANT(DOCK_SLOT_RIGHT_BR);
	BIND_ENUM_CONSTANT(DOCK_SLOT_MAX);

	BIND_ENUM_CONSTANT(AFTER_GUI_INPUT_PASS);
	BIND_ENUM_CONSTANT(AFTER_GUI_INPUT_STOP);
	BIND_ENUM_CONSTANT(AFTER_GUI_INPUT_CUSTOM);
}

EditorUndoRedoManager *EditorPlugin::get_undo_redo() {
	return EditorUndoRedoManager::get_singleton();
}

EditorPluginCreateFunc EditorPlugins::creation_funcs[MAX_CREATE_FUNCS];

int EditorPlugins::creation_func_count = 0;

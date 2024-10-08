/**************************************************************************/
/*  editor_interface.h                                                    */
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

#ifndef EDITOR_INTERFACE_H
#define EDITOR_INTERFACE_H

#include "core/io/resource.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/object/script_language.h"

class Control;
class EditorCommandPalette;
class EditorFileSystem;
class EditorInspector;
class EditorPaths;
class EditorPlugin;
class EditorResourcePreview;
class EditorSelection;
class EditorSettings;
class EditorUndoRedoManager;
class FileSystemDock;
class Mesh;
class Node;
class PropertySelector;
class SceneTreeDialog;
class ScriptEditor;
class SubViewport;
class Texture2D;
class Theme;
class VBoxContainer;
class Window;

class EditorInterface : public Object {
	GDCLASS(EditorInterface, Object);

	static EditorInterface *singleton;

	// Editor dialogs.

	PropertySelector *property_selector = nullptr;
	SceneTreeDialog *node_selector = nullptr;

	void _node_selected(const NodePath &p_node_paths, const Callable &p_callback);
	void _node_selection_canceled(const Callable &p_callback);
	void _property_selected(const String &p_property_name, const Callable &p_callback);
	void _property_selection_canceled(const Callable &p_callback);
	void _quick_open(const String &p_file_path, const Callable &p_callback);
	void _call_dialog_callback(const Callable &p_callback, const Variant &p_selected, const String &p_context);

	// Editor tools.

	TypedArray<Texture2D> _make_mesh_previews(const TypedArray<Mesh> &p_meshes, int p_preview_size);

protected:
	static void _bind_methods();

#ifndef DISABLE_DEPRECATED
	void _popup_node_selector_bind_compat_94323(const Callable &p_callback, const TypedArray<StringName> &p_valid_types = TypedArray<StringName>());
	void _popup_property_selector_bind_compat_94323(Object *p_object, const Callable &p_callback, const PackedInt32Array &p_type_filter = PackedInt32Array());

	static void _bind_compatibility_methods();
#endif

public:
	static EditorInterface *get_singleton() { return singleton; }

	void restart_editor(bool p_save = true);

	// Editor tools.

	EditorCommandPalette *get_command_palette() const;
	EditorFileSystem *get_resource_file_system() const;
	EditorPaths *get_editor_paths() const;
	EditorResourcePreview *get_resource_previewer() const;
	EditorSelection *get_selection() const;
	Ref<EditorSettings> get_editor_settings() const;
	EditorUndoRedoManager *get_editor_undo_redo() const;

	Vector<Ref<Texture2D>> make_mesh_previews(const Vector<Ref<Mesh>> &p_meshes, Vector<Transform3D> *p_transforms, int p_preview_size);

	void set_plugin_enabled(const String &p_plugin, bool p_enabled);
	bool is_plugin_enabled(const String &p_plugin) const;

	// Editor GUI.

	Ref<Theme> get_editor_theme() const;

	Control *get_base_control() const;
	VBoxContainer *get_editor_main_screen() const;
	ScriptEditor *get_script_editor() const;
	SubViewport *get_editor_viewport_2d() const;
	SubViewport *get_editor_viewport_3d(int p_idx = 0) const;

	void set_main_screen_editor(const String &p_name);
	void set_distraction_free_mode(bool p_enter);
	bool is_distraction_free_mode_enabled() const;
	bool is_multi_window_enabled() const;

	float get_editor_scale() const;

	void popup_dialog(Window *p_dialog, const Rect2i &p_screen_rect = Rect2i());
	void popup_dialog_centered(Window *p_dialog, const Size2i &p_minsize = Size2i());
	void popup_dialog_centered_ratio(Window *p_dialog, float p_ratio = 0.8);
	void popup_dialog_centered_clamped(Window *p_dialog, const Size2i &p_size = Size2i(), float p_fallback_ratio = 0.75);

	String get_current_feature_profile() const;
	void set_current_feature_profile(const String &p_profile_name);

	// Editor dialogs.

	void popup_node_selector(const Callable &p_callback, const TypedArray<StringName> &p_valid_types = TypedArray<StringName>(), Node *p_current_value = nullptr);
	// Must use Vector<int> because exposing Vector<Variant::Type> is not supported.
	void popup_property_selector(Object *p_object, const Callable &p_callback, const PackedInt32Array &p_type_filter = PackedInt32Array(), const String &p_current_value = String());
	void popup_quick_open(const Callable &p_callback, const TypedArray<StringName> &p_base_types = TypedArray<StringName>());

	// Editor docks.

	FileSystemDock *get_file_system_dock() const;
	void select_file(const String &p_file);
	Vector<String> get_selected_paths() const;
	String get_current_path() const;
	String get_current_directory() const;

	EditorInspector *get_inspector() const;

	// Object/Resource/Node editing.

	void inspect_object(Object *p_obj, const String &p_for_property = String(), bool p_inspector_only = false);

	void edit_resource(const Ref<Resource> &p_resource);
	void edit_node(Node *p_node);
	void edit_script(const Ref<Script> &p_script, int p_line = -1, int p_col = 0, bool p_grab_focus = true);
	void open_scene_from_path(const String &scene_path);
	void reload_scene_from_path(const String &scene_path);

	PackedStringArray get_open_scenes() const;
	Node *get_edited_scene_root() const;

	Error save_scene();
	void save_scene_as(const String &p_scene, bool p_with_preview = true);
	void mark_scene_as_unsaved();
	void save_all_scenes();

	// Scene playback.

	void play_main_scene();
	void play_current_scene();
	void play_custom_scene(const String &scene_path);
	void stop_playing_scene();
	bool is_playing_scene() const;
	String get_playing_scene() const;

	void set_movie_maker_enabled(bool p_enabled);
	bool is_movie_maker_enabled() const;

#ifdef TOOLS_ENABLED
	virtual void get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const override;
#endif

	// Base.
	static void create();
	static void free();

	EditorInterface();
};

#endif // EDITOR_INTERFACE_H

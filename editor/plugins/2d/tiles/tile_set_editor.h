/**************************************************************************/
/*  tile_set_editor.h                                                     */
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

#ifndef TILE_SET_EDITOR_H
#define TILE_SET_EDITOR_H

#include "atlas_merging_dialog.h"
#include "scene/gui/tab_bar.h"
#include "scene/resources/2d/tile_set.h"
#include "tile_proxies_manager_dialog.h"
#include "tile_set_atlas_source_editor.h"
#include "tile_set_scenes_collection_source_editor.h"

class AcceptDialog;
class SpinBox;
class HBoxContainer;
class SplitContainer;
class EditorFileDialog;
class EditorInspectorPlugin;

class TileSetEditor : public Control {
	GDCLASS(TileSetEditor, Control);

	static TileSetEditor *singleton;

private:
	bool read_only = false;

	Ref<TileSet> tile_set;
	bool tile_set_changed_needs_update = false;
	HSplitContainer *split_container = nullptr;

	// TabBar.
	HBoxContainer *tile_set_toolbar = nullptr;
	TabBar *tabs_bar = nullptr;

	// Tiles.
	Label *no_source_selected_label = nullptr;
	TileSetAtlasSourceEditor *tile_set_atlas_source_editor = nullptr;
	TileSetScenesCollectionSourceEditor *tile_set_scenes_collection_source_editor = nullptr;

	void _drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);
	bool _can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	void _load_texture_files(const Vector<String> &p_paths);

	void _update_sources_list(int force_selected_id = -1);

	// Sources management.
	Button *sources_delete_button = nullptr;
	MenuButton *sources_add_button = nullptr;
	MenuButton *source_sort_button = nullptr;
	MenuButton *sources_advanced_menu_button = nullptr;
	ItemList *sources_list = nullptr;
	Ref<Texture2D> missing_texture_texture;
	void _source_selected(int p_source_index);
	void _source_delete_pressed();
	void _source_add_id_pressed(int p_id_pressed);
	void _sources_advanced_menu_id_pressed(int p_id_pressed);
	void _set_source_sort(int p_sort);

	EditorFileDialog *texture_file_dialog = nullptr;
	AtlasMergingDialog *atlas_merging_dialog = nullptr;
	TileProxiesManagerDialog *tile_proxies_manager_dialog = nullptr;

	bool first_edit = true;

	// Patterns.
	ItemList *patterns_item_list = nullptr;
	Label *patterns_help_label = nullptr;
	void _patterns_item_list_gui_input(const Ref<InputEvent> &p_event);
	void _pattern_preview_done(Ref<TileMapPattern> p_pattern, Ref<Texture2D> p_texture);
	bool select_last_pattern = false;
	void _update_patterns_list();

	// Expanded editor.
	PanelContainer *expanded_area = nullptr;
	Control *expanded_editor = nullptr;
	ObjectID expanded_editor_parent;
	LocalVector<SplitContainer *> disable_on_expand;

	void _tile_set_changed();
	void _tab_changed(int p_tab_changed);

	void _move_tile_set_array_element(Object *p_undo_redo, Object *p_edited, const String &p_array_prefix, int p_from_index, int p_to_pos);
	void _undo_redo_inspector_callback(Object *p_undo_redo, Object *p_edited, const String &p_property, const Variant &p_new_value);

protected:
	void _notification(int p_what);

public:
	_FORCE_INLINE_ static TileSetEditor *get_singleton() { return singleton; }

	void edit(Ref<TileSet> p_tile_set);

	void add_expanded_editor(Control *p_editor);
	void remove_expanded_editor();
	void register_split(SplitContainer *p_split);

	TileSetEditor();
};

class TileSourceInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(TileSourceInspectorPlugin, EditorInspectorPlugin);

	AcceptDialog *id_edit_dialog = nullptr;
	Label *id_label = nullptr;
	SpinBox *id_input = nullptr;
	Object *edited_source = nullptr;

	void _show_id_edit_dialog(Object *p_for_source);
	void _confirm_change_id();

public:
	virtual bool can_handle(Object *p_object) override;
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide = false) override;
};

#endif // TILE_SET_EDITOR_H

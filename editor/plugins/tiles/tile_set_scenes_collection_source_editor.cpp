/**************************************************************************/
/*  tile_set_scenes_collection_source_editor.cpp                          */
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

#include "tile_set_scenes_collection_source_editor.h"

#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/editor_resource_preview.h"
#include "editor/editor_settings.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/plugins/tiles/tile_set_editor.h"
#include "editor/themes/editor_scale.h"

#include "scene/gui/button.h"
#include "scene/gui/item_list.h"
#include "scene/gui/label.h"
#include "scene/gui/split_container.h"

void TileSetScenesCollectionSourceEditor::TileSetScenesCollectionProxyObject::set_id(int p_id) {
	ERR_FAIL_COND(p_id < 0);
	if (source_id == p_id) {
		return;
	}
	ERR_FAIL_COND_MSG(tile_set->has_source(p_id), vformat("Cannot change TileSet Scenes Collection source ID. Another TileSet source exists with id %d.", p_id));

	int previous_source = source_id;
	source_id = p_id; // source_id must be updated before, because it's used by the source list update.
	tile_set->set_source_id(previous_source, p_id);
	emit_signal(CoreStringName(changed), "id");
}

int TileSetScenesCollectionSourceEditor::TileSetScenesCollectionProxyObject::get_id() {
	return source_id;
}

bool TileSetScenesCollectionSourceEditor::TileSetScenesCollectionProxyObject::_set(const StringName &p_name, const Variant &p_value) {
	String name = p_name;
	if (name == "name") {
		// Use the resource_name property to store the source's name.
		name = "resource_name";
	}
	bool valid = false;
	tile_set_scenes_collection_source->set(name, p_value, &valid);
	if (valid) {
		emit_signal(CoreStringName(changed), String(name).utf8().get_data());
	}
	return valid;
}

bool TileSetScenesCollectionSourceEditor::TileSetScenesCollectionProxyObject::_get(const StringName &p_name, Variant &r_ret) const {
	if (!tile_set_scenes_collection_source) {
		return false;
	}
	String name = p_name;
	if (name == "name") {
		// Use the resource_name property to store the source's name.
		name = "resource_name";
	}
	bool valid = false;
	r_ret = tile_set_scenes_collection_source->get(name, &valid);
	return valid;
}

void TileSetScenesCollectionSourceEditor::TileSetScenesCollectionProxyObject::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::STRING, "name", PROPERTY_HINT_NONE, ""));
}

void TileSetScenesCollectionSourceEditor::TileSetScenesCollectionProxyObject::_bind_methods() {
	// -- Shape and layout --
	ClassDB::bind_method(D_METHOD("set_id", "id"), &TileSetScenesCollectionSourceEditor::TileSetScenesCollectionProxyObject::set_id);
	ClassDB::bind_method(D_METHOD("get_id"), &TileSetScenesCollectionSourceEditor::TileSetScenesCollectionProxyObject::get_id);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "id"), "set_id", "get_id");

	ADD_SIGNAL(MethodInfo("changed", PropertyInfo(Variant::STRING, "what")));
}

void TileSetScenesCollectionSourceEditor::TileSetScenesCollectionProxyObject::edit(Ref<TileSet> p_tile_set, TileSetScenesCollectionSource *p_tile_set_scenes_collection_source, int p_source_id) {
	ERR_FAIL_COND(p_tile_set.is_null());
	ERR_FAIL_NULL(p_tile_set_scenes_collection_source);
	ERR_FAIL_COND(p_source_id < 0);
	ERR_FAIL_COND(p_tile_set->get_source(p_source_id) != p_tile_set_scenes_collection_source);

	if (tile_set == p_tile_set && tile_set_scenes_collection_source == p_tile_set_scenes_collection_source && source_id == p_source_id) {
		return;
	}

	// Disconnect to changes.
	if (tile_set_scenes_collection_source) {
		tile_set_scenes_collection_source->disconnect(CoreStringName(property_list_changed), callable_mp((Object *)this, &Object::notify_property_list_changed));
	}

	tile_set = p_tile_set;
	tile_set_scenes_collection_source = p_tile_set_scenes_collection_source;
	source_id = p_source_id;

	// Connect to changes.
	if (tile_set_scenes_collection_source) {
		if (!tile_set_scenes_collection_source->is_connected(CoreStringName(property_list_changed), callable_mp((Object *)this, &Object::notify_property_list_changed))) {
			tile_set_scenes_collection_source->connect(CoreStringName(property_list_changed), callable_mp((Object *)this, &Object::notify_property_list_changed));
		}
	}

	notify_property_list_changed();
}

// -- Proxy object used by the tile inspector --
bool TileSetScenesCollectionSourceEditor::SceneTileProxyObject::_set(const StringName &p_name, const Variant &p_value) {
	if (!tile_set_scenes_collection_source) {
		return false;
	}

	if (p_name == "id") {
		int as_int = int(p_value);
		ERR_FAIL_COND_V(as_int < 0, false);
		ERR_FAIL_COND_V(tile_set_scenes_collection_source->has_scene_tile_id(as_int), false);
		tile_set_scenes_collection_source->set_scene_tile_id(scene_id, as_int);
		scene_id = as_int;
		emit_signal(CoreStringName(changed), "id");
		for (int i = 0; i < tile_set_scenes_collection_source_editor->scene_tiles_list->get_item_count(); i++) {
			if (int(tile_set_scenes_collection_source_editor->scene_tiles_list->get_item_metadata(i)) == scene_id) {
				tile_set_scenes_collection_source_editor->scene_tiles_list->select(i);
				break;
			}
		}
		return true;
	} else if (p_name == "scene") {
		tile_set_scenes_collection_source->set_scene_tile_scene(scene_id, p_value);
		emit_signal(CoreStringName(changed), "scene");
		return true;
	} else if (p_name == "display_placeholder") {
		tile_set_scenes_collection_source->set_scene_tile_display_placeholder(scene_id, p_value);
		emit_signal(CoreStringName(changed), "display_placeholder");
		return true;
	}

	return false;
}

bool TileSetScenesCollectionSourceEditor::SceneTileProxyObject::_get(const StringName &p_name, Variant &r_ret) const {
	if (!tile_set_scenes_collection_source) {
		return false;
	}

	if (p_name == "id") {
		r_ret = scene_id;
		return true;
	} else if (p_name == "scene") {
		r_ret = tile_set_scenes_collection_source->get_scene_tile_scene(scene_id);
		return true;
	} else if (p_name == "display_placeholder") {
		r_ret = tile_set_scenes_collection_source->get_scene_tile_display_placeholder(scene_id);
		return true;
	}

	return false;
}

void TileSetScenesCollectionSourceEditor::SceneTileProxyObject::_get_property_list(List<PropertyInfo> *p_list) const {
	if (!tile_set_scenes_collection_source) {
		return;
	}

	p_list->push_back(PropertyInfo(Variant::INT, "id", PROPERTY_HINT_NONE, ""));
	p_list->push_back(PropertyInfo(Variant::OBJECT, "scene", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"));
	p_list->push_back(PropertyInfo(Variant::BOOL, "display_placeholder", PROPERTY_HINT_NONE, ""));
}

void TileSetScenesCollectionSourceEditor::SceneTileProxyObject::edit(TileSetScenesCollectionSource *p_tile_set_scenes_collection_source, int p_scene_id) {
	ERR_FAIL_NULL(p_tile_set_scenes_collection_source);
	ERR_FAIL_COND(!p_tile_set_scenes_collection_source->has_scene_tile_id(p_scene_id));

	if (tile_set_scenes_collection_source == p_tile_set_scenes_collection_source && scene_id == p_scene_id) {
		return;
	}

	tile_set_scenes_collection_source = p_tile_set_scenes_collection_source;
	scene_id = p_scene_id;

	notify_property_list_changed();
}

void TileSetScenesCollectionSourceEditor::SceneTileProxyObject::_bind_methods() {
	ADD_SIGNAL(MethodInfo("changed", PropertyInfo(Variant::STRING, "what")));
}

void TileSetScenesCollectionSourceEditor::_scenes_collection_source_proxy_object_changed(const String &p_what) {
	if (p_what == "id") {
		emit_signal(SNAME("source_id_changed"), scenes_collection_source_proxy_object->get_id());
	}
}

void TileSetScenesCollectionSourceEditor::_tile_set_scenes_collection_source_changed() {
	tile_set_scenes_collection_source_changed_needs_update = true;
}

void TileSetScenesCollectionSourceEditor::_scene_thumbnail_done(const String &p_path, const Ref<Texture2D> &p_preview, const Ref<Texture2D> &p_small_preview, const Variant &p_ud) {
	int index = p_ud;

	if (index >= 0 && index < scene_tiles_list->get_item_count()) {
		scene_tiles_list->set_item_icon(index, p_preview);
	}
}

void TileSetScenesCollectionSourceEditor::_scenes_list_item_activated(int p_index) {
	Ref<PackedScene> packed_scene = tile_set_scenes_collection_source->get_scene_tile_scene(scene_tiles_list->get_item_metadata(p_index));
	if (packed_scene.is_valid()) {
		EditorNode::get_singleton()->load_scene(packed_scene->get_path());
	}
}

void TileSetScenesCollectionSourceEditor::_source_add_pressed() {
	if (!scene_select_dialog) {
		scene_select_dialog = memnew(EditorFileDialog);
		add_child(scene_select_dialog);
		scene_select_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
		scene_select_dialog->connect("file_selected", callable_mp(this, &TileSetScenesCollectionSourceEditor::_scene_file_selected));

		for (const String &E : Vector<String>{ "tscn", "scn" }) {
			scene_select_dialog->add_filter("*." + E, E.to_upper());
		}
	}
	scene_select_dialog->popup_file_dialog();
}

void TileSetScenesCollectionSourceEditor::_scene_file_selected(const String &p_path) {
	Ref<PackedScene> scene = ResourceLoader::load(p_path);

	int scene_id = tile_set_scenes_collection_source->get_next_scene_tile_id();
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add a Scene Tile"));
	undo_redo->add_do_method(tile_set_scenes_collection_source, "create_scene_tile", scene, scene_id);
	undo_redo->add_undo_method(tile_set_scenes_collection_source, "remove_scene_tile", scene_id);
	undo_redo->commit_action();
	_update_scenes_list();
	_update_action_buttons();
	_update_tile_inspector();
}

void TileSetScenesCollectionSourceEditor::_source_delete_pressed() {
	Vector<int> selected_indices = scene_tiles_list->get_selected_items();
	ERR_FAIL_COND(selected_indices.is_empty());
	int scene_id = scene_tiles_list->get_item_metadata(selected_indices[0]);

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Remove a Scene Tile"));
	undo_redo->add_do_method(tile_set_scenes_collection_source, "remove_scene_tile", scene_id);
	undo_redo->add_undo_method(tile_set_scenes_collection_source, "create_scene_tile", tile_set_scenes_collection_source->get_scene_tile_scene(scene_id), scene_id);
	undo_redo->commit_action();
	_update_scenes_list();
	_update_action_buttons();
	_update_tile_inspector();
}

void TileSetScenesCollectionSourceEditor::_update_source_inspector() {
	// Update the proxy object.
	scenes_collection_source_proxy_object->edit(tile_set, tile_set_scenes_collection_source, tile_set_source_id);
}

void TileSetScenesCollectionSourceEditor::_update_tile_inspector() {
	Vector<int> selected_indices = scene_tiles_list->get_selected_items();
	bool has_atlas_tile_selected = (selected_indices.size() > 0);

	// Update the proxy object.
	if (has_atlas_tile_selected) {
		int scene_id = scene_tiles_list->get_item_metadata(selected_indices[0]);
		tile_proxy_object->edit(tile_set_scenes_collection_source, scene_id);
	}

	// Update visibility.
	tile_inspector_label->set_visible(has_atlas_tile_selected);
	tile_inspector->set_visible(has_atlas_tile_selected);
}

void TileSetScenesCollectionSourceEditor::_update_action_buttons() {
	Vector<int> selected_indices = scene_tiles_list->get_selected_items();
	scene_tile_delete_button->set_disabled(selected_indices.size() <= 0 || read_only);
}

void TileSetScenesCollectionSourceEditor::_update_scenes_list() {
	if (!tile_set_scenes_collection_source) {
		return;
	}

	// Get the previously selected id.
	Vector<int> selected_indices = scene_tiles_list->get_selected_items();
	int old_selected_scene_id = (selected_indices.size() > 0) ? int(scene_tiles_list->get_item_metadata(selected_indices[0])) : -1;

	// Clear the list.
	scene_tiles_list->clear();

	// Rebuild the list.
	int to_reselect = -1;
	for (int i = 0; i < tile_set_scenes_collection_source->get_scene_tiles_count(); i++) {
		int scene_id = tile_set_scenes_collection_source->get_scene_tile_id(i);

		Ref<PackedScene> scene = tile_set_scenes_collection_source->get_scene_tile_scene(scene_id);

		int item_index = 0;
		if (scene.is_valid()) {
			item_index = scene_tiles_list->add_item(vformat("%s (path:%s id:%d)", scene->get_path().get_file().get_basename(), scene->get_path(), scene_id));
			Variant udata = i;
			EditorResourcePreview::get_singleton()->queue_edited_resource_preview(scene, this, "_scene_thumbnail_done", udata);
		} else {
			item_index = scene_tiles_list->add_item(TTR("Tile with Invalid Scene"), get_editor_theme_icon(SNAME("PackedScene")));
		}
		scene_tiles_list->set_item_metadata(item_index, scene_id);

		if (old_selected_scene_id >= 0 && scene_id == old_selected_scene_id) {
			to_reselect = i;
		}
	}
	if (scene_tiles_list->get_item_count() == 0) {
		scene_tiles_list->add_item(TTR("Drag and drop scenes here or use the Add button."));
		scene_tiles_list->set_item_disabled(-1, true);
	}

	// Reselect if needed.
	if (to_reselect >= 0) {
		scene_tiles_list->select(to_reselect);
	}

	// Icon size update.
	int int_size = int(EDITOR_GET("filesystem/file_dialog/thumbnail_size")) * EDSCALE;
	scene_tiles_list->set_fixed_icon_size(Vector2(int_size, int_size));
}

void TileSetScenesCollectionSourceEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			scenes_collection_source_inspector->edit(scenes_collection_source_proxy_object);
			scenes_collection_source_inspector->add_custom_property_description("TileSetScenesCollectionProxyObject", "id", TTR("The tile's unique identifier within this TileSet. Each tile stores its source ID, so changing one may make tiles invalid."));
			scenes_collection_source_inspector->add_custom_property_description("TileSetScenesCollectionProxyObject", "name", TTR("The human-readable name for the scene collection. Use a descriptive name here for organizational purposes (such as \"obstacles\", \"decoration\", etc.)."));

			tile_inspector->edit(tile_proxy_object);
			tile_inspector->add_custom_property_description("SceneTileProxyObject", "id", TTR("ID of the scene tile in the collection. Each painted tile has associated ID, so changing this property may cause your TileMaps to not display properly."));
			tile_inspector->add_custom_property_description("SceneTileProxyObject", "scene", TTR("Absolute path to the scene associated with this tile."));
			tile_inspector->add_custom_property_description("SceneTileProxyObject", "display_placeholder", TTR("If [code]true[/code], a placeholder marker will be displayed on top of the scene's preview. The marker is displayed anyway if the scene has no valid preview."));
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			scene_tile_add_button->set_button_icon(get_editor_theme_icon(SNAME("Add")));
			scene_tile_delete_button->set_button_icon(get_editor_theme_icon(SNAME("Remove")));
			_update_scenes_list();
		} break;

		case NOTIFICATION_INTERNAL_PROCESS: {
			if (tile_set_scenes_collection_source_changed_needs_update) {
				read_only = false;
				// Add the listener again and check for read-only status.
				if (tile_set.is_valid()) {
					read_only = EditorNode::get_singleton()->is_resource_read_only(tile_set);
				}

				// Update everything.
				_update_source_inspector();
				_update_scenes_list();
				_update_action_buttons();
				_update_tile_inspector();
				tile_set_scenes_collection_source_changed_needs_update = false;
			}
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			// Update things just in case.
			_update_scenes_list();
			_update_action_buttons();
		} break;
	}
}

void TileSetScenesCollectionSourceEditor::edit(Ref<TileSet> p_tile_set, TileSetScenesCollectionSource *p_tile_set_scenes_collection_source, int p_source_id) {
	ERR_FAIL_COND(p_tile_set.is_null());
	ERR_FAIL_NULL(p_tile_set_scenes_collection_source);
	ERR_FAIL_COND(p_source_id < 0);
	ERR_FAIL_COND(p_tile_set->get_source(p_source_id) != p_tile_set_scenes_collection_source);

	bool new_read_only_state = false;
	if (p_tile_set.is_valid()) {
		new_read_only_state = EditorNode::get_singleton()->is_resource_read_only(p_tile_set);
	}

	if (p_tile_set == tile_set && p_tile_set_scenes_collection_source == tile_set_scenes_collection_source && p_source_id == tile_set_source_id && new_read_only_state == read_only) {
		return;
	}

	// Remove listener for old objects.
	if (tile_set_scenes_collection_source) {
		tile_set_scenes_collection_source->disconnect_changed(callable_mp(this, &TileSetScenesCollectionSourceEditor::_tile_set_scenes_collection_source_changed));
	}

	// Change the edited object.
	tile_set = p_tile_set;
	tile_set_scenes_collection_source = p_tile_set_scenes_collection_source;
	tile_set_source_id = p_source_id;

	// Read-only status is false by default
	read_only = new_read_only_state;

	if (tile_set.is_valid()) {
		scenes_collection_source_inspector->set_read_only(read_only);
		tile_inspector->set_read_only(read_only);

		scene_tile_add_button->set_disabled(read_only);
	}

	// Add the listener again.
	if (tile_set_scenes_collection_source) {
		tile_set_scenes_collection_source->connect_changed(callable_mp(this, &TileSetScenesCollectionSourceEditor::_tile_set_scenes_collection_source_changed));
	}

	// Update everything.
	_update_source_inspector();
	_update_scenes_list();
	_update_action_buttons();
	_update_tile_inspector();
}

void TileSetScenesCollectionSourceEditor::_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	if (!_can_drop_data_fw(p_point, p_data, p_from)) {
		return;
	}

	if (p_from == scene_tiles_list) {
		// Handle dropping a texture in the list of atlas resources.
		Dictionary d = p_data;
		Vector<String> files = d["files"];
		for (int i = 0; i < files.size(); i++) {
			Ref<PackedScene> resource = ResourceLoader::load(files[i]);
			if (resource.is_valid()) {
				int scene_id = tile_set_scenes_collection_source->get_next_scene_tile_id();
				EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
				undo_redo->create_action(TTR("Add a Scene Tile"));
				undo_redo->add_do_method(tile_set_scenes_collection_source, "create_scene_tile", resource, scene_id);
				undo_redo->add_undo_method(tile_set_scenes_collection_source, "remove_scene_tile", scene_id);
				undo_redo->commit_action();
			}
		}

		_update_scenes_list();
		_update_action_buttons();
		_update_tile_inspector();
	}
}

bool TileSetScenesCollectionSourceEditor::_can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	if (p_from == scene_tiles_list) {
		Dictionary d = p_data;

		if (!d.has("type")) {
			return false;
		}

		// Check if we have a Texture2D.
		if (String(d["type"]) == "files") {
			Vector<String> files = d["files"];

			if (files.size() == 0) {
				return false;
			}

			for (int i = 0; i < files.size(); i++) {
				String ftype = EditorFileSystem::get_singleton()->get_file_type(files[i]);

				if (!ClassDB::is_parent_class(ftype, "PackedScene")) {
					return false;
				}
			}

			return true;
		}
	}
	return false;
}

void TileSetScenesCollectionSourceEditor::_bind_methods() {
	ADD_SIGNAL(MethodInfo("source_id_changed", PropertyInfo(Variant::INT, "source_id")));

	ClassDB::bind_method(D_METHOD("_scene_thumbnail_done"), &TileSetScenesCollectionSourceEditor::_scene_thumbnail_done);
}

TileSetScenesCollectionSourceEditor::TileSetScenesCollectionSourceEditor() {
	// -- Right side --
	HSplitContainer *split_container_right_side = memnew(HSplitContainer);
	split_container_right_side->set_h_size_flags(SIZE_EXPAND_FILL);
	add_child(split_container_right_side);

	// Middle panel.
	ScrollContainer *middle_panel = memnew(ScrollContainer);
	middle_panel->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	middle_panel->set_custom_minimum_size(Size2(200, 0) * EDSCALE);
	split_container_right_side->add_child(middle_panel);

	VBoxContainer *middle_vbox_container = memnew(VBoxContainer);
	middle_vbox_container->set_h_size_flags(SIZE_EXPAND_FILL);
	middle_panel->add_child(middle_vbox_container);

	// Scenes collection source inspector.
	scenes_collection_source_inspector_label = memnew(Label);
	scenes_collection_source_inspector_label->set_text(TTR("Scenes collection properties:"));
	middle_vbox_container->add_child(scenes_collection_source_inspector_label);

	scenes_collection_source_proxy_object = memnew(TileSetScenesCollectionProxyObject());
	scenes_collection_source_proxy_object->connect(CoreStringName(changed), callable_mp(this, &TileSetScenesCollectionSourceEditor::_scenes_collection_source_proxy_object_changed));

	scenes_collection_source_inspector = memnew(EditorInspector);
	scenes_collection_source_inspector->set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	scenes_collection_source_inspector->set_use_doc_hints(true);
	middle_vbox_container->add_child(scenes_collection_source_inspector);

	// Tile inspector.
	tile_inspector_label = memnew(Label);
	tile_inspector_label->set_text(TTR("Tile properties:"));
	tile_inspector_label->hide();
	middle_vbox_container->add_child(tile_inspector_label);

	tile_proxy_object = memnew(SceneTileProxyObject(this));
	tile_proxy_object->connect(CoreStringName(changed), callable_mp(this, &TileSetScenesCollectionSourceEditor::_update_scenes_list).unbind(1));
	tile_proxy_object->connect(CoreStringName(changed), callable_mp(this, &TileSetScenesCollectionSourceEditor::_update_action_buttons).unbind(1));

	tile_inspector = memnew(EditorInspector);
	tile_inspector->set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	tile_inspector->set_use_doc_hints(true);
	tile_inspector->set_use_folding(true);
	middle_vbox_container->add_child(tile_inspector);

	// Scenes list.
	VBoxContainer *right_vbox_container = memnew(VBoxContainer);
	split_container_right_side->add_child(right_vbox_container);

	scene_tiles_list = memnew(ItemList);
	scene_tiles_list->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	scene_tiles_list->set_h_size_flags(SIZE_EXPAND_FILL);
	scene_tiles_list->set_v_size_flags(SIZE_EXPAND_FILL);
	SET_DRAG_FORWARDING_CDU(scene_tiles_list, TileSetScenesCollectionSourceEditor);
	scene_tiles_list->connect(SceneStringName(item_selected), callable_mp(this, &TileSetScenesCollectionSourceEditor::_update_tile_inspector).unbind(1));
	scene_tiles_list->connect(SceneStringName(item_selected), callable_mp(this, &TileSetScenesCollectionSourceEditor::_update_action_buttons).unbind(1));
	scene_tiles_list->connect("item_activated", callable_mp(this, &TileSetScenesCollectionSourceEditor::_scenes_list_item_activated));
	scene_tiles_list->set_texture_filter(CanvasItem::TEXTURE_FILTER_NEAREST);
	right_vbox_container->add_child(scene_tiles_list);

	HBoxContainer *scenes_bottom_actions = memnew(HBoxContainer);
	right_vbox_container->add_child(scenes_bottom_actions);

	scene_tile_add_button = memnew(Button);
	scene_tile_add_button->set_theme_type_variation(SceneStringName(FlatButton));
	scene_tile_add_button->connect(SceneStringName(pressed), callable_mp(this, &TileSetScenesCollectionSourceEditor::_source_add_pressed));
	scenes_bottom_actions->add_child(scene_tile_add_button);

	scene_tile_delete_button = memnew(Button);
	scene_tile_delete_button->set_theme_type_variation(SceneStringName(FlatButton));
	scene_tile_delete_button->set_disabled(true);
	scene_tile_delete_button->connect(SceneStringName(pressed), callable_mp(this, &TileSetScenesCollectionSourceEditor::_source_delete_pressed));
	scenes_bottom_actions->add_child(scene_tile_delete_button);

	EditorInspector::add_inspector_plugin(memnew(TileSourceInspectorPlugin));
}

TileSetScenesCollectionSourceEditor::~TileSetScenesCollectionSourceEditor() {
	memdelete(scenes_collection_source_proxy_object);
	memdelete(tile_proxy_object);
}

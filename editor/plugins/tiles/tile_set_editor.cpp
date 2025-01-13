/**************************************************************************/
/*  tile_set_editor.cpp                                                   */
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

#include "tile_set_editor.h"

#include "tile_data_editors.h"
#include "tiles_editor_plugin.h"

#include "editor/editor_file_system.h"
#include "editor/editor_inspector.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/themes/editor_scale.h"

#include "scene/gui/box_container.h"
#include "scene/gui/control.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/tab_container.h"

TileSetEditor *TileSetEditor::singleton = nullptr;

void TileSetEditor::_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	ERR_FAIL_COND(!tile_set.is_valid());

	if (!_can_drop_data_fw(p_point, p_data, p_from)) {
		return;
	}

	if (p_from == sources_list) {
		// Handle dropping a texture in the list of atlas resources.
		Dictionary d = p_data;
		Vector<String> files = d["files"];
		_load_texture_files(files);
	}
}

bool TileSetEditor::_can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	ERR_FAIL_COND_V(!tile_set.is_valid(), false);

	if (read_only) {
		return false;
	}

	if (p_from == sources_list) {
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

				if (!ClassDB::is_parent_class(ftype, "Texture2D")) {
					return false;
				}
			}

			return true;
		}
	}
	return false;
}

void TileSetEditor::_load_texture_files(const Vector<String> &p_paths) {
	int source_id = TileSet::INVALID_SOURCE;
	Vector<Ref<TileSetAtlasSource>> atlases;

	for (const String &p_path : p_paths) {
		Ref<Texture2D> texture = ResourceLoader::load(p_path);

		if (texture.is_null()) {
			EditorNode::get_singleton()->show_warning(TTR("Invalid texture selected."));
			continue;
		}

		// Retrieve the id for the next created source.
		source_id = tile_set->get_next_source_id();

		// Actually create the new source.
		Ref<TileSetAtlasSource> atlas_source = memnew(TileSetAtlasSource);
		atlas_source->set_texture(texture);
		atlas_source->set_texture_region_size(tile_set->get_tile_size());

		EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
		undo_redo->create_action(TTR("Add a new atlas source"));
		undo_redo->add_do_method(*tile_set, "add_source", atlas_source, source_id);
		undo_redo->add_undo_method(*tile_set, "remove_source", source_id);
		undo_redo->commit_action();

		atlases.append(atlas_source);
	}

	if (!atlases.is_empty()) {
		tile_set_atlas_source_editor->init_new_atlases(atlases);
	}

	// Update the selected source (thus triggering an update).
	_update_sources_list(source_id);
}

void TileSetEditor::_update_sources_list(int force_selected_id) {
	if (tile_set.is_null()) {
		return;
	}

	// Get the previously selected id.
	int old_selected = TileSet::INVALID_SOURCE;
	if (sources_list->get_current() >= 0) {
		int source_id = sources_list->get_item_metadata(sources_list->get_current());
		if (tile_set->has_source(source_id)) {
			old_selected = source_id;
		}
	}

	int to_select = TileSet::INVALID_SOURCE;
	if (force_selected_id >= 0) {
		to_select = force_selected_id;
	} else if (old_selected >= 0) {
		to_select = old_selected;
	}

	// Clear the list.
	sources_list->clear();

	// Update the atlas sources.
	List<int> source_ids = TilesEditorUtils::get_singleton()->get_sorted_sources(tile_set);
	for (const int &source_id : source_ids) {
		TileSetSource *source = *tile_set->get_source(source_id);

		Ref<Texture2D> texture;
		String item_text;

		// Common to all type of sources.
		if (!source->get_name().is_empty()) {
			item_text = source->get_name();
		}

		// Atlas source.
		TileSetAtlasSource *atlas_source = Object::cast_to<TileSetAtlasSource>(source);
		if (atlas_source) {
			texture = atlas_source->get_texture();
			if (item_text.is_empty()) {
				if (texture.is_valid()) {
					item_text = texture->get_path().get_file();
				} else {
					item_text = vformat(TTR("No Texture Atlas Source (ID: %d)"), source_id);
				}
			}
		}

		// Scene collection source.
		TileSetScenesCollectionSource *scene_collection_source = Object::cast_to<TileSetScenesCollectionSource>(source);
		if (scene_collection_source) {
			texture = get_editor_theme_icon(SNAME("PackedScene"));
			if (item_text.is_empty()) {
				if (scene_collection_source->get_scene_tiles_count() > 0) {
					item_text = vformat(TTR("Scene Collection Source (ID: %d)"), source_id);
				} else {
					item_text = vformat(TTR("Empty Scene Collection Source (ID: %d)"), source_id);
				}
			}
		}

		// Use default if not valid.
		if (item_text.is_empty()) {
			item_text = vformat(TTR("Unknown Type Source (ID: %d)"), source_id);
		}
		if (!texture.is_valid()) {
			texture = missing_texture_texture;
		}

		sources_list->add_item(item_text, texture);
		sources_list->set_item_metadata(-1, source_id);
	}

	// Set again the current selected item if needed.
	if (to_select >= 0) {
		for (int i = 0; i < sources_list->get_item_count(); i++) {
			if ((int)sources_list->get_item_metadata(i) == to_select) {
				sources_list->set_current(i);
				sources_list->ensure_current_is_visible();
				if (old_selected != to_select) {
					sources_list->emit_signal(SceneStringName(item_selected), sources_list->get_current());
				}
				break;
			}
		}
	}

	// If nothing is selected, select the first entry.
	if (sources_list->get_current() < 0 && sources_list->get_item_count() > 0) {
		sources_list->set_current(0);
		if (old_selected != int(sources_list->get_item_metadata(0))) {
			sources_list->emit_signal(SceneStringName(item_selected), sources_list->get_current());
		}
	}

	// If there is no source left, hide all editors and show the label.
	_source_selected(sources_list->get_current());

	// Synchronize the lists.
	TilesEditorUtils::get_singleton()->set_sources_lists_current(sources_list->get_current());
}

void TileSetEditor::_source_selected(int p_source_index) {
	ERR_FAIL_COND(!tile_set.is_valid());

	// Update the selected source.
	sources_delete_button->set_disabled(p_source_index < 0 || read_only);

	if (p_source_index >= 0) {
		int source_id = sources_list->get_item_metadata(p_source_index);
		TileSetAtlasSource *atlas_source = Object::cast_to<TileSetAtlasSource>(*tile_set->get_source(source_id));
		TileSetScenesCollectionSource *scenes_collection_source = Object::cast_to<TileSetScenesCollectionSource>(*tile_set->get_source(source_id));
		if (atlas_source) {
			no_source_selected_label->hide();
			tile_set_atlas_source_editor->edit(*tile_set, atlas_source, source_id);
			tile_set_atlas_source_editor->show();
			tile_set_scenes_collection_source_editor->hide();
		} else if (scenes_collection_source) {
			no_source_selected_label->hide();
			tile_set_atlas_source_editor->hide();
			tile_set_scenes_collection_source_editor->edit(*tile_set, scenes_collection_source, source_id);
			tile_set_scenes_collection_source_editor->show();
		} else {
			no_source_selected_label->show();
			tile_set_atlas_source_editor->hide();
			tile_set_scenes_collection_source_editor->hide();
		}
	} else {
		no_source_selected_label->show();
		tile_set_atlas_source_editor->hide();
		tile_set_scenes_collection_source_editor->hide();
	}
}

void TileSetEditor::_source_delete_pressed() {
	ERR_FAIL_COND(!tile_set.is_valid());

	// Update the selected source.
	int to_delete = sources_list->get_item_metadata(sources_list->get_current());

	Ref<TileSetSource> source = tile_set->get_source(to_delete);

	// Remove the source.
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Remove source"));
	undo_redo->add_do_method(*tile_set, "remove_source", to_delete);
	undo_redo->add_undo_method(*tile_set, "add_source", source, to_delete);
	undo_redo->commit_action();

	_update_sources_list();
}

void TileSetEditor::_source_add_id_pressed(int p_id_pressed) {
	ERR_FAIL_COND(!tile_set.is_valid());

	switch (p_id_pressed) {
		case 0: {
			if (!texture_file_dialog) {
				texture_file_dialog = memnew(EditorFileDialog);
				add_child(texture_file_dialog);
				texture_file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILES);
				texture_file_dialog->connect("files_selected", callable_mp(this, &TileSetEditor::_load_texture_files));

				List<String> extensions;
				ResourceLoader::get_recognized_extensions_for_type("Texture2D", &extensions);
				for (const String &E : extensions) {
					texture_file_dialog->add_filter("*." + E, E.to_upper());
				}
			}
			texture_file_dialog->popup_file_dialog();
		} break;
		case 1: {
			int source_id = tile_set->get_next_source_id();

			Ref<TileSetScenesCollectionSource> scene_collection_source = memnew(TileSetScenesCollectionSource);

			// Add a new source.
			EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
			undo_redo->create_action(TTR("Add atlas source"));
			undo_redo->add_do_method(*tile_set, "add_source", scene_collection_source, source_id);
			undo_redo->add_undo_method(*tile_set, "remove_source", source_id);
			undo_redo->commit_action();

			_update_sources_list(source_id);
		} break;
		default:
			ERR_FAIL();
	}
}

void TileSetEditor::_sources_advanced_menu_id_pressed(int p_id_pressed) {
	ERR_FAIL_COND(!tile_set.is_valid());

	switch (p_id_pressed) {
		case 0: {
			atlas_merging_dialog->update_tile_set(tile_set);
			atlas_merging_dialog->popup_centered_ratio(0.5);
		} break;
		case 1: {
			tile_proxies_manager_dialog->update_tile_set(tile_set);
			tile_proxies_manager_dialog->popup_centered_ratio(0.5);
		} break;
	}
}

void TileSetEditor::_set_source_sort(int p_sort) {
	TilesEditorUtils::get_singleton()->set_sorting_option(p_sort);
	for (int i = 0; i != TilesEditorUtils::SOURCE_SORT_MAX; i++) {
		source_sort_button->get_popup()->set_item_checked(i, (i == (int)p_sort));
	}

	int old_selected = TileSet::INVALID_SOURCE;
	if (sources_list->get_current() >= 0) {
		int source_id = sources_list->get_item_metadata(sources_list->get_current());
		if (tile_set->has_source(source_id)) {
			old_selected = source_id;
		}
	}
	_update_sources_list(old_selected);

	if (!first_edit) {
		EditorSettings::get_singleton()->set_project_metadata("editor_metadata", "tile_source_sort", p_sort);
	}
}

void TileSetEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			sources_delete_button->set_button_icon(get_editor_theme_icon(SNAME("Remove")));
			sources_add_button->set_button_icon(get_editor_theme_icon(SNAME("Add")));
			source_sort_button->set_button_icon(get_editor_theme_icon(SNAME("Sort")));
			sources_advanced_menu_button->set_button_icon(get_editor_theme_icon(SNAME("GuiTabMenuHl")));
			missing_texture_texture = get_editor_theme_icon(SNAME("TileSet"));
			expanded_area->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SceneStringName(panel), "Tree"));
			_update_sources_list();
		} break;

		case NOTIFICATION_INTERNAL_PROCESS: {
			if (tile_set_changed_needs_update) {
				if (tile_set.is_valid()) {
					tile_set->set_edited(true);
				}

				read_only = false;
				if (tile_set.is_valid()) {
					read_only = EditorNode::get_singleton()->is_resource_read_only(tile_set);
				}

				_update_sources_list();
				_update_patterns_list();

				sources_add_button->set_disabled(read_only);
				sources_advanced_menu_button->set_disabled(read_only);
				source_sort_button->set_disabled(read_only);

				tile_set_changed_needs_update = false;
			}
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_visible_in_tree()) {
				remove_expanded_editor();
			}
		} break;
	}
}

void TileSetEditor::_patterns_item_list_gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(!tile_set.is_valid());

	if (EditorNode::get_singleton()->is_resource_read_only(tile_set)) {
		return;
	}

	if (ED_IS_SHORTCUT("tiles_editor/delete", p_event) && p_event->is_pressed() && !p_event->is_echo()) {
		Vector<int> selected = patterns_item_list->get_selected_items();
		EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
		undo_redo->create_action(TTR("Remove TileSet patterns"));
		for (int i = 0; i < selected.size(); i++) {
			int pattern_index = selected[i];
			undo_redo->add_do_method(*tile_set, "remove_pattern", pattern_index);
			undo_redo->add_undo_method(*tile_set, "add_pattern", tile_set->get_pattern(pattern_index), pattern_index);
		}
		undo_redo->commit_action();
		patterns_item_list->accept_event();
	}
}

void TileSetEditor::_pattern_preview_done(Ref<TileMapPattern> p_pattern, Ref<Texture2D> p_texture) {
	// TODO optimize ?
	for (int i = 0; i < patterns_item_list->get_item_count(); i++) {
		if (patterns_item_list->get_item_metadata(i) == p_pattern) {
			patterns_item_list->set_item_icon(i, p_texture);
			break;
		}
	}
}

void TileSetEditor::_update_patterns_list() {
	ERR_FAIL_COND(!tile_set.is_valid());

	// Recreate the items.
	patterns_item_list->clear();
	for (int i = 0; i < tile_set->get_patterns_count(); i++) {
		int id = patterns_item_list->add_item("");
		patterns_item_list->set_item_metadata(id, tile_set->get_pattern(i));
		patterns_item_list->set_item_tooltip(id, vformat(TTR("Index: %d"), i));
		TilesEditorUtils::get_singleton()->queue_pattern_preview(tile_set, tile_set->get_pattern(i), callable_mp(this, &TileSetEditor::_pattern_preview_done));
	}

	// Update the label visibility.
	patterns_help_label->set_visible(patterns_item_list->get_item_count() == 0);
}

void TileSetEditor::_tile_set_changed() {
	tile_set_changed_needs_update = true;
}

void TileSetEditor::_tab_changed(int p_tab_changed) {
	split_container->set_visible(p_tab_changed == 0);
	patterns_item_list->set_visible(p_tab_changed == 1);
}

void TileSetEditor::_move_tile_set_array_element(Object *p_undo_redo, Object *p_edited, const String &p_array_prefix, int p_from_index, int p_to_pos) {
	EditorUndoRedoManager *undo_redo_man = Object::cast_to<EditorUndoRedoManager>(p_undo_redo);
	ERR_FAIL_NULL(undo_redo_man);

	TileSet *ed_tile_set = Object::cast_to<TileSet>(p_edited);
	if (!ed_tile_set) {
		return;
	}

	Vector<String> components = String(p_array_prefix).split("/", true, 2);

	// Compute the array indices to save.
	int begin = 0;
	int end;
	if (p_array_prefix == "occlusion_layer_") {
		end = ed_tile_set->get_occlusion_layers_count();
	} else if (p_array_prefix == "physics_layer_") {
		end = ed_tile_set->get_physics_layers_count();
	} else if (p_array_prefix == "terrain_set_") {
		end = ed_tile_set->get_terrain_sets_count();
	} else if (components.size() >= 2 && components[0].begins_with("terrain_set_") && components[0].trim_prefix("terrain_set_").is_valid_int() && components[1] == "terrain_") {
		int terrain_set = components[0].trim_prefix("terrain_set_").to_int();
		end = ed_tile_set->get_terrains_count(terrain_set);
	} else if (p_array_prefix == "navigation_layer_") {
		end = ed_tile_set->get_navigation_layers_count();
	} else if (p_array_prefix == "custom_data_layer_") {
		end = ed_tile_set->get_custom_data_layers_count();
	} else {
		ERR_FAIL_MSG("Invalid array prefix for TileSet.");
	}
	if (p_from_index < 0) {
		// Adding new.
		if (p_to_pos >= 0) {
			begin = p_to_pos;
		} else {
			end = 0; // Nothing to save when adding at the end.
		}
	} else if (p_to_pos < 0) {
		// Removing.
		begin = p_from_index;
	} else {
		// Moving.
		begin = MIN(p_from_index, p_to_pos);
		end = MIN(MAX(p_from_index, p_to_pos) + 1, end);
	}

#define ADD_UNDO(obj, property) undo_redo_man->add_undo_property(obj, property, obj->get(property));

	// Add undo method to adding array element.
	if (p_array_prefix == "occlusion_layer_") {
		if (p_from_index < 0) {
			undo_redo_man->add_undo_method(ed_tile_set, "remove_occlusion_layer", p_to_pos < 0 ? ed_tile_set->get_occlusion_layers_count() : p_to_pos);
		}
	} else if (p_array_prefix == "physics_layer_") {
		if (p_from_index < 0) {
			undo_redo_man->add_undo_method(ed_tile_set, "remove_physics_layer", p_to_pos < 0 ? ed_tile_set->get_physics_layers_count() : p_to_pos);
		}
	} else if (p_array_prefix == "terrain_set_") {
		if (p_from_index < 0) {
			undo_redo_man->add_undo_method(ed_tile_set, "remove_terrain_set", p_to_pos < 0 ? ed_tile_set->get_terrain_sets_count() : p_to_pos);
		}
	} else if (components.size() >= 2 && components[0].begins_with("terrain_set_") && components[0].trim_prefix("terrain_set_").is_valid_int() && components[1] == "terrain_") {
		int terrain_set = components[0].trim_prefix("terrain_set_").to_int();
		if (p_from_index < 0) {
			undo_redo_man->add_undo_method(ed_tile_set, "remove_terrain", terrain_set, p_to_pos < 0 ? ed_tile_set->get_terrains_count(terrain_set) : p_to_pos);
		}
	} else if (p_array_prefix == "navigation_layer_") {
		if (p_from_index < 0) {
			undo_redo_man->add_undo_method(ed_tile_set, "remove_navigation_layer", p_to_pos < 0 ? ed_tile_set->get_navigation_layers_count() : p_to_pos);
		}
	} else if (p_array_prefix == "custom_data_layer_") {
		if (p_from_index < 0) {
			undo_redo_man->add_undo_method(ed_tile_set, "remove_custom_data_layer", p_to_pos < 0 ? ed_tile_set->get_custom_data_layers_count() : p_to_pos);
		}
	}

	// Save layers' properties.
	List<PropertyInfo> properties;
	ed_tile_set->get_property_list(&properties);
	for (PropertyInfo pi : properties) {
		if (pi.name.begins_with(p_array_prefix)) {
			String str = pi.name.trim_prefix(p_array_prefix);
			int to_char_index = 0;
			while (to_char_index < str.length()) {
				if (!is_digit(str[to_char_index])) {
					break;
				}
				to_char_index++;
			}
			if (to_char_index > 0) {
				int array_index = str.left(to_char_index).to_int();
				if (array_index >= begin && array_index < end) {
					ADD_UNDO(ed_tile_set, pi.name);
				}
			}
		}
	}

	// Save properties for TileSetAtlasSources tile data
	for (int i = 0; i < ed_tile_set->get_source_count(); i++) {
		int source_id = ed_tile_set->get_source_id(i);

		Ref<TileSetAtlasSource> tas = ed_tile_set->get_source(source_id);
		if (tas.is_valid()) {
			for (int j = 0; j < tas->get_tiles_count(); j++) {
				Vector2i tile_id = tas->get_tile_id(j);
				for (int k = 0; k < tas->get_alternative_tiles_count(tile_id); k++) {
					int alternative_id = tas->get_alternative_tile_id(tile_id, k);
					TileData *tile_data = tas->get_tile_data(tile_id, alternative_id);
					ERR_FAIL_NULL(tile_data);

					// Actually saving stuff.
					if (p_array_prefix == "occlusion_layer_") {
						for (int layer_index = begin; layer_index < end; layer_index++) {
							ADD_UNDO(tile_data, vformat("occlusion_layer_%d/polygon", layer_index));
						}
					} else if (p_array_prefix == "physics_layer_") {
						for (int layer_index = begin; layer_index < end; layer_index++) {
							ADD_UNDO(tile_data, vformat("physics_layer_%d/polygons_count", layer_index));
							for (int polygon_index = 0; polygon_index < tile_data->get_collision_polygons_count(layer_index); polygon_index++) {
								ADD_UNDO(tile_data, vformat("physics_layer_%d/polygon_%d/points", layer_index, polygon_index));
								ADD_UNDO(tile_data, vformat("physics_layer_%d/polygon_%d/one_way", layer_index, polygon_index));
								ADD_UNDO(tile_data, vformat("physics_layer_%d/polygon_%d/one_way_margin", layer_index, polygon_index));
							}
						}
					} else if (p_array_prefix == "terrain_set_") {
						ADD_UNDO(tile_data, "terrain_set");
						for (int terrain_set_index = begin; terrain_set_index < end; terrain_set_index++) {
							for (int l = 0; l < TileSet::CELL_NEIGHBOR_MAX; l++) {
								TileSet::CellNeighbor bit = TileSet::CellNeighbor(l);
								if (tile_data->is_valid_terrain_peering_bit(bit)) {
									ADD_UNDO(tile_data, "terrains_peering_bit/" + String(TileSet::CELL_NEIGHBOR_ENUM_TO_TEXT[l]));
								}
							}
						}
					} else if (components.size() >= 2 && components[0].begins_with("terrain_set_") && components[0].trim_prefix("terrain_set_").is_valid_int() && components[1] == "terrain_") {
						for (int terrain_index = 0; terrain_index < TileSet::CELL_NEIGHBOR_MAX; terrain_index++) {
							TileSet::CellNeighbor bit = TileSet::CellNeighbor(terrain_index);
							if (tile_data->is_valid_terrain_peering_bit(bit)) {
								ADD_UNDO(tile_data, "terrains_peering_bit/" + String(TileSet::CELL_NEIGHBOR_ENUM_TO_TEXT[terrain_index]));
							}
						}
					} else if (p_array_prefix == "navigation_layer_") {
						for (int layer_index = begin; layer_index < end; layer_index++) {
							ADD_UNDO(tile_data, vformat("navigation_layer_%d/polygon", layer_index));
						}
					} else if (p_array_prefix == "custom_data_layer_") {
						for (int layer_index = begin; layer_index < end; layer_index++) {
							ADD_UNDO(tile_data, vformat("custom_data_%d", layer_index));
						}
					}
				}
			}
		}
	}
#undef ADD_UNDO

	// Add do method to add/remove array element.
	if (p_array_prefix == "occlusion_layer_") {
		if (p_from_index < 0) {
			undo_redo_man->add_do_method(ed_tile_set, "add_occlusion_layer", p_to_pos);
		} else if (p_to_pos < 0) {
			undo_redo_man->add_do_method(ed_tile_set, "remove_occlusion_layer", p_from_index);
		} else {
			undo_redo_man->add_do_method(ed_tile_set, "move_occlusion_layer", p_from_index, p_to_pos);
		}
	} else if (p_array_prefix == "physics_layer_") {
		if (p_from_index < 0) {
			undo_redo_man->add_do_method(ed_tile_set, "add_physics_layer", p_to_pos);
		} else if (p_to_pos < 0) {
			undo_redo_man->add_do_method(ed_tile_set, "remove_physics_layer", p_from_index);
		} else {
			undo_redo_man->add_do_method(ed_tile_set, "move_physics_layer", p_from_index, p_to_pos);
		}
	} else if (p_array_prefix == "terrain_set_") {
		if (p_from_index < 0) {
			undo_redo_man->add_do_method(ed_tile_set, "add_terrain_set", p_to_pos);
		} else if (p_to_pos < 0) {
			undo_redo_man->add_do_method(ed_tile_set, "remove_terrain_set", p_from_index);
		} else {
			undo_redo_man->add_do_method(ed_tile_set, "move_terrain_set", p_from_index, p_to_pos);
		}
	} else if (components.size() >= 2 && components[0].begins_with("terrain_set_") && components[0].trim_prefix("terrain_set_").is_valid_int() && components[1] == "terrain_") {
		int terrain_set = components[0].trim_prefix("terrain_set_").to_int();
		if (p_from_index < 0) {
			undo_redo_man->add_do_method(ed_tile_set, "add_terrain", terrain_set, p_to_pos);
		} else if (p_to_pos < 0) {
			undo_redo_man->add_do_method(ed_tile_set, "remove_terrain", terrain_set, p_from_index);
		} else {
			undo_redo_man->add_do_method(ed_tile_set, "move_terrain", terrain_set, p_from_index, p_to_pos);
		}
	} else if (p_array_prefix == "navigation_layer_") {
		if (p_from_index < 0) {
			undo_redo_man->add_do_method(ed_tile_set, "add_navigation_layer", p_to_pos);
		} else if (p_to_pos < 0) {
			undo_redo_man->add_do_method(ed_tile_set, "remove_navigation_layer", p_from_index);
		} else {
			undo_redo_man->add_do_method(ed_tile_set, "move_navigation_layer", p_from_index, p_to_pos);
		}
	} else if (p_array_prefix == "custom_data_layer_") {
		if (p_from_index < 0) {
			undo_redo_man->add_do_method(ed_tile_set, "add_custom_data_layer", p_to_pos);
		} else if (p_to_pos < 0) {
			undo_redo_man->add_do_method(ed_tile_set, "remove_custom_data_layer", p_from_index);
		} else {
			undo_redo_man->add_do_method(ed_tile_set, "move_custom_data_layer", p_from_index, p_to_pos);
		}
	}
}

void TileSetEditor::_undo_redo_inspector_callback(Object *p_undo_redo, Object *p_edited, const String &p_property, const Variant &p_new_value) {
	EditorUndoRedoManager *undo_redo_man = Object::cast_to<EditorUndoRedoManager>(p_undo_redo);
	ERR_FAIL_NULL(undo_redo_man);

#define ADD_UNDO(obj, property) undo_redo_man->add_undo_property(obj, property, obj->get(property));
	TileSet *ed_tile_set = Object::cast_to<TileSet>(p_edited);
	if (ed_tile_set) {
		Vector<String> components = p_property.split("/", true, 3);
		for (int i = 0; i < ed_tile_set->get_source_count(); i++) {
			int source_id = ed_tile_set->get_source_id(i);

			Ref<TileSetAtlasSource> tas = ed_tile_set->get_source(source_id);
			if (tas.is_valid()) {
				for (int j = 0; j < tas->get_tiles_count(); j++) {
					Vector2i tile_id = tas->get_tile_id(j);
					for (int k = 0; k < tas->get_alternative_tiles_count(tile_id); k++) {
						int alternative_id = tas->get_alternative_tile_id(tile_id, k);
						TileData *tile_data = tas->get_tile_data(tile_id, alternative_id);
						ERR_FAIL_NULL(tile_data);

						if (components.size() == 2 && components[0].begins_with("terrain_set_") && components[0].trim_prefix("terrain_set_").is_valid_int() && components[1] == "mode") {
							ADD_UNDO(tile_data, "terrain_set");
							ADD_UNDO(tile_data, "terrain");
							for (int l = 0; l < TileSet::CELL_NEIGHBOR_MAX; l++) {
								TileSet::CellNeighbor bit = TileSet::CellNeighbor(l);
								if (tile_data->is_valid_terrain_peering_bit(bit)) {
									ADD_UNDO(tile_data, "terrains_peering_bit/" + String(TileSet::CELL_NEIGHBOR_ENUM_TO_TEXT[l]));
								}
							}
						} else if (components.size() == 2 && components[0].begins_with("custom_data_layer_") && components[0].trim_prefix("custom_data_layer_").is_valid_int() && components[1] == "type") {
							int custom_data_layer = components[0].trim_prefix("custom_data_layer_").is_valid_int();
							ADD_UNDO(tile_data, vformat("custom_data_%d", custom_data_layer));
						}
					}
				}
			}
		}
	}
#undef ADD_UNDO
}

void TileSetEditor::edit(Ref<TileSet> p_tile_set) {
	bool new_read_only_state = false;
	if (p_tile_set.is_valid()) {
		new_read_only_state = EditorNode::get_singleton()->is_resource_read_only(p_tile_set);
	}

	if (p_tile_set == tile_set && new_read_only_state == read_only) {
		return;
	}

	// Remove listener.
	if (tile_set.is_valid()) {
		tile_set->disconnect_changed(callable_mp(this, &TileSetEditor::_tile_set_changed));
	}

	// Change the edited object.
	tile_set = p_tile_set;

	// Read-only status is false by default
	read_only = new_read_only_state;

	// Add the listener again and check for read-only status.
	if (tile_set.is_valid()) {
		sources_add_button->set_disabled(read_only);
		sources_advanced_menu_button->set_disabled(read_only);
		source_sort_button->set_disabled(read_only);

		tile_set->connect_changed(callable_mp(this, &TileSetEditor::_tile_set_changed));
		if (first_edit) {
			_set_source_sort(EditorSettings::get_singleton()->get_project_metadata("editor_metadata", "tile_source_sort", 0));
			first_edit = false;
		} else {
			_update_sources_list();
		}
		_update_patterns_list();
	}
}

void TileSetEditor::add_expanded_editor(Control *p_editor) {
	expanded_editor = p_editor;
	expanded_editor_parent = p_editor->get_parent()->get_instance_id();

	// Find the scrollable control this node belongs to.
	Node *check_parent = expanded_editor->get_parent();
	Control *parent_container = nullptr;
	while (check_parent) {
		parent_container = Object::cast_to<EditorInspector>(check_parent);
		if (parent_container) {
			break;
		}
		parent_container = Object::cast_to<ScrollContainer>(check_parent);
		if (parent_container) {
			break;
		}
		check_parent = check_parent->get_parent();
	}
	ERR_FAIL_NULL(parent_container);

	expanded_editor->set_meta("reparented", true);
	expanded_editor->reparent(expanded_area);
	expanded_area->show();
	expanded_area->set_size(Vector2(parent_container->get_global_rect().get_end().x - expanded_area->get_global_position().x, expanded_area->get_size().y));

	for (SplitContainer *split : disable_on_expand) {
		split->set_dragger_visibility(SplitContainer::DRAGGER_HIDDEN);
	}
}

void TileSetEditor::remove_expanded_editor() {
	if (!expanded_editor) {
		return;
	}

	Node *original_parent = Object::cast_to<Node>(ObjectDB::get_instance(expanded_editor_parent));
	if (original_parent) {
		expanded_editor->remove_meta("reparented");
		expanded_editor->reparent(original_parent);
	} else {
		expanded_editor->queue_free();
	}
	expanded_editor = nullptr;
	expanded_editor_parent = ObjectID();
	expanded_area->hide();

	for (SplitContainer *split : disable_on_expand) {
		split->set_dragger_visibility(SplitContainer::DRAGGER_VISIBLE);
	}
}

void TileSetEditor::register_split(SplitContainer *p_split) {
	disable_on_expand.push_back(p_split);
}

TileSetEditor::TileSetEditor() {
	singleton = this;

	set_process_internal(true);

	VBoxContainer *main_vb = memnew(VBoxContainer);
	add_child(main_vb);
	main_vb->set_anchors_and_offsets_preset(PRESET_FULL_RECT);

	// TabBar.
	tabs_bar = memnew(TabBar);
	tabs_bar->set_tab_alignment(TabBar::ALIGNMENT_CENTER);
	tabs_bar->set_clip_tabs(false);
	tabs_bar->add_tab(TTR("Tile Sources"));
	tabs_bar->add_tab(TTR("Patterns"));
	tabs_bar->connect("tab_changed", callable_mp(this, &TileSetEditor::_tab_changed));

	tile_set_toolbar = memnew(HBoxContainer);
	tile_set_toolbar->set_h_size_flags(SIZE_EXPAND_FILL);
	tile_set_toolbar->add_child(tabs_bar);
	main_vb->add_child(tile_set_toolbar);

	//// Tiles ////
	// Split container.
	split_container = memnew(HSplitContainer);
	split_container->set_name(TTR("Tiles"));
	split_container->set_h_size_flags(SIZE_EXPAND_FILL);
	split_container->set_v_size_flags(SIZE_EXPAND_FILL);
	main_vb->add_child(split_container);

	// Sources list.
	VBoxContainer *split_container_left_side = memnew(VBoxContainer);
	split_container_left_side->set_h_size_flags(SIZE_EXPAND_FILL);
	split_container_left_side->set_v_size_flags(SIZE_EXPAND_FILL);
	split_container_left_side->set_stretch_ratio(0.25);
	split_container_left_side->set_custom_minimum_size(Size2(70, 0) * EDSCALE);
	split_container->add_child(split_container_left_side);

	source_sort_button = memnew(MenuButton);
	source_sort_button->set_flat(false);
	source_sort_button->set_theme_type_variation(SceneStringName(FlatButton));
	source_sort_button->set_tooltip_text(TTR("Sort Sources"));

	PopupMenu *p = source_sort_button->get_popup();
	p->connect(SceneStringName(id_pressed), callable_mp(this, &TileSetEditor::_set_source_sort));
	p->add_radio_check_item(TTR("Sort by ID (Ascending)"), TilesEditorUtils::SOURCE_SORT_ID);
	p->add_radio_check_item(TTR("Sort by ID (Descending)"), TilesEditorUtils::SOURCE_SORT_ID_REVERSE);
	p->add_radio_check_item(TTR("Sort by Name (Ascending)"), TilesEditorUtils::SOURCE_SORT_NAME);
	p->add_radio_check_item(TTR("Sort by Name (Descending)"), TilesEditorUtils::SOURCE_SORT_NAME_REVERSE);
	p->set_item_checked(TilesEditorUtils::SOURCE_SORT_ID, true);

	sources_list = memnew(ItemList);
	sources_list->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	sources_list->set_fixed_icon_size(Size2(60, 60) * EDSCALE);
	sources_list->set_h_size_flags(SIZE_EXPAND_FILL);
	sources_list->set_v_size_flags(SIZE_EXPAND_FILL);
	sources_list->set_theme_type_variation("ItemListSecondary");
	sources_list->connect(SceneStringName(item_selected), callable_mp(this, &TileSetEditor::_source_selected));
	sources_list->connect(SceneStringName(item_selected), callable_mp(TilesEditorUtils::get_singleton(), &TilesEditorUtils::set_sources_lists_current));
	sources_list->connect(SceneStringName(visibility_changed), callable_mp(TilesEditorUtils::get_singleton(), &TilesEditorUtils::synchronize_sources_list).bind(sources_list, source_sort_button));
	sources_list->add_user_signal(MethodInfo("sort_request"));
	sources_list->connect("sort_request", callable_mp(this, &TileSetEditor::_update_sources_list).bind(-1));
	sources_list->set_texture_filter(CanvasItem::TEXTURE_FILTER_NEAREST);
	SET_DRAG_FORWARDING_CDU(sources_list, TileSetEditor);
	split_container_left_side->add_child(sources_list);

	HBoxContainer *sources_bottom_actions = memnew(HBoxContainer);
	sources_bottom_actions->set_alignment(BoxContainer::ALIGNMENT_END);
	split_container_left_side->add_child(sources_bottom_actions);

	sources_delete_button = memnew(Button);
	sources_delete_button->set_theme_type_variation(SceneStringName(FlatButton));
	sources_delete_button->set_disabled(true);
	sources_delete_button->connect(SceneStringName(pressed), callable_mp(this, &TileSetEditor::_source_delete_pressed));
	sources_bottom_actions->add_child(sources_delete_button);

	sources_add_button = memnew(MenuButton);
	sources_add_button->set_flat(false);
	sources_add_button->set_theme_type_variation(SceneStringName(FlatButton));
	sources_add_button->get_popup()->add_item(TTR("Atlas"));
	sources_add_button->get_popup()->set_item_tooltip(-1, TTR("A palette of tiles made from a texture."));
	sources_add_button->get_popup()->add_item(TTR("Scenes Collection"));
	sources_add_button->get_popup()->set_item_tooltip(-1, TTR("A collection of scenes that can be instantiated and placed as tiles."));
	sources_add_button->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &TileSetEditor::_source_add_id_pressed));
	sources_bottom_actions->add_child(sources_add_button);

	sources_advanced_menu_button = memnew(MenuButton);
	sources_advanced_menu_button->set_flat(false);
	sources_advanced_menu_button->set_theme_type_variation(SceneStringName(FlatButton));
	sources_advanced_menu_button->get_popup()->add_item(TTR("Open Atlas Merging Tool"));
	sources_advanced_menu_button->get_popup()->add_item(TTR("Manage Tile Proxies"));
	sources_advanced_menu_button->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &TileSetEditor::_sources_advanced_menu_id_pressed));
	sources_bottom_actions->add_child(sources_advanced_menu_button);
	sources_bottom_actions->add_child(source_sort_button);

	atlas_merging_dialog = memnew(AtlasMergingDialog);
	add_child(atlas_merging_dialog);

	tile_proxies_manager_dialog = memnew(TileProxiesManagerDialog);
	add_child(tile_proxies_manager_dialog);

	// Right side container.
	VBoxContainer *split_container_right_side = memnew(VBoxContainer);
	split_container_right_side->set_h_size_flags(SIZE_EXPAND_FILL);
	split_container_right_side->set_v_size_flags(SIZE_EXPAND_FILL);
	split_container->add_child(split_container_right_side);

	// No source selected.
	no_source_selected_label = memnew(Label);
	no_source_selected_label->set_text(TTR("No TileSet source selected. Select or create a TileSet source.\nYou can create a new source by using the Add button on the left or by dropping a tileset texture onto the source list."));
	no_source_selected_label->set_h_size_flags(SIZE_EXPAND_FILL);
	no_source_selected_label->set_v_size_flags(SIZE_EXPAND_FILL);
	no_source_selected_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	no_source_selected_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	split_container_right_side->add_child(no_source_selected_label);

	// Atlases editor.
	tile_set_atlas_source_editor = memnew(TileSetAtlasSourceEditor);
	tile_set_atlas_source_editor->set_h_size_flags(SIZE_EXPAND_FILL);
	tile_set_atlas_source_editor->set_v_size_flags(SIZE_EXPAND_FILL);
	tile_set_atlas_source_editor->connect("source_id_changed", callable_mp(this, &TileSetEditor::_update_sources_list));
	split_container_right_side->add_child(tile_set_atlas_source_editor);
	tile_set_atlas_source_editor->hide();

	// Scenes collection editor.
	tile_set_scenes_collection_source_editor = memnew(TileSetScenesCollectionSourceEditor);
	tile_set_scenes_collection_source_editor->set_h_size_flags(SIZE_EXPAND_FILL);
	tile_set_scenes_collection_source_editor->set_v_size_flags(SIZE_EXPAND_FILL);
	tile_set_scenes_collection_source_editor->connect("source_id_changed", callable_mp(this, &TileSetEditor::_update_sources_list));
	split_container_right_side->add_child(tile_set_scenes_collection_source_editor);
	tile_set_scenes_collection_source_editor->hide();

	//// Patterns ////
	int thumbnail_size = 64;
	patterns_item_list = memnew(ItemList);
	patterns_item_list->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	patterns_item_list->set_max_columns(0);
	patterns_item_list->set_icon_mode(ItemList::ICON_MODE_TOP);
	patterns_item_list->set_fixed_column_width(thumbnail_size * 3 / 2);
	patterns_item_list->set_max_text_lines(2);
	patterns_item_list->set_fixed_icon_size(Size2(thumbnail_size, thumbnail_size));
	patterns_item_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	patterns_item_list->set_theme_type_variation("ItemListSecondary");
	patterns_item_list->connect(SceneStringName(gui_input), callable_mp(this, &TileSetEditor::_patterns_item_list_gui_input));
	main_vb->add_child(patterns_item_list);
	patterns_item_list->hide();

	patterns_help_label = memnew(Label);
	patterns_help_label->set_text(TTR("Add new patterns in the TileMap editing mode."));
	patterns_help_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	patterns_help_label->set_anchors_and_offsets_preset(Control::PRESET_CENTER);
	patterns_item_list->add_child(patterns_help_label);

	// Expanded editor
	expanded_area = memnew(PanelContainer);
	add_child(expanded_area);
	expanded_area->set_anchors_and_offsets_preset(PRESET_LEFT_WIDE);
	expanded_area->hide();

	// Registers UndoRedo inspector callback.
	EditorNode::get_editor_data().add_move_array_element_function(SNAME("TileSet"), callable_mp(this, &TileSetEditor::_move_tile_set_array_element));
	EditorNode::get_editor_data().add_undo_redo_inspector_hook_callback(callable_mp(this, &TileSetEditor::_undo_redo_inspector_callback));
}

void TileSourceInspectorPlugin::_show_id_edit_dialog(Object *p_for_source) {
	if (!id_edit_dialog) {
		id_edit_dialog = memnew(ConfirmationDialog);
		TileSetEditor::get_singleton()->add_child(id_edit_dialog);

		VBoxContainer *vbox = memnew(VBoxContainer);
		id_edit_dialog->add_child(vbox);

		Label *label = memnew(Label(TTR("Warning: Modifying a source ID will result in all TileMaps using that source to reference an invalid source instead. This may result in unexpected data loss. Change this ID carefully.")));
		label->set_autowrap_mode(TextServer::AUTOWRAP_WORD);
		vbox->add_child(label);

		id_input = memnew(SpinBox);
		vbox->add_child(id_input);
		id_input->set_max(INT_MAX);

		id_edit_dialog->connect(SceneStringName(confirmed), callable_mp(this, &TileSourceInspectorPlugin::_confirm_change_id));
	}
	edited_source = p_for_source;
	id_input->set_value(p_for_source->get("id"));
	id_edit_dialog->popup_centered(Vector2i(400, 0) * EDSCALE);
	callable_mp((Control *)id_input->get_line_edit(), &Control::grab_focus).call_deferred();
}

void TileSourceInspectorPlugin::_confirm_change_id() {
	edited_source->set("id", id_input->get_value());
	id_label->set_text(itos(edited_source->get("id"))); // Use get(), because the provided ID might've been invalid.
}

bool TileSourceInspectorPlugin::can_handle(Object *p_object) {
	return p_object->is_class("TileSetAtlasSourceProxyObject") || p_object->is_class("TileSetScenesCollectionProxyObject");
}

bool TileSourceInspectorPlugin::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	if (p_path == "id") {
		const Variant value = p_object->get("id");
		if (value.get_type() == Variant::NIL) { // May happen if the object is not yet initialized.
			return true;
		}

		EditorProperty *ep = memnew(EditorProperty);

		HBoxContainer *hbox = memnew(HBoxContainer);
		hbox->set_alignment(BoxContainer::ALIGNMENT_CENTER);

		id_label = memnew(Label(itos(value)));
		id_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		hbox->add_child(id_label);

		Button *button = memnew(Button(TTR("Edit")));
		button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		hbox->add_child(button);
		button->connect(SceneStringName(pressed), callable_mp(this, &TileSourceInspectorPlugin::_show_id_edit_dialog).bind(p_object));

		ep->add_child(hbox);
		add_property_editor(p_path, ep);
		return true;
	}
	return false;
}

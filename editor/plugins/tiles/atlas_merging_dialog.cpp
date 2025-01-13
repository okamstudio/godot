/**************************************************************************/
/*  atlas_merging_dialog.cpp                                              */
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

#include "atlas_merging_dialog.h"

#include "editor/editor_properties_vector.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/control.h"
#include "scene/gui/split_container.h"
#include "scene/resources/image_texture.h"

void AtlasMergingDialog::_property_changed(const StringName &p_property, const Variant &p_value, const String &p_field, bool p_changing) {
	_set(p_property, p_value);
}

void AtlasMergingDialog::_generate_merged(const Vector<Ref<TileSetAtlasSource>> &p_atlas_sources, int p_max_columns) {
	merged.instantiate();
	merged_mapping.clear();

	if (p_atlas_sources.size() >= 2) {
		Ref<Image> output_image = Image::create_empty(1, 1, false, Image::FORMAT_RGBA8);

		// Compute the new texture region size.
		Vector2i new_texture_region_size;
		for (int source_index = 0; source_index < p_atlas_sources.size(); source_index++) {
			const Ref<TileSetAtlasSource> &atlas_source = p_atlas_sources[source_index];
			new_texture_region_size = new_texture_region_size.max(atlas_source->get_texture_region_size());
		}

		// Generate the new texture.
		Vector2i atlas_offset;
		int line_height = 0;
		for (int source_index = 0; source_index < p_atlas_sources.size(); source_index++) {
			const Ref<TileSetAtlasSource> &atlas_source = p_atlas_sources[source_index];
			Ref<Image> input_image = atlas_source->get_texture()->get_image();
			if (input_image->get_format() != Image::FORMAT_RGBA8) {
				input_image->convert(Image::FORMAT_RGBA8);
			}
			merged_mapping.push_back(HashMap<Vector2i, Vector2i>());

			// Layout the tiles.
			Vector2i atlas_size;

			for (int tile_index = 0; tile_index < atlas_source->get_tiles_count(); tile_index++) {
				Vector2i tile_id = atlas_source->get_tile_id(tile_index);
				atlas_size = atlas_size.max(tile_id + atlas_source->get_tile_size_in_atlas(tile_id));

				Rect2i new_tile_rect_in_atlas = Rect2i(atlas_offset + tile_id, atlas_source->get_tile_size_in_atlas(tile_id));

				// Copy the texture.
				for (int frame = 0; frame < atlas_source->get_tile_animation_frames_count(tile_id); frame++) {
					Rect2i src_rect = atlas_source->get_tile_texture_region(tile_id, frame);
					Vector2i new_position = new_tile_rect_in_atlas.position * new_texture_region_size;
					if (frame > 0) {
						new_position += src_rect.size * Vector2i(frame, 0);
						atlas_size.x = MAX(frame + 1, atlas_size.x);
					}
					Rect2 dst_rect_wide = Rect2i(new_position, new_tile_rect_in_atlas.size * new_texture_region_size);
					if (dst_rect_wide.get_end().x > output_image->get_width() || dst_rect_wide.get_end().y > output_image->get_height()) {
						output_image->crop(MAX(dst_rect_wide.get_end().x, output_image->get_width()), MAX(dst_rect_wide.get_end().y, output_image->get_height()));
					}
					output_image->blit_rect(input_image, src_rect, dst_rect_wide.get_center() - src_rect.size / 2);
				}

				// Add to the mapping.
				merged_mapping[source_index][tile_id] = new_tile_rect_in_atlas.position;
			}

			// Compute the atlas offset.
			line_height = MAX(atlas_size.y, line_height);
			atlas_offset.x += atlas_size.x;
			if (atlas_offset.x >= p_max_columns) {
				atlas_offset.x = 0;
				atlas_offset.y += line_height;
				line_height = 0;
			}
		}

		merged->set_name(p_atlas_sources[0]->get_name());
		merged->set_texture(ImageTexture::create_from_image(output_image));
		merged->set_texture_region_size(new_texture_region_size);

		// Copy the tiles to the merged TileSetAtlasSource.
		for (int source_index = 0; source_index < p_atlas_sources.size(); source_index++) {
			const Ref<TileSetAtlasSource> &atlas_source = p_atlas_sources[source_index];
			for (KeyValue<Vector2i, Vector2i> tile_mapping : merged_mapping[source_index]) {
				// Create tiles and alternatives, then copy their properties.
				for (int alternative_index = 0; alternative_index < atlas_source->get_alternative_tiles_count(tile_mapping.key); alternative_index++) {
					int alternative_id = atlas_source->get_alternative_tile_id(tile_mapping.key, alternative_index);
					int changed_id = -1;
					if (alternative_id == 0) {
						merged->create_tile(tile_mapping.value, atlas_source->get_tile_size_in_atlas(tile_mapping.key));
						int count = atlas_source->get_tile_animation_frames_count(tile_mapping.key);
						merged->set_tile_animation_frames_count(tile_mapping.value, count);
						for (int i = 0; i < count; i++) {
							merged->set_tile_animation_frame_duration(tile_mapping.value, i, atlas_source->get_tile_animation_frame_duration(tile_mapping.key, i));
						}
						merged->set_tile_animation_speed(tile_mapping.value, atlas_source->get_tile_animation_speed(tile_mapping.key));
						merged->set_tile_animation_mode(tile_mapping.value, atlas_source->get_tile_animation_mode(tile_mapping.key));
					} else {
						changed_id = merged->create_alternative_tile(tile_mapping.value, alternative_index);
					}

					// Copy the properties.
					TileData *src_tile_data = atlas_source->get_tile_data(tile_mapping.key, alternative_id);
					List<PropertyInfo> properties;
					src_tile_data->get_property_list(&properties);

					TileData *dst_tile_data = merged->get_tile_data(tile_mapping.value, changed_id == -1 ? alternative_id : changed_id);
					for (PropertyInfo property : properties) {
						if (!(property.usage & PROPERTY_USAGE_STORAGE)) {
							continue;
						}
						Variant value = src_tile_data->get(property.name);
						Variant default_value = ClassDB::class_get_default_property_value("TileData", property.name);
						if (default_value.get_type() != Variant::NIL && bool(Variant::evaluate(Variant::OP_EQUAL, value, default_value))) {
							continue;
						}
						dst_tile_data->set(property.name, value);
					}
				}
			}
		}
	}
}

void AtlasMergingDialog::_update_texture() {
	Vector<int> selected = atlas_merging_atlases_list->get_selected_items();
	if (selected.size() >= 2) {
		Vector<Ref<TileSetAtlasSource>> to_merge;
		for (int i = 0; i < selected.size(); i++) {
			int source_id = atlas_merging_atlases_list->get_item_metadata(selected[i]);
			to_merge.push_back(tile_set->get_source(source_id));
		}
		_generate_merged(to_merge, next_line_after_column);
		preview->set_texture(merged->get_texture());
		preview->show();
		select_2_atlases_label->hide();
		get_ok_button()->set_disabled(false);
		merge_button->set_disabled(false);
	} else {
		_generate_merged(Vector<Ref<TileSetAtlasSource>>(), next_line_after_column);
		preview->set_texture(Ref<Texture2D>());
		preview->hide();
		select_2_atlases_label->show();
		get_ok_button()->set_disabled(true);
		merge_button->set_disabled(true);
	}
}

void AtlasMergingDialog::_merge_confirmed(const String &p_path) {
	ERR_FAIL_COND(!merged.is_valid());

	Ref<ImageTexture> output_image_texture = merged->get_texture();
	output_image_texture->get_image()->save_png(p_path);

	ResourceLoader::import(p_path);

	Ref<Texture2D> new_texture_resource = ResourceLoader::load(p_path, "Texture2D");
	merged->set_texture(new_texture_resource);

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Merge TileSetAtlasSource"));
	int next_id = tile_set->get_next_source_id();
	undo_redo->add_do_method(*tile_set, "add_source", merged, next_id);
	undo_redo->add_undo_method(*tile_set, "remove_source", next_id);

	if (delete_original_atlases) {
		// Delete originals if needed.
		Vector<int> selected = atlas_merging_atlases_list->get_selected_items();
		for (int i = 0; i < selected.size(); i++) {
			int source_id = atlas_merging_atlases_list->get_item_metadata(selected[i]);
			Ref<TileSetAtlasSource> tas = tile_set->get_source(source_id);
			undo_redo->add_do_method(*tile_set, "remove_source", source_id);
			undo_redo->add_undo_method(*tile_set, "add_source", tas, source_id);

			// Add the tile proxies.
			for (int tile_index = 0; tile_index < tas->get_tiles_count(); tile_index++) {
				Vector2i tile_id = tas->get_tile_id(tile_index);
				undo_redo->add_do_method(*tile_set, "set_coords_level_tile_proxy", source_id, tile_id, next_id, merged_mapping[i][tile_id]);
				if (tile_set->has_coords_level_tile_proxy(source_id, tile_id)) {
					Array a = tile_set->get_coords_level_tile_proxy(source_id, tile_id);
					undo_redo->add_undo_method(*tile_set, "set_coords_level_tile_proxy", a[0], a[1]);
				} else {
					undo_redo->add_undo_method(*tile_set, "remove_coords_level_tile_proxy", source_id, tile_id);
				}
			}
		}
	}
	undo_redo->commit_action();
	commited_actions_count++;

	hide();
}

void AtlasMergingDialog::ok_pressed() {
	delete_original_atlases = false;
	editor_file_dialog->popup_file_dialog();
}

void AtlasMergingDialog::cancel_pressed() {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	for (int i = 0; i < commited_actions_count; i++) {
		undo_redo->undo();
	}
	commited_actions_count = 0;
}

void AtlasMergingDialog::custom_action(const String &p_action) {
	if (p_action == "merge") {
		delete_original_atlases = true;
		editor_file_dialog->popup_file_dialog();
	}
}

bool AtlasMergingDialog::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "next_line_after_column" && p_value.get_type() == Variant::INT) {
		next_line_after_column = p_value;
		_update_texture();
		return true;
	}
	return false;
}

bool AtlasMergingDialog::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == "next_line_after_column") {
		r_ret = next_line_after_column;
		return true;
	}
	return false;
}

void AtlasMergingDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (is_visible()) {
				_update_texture();
			}
		} break;
	}
}

void AtlasMergingDialog::update_tile_set(Ref<TileSet> p_tile_set) {
	ERR_FAIL_COND(!p_tile_set.is_valid());
	tile_set = p_tile_set;

	atlas_merging_atlases_list->clear();
	for (int i = 0; i < p_tile_set->get_source_count(); i++) {
		int source_id = p_tile_set->get_source_id(i);
		Ref<TileSetAtlasSource> atlas_source = p_tile_set->get_source(source_id);
		if (atlas_source.is_valid()) {
			Ref<Texture2D> texture = atlas_source->get_texture();
			if (texture.is_valid()) {
				String item_text = vformat(TTR("%s (ID: %d)"), texture->get_path().get_file(), source_id);
				atlas_merging_atlases_list->add_item(item_text, texture);
				atlas_merging_atlases_list->set_item_metadata(-1, source_id);
			}
		}
	}

	get_ok_button()->set_disabled(true);
	merge_button->set_disabled(true);

	commited_actions_count = 0;
}

AtlasMergingDialog::AtlasMergingDialog() {
	// Atlas merging window.
	set_title(TTR("Atlas Merging"));
	set_hide_on_ok(false);

	// Ok buttons
	set_ok_button_text(TTR("Merge (Keep original Atlases)"));
	get_ok_button()->set_disabled(true);
	merge_button = add_button(TTR("Merge"), true, "merge");
	merge_button->set_disabled(true);

	HSplitContainer *atlas_merging_h_split_container = memnew(HSplitContainer);
	atlas_merging_h_split_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	atlas_merging_h_split_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(atlas_merging_h_split_container);

	// Atlas sources item list.
	atlas_merging_atlases_list = memnew(ItemList);
	atlas_merging_atlases_list->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	atlas_merging_atlases_list->set_fixed_icon_size(Size2(60, 60) * EDSCALE);
	atlas_merging_atlases_list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	atlas_merging_atlases_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	atlas_merging_atlases_list->set_texture_filter(CanvasItem::TEXTURE_FILTER_NEAREST_WITH_MIPMAPS);
	atlas_merging_atlases_list->set_custom_minimum_size(Size2(100, 200));
	atlas_merging_atlases_list->set_select_mode(ItemList::SELECT_MULTI);
	atlas_merging_atlases_list->set_theme_type_variation("ItemListSecondary");
	atlas_merging_atlases_list->connect("multi_selected", callable_mp(this, &AtlasMergingDialog::_update_texture).unbind(2));
	atlas_merging_h_split_container->add_child(atlas_merging_atlases_list);

	VBoxContainer *atlas_merging_right_panel = memnew(VBoxContainer);
	atlas_merging_right_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	atlas_merging_right_panel->set_texture_filter(CanvasItem::TEXTURE_FILTER_NEAREST_WITH_MIPMAPS);
	atlas_merging_h_split_container->add_child(atlas_merging_right_panel);

	// Settings.
	Label *settings_label = memnew(Label);
	settings_label->set_text(TTR("Settings:"));
	atlas_merging_right_panel->add_child(settings_label);

	columns_editor_property = memnew(EditorPropertyInteger);
	columns_editor_property->set_label(TTR("Next Line After Column"));
	columns_editor_property->set_object_and_property(this, "next_line_after_column");
	columns_editor_property->update_property();
	columns_editor_property->connect("property_changed", callable_mp(this, &AtlasMergingDialog::_property_changed));
	atlas_merging_right_panel->add_child(columns_editor_property);

	// Preview.
	Label *preview_label = memnew(Label);
	preview_label->set_text(TTR("Preview:"));
	atlas_merging_right_panel->add_child(preview_label);

	preview = memnew(TextureRect);
	preview->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	preview->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	preview->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	preview->hide();
	preview->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
	atlas_merging_right_panel->add_child(preview);

	select_2_atlases_label = memnew(Label);
	select_2_atlases_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	select_2_atlases_label->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	select_2_atlases_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	select_2_atlases_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	select_2_atlases_label->set_text(TTR("Please select two atlases or more."));
	atlas_merging_right_panel->add_child(select_2_atlases_label);

	// The file dialog to choose the texture path.
	editor_file_dialog = memnew(EditorFileDialog);
	editor_file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	editor_file_dialog->add_filter("*.png");
	editor_file_dialog->connect("file_selected", callable_mp(this, &AtlasMergingDialog::_merge_confirmed));
	add_child(editor_file_dialog);
}

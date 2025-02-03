/**************************************************************************/
/*  tile_set_editor_plugin.cpp                                            */
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

#include "tile_set_editor_plugin.h"

#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "editor/editor_scale.h"
#include "editor/plugins/canvas_item_editor_plugin.h"
#include "scene/2d/physics_body_2d.h"
#include "scene/2d/sprite.h"

void TileSetEditor::edit(const Ref<TileSet> &p_tileset) {
	tileset = p_tileset;
	tileset->add_change_receptor(this);

	texture_list->clear();
	texture_map.clear();
	update_texture_list();
}

void TileSetEditor::_import_node(Node *p_node, Ref<TileSet> p_library) {
	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *child = p_node->get_child(i);

		if (!Object::cast_to<Sprite>(child)) {
			if (child->get_child_count() > 0) {
				_import_node(child, p_library);
			}

			continue;
		}

		Sprite *mi = Object::cast_to<Sprite>(child);
		Ref<Texture> texture = mi->get_texture();
		Ref<Texture> normal_map = mi->get_normal_map();
		Ref<ShaderMaterial> material = mi->get_material();

		if (texture.is_null()) {
			continue;
		}

		int id = p_library->find_tile_by_name(mi->get_name());
		if (id < 0) {
			id = p_library->get_last_unused_tile_id();
			p_library->create_tile(id);
			p_library->tile_set_name(id, mi->get_name());
		}

		p_library->tile_set_texture(id, texture);
		p_library->tile_set_normal_map(id, normal_map);
		p_library->tile_set_material(id, material);

		p_library->tile_set_modulate(id, mi->get_modulate());

		Vector2 phys_offset;
		Size2 s;

		if (mi->is_region()) {
			s = mi->get_region_rect().size;
			p_library->tile_set_region(id, mi->get_region_rect());
		} else {
			const int frame = mi->get_frame();
			const int hframes = mi->get_hframes();
			s = texture->get_size() / Size2(hframes, mi->get_vframes());
			p_library->tile_set_region(id, Rect2(Vector2(frame % hframes, frame / hframes) * s, s));
		}

		if (mi->is_centered()) {
			phys_offset += -s / 2;
		}

		Vector<TileSet::ShapeData> collisions;
		Ref<NavigationPolygon> nav_poly;
		Ref<OccluderPolygon2D> occluder;
		bool found_collisions = false;

		for (int j = 0; j < mi->get_child_count(); j++) {
			Node *child2 = mi->get_child(j);

			if (Object::cast_to<NavigationPolygonInstance>(child2)) {
				nav_poly = Object::cast_to<NavigationPolygonInstance>(child2)->get_navigation_polygon();
			}

			if (Object::cast_to<LightOccluder2D>(child2)) {
				occluder = Object::cast_to<LightOccluder2D>(child2)->get_occluder_polygon();
			}

			if (!Object::cast_to<StaticBody2D>(child2)) {
				continue;
			}

			found_collisions = true;

			StaticBody2D *sb = Object::cast_to<StaticBody2D>(child2);

			List<uint32_t> shapes;
			sb->get_shape_owners(&shapes);

			for (List<uint32_t>::Element *E = shapes.front(); E; E = E->next()) {
				if (sb->is_shape_owner_disabled(E->get())) {
					continue;
				}

				Transform2D shape_transform = sb->get_transform() * sb->shape_owner_get_transform(E->get());
				bool one_way = sb->is_shape_owner_one_way_collision_enabled(E->get());

				shape_transform[2] -= phys_offset;

				for (int k = 0; k < sb->shape_owner_get_shape_count(E->get()); k++) {
					Ref<Shape2D> shape = sb->shape_owner_get_shape(E->get(), k);
					TileSet::ShapeData shape_data;
					shape_data.shape = shape;
					shape_data.shape_transform = shape_transform;
					shape_data.one_way_collision = one_way;
					collisions.push_back(shape_data);
				}
			}
		}

		if (found_collisions) {
			p_library->tile_set_shapes(id, collisions);
		}

		p_library->tile_set_texture_offset(id, mi->get_offset());
		p_library->tile_set_navigation_polygon(id, nav_poly);
		p_library->tile_set_light_occluder(id, occluder);
		p_library->tile_set_occluder_offset(id, -phys_offset);
		p_library->tile_set_navigation_polygon_offset(id, -phys_offset);
		p_library->tile_set_z_index(id, mi->get_z_index());
	}
}

void TileSetEditor::_import_scene(Node *p_scene, Ref<TileSet> p_library, bool p_merge) {
	if (!p_merge) {
		p_library->clear();
	}

	_import_node(p_scene, p_library);
}

void TileSetEditor::_undo_redo_import_scene(Node *p_scene, bool p_merge) {
	_import_scene(p_scene, tileset, p_merge);
}

Error TileSetEditor::update_library_file(Node *p_base_scene, Ref<TileSet> ml, bool p_merge) {
	_import_scene(p_base_scene, ml, p_merge);
	return OK;
}

Variant TileSetEditor::get_drag_data_fw(const Point2 &p_point, Control *p_from) {
	return false;
}

bool TileSetEditor::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	Dictionary d = p_data;

	if (!d.has("type")) {
		return false;
	}

	if (d.has("from") && (Object *)(d["from"]) == texture_list) {
		return false;
	}

	if (String(d["type"]) == "resource" && d.has("resource")) {
		RES r = d["resource"];

		Ref<Texture> texture = r;

		if (texture.is_valid()) {
			return true;
		}
	}

	if (String(d["type"]) == "files") {
		Vector<String> files = d["files"];

		if (files.size() == 0) {
			return false;
		}

		for (int i = 0; i < files.size(); i++) {
			String file = files[i];
			String ftype = EditorFileSystem::get_singleton()->get_file_type(file);

			if (!ClassDB::is_parent_class(ftype, "Texture")) {
				return false;
			}
		}

		return true;
	}
	return false;
}

void TileSetEditor::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	if (!can_drop_data_fw(p_point, p_data, p_from)) {
		return;
	}

	Dictionary d = p_data;

	if (!d.has("type")) {
		return;
	}

	if (String(d["type"]) == "resource" && d.has("resource")) {
		RES r = d["resource"];

		Ref<Texture> texture = r;

		if (texture.is_valid()) {
			add_texture(texture);
		}

		if (texture_list->get_item_count() > 0) {
			update_texture_list_icon();
			texture_list->select(texture_list->get_item_count() - 1);
			_on_texture_list_selected(texture_list->get_item_count() - 1);
		}
	}

	if (String(d["type"]) == "files") {
		PoolVector<String> files = d["files"];

		_on_textures_added(files);
	}
}

void TileSetEditor::_bind_methods() {
	ClassDB::bind_method("_undo_redo_import_scene", &TileSetEditor::_undo_redo_import_scene);
	ClassDB::bind_method("_on_tileset_toolbar_button_pressed", &TileSetEditor::_on_tileset_toolbar_button_pressed);
	ClassDB::bind_method("_on_textures_added", &TileSetEditor::_on_textures_added);
	ClassDB::bind_method("_on_tileset_toolbar_confirm", &TileSetEditor::_on_tileset_toolbar_confirm);
	ClassDB::bind_method("_on_texture_list_selected", &TileSetEditor::_on_texture_list_selected);
	ClassDB::bind_method("_on_edit_mode_changed", &TileSetEditor::_on_edit_mode_changed);
	ClassDB::bind_method("_on_scroll_container_input", &TileSetEditor::_on_scroll_container_input);
	ClassDB::bind_method("_on_workspace_mode_changed", &TileSetEditor::_on_workspace_mode_changed);
	ClassDB::bind_method("_on_workspace_overlay_draw", &TileSetEditor::_on_workspace_overlay_draw);
	ClassDB::bind_method("_on_workspace_process", &TileSetEditor::_on_workspace_process);
	ClassDB::bind_method("_on_workspace_draw", &TileSetEditor::_on_workspace_draw);
	ClassDB::bind_method("_on_workspace_input", &TileSetEditor::_on_workspace_input);
	ClassDB::bind_method("_on_tool_clicked", &TileSetEditor::_on_tool_clicked);
	ClassDB::bind_method("_on_priority_changed", &TileSetEditor::_on_priority_changed);
	ClassDB::bind_method("_on_z_index_changed", &TileSetEditor::_on_z_index_changed);
	ClassDB::bind_method("_on_grid_snap_toggled", &TileSetEditor::_on_grid_snap_toggled);
	ClassDB::bind_method("_set_snap_step", &TileSetEditor::_set_snap_step);
	ClassDB::bind_method("_set_snap_off", &TileSetEditor::_set_snap_off);
	ClassDB::bind_method("_set_snap_sep", &TileSetEditor::_set_snap_sep);
	ClassDB::bind_method("_validate_current_tile_id", &TileSetEditor::_validate_current_tile_id);
	ClassDB::bind_method("_zoom_in", &TileSetEditor::_zoom_in);
	ClassDB::bind_method("_zoom_out", &TileSetEditor::_zoom_out);
	ClassDB::bind_method("_zoom_reset", &TileSetEditor::_zoom_reset);
	ClassDB::bind_method("_select_edited_shape_coord", &TileSetEditor::_select_edited_shape_coord);
	ClassDB::bind_method("_sort_tiles", &TileSetEditor::_sort_tiles);

	ClassDB::bind_method(D_METHOD("get_drag_data_fw"), &TileSetEditor::get_drag_data_fw);
	ClassDB::bind_method(D_METHOD("can_drop_data_fw"), &TileSetEditor::can_drop_data_fw);
	ClassDB::bind_method(D_METHOD("drop_data_fw"), &TileSetEditor::drop_data_fw);

	ClassDB::bind_method("edit", &TileSetEditor::edit);
	ClassDB::bind_method("add_texture", &TileSetEditor::add_texture);
	ClassDB::bind_method("remove_texture", &TileSetEditor::remove_texture);
	ClassDB::bind_method("update_texture_list_icon", &TileSetEditor::update_texture_list_icon);
	ClassDB::bind_method("update_workspace_minsize", &TileSetEditor::update_workspace_minsize);
}

void TileSetEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			add_constant_override("autohide", 1); // Fixes the dragger always showing up.
		} break;
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			tileset_toolbar_buttons[TOOL_TILESET_ADD_TEXTURE]->set_icon(get_icon("ToolAddNode", "EditorIcons"));
			tileset_toolbar_buttons[TOOL_TILESET_REMOVE_TEXTURE]->set_icon(get_icon("Remove", "EditorIcons"));
			tileset_toolbar_tools->set_icon(get_icon("Tools", "EditorIcons"));

			tool_workspacemode[WORKSPACE_EDIT]->set_icon(get_icon("Edit", "EditorIcons"));
			tool_workspacemode[WORKSPACE_CREATE_SINGLE]->set_icon(get_icon("AddSingleTile", "EditorIcons"));
			tool_workspacemode[WORKSPACE_CREATE_AUTOTILE]->set_icon(get_icon("AddAutotile", "EditorIcons"));
			tool_workspacemode[WORKSPACE_CREATE_ATLAS]->set_icon(get_icon("AddAtlasTile", "EditorIcons"));

			tools[TOOL_SELECT]->set_icon(get_icon("ToolSelect", "EditorIcons"));
			tools[BITMASK_COPY]->set_icon(get_icon("ActionCopy", "EditorIcons"));
			tools[BITMASK_PASTE]->set_icon(get_icon("ActionPaste", "EditorIcons"));
			tools[BITMASK_CLEAR]->set_icon(get_icon("Clear", "EditorIcons"));
			tools[SHAPE_NEW_POLYGON]->set_icon(get_icon("CollisionPolygon2D", "EditorIcons"));
			tools[SHAPE_NEW_RECTANGLE]->set_icon(get_icon("CollisionShape2D", "EditorIcons"));
			tools[SELECT_PREVIOUS]->set_icon(get_icon("ArrowLeft", "EditorIcons"));
			tools[SELECT_NEXT]->set_icon(get_icon("ArrowRight", "EditorIcons"));
			tools[SHAPE_DELETE]->set_icon(get_icon("Remove", "EditorIcons"));
			tools[SHAPE_KEEP_INSIDE_TILE]->set_icon(get_icon("Snap", "EditorIcons"));
			tools[TOOL_GRID_SNAP]->set_icon(get_icon("SnapGrid", "EditorIcons"));
			tools[ZOOM_OUT]->set_icon(get_icon("ZoomLess", "EditorIcons"));
			tools[ZOOM_1]->set_icon(get_icon("ZoomReset", "EditorIcons"));
			tools[ZOOM_IN]->set_icon(get_icon("ZoomMore", "EditorIcons"));
			tools[VISIBLE_INFO]->set_icon(get_icon("InformationSign", "EditorIcons"));
			_update_toggle_shape_button();

			tool_editmode[EDITMODE_REGION]->set_icon(get_icon("RegionEdit", "EditorIcons"));
			tool_editmode[EDITMODE_COLLISION]->set_icon(get_icon("StaticBody2D", "EditorIcons"));
			tool_editmode[EDITMODE_OCCLUSION]->set_icon(get_icon("LightOccluder2D", "EditorIcons"));
			tool_editmode[EDITMODE_NAVIGATION]->set_icon(get_icon("Navigation2D", "EditorIcons"));
			tool_editmode[EDITMODE_BITMASK]->set_icon(get_icon("PackedDataContainer", "EditorIcons"));
			tool_editmode[EDITMODE_PRIORITY]->set_icon(get_icon("MaterialPreviewLight1", "EditorIcons"));
			tool_editmode[EDITMODE_ICON]->set_icon(get_icon("LargeTexture", "EditorIcons"));
			tool_editmode[EDITMODE_Z_INDEX]->set_icon(get_icon("Sort", "EditorIcons"));

			scroll->add_style_override("bg", get_stylebox("bg", "Tree"));
		} break;
	}
}

TileSetEditor::TileSetEditor(EditorNode *p_editor) {
	editor = p_editor;
	undo_redo = EditorNode::get_undo_redo();
	current_tile = -1;

	VBoxContainer *left_container = memnew(VBoxContainer);
	add_child(left_container);

	texture_list = memnew(ItemList);
	left_container->add_child(texture_list);
	texture_list->set_v_size_flags(SIZE_EXPAND_FILL);
	texture_list->set_custom_minimum_size(Size2(200, 0));
	texture_list->connect("item_selected", this, "_on_texture_list_selected");
	texture_list->set_drag_forwarding(this);

	HBoxContainer *tileset_toolbar_container = memnew(HBoxContainer);
	left_container->add_child(tileset_toolbar_container);

	tileset_toolbar_buttons[TOOL_TILESET_ADD_TEXTURE] = memnew(ToolButton);
	tileset_toolbar_buttons[TOOL_TILESET_ADD_TEXTURE]->connect("pressed", this, "_on_tileset_toolbar_button_pressed", varray(TOOL_TILESET_ADD_TEXTURE));
	tileset_toolbar_container->add_child(tileset_toolbar_buttons[TOOL_TILESET_ADD_TEXTURE]);
	tileset_toolbar_buttons[TOOL_TILESET_ADD_TEXTURE]->set_tooltip(TTR("Add Texture(s) to TileSet."));

	tileset_toolbar_buttons[TOOL_TILESET_REMOVE_TEXTURE] = memnew(ToolButton);
	tileset_toolbar_buttons[TOOL_TILESET_REMOVE_TEXTURE]->connect("pressed", this, "_on_tileset_toolbar_button_pressed", varray(TOOL_TILESET_REMOVE_TEXTURE));
	tileset_toolbar_container->add_child(tileset_toolbar_buttons[TOOL_TILESET_REMOVE_TEXTURE]);
	tileset_toolbar_buttons[TOOL_TILESET_REMOVE_TEXTURE]->set_tooltip(TTR("Remove selected Texture from TileSet."));

	Control *toolbar_separator = memnew(Control);
	toolbar_separator->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tileset_toolbar_container->add_child(toolbar_separator);

	tileset_toolbar_tools = memnew(MenuButton);
	tileset_toolbar_tools->set_text(TTR("Tools"));
	tileset_toolbar_tools->get_popup()->add_item(TTR("Create from Scene"), TOOL_TILESET_CREATE_SCENE);
	tileset_toolbar_tools->get_popup()->add_item(TTR("Merge from Scene"), TOOL_TILESET_MERGE_SCENE);

	tileset_toolbar_tools->get_popup()->connect("id_pressed", this, "_on_tileset_toolbar_button_pressed");
	tileset_toolbar_container->add_child(tileset_toolbar_tools);

	//---------------
	VBoxContainer *right_container = memnew(VBoxContainer);
	right_container->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(right_container);

	dragging_point = -1;
	creating_shape = false;
	snap_step = Vector2(32, 32);
	snap_offset = WORKSPACE_MARGIN;

	set_custom_minimum_size(Size2(0, 150));

	VBoxContainer *main_vb = memnew(VBoxContainer);
	right_container->add_child(main_vb);
	main_vb->set_v_size_flags(SIZE_EXPAND_FILL);

	HBoxContainer *tool_hb = memnew(HBoxContainer);
	Ref<ButtonGroup> g(memnew(ButtonGroup));

	String workspace_label[WORKSPACE_MODE_MAX] = {
		TTR("Edit"),
		TTR("New Single Tile"),
		TTR("New Autotile"),
		TTR("New Atlas")
	};
	for (int i = 0; i < (int)WORKSPACE_MODE_MAX; i++) {
		tool_workspacemode[i] = memnew(Button);
		tool_workspacemode[i]->set_text(workspace_label[i]);
		tool_workspacemode[i]->set_toggle_mode(true);
		tool_workspacemode[i]->set_button_group(g);
		tool_workspacemode[i]->connect("pressed", this, "_on_workspace_mode_changed", varray(i));
		tool_hb->add_child(tool_workspacemode[i]);
	}

	Control *spacer = memnew(Control);
	spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tool_hb->add_child(spacer);
	tool_hb->move_child(spacer, WORKSPACE_CREATE_SINGLE);

	tools[SELECT_NEXT] = memnew(ToolButton);
	tool_hb->add_child(tools[SELECT_NEXT]);
	tool_hb->move_child(tools[SELECT_NEXT], WORKSPACE_CREATE_SINGLE);
	tools[SELECT_NEXT]->set_shortcut(ED_SHORTCUT("tileset_editor/next_shape", TTR("Next Coordinate"), KEY_PAGEDOWN));
	tools[SELECT_NEXT]->connect("pressed", this, "_on_tool_clicked", varray(SELECT_NEXT));
	tools[SELECT_NEXT]->set_tooltip(TTR("Select the next shape, subtile, or Tile."));
	tools[SELECT_PREVIOUS] = memnew(ToolButton);
	tool_hb->add_child(tools[SELECT_PREVIOUS]);
	tool_hb->move_child(tools[SELECT_PREVIOUS], WORKSPACE_CREATE_SINGLE);
	tools[SELECT_PREVIOUS]->set_shortcut(ED_SHORTCUT("tileset_editor/previous_shape", TTR("Previous Coordinate"), KEY_PAGEUP));
	tools[SELECT_PREVIOUS]->set_tooltip(TTR("Select the previous shape, subtile, or Tile."));
	tools[SELECT_PREVIOUS]->connect("pressed", this, "_on_tool_clicked", varray(SELECT_PREVIOUS));

	VSeparator *separator_shape_selection = memnew(VSeparator);
	tool_hb->add_child(separator_shape_selection);
	tool_hb->move_child(separator_shape_selection, WORKSPACE_CREATE_SINGLE);

	tool_workspacemode[WORKSPACE_EDIT]->set_pressed(true);
	workspace_mode = WORKSPACE_EDIT;

	main_vb->add_child(tool_hb);
	main_vb->add_child(memnew(HSeparator));

	tool_hb = memnew(HBoxContainer);

	g = Ref<ButtonGroup>(memnew(ButtonGroup));
	String label[EDITMODE_MAX] = {
		TTR("Region"),
		TTR("Collision"),
		TTR("Occlusion"),
		TTR("Navigation"),
		TTR("Bitmask"),
		TTR("Priority"),
		TTR("Icon"),
		TTR("Z Index")
	};
	for (int i = 0; i < (int)EDITMODE_MAX; i++) {
		tool_editmode[i] = memnew(Button);
		tool_editmode[i]->set_text(label[i]);
		tool_editmode[i]->set_toggle_mode(true);
		tool_editmode[i]->set_button_group(g);
		tool_editmode[i]->connect("pressed", this, "_on_edit_mode_changed", varray(i));
		tool_hb->add_child(tool_editmode[i]);
	}
	tool_editmode[EDITMODE_COLLISION]->set_pressed(true);
	edit_mode = EDITMODE_COLLISION;

	tool_editmode[EDITMODE_REGION]->set_shortcut(ED_SHORTCUT("tileset_editor/editmode_region", TTR("Region Mode"), KEY_1));
	tool_editmode[EDITMODE_COLLISION]->set_shortcut(ED_SHORTCUT("tileset_editor/editmode_collision", TTR("Collision Mode"), KEY_2));
	tool_editmode[EDITMODE_OCCLUSION]->set_shortcut(ED_SHORTCUT("tileset_editor/editmode_occlusion", TTR("Occlusion Mode"), KEY_3));
	tool_editmode[EDITMODE_NAVIGATION]->set_shortcut(ED_SHORTCUT("tileset_editor/editmode_navigation", TTR("Navigation Mode"), KEY_4));
	tool_editmode[EDITMODE_BITMASK]->set_shortcut(ED_SHORTCUT("tileset_editor/editmode_bitmask", TTR("Bitmask Mode"), KEY_5));
	tool_editmode[EDITMODE_PRIORITY]->set_shortcut(ED_SHORTCUT("tileset_editor/editmode_priority", TTR("Priority Mode"), KEY_6));
	tool_editmode[EDITMODE_ICON]->set_shortcut(ED_SHORTCUT("tileset_editor/editmode_icon", TTR("Icon Mode"), KEY_7));
	tool_editmode[EDITMODE_Z_INDEX]->set_shortcut(ED_SHORTCUT("tileset_editor/editmode_z_index", TTR("Z Index Mode"), KEY_8));

	main_vb->add_child(tool_hb);
	separator_editmode = memnew(HSeparator);
	main_vb->add_child(separator_editmode);

	toolbar = memnew(HBoxContainer);
	Ref<ButtonGroup> tg(memnew(ButtonGroup));

	tools[TOOL_SELECT] = memnew(ToolButton);
	toolbar->add_child(tools[TOOL_SELECT]);
	tools[TOOL_SELECT]->set_toggle_mode(true);
	tools[TOOL_SELECT]->set_button_group(tg);
	tools[TOOL_SELECT]->set_pressed(true);
	tools[TOOL_SELECT]->connect("pressed", this, "_on_tool_clicked", varray(TOOL_SELECT));

	separator_bitmask = memnew(VSeparator);
	toolbar->add_child(separator_bitmask);
	tools[BITMASK_COPY] = memnew(ToolButton);
	tools[BITMASK_COPY]->set_tooltip(TTR("Copy bitmask."));
	tools[BITMASK_COPY]->connect("pressed", this, "_on_tool_clicked", varray(BITMASK_COPY));
	toolbar->add_child(tools[BITMASK_COPY]);
	tools[BITMASK_PASTE] = memnew(ToolButton);
	tools[BITMASK_PASTE]->set_tooltip(TTR("Paste bitmask."));
	tools[BITMASK_PASTE]->connect("pressed", this, "_on_tool_clicked", varray(BITMASK_PASTE));
	toolbar->add_child(tools[BITMASK_PASTE]);
	tools[BITMASK_CLEAR] = memnew(ToolButton);
	tools[BITMASK_CLEAR]->set_tooltip(TTR("Erase bitmask."));
	tools[BITMASK_CLEAR]->connect("pressed", this, "_on_tool_clicked", varray(BITMASK_CLEAR));
	toolbar->add_child(tools[BITMASK_CLEAR]);

	tools[SHAPE_NEW_RECTANGLE] = memnew(ToolButton);
	toolbar->add_child(tools[SHAPE_NEW_RECTANGLE]);
	tools[SHAPE_NEW_RECTANGLE]->set_toggle_mode(true);
	tools[SHAPE_NEW_RECTANGLE]->set_button_group(tg);
	tools[SHAPE_NEW_RECTANGLE]->set_tooltip(TTR("Create a new rectangle."));
	tools[SHAPE_NEW_RECTANGLE]->connect("pressed", this, "_on_tool_clicked", varray(SHAPE_NEW_RECTANGLE));
	tools[SHAPE_NEW_RECTANGLE]->set_shortcut(ED_SHORTCUT("tileset_editor/shape_new_rectangle", TTR("New Rectangle"), KEY_MASK_SHIFT | KEY_R));

	tools[SHAPE_NEW_POLYGON] = memnew(ToolButton);
	toolbar->add_child(tools[SHAPE_NEW_POLYGON]);
	tools[SHAPE_NEW_POLYGON]->set_toggle_mode(true);
	tools[SHAPE_NEW_POLYGON]->set_button_group(tg);
	tools[SHAPE_NEW_POLYGON]->set_tooltip(TTR("Create a new polygon."));
	tools[SHAPE_NEW_POLYGON]->connect("pressed", this, "_on_tool_clicked", varray(SHAPE_NEW_POLYGON));
	tools[SHAPE_NEW_POLYGON]->set_shortcut(ED_SHORTCUT("tileset_editor/shape_new_polygon", TTR("New Polygon"), KEY_MASK_SHIFT | KEY_P));

	separator_shape_toggle = memnew(VSeparator);
	toolbar->add_child(separator_shape_toggle);
	tools[SHAPE_TOGGLE_TYPE] = memnew(ToolButton);
	tools[SHAPE_TOGGLE_TYPE]->connect("pressed", this, "_on_tool_clicked", varray(SHAPE_TOGGLE_TYPE));
	toolbar->add_child(tools[SHAPE_TOGGLE_TYPE]);

	separator_delete = memnew(VSeparator);
	toolbar->add_child(separator_delete);
	tools[SHAPE_DELETE] = memnew(ToolButton);
	tools[SHAPE_DELETE]->connect("pressed", this, "_on_tool_clicked", varray(SHAPE_DELETE));
	tools[SHAPE_DELETE]->set_shortcut(ED_SHORTCUT("tileset_editor/shape_delete", TTR("Delete Selected Shape"), KEY_MASK_SHIFT | KEY_BACKSPACE));
	toolbar->add_child(tools[SHAPE_DELETE]);

	spin_priority = memnew(SpinBox);
	spin_priority->set_min(1);
	spin_priority->set_max(255);
	spin_priority->set_step(1);
	spin_priority->set_custom_minimum_size(Size2(100, 0));
	spin_priority->connect("value_changed", this, "_on_priority_changed");
	spin_priority->hide();
	toolbar->add_child(spin_priority);

	spin_z_index = memnew(SpinBox);
	spin_z_index->set_min(VS::CANVAS_ITEM_Z_MIN);
	spin_z_index->set_max(VS::CANVAS_ITEM_Z_MAX);
	spin_z_index->set_step(1);
	spin_z_index->set_custom_minimum_size(Size2(100, 0));
	spin_z_index->connect("value_changed", this, "_on_z_index_changed");
	spin_z_index->hide();
	toolbar->add_child(spin_z_index);

	separator_grid = memnew(VSeparator);
	toolbar->add_child(separator_grid);
	tools[SHAPE_KEEP_INSIDE_TILE] = memnew(ToolButton);
	tools[SHAPE_KEEP_INSIDE_TILE]->set_toggle_mode(true);
	tools[SHAPE_KEEP_INSIDE_TILE]->set_pressed(true);
	tools[SHAPE_KEEP_INSIDE_TILE]->set_tooltip(TTR("Keep polygon inside region Rect."));
	toolbar->add_child(tools[SHAPE_KEEP_INSIDE_TILE]);
	tools[TOOL_GRID_SNAP] = memnew(ToolButton);
	tools[TOOL_GRID_SNAP]->set_toggle_mode(true);
	tools[TOOL_GRID_SNAP]->set_tooltip(TTR("Enable snap and show grid (configurable via the Inspector)."));
	tools[TOOL_GRID_SNAP]->connect("toggled", this, "_on_grid_snap_toggled");
	toolbar->add_child(tools[TOOL_GRID_SNAP]);

	Control *separator = memnew(Control);
	separator->set_h_size_flags(SIZE_EXPAND_FILL);
	toolbar->add_child(separator);

	tools[ZOOM_OUT] = memnew(ToolButton);
	tools[ZOOM_OUT]->connect("pressed", this, "_zoom_out");
	toolbar->add_child(tools[ZOOM_OUT]);
	tools[ZOOM_OUT]->set_tooltip(TTR("Zoom Out"));
	tools[ZOOM_1] = memnew(ToolButton);
	tools[ZOOM_1]->connect("pressed", this, "_zoom_reset");
	toolbar->add_child(tools[ZOOM_1]);
	tools[ZOOM_1]->set_tooltip(TTR("Zoom Reset"));
	tools[ZOOM_IN] = memnew(ToolButton);
	tools[ZOOM_IN]->connect("pressed", this, "_zoom_in");
	toolbar->add_child(tools[ZOOM_IN]);
	tools[ZOOM_IN]->set_tooltip(TTR("Zoom In"));

	tools[VISIBLE_INFO] = memnew(ToolButton);
	tools[VISIBLE_INFO]->set_toggle_mode(true);
	tools[VISIBLE_INFO]->set_tooltip(TTR("Display Tile Names (Hold Alt Key)"));
	toolbar->add_child(tools[VISIBLE_INFO]);

	main_vb->add_child(toolbar);

	scroll = memnew(ScrollContainer);
	main_vb->add_child(scroll);
	scroll->set_v_size_flags(SIZE_EXPAND_FILL);
	scroll->connect("gui_input", this, "_on_scroll_container_input");
	scroll->set_clip_contents(true);

	empty_message = memnew(Label);
	empty_message->set_text(TTR("Add or select a texture on the left panel to edit the tiles bound to it."));
	empty_message->set_valign(Label::VALIGN_CENTER);
	empty_message->set_align(Label::ALIGN_CENTER);
	empty_message->set_autowrap(true);
	empty_message->set_custom_minimum_size(Size2(100 * EDSCALE, 0));
	empty_message->set_v_size_flags(SIZE_EXPAND_FILL);
	main_vb->add_child(empty_message);

	workspace_container = memnew(Control);
	scroll->add_child(workspace_container);

	workspace_overlay = memnew(Control);
	workspace_overlay->connect("draw", this, "_on_workspace_overlay_draw");
	workspace_container->add_child(workspace_overlay);

	workspace = memnew(Control);
	workspace->set_focus_mode(FOCUS_ALL);
	workspace->connect("draw", this, "_on_workspace_draw");
	workspace->connect("gui_input", this, "_on_workspace_input");
	workspace->set_draw_behind_parent(true);
	workspace_overlay->add_child(workspace);

	preview = memnew(Sprite);
	workspace->add_child(preview);
	preview->set_centered(false);
	preview->set_draw_behind_parent(true);
	preview->set_position(WORKSPACE_MARGIN);

	//---------------
	cd = memnew(ConfirmationDialog);
	add_child(cd);
	cd->connect("confirmed", this, "_on_tileset_toolbar_confirm");

	//---------------
	err_dialog = memnew(AcceptDialog);
	add_child(err_dialog);

	//---------------
	texture_dialog = memnew(EditorFileDialog);
	texture_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
	texture_dialog->set_mode(EditorFileDialog::MODE_OPEN_FILES);
	texture_dialog->clear_filters();
	List<String> extensions;

	ResourceLoader::get_recognized_extensions_for_type("Texture", &extensions);
	for (List<String>::Element *E = extensions.front(); E; E = E->next()) {
		texture_dialog->add_filter("*." + E->get(), E->get().to_upper());
	}
	add_child(texture_dialog);
	texture_dialog->connect("files_selected", this, "_on_textures_added");

	//---------------
	helper = memnew(TilesetEditorContext(this));
	tile_names_visible = false;

	// Config scale.
	max_scale = 16.0f;
	min_scale = 0.01f;
	scale_ratio = 1.2f;
}

TileSetEditor::~TileSetEditor() {
	if (helper) {
		memdelete(helper);
	}
}

void TileSetEditor::_on_tileset_toolbar_button_pressed(int p_index) {
	option = p_index;
	switch (option) {
		case TOOL_TILESET_ADD_TEXTURE: {
			texture_dialog->popup_centered_ratio();
		} break;
		case TOOL_TILESET_REMOVE_TEXTURE: {
			if (get_current_texture().is_valid()) {
				cd->set_text(TTR("Remove selected texture? This will remove all tiles which use it."));
				cd->popup_centered(Size2(300, 60));
			} else {
				err_dialog->set_text(TTR("You haven't selected a texture to remove."));
				err_dialog->popup_centered(Size2(300, 60));
			}
		} break;
		case TOOL_TILESET_CREATE_SCENE: {
			cd->set_text(TTR("Create from scene? This will overwrite all current tiles."));
			cd->popup_centered(Size2(300, 60));
		} break;
		case TOOL_TILESET_MERGE_SCENE: {
			cd->set_text(TTR("Merge from scene?"));
			cd->popup_centered(Size2(300, 60));
		} break;
	}
}

void TileSetEditor::_on_tileset_toolbar_confirm() {
	switch (option) {
		case TOOL_TILESET_REMOVE_TEXTURE: {
			String current_texture_path = get_current_texture()->get_path();
			List<int> ids;
			tileset->get_tile_list(&ids);

			undo_redo->create_action(TTR("Remove Texture"));
			for (List<int>::Element *E = ids.front(); E; E = E->next()) {
				if (tileset->tile_get_texture(E->get())->get_path() == current_texture_path) {
					undo_redo->add_do_method(tileset.ptr(), "remove_tile", E->get());
					_undo_tile_removal(E->get());
				}
			}
			undo_redo->add_do_method(this, "remove_texture", get_current_texture());
			undo_redo->add_undo_method(this, "add_texture", get_current_texture());
			undo_redo->add_undo_method(this, "update_texture_list_icon");
			undo_redo->commit_action();
		} break;
		case TOOL_TILESET_MERGE_SCENE:
		case TOOL_TILESET_CREATE_SCENE: {
			EditorNode *en = editor;
			Node *scene = en->get_edited_scene();
			if (!scene) {
				break;
			}

			List<int> ids;
			tileset->get_tile_list(&ids);

			undo_redo->create_action(TTR(option == TOOL_TILESET_MERGE_SCENE ? "Merge Tileset from Scene" : "Create Tileset from Scene"));
			undo_redo->add_do_method(this, "_undo_redo_import_scene", scene, option == TOOL_TILESET_MERGE_SCENE);
			undo_redo->add_undo_method(tileset.ptr(), "clear");
			for (List<int>::Element *E = ids.front(); E; E = E->next()) {
				_undo_tile_removal(E->get());
			}
			undo_redo->add_do_method(this, "edit", tileset);
			undo_redo->add_undo_method(this, "edit", tileset);
			undo_redo->commit_action();
		} break;
	}
}

void TileSetEditor::_on_texture_list_selected(int p_index) {
	if (get_current_texture().is_valid()) {
		current_item_index = p_index;
		preview->set_texture(get_current_texture());
		update_workspace_tile_mode();
		update_workspace_minsize();
	} else {
		current_item_index = -1;
		preview->set_texture(nullptr);
		workspace->set_custom_minimum_size(Size2i());
		update_workspace_tile_mode();
	}

	set_current_tile(-1);
	workspace->update();
}

void TileSetEditor::_on_textures_added(const PoolStringArray &p_paths) {
	int invalid_count = 0;
	for (int i = 0; i < p_paths.size(); i++) {
		Ref<Texture> t = Ref<Texture>(ResourceLoader::load(p_paths[i]));

		ERR_CONTINUE_MSG(!t.is_valid(), "'" + p_paths[i] + "' is not a valid texture.");

		if (texture_map.has(t->get_path())) {
			invalid_count++;
		} else {
			add_texture(t);
		}
	}

	if (texture_list->get_item_count() > 0) {
		update_texture_list_icon();
		texture_list->select(texture_list->get_item_count() - 1);
		_on_texture_list_selected(texture_list->get_item_count() - 1);
	}

	if (invalid_count > 0) {
		err_dialog->set_text(vformat(TTR("%s file(s) were not added because was already on the list."), String::num(invalid_count, 0)));
		err_dialog->popup_centered(Size2(300, 60));
	}
}

void TileSetEditor::_on_edit_mode_changed(int p_edit_mode) {
	draw_handles = false;
	creating_shape = false;
	edit_mode = (EditMode)p_edit_mode;
	switch (edit_mode) {
		case EDITMODE_REGION: {
			tools[TOOL_SELECT]->show();

			separator_bitmask->hide();
			tools[BITMASK_COPY]->hide();
			tools[BITMASK_PASTE]->hide();
			tools[BITMASK_CLEAR]->hide();
			tools[SHAPE_NEW_POLYGON]->hide();
			tools[SHAPE_NEW_RECTANGLE]->hide();

			if (workspace_mode == WORKSPACE_EDIT) {
				separator_delete->show();
				tools[SHAPE_DELETE]->show();
			} else {
				separator_delete->hide();
				tools[SHAPE_DELETE]->hide();
			}

			separator_grid->show();
			tools[SHAPE_KEEP_INSIDE_TILE]->hide();
			tools[TOOL_GRID_SNAP]->show();

			tools[TOOL_SELECT]->set_pressed(true);
			tools[TOOL_SELECT]->set_tooltip(TTR("Drag handles to edit Rect.\nClick on another Tile to edit it."));
			tools[SHAPE_DELETE]->set_tooltip(TTR("Delete selected Rect."));
			spin_priority->hide();
			spin_z_index->hide();
		} break;
		case EDITMODE_COLLISION:
		case EDITMODE_OCCLUSION:
		case EDITMODE_NAVIGATION: {
			tools[TOOL_SELECT]->show();

			separator_bitmask->hide();
			tools[BITMASK_COPY]->hide();
			tools[BITMASK_PASTE]->hide();
			tools[BITMASK_CLEAR]->hide();
			tools[SHAPE_NEW_POLYGON]->show();
			tools[SHAPE_NEW_RECTANGLE]->show();

			separator_delete->show();
			tools[SHAPE_DELETE]->show();

			separator_grid->show();
			tools[SHAPE_KEEP_INSIDE_TILE]->show();
			tools[TOOL_GRID_SNAP]->show();

			tools[TOOL_SELECT]->set_tooltip(TTR("Select current edited sub-tile.\nClick on another Tile to edit it."));
			tools[SHAPE_DELETE]->set_tooltip(TTR("Delete polygon."));
			spin_priority->hide();
			spin_z_index->hide();

			_select_edited_shape_coord();
		} break;
		case EDITMODE_BITMASK: {
			tools[TOOL_SELECT]->show();

			separator_bitmask->show();
			tools[BITMASK_COPY]->show();
			tools[BITMASK_PASTE]->show();
			tools[BITMASK_CLEAR]->show();
			tools[SHAPE_NEW_POLYGON]->hide();
			tools[SHAPE_NEW_RECTANGLE]->hide();

			separator_delete->hide();
			tools[SHAPE_DELETE]->hide();

			tools[SHAPE_KEEP_INSIDE_TILE]->hide();

			tools[TOOL_SELECT]->set_pressed(true);
			tools[TOOL_SELECT]->set_tooltip(TTR("LMB: Set bit on.\nRMB: Set bit off.\nShift+LMB: Set wildcard bit.\nClick on another Tile to edit it."));
			spin_priority->hide();
		} break;
		case EDITMODE_Z_INDEX:
		case EDITMODE_PRIORITY:
		case EDITMODE_ICON: {
			tools[TOOL_SELECT]->show();

			separator_bitmask->hide();
			tools[BITMASK_COPY]->hide();
			tools[BITMASK_PASTE]->hide();
			tools[BITMASK_CLEAR]->hide();
			tools[SHAPE_NEW_POLYGON]->hide();
			tools[SHAPE_NEW_RECTANGLE]->hide();

			separator_delete->hide();
			tools[SHAPE_DELETE]->hide();

			separator_grid->show();
			tools[SHAPE_KEEP_INSIDE_TILE]->hide();
			tools[TOOL_GRID_SNAP]->show();

			if (edit_mode == EDITMODE_ICON) {
				tools[TOOL_SELECT]->set_tooltip(TTR("Select sub-tile to use as icon, this will be also used on invalid autotile bindings.\nClick on another Tile to edit it."));
				spin_priority->hide();
				spin_z_index->hide();
			} else if (edit_mode == EDITMODE_PRIORITY) {
				tools[TOOL_SELECT]->set_tooltip(TTR("Select sub-tile to change its priority.\nClick on another Tile to edit it."));
				spin_priority->show();
				spin_z_index->hide();
			} else {
				tools[TOOL_SELECT]->set_tooltip(TTR("Select sub-tile to change its z index.\nClick on another Tile to edit it."));
				spin_priority->hide();
				spin_z_index->show();
			}
		} break;
		default: {
		}
	}
	_update_toggle_shape_button();
	workspace->update();
}

void TileSetEditor::_on_workspace_mode_changed(int p_workspace_mode) {
	workspace_mode = (WorkspaceMode)p_workspace_mode;
	if (p_workspace_mode == WORKSPACE_EDIT) {
		update_workspace_tile_mode();
	} else {
		for (int i = 0; i < EDITMODE_MAX; i++) {
			tool_editmode[i]->hide();
		}
		tool_editmode[EDITMODE_REGION]->show();
		tool_editmode[EDITMODE_REGION]->set_pressed(true);
		_on_edit_mode_changed(EDITMODE_REGION);
		separator_editmode->show();
	}
}

void TileSetEditor::_on_workspace_draw() {
	if (tileset.is_null() || !get_current_texture().is_valid()) {
		return;
	}

	const Color COLOR_AUTOTILE = Color(0.3, 0.6, 1);
	const Color COLOR_SINGLE = Color(1, 1, 0.3);
	const Color COLOR_ATLAS = Color(0.8, 0.8, 0.8);
	const Color COLOR_SUBDIVISION = Color(0.3, 0.7, 0.6);

	draw_handles = false;

	draw_highlight_current_tile();

	draw_grid_snap();
	if (get_current_tile() >= 0) {
		int spacing = tileset->autotile_get_spacing(get_current_tile());
		Vector2 size = tileset->autotile_get_size(get_current_tile());
		Rect2i region = tileset->tile_get_region(get_current_tile());

		switch (edit_mode) {
			case EDITMODE_ICON: {
				Vector2 coord = tileset->autotile_get_icon_coordinate(get_current_tile());
				draw_highlight_subtile(coord);
			} break;
			case EDITMODE_BITMASK: {
				Color c(1, 0, 0, 0.5);
				Color ci(0.3, 0.6, 1, 0.5);
				for (int x = 0; x < region.size.x / (spacing + size.x); x++) {
					for (int y = 0; y < region.size.y / (spacing + size.y); y++) {
						Vector2 coord(x, y);
						Point2 anchor(coord.x * (spacing + size.x), coord.y * (spacing + size.y));
						anchor += WORKSPACE_MARGIN;
						anchor += region.position;
						uint32_t mask = tileset->autotile_get_bitmask(get_current_tile(), coord);
						if (tileset->autotile_get_bitmask_mode(get_current_tile()) == TileSet::BITMASK_2X2) {
							if (mask & TileSet::BIND_IGNORE_TOPLEFT) {
								workspace->draw_rect(Rect2(anchor, size / 4), ci);
								workspace->draw_rect(Rect2(anchor + size / 4, size / 4), ci);
							} else if (mask & TileSet::BIND_TOPLEFT) {
								workspace->draw_rect(Rect2(anchor, size / 2), c);
							}
							if (mask & TileSet::BIND_IGNORE_TOPRIGHT) {
								workspace->draw_rect(Rect2(anchor + Vector2(size.x / 2, 0), size / 4), ci);
								workspace->draw_rect(Rect2(anchor + Vector2(size.x * 3 / 4, size.y / 4), size / 4), ci);
							} else if (mask & TileSet::BIND_TOPRIGHT) {
								workspace->draw_rect(Rect2(anchor + Vector2(size.x / 2, 0), size / 2), c);
							}
							if (mask & TileSet::BIND_IGNORE_BOTTOMLEFT) {
								workspace->draw_rect(Rect2(anchor + Vector2(0, size.y / 2), size / 4), ci);
								workspace->draw_rect(Rect2(anchor + Vector2(size.x / 4, size.y * 3 / 4), size / 4), ci);
							} else if (mask & TileSet::BIND_BOTTOMLEFT) {
								workspace->draw_rect(Rect2(anchor + Vector2(0, size.y / 2), size / 2), c);
							}
							if (mask & TileSet::BIND_IGNORE_BOTTOMRIGHT) {
								workspace->draw_rect(Rect2(anchor + size / 2, size / 4), ci);
								workspace->draw_rect(Rect2(anchor + size * 3 / 4, size / 4), ci);
							} else if (mask & TileSet::BIND_BOTTOMRIGHT) {
								workspace->draw_rect(Rect2(anchor + size / 2, size / 2), c);
							}
						} else {
							if (mask & TileSet::BIND_IGNORE_TOPLEFT) {
								workspace->draw_rect(Rect2(anchor, size / 6), ci);
								workspace->draw_rect(Rect2(anchor + size / 6, size / 6), ci);
							} else if (mask & TileSet::BIND_TOPLEFT) {
								workspace->draw_rect(Rect2(anchor, size / 3), c);
							}
							if (mask & TileSet::BIND_IGNORE_TOP) {
								workspace->draw_rect(Rect2(anchor + Vector2(size.x / 3, 0), size / 6), ci);
								workspace->draw_rect(Rect2(anchor + Vector2(size.x / 2, size.y / 6), size / 6), ci);
							} else if (mask & TileSet::BIND_TOP) {
								workspace->draw_rect(Rect2(anchor + Vector2(size.x / 3, 0), size / 3), c);
							}
							if (mask & TileSet::BIND_IGNORE_TOPRIGHT) {
								workspace->draw_rect(Rect2(anchor + Vector2(size.x * 4 / 6, 0), size / 6), ci);
								workspace->draw_rect(Rect2(anchor + Vector2(size.x * 5 / 6, size.y / 6), size / 6), ci);
							} else if (mask & TileSet::BIND_TOPRIGHT) {
								workspace->draw_rect(Rect2(anchor + Vector2((size.x / 3) * 2, 0), size / 3), c);
							}
							if (mask & TileSet::BIND_IGNORE_LEFT) {
								workspace->draw_rect(Rect2(anchor + Vector2(0, size.y / 3), size / 6), ci);
								workspace->draw_rect(Rect2(anchor + Vector2(size.x / 6, size.y / 2), size / 6), ci);
							} else if (mask & TileSet::BIND_LEFT) {
								workspace->draw_rect(Rect2(anchor + Vector2(0, size.y / 3), size / 3), c);
							}
							if (mask & TileSet::BIND_IGNORE_CENTER) {
								workspace->draw_rect(Rect2(anchor + size / 3, size / 6), ci);
								workspace->draw_rect(Rect2(anchor + size / 2, size / 6), ci);
							} else if (mask & TileSet::BIND_CENTER) {
								workspace->draw_rect(Rect2(anchor + Vector2(size.x / 3, size.y / 3), size / 3), c);
							}
							if (mask & TileSet::BIND_IGNORE_RIGHT) {
								workspace->draw_rect(Rect2(anchor + Vector2(size.x * 4 / 6, size.y / 3), size / 6), ci);
								workspace->draw_rect(Rect2(anchor + Vector2(size.x * 5 / 6, size.y / 2), size / 6), ci);
							} else if (mask & TileSet::BIND_RIGHT) {
								workspace->draw_rect(Rect2(anchor + Vector2((size.x / 3) * 2, size.y / 3), size / 3), c);
							}
							if (mask & TileSet::BIND_IGNORE_BOTTOMLEFT) {
								workspace->draw_rect(Rect2(anchor + Vector2(0, size.y * 4 / 6), size / 6), ci);
								workspace->draw_rect(Rect2(anchor + Vector2(size.x / 6, size.y * 5 / 6), size / 6), ci);
							} else if (mask & TileSet::BIND_BOTTOMLEFT) {
								workspace->draw_rect(Rect2(anchor + Vector2(0, (size.y / 3) * 2), size / 3), c);
							}
							if (mask & TileSet::BIND_IGNORE_BOTTOM) {
								workspace->draw_rect(Rect2(anchor + Vector2(size.x / 3, size.y * 4 / 6), size / 6), ci);
								workspace->draw_rect(Rect2(anchor + Vector2(size.x / 2, size.y * 5 / 6), size / 6), ci);
							} else if (mask & TileSet::BIND_BOTTOM) {
								workspace->draw_rect(Rect2(anchor + Vector2(size.x / 3, (size.y / 3) * 2), size / 3), c);
							}
							if (mask & TileSet::BIND_IGNORE_BOTTOMRIGHT) {
								workspace->draw_rect(Rect2(anchor + size * 4 / 6, size / 6), ci);
								workspace->draw_rect(Rect2(anchor + size * 5 / 6, size / 6), ci);
							} else if (mask & TileSet::BIND_BOTTOMRIGHT) {
								workspace->draw_rect(Rect2(anchor + (size / 3) * 2, size / 3), c);
							}
						}
					}
				}
			} break;
			case EDITMODE_COLLISION:
			case EDITMODE_OCCLUSION:
			case EDITMODE_NAVIGATION: {
				if (tileset->tile_get_tile_mode(get_current_tile()) == TileSet::AUTO_TILE || tileset->tile_get_tile_mode(get_current_tile()) == TileSet::ATLAS_TILE) {
					draw_highlight_subtile(edited_shape_coord);
				}
				draw_polygon_shapes();
				draw_grid_snap();
			} break;
			case EDITMODE_PRIORITY: {
				spin_priority->set_value(tileset->autotile_get_subtile_priority(get_current_tile(), edited_shape_coord));
				uint32_t mask = tileset->autotile_get_bitmask(get_current_tile(), edited_shape_coord);
				Vector<Vector2> queue_others;
				int total = 0;
				for (Map<Vector2, uint32_t>::Element *E = tileset->autotile_get_bitmask_map(get_current_tile()).front(); E; E = E->next()) {
					if (E->value() == mask) {
						total += tileset->autotile_get_subtile_priority(get_current_tile(), E->key());
						if (E->key() != edited_shape_coord) {
							queue_others.push_back(E->key());
						}
					}
				}
				spin_priority->set_suffix(" / " + String::num(total, 0));
				draw_highlight_subtile(edited_shape_coord, queue_others);
			} break;
			case EDITMODE_Z_INDEX: {
				spin_z_index->set_value(tileset->autotile_get_z_index(get_current_tile(), edited_shape_coord));
				draw_highlight_subtile(edited_shape_coord);
			} break;
			default: {
			}
		}
	}

	String current_texture_path = get_current_texture()->get_path();
	List<int> tiles;
	tileset->get_tile_list(&tiles);
	for (List<int>::Element *E = tiles.front(); E; E = E->next()) {
		int t_id = E->get();
		if (tileset->tile_get_texture(t_id)->get_path() == current_texture_path && (t_id != get_current_tile() || edit_mode != EDITMODE_REGION || workspace_mode != WORKSPACE_EDIT)) {
			Rect2i region = tileset->tile_get_region(t_id);
			region.position += WORKSPACE_MARGIN;
			Color c;
			if (tileset->tile_get_tile_mode(t_id) == TileSet::SINGLE_TILE) {
				c = COLOR_SINGLE;
			} else if (tileset->tile_get_tile_mode(t_id) == TileSet::AUTO_TILE) {
				c = COLOR_AUTOTILE;
			} else if (tileset->tile_get_tile_mode(t_id) == TileSet::ATLAS_TILE) {
				c = COLOR_ATLAS;
			}
			draw_tile_subdivision(t_id, COLOR_SUBDIVISION);
			workspace->draw_rect(region, c, false);
		}
	}

	if (edit_mode == EDITMODE_REGION) {
		if (workspace_mode != WORKSPACE_EDIT) {
			Rect2i region = edited_region;
			Color c;
			if (workspace_mode == WORKSPACE_CREATE_SINGLE) {
				c = COLOR_SINGLE;
			} else if (workspace_mode == WORKSPACE_CREATE_AUTOTILE) {
				c = COLOR_AUTOTILE;
			} else if (workspace_mode == WORKSPACE_CREATE_ATLAS) {
				c = COLOR_ATLAS;
			}
			workspace->draw_rect(region, c, false);
			draw_edited_region_subdivision();
		} else {
			int t_id = get_current_tile();
			if (t_id < 0) {
				return;
			}

			Rect2i region;
			if (draw_edited_region) {
				region = edited_region;
			} else {
				region = tileset->tile_get_region(t_id);
				region.position += WORKSPACE_MARGIN;
			}

			if (draw_edited_region) {
				draw_edited_region_subdivision();
			} else {
				draw_tile_subdivision(t_id, COLOR_SUBDIVISION);
			}

			Color c;
			if (tileset->tile_get_tile_mode(t_id) == TileSet::SINGLE_TILE) {
				c = COLOR_SINGLE;
			} else if (tileset->tile_get_tile_mode(t_id) == TileSet::AUTO_TILE) {
				c = COLOR_AUTOTILE;
			} else if (tileset->tile_get_tile_mode(t_id) == TileSet::ATLAS_TILE) {
				c = COLOR_ATLAS;
			}
			workspace->draw_rect(region, c, false);
		}
	}

	workspace_overlay->update();
}

void TileSetEditor::_on_workspace_process() {
	if (Input::get_singleton()->is_key_pressed(KEY_ALT) || tools[VISIBLE_INFO]->is_pressed()) {
		if (!tile_names_visible) {
			tile_names_visible = true;
			workspace_overlay->update();
		}
	} else if (tile_names_visible) {
		tile_names_visible = false;
		workspace_overlay->update();
	}
}

void TileSetEditor::_on_workspace_overlay_draw() {
	if (!tileset.is_valid() || !get_current_texture().is_valid()) {
		return;
	}

	const Color COLOR_AUTOTILE = Color(0.266373, 0.565288, 0.988281);
	const Color COLOR_SINGLE = Color(0.988281, 0.909323, 0.266373);
	const Color COLOR_ATLAS = Color(0.78653, 0.812835, 0.832031);

	if (tile_names_visible) {
		String current_texture_path = get_current_texture()->get_path();
		List<int> tiles;
		tileset->get_tile_list(&tiles);
		for (List<int>::Element *E = tiles.front(); E; E = E->next()) {
			int t_id = E->get();
			if (tileset->tile_get_texture(t_id)->get_path() != current_texture_path) {
				continue;
			}

			Rect2 region = tileset->tile_get_region(t_id);
			region.position += WORKSPACE_MARGIN;
			region.position *= workspace->get_scale().x;
			Color c;
			if (tileset->tile_get_tile_mode(t_id) == TileSet::SINGLE_TILE) {
				c = COLOR_SINGLE;
			} else if (tileset->tile_get_tile_mode(t_id) == TileSet::AUTO_TILE) {
				c = COLOR_AUTOTILE;
			} else if (tileset->tile_get_tile_mode(t_id) == TileSet::ATLAS_TILE) {
				c = COLOR_ATLAS;
			}
			String tile_id_name = String::num(t_id, 0) + ": " + tileset->tile_get_name(t_id);
			Ref<Font> font = get_font("font", "Label");
			region.set_size(font->get_string_size(tile_id_name));
			workspace_overlay->draw_rect(region, c);
			region.position.y += region.size.y - 2;
			c = Color(0.1, 0.1, 0.1);
			workspace_overlay->draw_string(font, region.position, tile_id_name, c);
		}
	}

	int t_id = get_current_tile();
	if (t_id < 0) {
		return;
	}

	Ref<Texture> handle = get_icon("EditorHandle", "EditorIcons");
	if (draw_handles) {
		for (int i = 0; i < current_shape.size(); i++) {
			workspace_overlay->draw_texture(handle, current_shape[i] * workspace->get_scale().x - handle->get_size() * 0.5);
		}
	}
}

int TileSetEditor::get_grabbed_point(const Vector2 &p_mouse_pos, real_t p_grab_threshold) {
	Transform2D xform = workspace->get_transform();

	int grabbed_point = -1;
	real_t min_distance = 1e10;

	for (int i = 0; i < current_shape.size(); i++) {
		const real_t distance = xform.xform(current_shape[i]).distance_to(xform.xform(p_mouse_pos));
		if (distance < p_grab_threshold && distance < min_distance) {
			min_distance = distance;
			grabbed_point = i;
		}
	}

	return grabbed_point;
}

bool TileSetEditor::is_within_grabbing_distance_of_first_point(const Vector2 &p_pos, real_t p_grab_threshold) {
	Transform2D xform = workspace->get_transform();

	const real_t distance = xform.xform(current_shape[0]).distance_to(xform.xform(p_pos));

	return distance < p_grab_threshold;
}

void TileSetEditor::_on_scroll_container_input(const Ref<InputEvent> &p_event) {
	const Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid()) {
		// Zoom in/out using Ctrl + mouse wheel. This is done on the ScrollContainer
		// to allow performing this action anywhere, even if the cursor isn't
		// hovering the texture in the workspace.
		if (mb->get_button_index() == BUTTON_WHEEL_UP && mb->is_pressed() && mb->get_control()) {
			_zoom_on_position(scale_ratio, mb->get_position());
			// Don't scroll up after zooming in.
			accept_event();
		} else if (mb->get_button_index() == BUTTON_WHEEL_DOWN && mb->is_pressed() && mb->get_control()) {
			_zoom_on_position(1 / scale_ratio, mb->get_position());
			// Don't scroll down after zooming out.
			accept_event();
		}
	}
}

void TileSetEditor::_on_workspace_input(const Ref<InputEvent> &p_ie) {
	if (tileset.is_null() || !get_current_texture().is_valid()) {
		return;
	}

	static bool dragging;
	static bool erasing;
	static bool alternative;
	draw_edited_region = false;

	Rect2 current_tile_region = Rect2();
	if (get_current_tile() >= 0) {
		current_tile_region = tileset->tile_get_region(get_current_tile());
	}
	current_tile_region.position += WORKSPACE_MARGIN;

	const Ref<InputEventMouseButton> mb = p_ie;
	const Ref<InputEventMouseMotion> mm = p_ie;

	if (mb.is_valid()) {
		if (mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT && !creating_shape) {
			if (!current_tile_region.has_point(mb->get_position())) {
				String current_texture_path = get_current_texture()->get_path();
				List<int> tiles;
				tileset->get_tile_list(&tiles);
				for (List<int>::Element *E = tiles.front(); E; E = E->next()) {
					int t_id = E->get();
					if (tileset->tile_get_texture(t_id)->get_path() == current_texture_path) {
						Rect2 r = tileset->tile_get_region(t_id);
						r.position += WORKSPACE_MARGIN;
						if (r.has_point(mb->get_position())) {
							set_current_tile(t_id);
							workspace->update();
							workspace_overlay->update();
							return;
						}
					}
				}
			}
		}
	}
	// Drag Middle Mouse
	if (mm.is_valid()) {
		if (mm->get_button_mask() & BUTTON_MASK_MIDDLE) {
			Vector2 dragged(mm->get_relative().x, mm->get_relative().y);
			scroll->set_h_scroll(scroll->get_h_scroll() - dragged.x * workspace->get_scale().x);
			scroll->set_v_scroll(scroll->get_v_scroll() - dragged.y * workspace->get_scale().x);
		}
	}

	if (edit_mode == EDITMODE_REGION) {
		if (mb.is_valid()) {
			if (mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {
				if (get_current_tile() >= 0 || workspace_mode != WORKSPACE_EDIT) {
					dragging = true;
					region_from = mb->get_position();
					edited_region = Rect2(region_from, Size2());
					workspace->update();
					workspace_overlay->update();
					return;
				}
			} else if (dragging && mb->is_pressed() && mb->get_button_index() == BUTTON_RIGHT) {
				dragging = false;
				edited_region = Rect2();
				workspace->update();
				workspace_overlay->update();
				return;
			} else if (dragging && !mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {
				dragging = false;
				update_edited_region(mb->get_position());
				edited_region.position -= WORKSPACE_MARGIN;
				if (!edited_region.has_no_area()) {
					if (get_current_tile() >= 0 && workspace_mode == WORKSPACE_EDIT) {
						undo_redo->create_action(TTR("Set Tile Region"));
						undo_redo->add_do_method(tileset.ptr(), "tile_set_region", get_current_tile(), edited_region);
						undo_redo->add_undo_method(tileset.ptr(), "tile_set_region", get_current_tile(), tileset->tile_get_region(get_current_tile()));

						Size2 tile_workspace_size = edited_region.position + edited_region.size + WORKSPACE_MARGIN * 2;
						Size2 workspace_minsize = workspace->get_custom_minimum_size();
						// If the new region is bigger, just directly change the workspace size to avoid checking all other tiles.
						if (tile_workspace_size.x > workspace_minsize.x || tile_workspace_size.y > workspace_minsize.y) {
							Size2 max_workspace_size = Size2(MAX(tile_workspace_size.x, workspace_minsize.x), MAX(tile_workspace_size.y, workspace_minsize.y));
							undo_redo->add_do_method(workspace, "set_custom_minimum_size", max_workspace_size);
							undo_redo->add_undo_method(workspace, "set_custom_minimum_size", workspace_minsize);
							undo_redo->add_do_method(workspace_container, "set_custom_minimum_size", max_workspace_size);
							undo_redo->add_undo_method(workspace_container, "set_custom_minimum_size", workspace_minsize);
							undo_redo->add_do_method(workspace_overlay, "set_custom_minimum_size", max_workspace_size);
							undo_redo->add_undo_method(workspace_overlay, "set_custom_minimum_size", workspace_minsize);
						} else if (workspace_minsize.x > get_current_texture()->get_size().x + WORKSPACE_MARGIN.x * 2 || workspace_minsize.y > get_current_texture()->get_size().y + WORKSPACE_MARGIN.y * 2) {
							undo_redo->add_do_method(this, "update_workspace_minsize");
							undo_redo->add_undo_method(this, "update_workspace_minsize");
						}

						edited_region = Rect2();

						undo_redo->add_do_method(workspace, "update");
						undo_redo->add_undo_method(workspace, "update");
						undo_redo->add_do_method(workspace_overlay, "update");
						undo_redo->add_undo_method(workspace_overlay, "update");
						undo_redo->commit_action();
					} else {
						int t_id = tileset->get_last_unused_tile_id();
						undo_redo->create_action(TTR("Create Tile"));
						undo_redo->add_do_method(tileset.ptr(), "create_tile", t_id);
						undo_redo->add_undo_method(tileset.ptr(), "remove_tile", t_id);
						undo_redo->add_undo_method(this, "_validate_current_tile_id");
						undo_redo->add_do_method(tileset.ptr(), "tile_set_texture", t_id, get_current_texture());
						undo_redo->add_do_method(tileset.ptr(), "tile_set_region", t_id, edited_region);
						undo_redo->add_do_method(tileset.ptr(), "tile_set_name", t_id, get_current_texture()->get_path().get_file() + " " + String::num(t_id, 0));
						if (workspace_mode != WORKSPACE_CREATE_SINGLE) {
							undo_redo->add_do_method(tileset.ptr(), "autotile_set_size", t_id, snap_step);
							undo_redo->add_do_method(tileset.ptr(), "autotile_set_spacing", t_id, snap_separation.x);
							undo_redo->add_do_method(tileset.ptr(), "tile_set_tile_mode", t_id, workspace_mode == WORKSPACE_CREATE_AUTOTILE ? TileSet::AUTO_TILE : TileSet::ATLAS_TILE);
						}

						tool_workspacemode[WORKSPACE_EDIT]->set_pressed(true);
						tool_editmode[EDITMODE_COLLISION]->set_pressed(true);
						edit_mode = EDITMODE_COLLISION;

						Size2 tile_workspace_size = edited_region.position + edited_region.size + WORKSPACE_MARGIN * 2;
						Size2 workspace_minsize = workspace->get_custom_minimum_size();
						if (tile_workspace_size.x > workspace_minsize.x || tile_workspace_size.y > workspace_minsize.y) {
							Size2 new_workspace_minsize = Size2(MAX(tile_workspace_size.x, workspace_minsize.x), MAX(tile_workspace_size.y, workspace_minsize.y));
							undo_redo->add_do_method(workspace, "set_custom_minimum_size", new_workspace_minsize);
							undo_redo->add_undo_method(workspace, "set_custom_minimum_size", workspace_minsize);
							undo_redo->add_do_method(workspace_container, "set_custom_minimum_size", new_workspace_minsize);
							undo_redo->add_undo_method(workspace_container, "set_custom_minimum_size", workspace_minsize);
							undo_redo->add_do_method(workspace_overlay, "set_custom_minimum_size", new_workspace_minsize);
							undo_redo->add_undo_method(workspace_overlay, "set_custom_minimum_size", workspace_minsize);
						}

						edited_region = Rect2();

						undo_redo->add_do_method(workspace, "update");
						undo_redo->add_undo_method(workspace, "update");
						undo_redo->add_do_method(workspace_overlay, "update");
						undo_redo->add_undo_method(workspace_overlay, "update");
						undo_redo->commit_action();

						set_current_tile(t_id);
						_on_workspace_mode_changed(WORKSPACE_EDIT);
					}
				} else {
					edited_region = Rect2();
					workspace->update();
					workspace_overlay->update();
				}
				return;
			}
		} else if (mm.is_valid()) {
			if (dragging) {
				update_edited_region(mm->get_position());
				draw_edited_region = true;
				workspace->update();
				workspace_overlay->update();
				return;
			}
		}
	}

	if (workspace_mode == WORKSPACE_EDIT) {
		if (get_current_tile() >= 0) {
			int spacing = tileset->autotile_get_spacing(get_current_tile());
			Vector2 size = tileset->autotile_get_size(get_current_tile());
			switch (edit_mode) {
				case EDITMODE_ICON: {
					if (mb.is_valid()) {
						if (mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT && current_tile_region.has_point(mb->get_position())) {
							Vector2 coord((int)((mb->get_position().x - current_tile_region.position.x) / (spacing + size.x)), (int)((mb->get_position().y - current_tile_region.position.y) / (spacing + size.y)));
							undo_redo->create_action(TTR("Set Tile Icon"));
							undo_redo->add_do_method(tileset.ptr(), "autotile_set_icon_coordinate", get_current_tile(), coord);
							undo_redo->add_undo_method(tileset.ptr(), "autotile_set_icon_coordinate", get_current_tile(), tileset->autotile_get_icon_coordinate(get_current_tile()));
							undo_redo->add_do_method(workspace, "update");
							undo_redo->add_undo_method(workspace, "update");
							undo_redo->commit_action();
						}
					}
				} break;
				case EDITMODE_BITMASK: {
					if (mb.is_valid()) {
						if (mb->is_pressed()) {
							if (dragging) {
								return;
							}
							if ((mb->get_button_index() == BUTTON_RIGHT || mb->get_button_index() == BUTTON_LEFT) && current_tile_region.has_point(mb->get_position())) {
								dragging = true;
								erasing = (mb->get_button_index() == BUTTON_RIGHT);
								alternative = Input::get_singleton()->is_key_pressed(KEY_SHIFT);
								Vector2 coord((int)((mb->get_position().x - current_tile_region.position.x) / (spacing + size.x)), (int)((mb->get_position().y - current_tile_region.position.y) / (spacing + size.y)));
								Vector2 pos(coord.x * (spacing + size.x), coord.y * (spacing + size.y));
								pos = mb->get_position() - (pos + current_tile_region.position);
								uint32_t bit = 0;
								if (tileset->autotile_get_bitmask_mode(get_current_tile()) == TileSet::BITMASK_2X2) {
									if (pos.x < size.x / 2) {
										if (pos.y < size.y / 2) {
											bit = TileSet::BIND_TOPLEFT;
										} else {
											bit = TileSet::BIND_BOTTOMLEFT;
										}
									} else {
										if (pos.y < size.y / 2) {
											bit = TileSet::BIND_TOPRIGHT;
										} else {
											bit = TileSet::BIND_BOTTOMRIGHT;
										}
									}
								} else {
									if (pos.x < size.x / 3) {
										if (pos.y < size.y / 3) {
											bit = TileSet::BIND_TOPLEFT;
										} else if (pos.y > (size.y / 3) * 2) {
											bit = TileSet::BIND_BOTTOMLEFT;
										} else {
											bit = TileSet::BIND_LEFT;
										}
									} else if (pos.x > (size.x / 3) * 2) {
										if (pos.y < size.y / 3) {
											bit = TileSet::BIND_TOPRIGHT;
										} else if (pos.y > (size.y / 3) * 2) {
											bit = TileSet::BIND_BOTTOMRIGHT;
										} else {
											bit = TileSet::BIND_RIGHT;
										}
									} else {
										if (pos.y < size.y / 3) {
											bit = TileSet::BIND_TOP;
										} else if (pos.y > (size.y / 3) * 2) {
											bit = TileSet::BIND_BOTTOM;
										} else {
											bit = TileSet::BIND_CENTER;
										}
									}
								}

								uint32_t old_mask = tileset->autotile_get_bitmask(get_current_tile(), coord);
								uint32_t new_mask = old_mask;
								if (alternative) {
									new_mask &= ~bit;
									new_mask |= (bit << 16);
								} else if (erasing) {
									new_mask &= ~bit;
									new_mask &= ~(bit << 16);
								} else {
									new_mask |= bit;
									new_mask &= ~(bit << 16);
								}

								if (old_mask != new_mask) {
									undo_redo->create_action(TTR("Edit Tile Bitmask"));
									undo_redo->add_do_method(tileset.ptr(), "autotile_set_bitmask", get_current_tile(), coord, new_mask);
									undo_redo->add_undo_method(tileset.ptr(), "autotile_set_bitmask", get_current_tile(), coord, old_mask);
									undo_redo->add_do_method(workspace, "update");
									undo_redo->add_undo_method(workspace, "update");
									undo_redo->commit_action();
								}
							}
						} else {
							if ((erasing && mb->get_button_index() == BUTTON_RIGHT) || (!erasing && mb->get_button_index() == BUTTON_LEFT)) {
								dragging = false;
								erasing = false;
								alternative = false;
							}
						}
					}
					if (mm.is_valid()) {
						if (dragging && current_tile_region.has_point(mm->get_position())) {
							Vector2 coord((int)((mm->get_position().x - current_tile_region.position.x) / (spacing + size.x)), (int)((mm->get_position().y - current_tile_region.position.y) / (spacing + size.y)));
							Vector2 pos(coord.x * (spacing + size.x), coord.y * (spacing + size.y));
							pos = mm->get_position() - (pos + current_tile_region.position);
							uint32_t bit = 0;
							if (tileset->autotile_get_bitmask_mode(get_current_tile()) == TileSet::BITMASK_2X2) {
								if (pos.x < size.x / 2) {
									if (pos.y < size.y / 2) {
										bit = TileSet::BIND_TOPLEFT;
									} else {
										bit = TileSet::BIND_BOTTOMLEFT;
									}
								} else {
									if (pos.y < size.y / 2) {
										bit = TileSet::BIND_TOPRIGHT;
									} else {
										bit = TileSet::BIND_BOTTOMRIGHT;
									}
								}
							} else {
								if (pos.x < size.x / 3) {
									if (pos.y < size.y / 3) {
										bit = TileSet::BIND_TOPLEFT;
									} else if (pos.y > (size.y / 3) * 2) {
										bit = TileSet::BIND_BOTTOMLEFT;
									} else {
										bit = TileSet::BIND_LEFT;
									}
								} else if (pos.x > (size.x / 3) * 2) {
									if (pos.y < size.y / 3) {
										bit = TileSet::BIND_TOPRIGHT;
									} else if (pos.y > (size.y / 3) * 2) {
										bit = TileSet::BIND_BOTTOMRIGHT;
									} else {
										bit = TileSet::BIND_RIGHT;
									}
								} else {
									if (pos.y < size.y / 3) {
										bit = TileSet::BIND_TOP;
									} else if (pos.y > (size.y / 3) * 2) {
										bit = TileSet::BIND_BOTTOM;
									} else {
										bit = TileSet::BIND_CENTER;
									}
								}
							}

							uint32_t old_mask = tileset->autotile_get_bitmask(get_current_tile(), coord);
							uint32_t new_mask = old_mask;
							if (alternative) {
								new_mask &= ~bit;
								new_mask |= (bit << 16);
							} else if (erasing) {
								new_mask &= ~bit;
								new_mask &= ~(bit << 16);
							} else {
								new_mask |= bit;
								new_mask &= ~(bit << 16);
							}
							if (old_mask != new_mask) {
								undo_redo->create_action(TTR("Edit Tile Bitmask"));
								undo_redo->add_do_method(tileset.ptr(), "autotile_set_bitmask", get_current_tile(), coord, new_mask);
								undo_redo->add_undo_method(tileset.ptr(), "autotile_set_bitmask", get_current_tile(), coord, old_mask);
								undo_redo->add_do_method(workspace, "update");
								undo_redo->add_undo_method(workspace, "update");
								undo_redo->commit_action();
							}
						}
					}
				} break;
				case EDITMODE_COLLISION:
				case EDITMODE_OCCLUSION:
				case EDITMODE_NAVIGATION:
				case EDITMODE_PRIORITY:
				case EDITMODE_Z_INDEX: {
					Vector2 shape_anchor = Vector2(0, 0);
					if (tileset->tile_get_tile_mode(get_current_tile()) == TileSet::AUTO_TILE || tileset->tile_get_tile_mode(get_current_tile()) == TileSet::ATLAS_TILE) {
						shape_anchor = edited_shape_coord;
						shape_anchor.x *= (size.x + spacing);
						shape_anchor.y *= (size.y + spacing);
					}

					const real_t grab_threshold = EDITOR_GET("editors/poly_editor/point_grab_radius");
					shape_anchor += current_tile_region.position;
					if (tools[TOOL_SELECT]->is_pressed()) {
						if (current_shape.size() > 0) {
							for (int i = 0; i < current_shape.size(); i++) {
								current_shape.set(i, snap_point(current_shape[i]));
							}
							workspace->update();
						}

						if (mb.is_valid()) {
							if (mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {
								if (edit_mode != EDITMODE_PRIORITY && current_shape.size() > 0) {
									int grabbed_point = get_grabbed_point(mb->get_position(), grab_threshold);

									if (grabbed_point >= 0) {
										dragging_point = grabbed_point;
										workspace->update();
										return;
									}
								}
								if ((tileset->tile_get_tile_mode(get_current_tile()) == TileSet::AUTO_TILE || tileset->tile_get_tile_mode(get_current_tile()) == TileSet::ATLAS_TILE) && current_tile_region.has_point(mb->get_position())) {
									Vector2 coord((int)((mb->get_position().x - current_tile_region.position.x) / (spacing + size.x)), (int)((mb->get_position().y - current_tile_region.position.y) / (spacing + size.y)));
									if (edited_shape_coord != coord) {
										edited_shape_coord = coord;
										_select_edited_shape_coord();
									}
								}
								workspace->update();
							} else if (!mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {
								if (edit_mode == EDITMODE_COLLISION) {
									if (dragging_point >= 0) {
										dragging_point = -1;

										Vector<Vector2> points;

										for (int i = 0; i < current_shape.size(); i++) {
											Vector2 p = current_shape[i];
											if (tools[TOOL_GRID_SNAP]->is_pressed() || tools[SHAPE_KEEP_INSIDE_TILE]->is_pressed()) {
												p = snap_point(p);
											}
											points.push_back(p - shape_anchor);
										}

										undo_redo->create_action(TTR("Edit Collision Polygon"));
										_set_edited_shape_points(points);
										undo_redo->add_do_method(this, "_select_edited_shape_coord");
										undo_redo->add_undo_method(this, "_select_edited_shape_coord");
										undo_redo->commit_action();
									}
								} else if (edit_mode == EDITMODE_OCCLUSION) {
									if (dragging_point >= 0) {
										dragging_point = -1;

										PoolVector<Vector2> polygon;
										polygon.resize(current_shape.size());
										PoolVector<Vector2>::Write w = polygon.write();

										for (int i = 0; i < current_shape.size(); i++) {
											w[i] = current_shape[i] - shape_anchor;
										}

										w.release();

										undo_redo->create_action(TTR("Edit Occlusion Polygon"));
										undo_redo->add_do_method(edited_occlusion_shape.ptr(), "set_polygon", polygon);
										undo_redo->add_undo_method(edited_occlusion_shape.ptr(), "set_polygon", edited_occlusion_shape->get_polygon());
										undo_redo->add_do_method(this, "_select_edited_shape_coord");
										undo_redo->add_undo_method(this, "_select_edited_shape_coord");
										undo_redo->commit_action();
									}
								} else if (edit_mode == EDITMODE_NAVIGATION) {
									if (dragging_point >= 0) {
										dragging_point = -1;

										PoolVector<Vector2> polygon;
										Vector<int> indices;
										polygon.resize(current_shape.size());
										PoolVector<Vector2>::Write w = polygon.write();

										for (int i = 0; i < current_shape.size(); i++) {
											w[i] = current_shape[i] - shape_anchor;
											indices.push_back(i);
										}

										w.release();

										edited_navigation_shape->clear_outlines();
										edited_navigation_shape->add_outline(polygon);
										edited_navigation_shape->make_polygons_from_outlines();
										// FIXME Couldn't figure out the undo_redo quagmire
										//undo_redo->create_action(TTR("Edit Navigation Polygon"));
										//undo_redo->add_do_method(edited_navigation_shape.ptr(), "set_vertices", polygon);
										//undo_redo->add_undo_method(edited_navigation_shape.ptr(), "set_vertices", edited_navigation_shape->get_vertices());
										//undo_redo->add_do_method(edited_navigation_shape.ptr(), "clear_polygons");
										//undo_redo->add_undo_method(edited_navigation_shape.ptr(), "clear_polygons");
										//undo_redo->add_do_method(edited_navigation_shape.ptr(), "add_polygon", indices);
										//undo_redo->add_undo_method(edited_navigation_shape.ptr(), "add_polygon", edited_navigation_shape->get_polygon(0));
										//undo_redo->add_do_method(this, "_select_edited_shape_coord");
										//undo_redo->add_undo_method(this, "_select_edited_shape_coord");
										//undo_redo->commit_action();
									}
								}
							}
						} else if (mm.is_valid()) {
							if (dragging_point >= 0) {
								current_shape.set(dragging_point, snap_point(mm->get_position()));
								workspace->update();
							}
						}
					} else if (tools[SHAPE_NEW_POLYGON]->is_pressed()) {
						if (mb.is_valid()) {
							if (mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {
								Vector2 pos = mb->get_position();
								pos = snap_point(pos);
								if (creating_shape) {
									if (current_shape.size() > 2) {
										if (is_within_grabbing_distance_of_first_point(mb->get_position(), grab_threshold)) {
											close_shape(shape_anchor);
											workspace->update();
											return;
										}
									}
									current_shape.push_back(pos);
									workspace->update();
								} else {
									creating_shape = true;
									_set_edited_collision_shape(Ref<ConvexPolygonShape2D>());
									current_shape.resize(0);
									current_shape.push_back(snap_point(pos));
									workspace->update();
								}
							} else if (mb->is_pressed() && mb->get_button_index() == BUTTON_RIGHT) {
								if (creating_shape) {
									creating_shape = false;
									_select_edited_shape_coord();
									workspace->update();
								}
							}
						} else if (mm.is_valid()) {
							if (creating_shape) {
								workspace->update();
							}
						}
					} else if (tools[SHAPE_NEW_RECTANGLE]->is_pressed()) {
						if (mb.is_valid()) {
							if (mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {
								_set_edited_collision_shape(Ref<ConvexPolygonShape2D>());
								current_shape.resize(0);
								Vector2 pos = mb->get_position();
								pos = snap_point(pos);
								current_shape.push_back(pos);
								current_shape.push_back(pos);
								current_shape.push_back(pos);
								current_shape.push_back(pos);
								creating_shape = true;
								workspace->update();
								return;
							} else if (mb->is_pressed() && mb->get_button_index() == BUTTON_RIGHT) {
								if (creating_shape) {
									creating_shape = false;
									_select_edited_shape_coord();
									workspace->update();
								}
							} else if (!mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {
								if (creating_shape) {
									// if the first two corners are within grabbing distance of one another, expand the rect to fill the tile
									if (is_within_grabbing_distance_of_first_point(current_shape[1], grab_threshold)) {
										current_shape.set(0, snap_point(shape_anchor));
										current_shape.set(1, snap_point(shape_anchor + Vector2(current_tile_region.size.x, 0)));
										current_shape.set(2, snap_point(shape_anchor + current_tile_region.size));
										current_shape.set(3, snap_point(shape_anchor + Vector2(0, current_tile_region.size.y)));
									}

									close_shape(shape_anchor);
									workspace->update();
									return;
								}
							}
						} else if (mm.is_valid()) {
							if (creating_shape) {
								Vector2 pos = mm->get_position();
								pos = snap_point(pos);
								Vector2 p = current_shape[2];
								current_shape.set(3, snap_point(Vector2(pos.x, p.y)));
								current_shape.set(0, snap_point(pos));
								current_shape.set(1, snap_point(Vector2(p.x, pos.y)));
								workspace->update();
							}
						}
					}
				} break;
				default: {
				}
			}
		}
	}
}

void TileSetEditor::_on_tool_clicked(int p_tool) {
	if (p_tool == BITMASK_COPY) {
		bitmask_map_copy = tileset->autotile_get_bitmask_map(get_current_tile());
	} else if (p_tool == BITMASK_PASTE) {
		undo_redo->create_action(TTR("Paste Tile Bitmask"));
		undo_redo->add_do_method(tileset.ptr(), "autotile_clear_bitmask_map", get_current_tile());
		undo_redo->add_undo_method(tileset.ptr(), "autotile_clear_bitmask_map", get_current_tile());
		for (Map<Vector2, uint32_t>::Element *E = bitmask_map_copy.front(); E; E = E->next()) {
			undo_redo->add_do_method(tileset.ptr(), "autotile_set_bitmask", get_current_tile(), E->key(), E->value());
		}
		for (Map<Vector2, uint32_t>::Element *E = tileset->autotile_get_bitmask_map(get_current_tile()).front(); E; E = E->next()) {
			undo_redo->add_undo_method(tileset.ptr(), "autotile_set_bitmask", get_current_tile(), E->key(), E->value());
		}
		undo_redo->add_do_method(workspace, "update");
		undo_redo->add_undo_method(workspace, "update");
		undo_redo->commit_action();
	} else if (p_tool == BITMASK_CLEAR) {
		undo_redo->create_action(TTR("Clear Tile Bitmask"));
		undo_redo->add_do_method(tileset.ptr(), "autotile_clear_bitmask_map", get_current_tile());
		for (Map<Vector2, uint32_t>::Element *E = tileset->autotile_get_bitmask_map(get_current_tile()).front(); E; E = E->next()) {
			undo_redo->add_undo_method(tileset.ptr(), "autotile_set_bitmask", get_current_tile(), E->key(), E->value());
		}
		undo_redo->add_do_method(workspace, "update");
		undo_redo->add_undo_method(workspace, "update");
		undo_redo->commit_action();
	} else if (p_tool == SHAPE_TOGGLE_TYPE) {
		if (edited_collision_shape.is_valid()) {
			Ref<ConvexPolygonShape2D> convex = edited_collision_shape;
			Ref<ConcavePolygonShape2D> concave = edited_collision_shape;
			Ref<Shape2D> previous_shape = edited_collision_shape;
			Array sd = tileset->call("tile_get_shapes", get_current_tile());

			if (convex.is_valid()) {
				// Make concave.
				undo_redo->create_action(TTR("Make Polygon Concave"));
				Ref<ConcavePolygonShape2D> _concave = memnew(ConcavePolygonShape2D);
				edited_collision_shape = _concave;
				_set_edited_shape_points(_get_collision_shape_points(convex));
			} else if (concave.is_valid()) {
				// Make convex.
				undo_redo->create_action(TTR("Make Polygon Convex"));
				Ref<ConvexPolygonShape2D> _convex = memnew(ConvexPolygonShape2D);
				edited_collision_shape = _convex;
				_set_edited_shape_points(_get_collision_shape_points(concave));
			}
			for (int i = 0; i < sd.size(); i++) {
				if (sd[i].get("shape") == previous_shape) {
					undo_redo->add_undo_method(tileset.ptr(), "tile_set_shapes", get_current_tile(), sd.duplicate());
					sd.remove(i);
					break;
				}
			}

			undo_redo->add_do_method(tileset.ptr(), "tile_set_shapes", get_current_tile(), sd);
			if (tileset->tile_get_tile_mode(get_current_tile()) == TileSet::AUTO_TILE || tileset->tile_get_tile_mode(get_current_tile()) == TileSet::ATLAS_TILE) {
				undo_redo->add_do_method(tileset.ptr(), "tile_add_shape", get_current_tile(), edited_collision_shape, Transform2D(), false, edited_shape_coord);
			} else {
				undo_redo->add_do_method(tileset.ptr(), "tile_add_shape", get_current_tile(), edited_collision_shape, Transform2D());
			}
			undo_redo->add_do_method(this, "_select_edited_shape_coord");
			undo_redo->add_undo_method(this, "_select_edited_shape_coord");
			undo_redo->commit_action();

			_update_toggle_shape_button();
			workspace->update();
			workspace_container->update();
			helper->_change_notify("");
		}
	} else if (p_tool == SELECT_NEXT) {
		_select_next_shape();
	} else if (p_tool == SELECT_PREVIOUS) {
		_select_previous_shape();
	} else if (p_tool == SHAPE_DELETE) {
		if (creating_shape) {
			creating_shape = false;
			current_shape.resize(0);
			workspace->update();
		} else {
			switch (edit_mode) {
				case EDITMODE_REGION: {
					int t_id = get_current_tile();
					if (workspace_mode == WORKSPACE_EDIT && t_id >= 0) {
						undo_redo->create_action(TTR("Remove Tile"));
						undo_redo->add_do_method(tileset.ptr(), "remove_tile", t_id);
						_undo_tile_removal(t_id);
						undo_redo->add_do_method(this, "_validate_current_tile_id");

						Rect2 tile_region = tileset->tile_get_region(get_current_tile());
						Size2 tile_workspace_size = tile_region.position + tile_region.size;
						if (tile_workspace_size.x > get_current_texture()->get_size().x || tile_workspace_size.y > get_current_texture()->get_size().y) {
							undo_redo->add_do_method(this, "update_workspace_minsize");
							undo_redo->add_undo_method(this, "update_workspace_minsize");
						}

						undo_redo->add_do_method(workspace, "update");
						undo_redo->add_undo_method(workspace, "update");
						undo_redo->add_do_method(workspace_overlay, "update");
						undo_redo->add_undo_method(workspace_overlay, "update");
						undo_redo->commit_action();
					}
					tool_workspacemode[WORKSPACE_EDIT]->set_pressed(true);
					workspace_mode = WORKSPACE_EDIT;
					update_workspace_tile_mode();
				} break;
				case EDITMODE_COLLISION: {
					if (!edited_collision_shape.is_null()) {
						// Necessary to get the version that returns a Array instead of a Vector.
						Array sd = tileset->call("tile_get_shapes", get_current_tile());
						for (int i = 0; i < sd.size(); i++) {
							if (sd[i].get("shape") == edited_collision_shape) {
								undo_redo->create_action(TTR("Remove Collision Polygon"));
								undo_redo->add_undo_method(tileset.ptr(), "tile_set_shapes", get_current_tile(), sd.duplicate());
								sd.remove(i);
								undo_redo->add_do_method(tileset.ptr(), "tile_set_shapes", get_current_tile(), sd);
								undo_redo->add_do_method(this, "_select_edited_shape_coord");
								undo_redo->add_undo_method(this, "_select_edited_shape_coord");
								undo_redo->commit_action();
								break;
							}
						}
					}
				} break;
				case EDITMODE_OCCLUSION: {
					if (!edited_occlusion_shape.is_null()) {
						undo_redo->create_action(TTR("Remove Occlusion Polygon"));
						if (tileset->tile_get_tile_mode(get_current_tile()) == TileSet::SINGLE_TILE) {
							undo_redo->add_do_method(tileset.ptr(), "tile_set_light_occluder", get_current_tile(), Ref<OccluderPolygon2D>());
							undo_redo->add_undo_method(tileset.ptr(), "tile_set_light_occluder", get_current_tile(), tileset->tile_get_light_occluder(get_current_tile()));
						} else {
							undo_redo->add_do_method(tileset.ptr(), "autotile_set_light_occluder", get_current_tile(), Ref<OccluderPolygon2D>(), edited_shape_coord);
							undo_redo->add_undo_method(tileset.ptr(), "autotile_set_light_occluder", get_current_tile(), tileset->autotile_get_light_occluder(get_current_tile(), edited_shape_coord), edited_shape_coord);
						}
						undo_redo->add_do_method(this, "_select_edited_shape_coord");
						undo_redo->add_undo_method(this, "_select_edited_shape_coord");
						undo_redo->commit_action();
					}
				} break;
				case EDITMODE_NAVIGATION: {
					if (!edited_navigation_shape.is_null()) {
						undo_redo->create_action(TTR("Remove Navigation Polygon"));
						if (tileset->tile_get_tile_mode(get_current_tile()) == TileSet::SINGLE_TILE) {
							undo_redo->add_do_method(tileset.ptr(), "tile_set_navigation_polygon", get_current_tile(), Ref<NavigationPolygon>());
							undo_redo->add_undo_method(tileset.ptr(), "tile_set_navigation_polygon", get_current_tile(), tileset->tile_get_navigation_polygon(get_current_tile()));
						} else {
							undo_redo->add_do_method(tileset.ptr(), "autotile_set_navigation_polygon", get_current_tile(), Ref<NavigationPolygon>(), edited_shape_coord);
							undo_redo->add_undo_method(tileset.ptr(), "autotile_set_navigation_polygon", get_current_tile(), tileset->autotile_get_navigation_polygon(get_current_tile(), edited_shape_coord), edited_shape_coord);
						}
						undo_redo->add_do_method(this, "_select_edited_shape_coord");
						undo_redo->add_undo_method(this, "_select_edited_shape_coord");
						undo_redo->commit_action();
					}
				} break;
				default: {
				}
			}
		}
	} else if (p_tool == TOOL_SELECT || p_tool == SHAPE_NEW_POLYGON || p_tool == SHAPE_NEW_RECTANGLE) {
		if (creating_shape) {
			// Cancel Creation
			creating_shape = false;
			current_shape.resize(0);
			workspace->update();
		}
	}
}

void TileSetEditor::_on_priority_changed(float val) {
	if ((int)val == tileset->autotile_get_subtile_priority(get_current_tile(), edited_shape_coord)) {
		return;
	}

	undo_redo->create_action(TTR("Edit Tile Priority"));
	undo_redo->add_do_method(tileset.ptr(), "autotile_set_subtile_priority", get_current_tile(), edited_shape_coord, (int)val);
	undo_redo->add_undo_method(tileset.ptr(), "autotile_set_subtile_priority", get_current_tile(), edited_shape_coord, tileset->autotile_get_subtile_priority(get_current_tile(), edited_shape_coord));
	undo_redo->add_do_method(workspace, "update");
	undo_redo->add_undo_method(workspace, "update");
	undo_redo->commit_action();
}

void TileSetEditor::_on_z_index_changed(float val) {
	if ((int)val == tileset->autotile_get_z_index(get_current_tile(), edited_shape_coord)) {
		return;
	}

	undo_redo->create_action(TTR("Edit Tile Z Index"));
	undo_redo->add_do_method(tileset.ptr(), "autotile_set_z_index", get_current_tile(), edited_shape_coord, (int)val);
	undo_redo->add_undo_method(tileset.ptr(), "autotile_set_z_index", get_current_tile(), edited_shape_coord, tileset->autotile_get_z_index(get_current_tile(), edited_shape_coord));
	undo_redo->add_do_method(workspace, "update");
	undo_redo->add_undo_method(workspace, "update");
	undo_redo->commit_action();
}

void TileSetEditor::_on_grid_snap_toggled(bool p_val) {
	helper->set_snap_options_visible(p_val);
	workspace->update();
}

Vector<Vector2> TileSetEditor::_get_collision_shape_points(const Ref<Shape2D> &p_shape) {
	Ref<ConvexPolygonShape2D> convex = p_shape;
	Ref<ConcavePolygonShape2D> concave = p_shape;
	if (convex.is_valid()) {
		return convex->get_points();
	} else if (concave.is_valid()) {
		Vector<Vector2> points;
		for (int i = 0; i < concave->get_segments().size(); i += 2) {
			points.push_back(concave->get_segments()[i]);
		}
		return points;
	} else {
		return Vector<Vector2>();
	}
}

Vector<Vector2> TileSetEditor::_get_edited_shape_points() {
	return _get_collision_shape_points(edited_collision_shape);
}

void TileSetEditor::_set_edited_shape_points(const Vector<Vector2> &points) {
	Ref<ConvexPolygonShape2D> convex = edited_collision_shape;
	Ref<ConcavePolygonShape2D> concave = edited_collision_shape;
	if (convex.is_valid()) {
		undo_redo->add_do_method(convex.ptr(), "set_points", points);
		undo_redo->add_undo_method(convex.ptr(), "set_points", _get_edited_shape_points());
	} else if (concave.is_valid() && points.size() > 1) {
		PoolVector2Array segments;
		for (int i = 0; i < points.size() - 1; i++) {
			segments.push_back(points[i]);
			segments.push_back(points[i + 1]);
		}
		segments.push_back(points[points.size() - 1]);
		segments.push_back(points[0]);
		undo_redo->add_do_method(concave.ptr(), "set_segments", segments);
		undo_redo->add_undo_method(concave.ptr(), "set_segments", concave->get_segments());
	}
}

void TileSetEditor::_update_tile_data() {
	current_tile_data.clear();
	if (get_current_tile() < 0) {
		return;
	}

	Vector<TileSet::ShapeData> sd = tileset->tile_get_shapes(get_current_tile());
	if (tileset->tile_get_tile_mode(get_current_tile()) == TileSet::SINGLE_TILE) {
		SubtileData data;
		for (int i = 0; i < sd.size(); i++) {
			data.collisions.push_back(sd[i].shape);
		}
		data.navigation_shape = tileset->tile_get_navigation_polygon(get_current_tile());
		data.occlusion_shape = tileset->tile_get_light_occluder(get_current_tile());
		current_tile_data[Vector2i()] = data;
	} else {
		Vector2 cell_count = _get_subtiles_count(get_current_tile());
		for (int y = 0; y < cell_count.y; y++) {
			for (int x = 0; x < cell_count.x; x++) {
				SubtileData data;
				Vector2i coord(x, y);
				for (int i = 0; i < sd.size(); i++) {
					if (sd[i].autotile_coord == coord) {
						data.collisions.push_back(sd[i].shape);
					}
				}
				data.navigation_shape = tileset->autotile_get_navigation_polygon(get_current_tile(), coord);
				data.occlusion_shape = tileset->tile_get_light_occluder(get_current_tile());
				current_tile_data[coord] = data;
			}
		}
	}
}

void TileSetEditor::_update_toggle_shape_button() {
	Ref<ConvexPolygonShape2D> convex = edited_collision_shape;
	Ref<ConcavePolygonShape2D> concave = edited_collision_shape;
	separator_shape_toggle->show();
	tools[SHAPE_TOGGLE_TYPE]->show();
	if (edit_mode != EDITMODE_COLLISION || !edited_collision_shape.is_valid()) {
		separator_shape_toggle->hide();
		tools[SHAPE_TOGGLE_TYPE]->hide();
	} else if (concave.is_valid()) {
		tools[SHAPE_TOGGLE_TYPE]->set_icon(get_icon("ConvexPolygonShape2D", "EditorIcons"));
		tools[SHAPE_TOGGLE_TYPE]->set_text(TTR("Make Convex"));
	} else if (convex.is_valid()) {
		tools[SHAPE_TOGGLE_TYPE]->set_icon(get_icon("ConcavePolygonShape2D", "EditorIcons"));
		tools[SHAPE_TOGGLE_TYPE]->set_text(TTR("Make Concave"));
	} else {
		// Shouldn't happen
		separator_shape_toggle->hide();
		tools[SHAPE_TOGGLE_TYPE]->hide();
	}
}

void TileSetEditor::_select_next_tile() {
	Array tiles = _get_tiles_in_current_texture(true);
	if (tiles.size() == 0) {
		set_current_tile(-1);
	} else if (get_current_tile() == -1) {
		set_current_tile(tiles[0]);
	} else {
		int index = tiles.find(get_current_tile());
		if (index < 0) {
			set_current_tile(tiles[0]);
		} else if (index == tiles.size() - 1) {
			set_current_tile(tiles[0]);
		} else {
			set_current_tile(tiles[index + 1]);
		}
	}
	if (get_current_tile() == -1) {
		return;
	} else if (tileset->tile_get_tile_mode(get_current_tile()) == TileSet::SINGLE_TILE) {
		return;
	} else {
		switch (edit_mode) {
			case EDITMODE_COLLISION:
			case EDITMODE_OCCLUSION:
			case EDITMODE_NAVIGATION:
			case EDITMODE_PRIORITY:
			case EDITMODE_Z_INDEX: {
				edited_shape_coord = Vector2();
				_select_edited_shape_coord();
			} break;
			default: {
			}
		}
	}
}

void TileSetEditor::_select_previous_tile() {
	Array tiles = _get_tiles_in_current_texture(true);
	if (tiles.size() == 0) {
		set_current_tile(-1);
	} else if (get_current_tile() == -1) {
		set_current_tile(tiles[tiles.size() - 1]);
	} else {
		int index = tiles.find(get_current_tile());
		if (index <= 0) {
			set_current_tile(tiles[tiles.size() - 1]);
		} else {
			set_current_tile(tiles[index - 1]);
		}
	}
	if (get_current_tile() == -1) {
		return;
	} else if (tileset->tile_get_tile_mode(get_current_tile()) == TileSet::SINGLE_TILE) {
		return;
	} else {
		switch (edit_mode) {
			case EDITMODE_COLLISION:
			case EDITMODE_OCCLUSION:
			case EDITMODE_NAVIGATION:
			case EDITMODE_PRIORITY:
			case EDITMODE_Z_INDEX: {
				edited_shape_coord = _get_subtiles_count(get_current_tile()) - Vector2(1, 1);
				_select_edited_shape_coord();
			} break;
			default: {
			}
		}
	}
}

Array TileSetEditor::_get_tiles_in_current_texture(bool sorted) {
	Array a;
	List<int> all_tiles;
	if (!get_current_texture().is_valid()) {
		return a;
	}
	tileset->get_tile_list(&all_tiles);
	for (int i = 0; i < all_tiles.size(); i++) {
		if (tileset->tile_get_texture(all_tiles[i]) == get_current_texture()) {
			a.push_back(all_tiles[i]);
		}
	}
	if (sorted) {
		a.sort_custom(this, "_sort_tiles");
	}
	return a;
}

bool TileSetEditor::_sort_tiles(Variant p_a, Variant p_b) {
	int a = p_a;
	int b = p_b;

	Vector2 pos_a = tileset->tile_get_region(a).position;
	Vector2 pos_b = tileset->tile_get_region(b).position;
	if (pos_a.y < pos_b.y) {
		return true;

	} else if (pos_a.y == pos_b.y) {
		return (pos_a.x < pos_b.x);
	} else {
		return false;
	}
}

Vector2 TileSetEditor::_get_subtiles_count(int p_tile_id) {
	const int spacing = tileset->autotile_get_spacing(p_tile_id);
	const Vector2 region_size = tileset->tile_get_region(p_tile_id).size;
	const Vector2 subtile_size = tileset->autotile_get_size(p_tile_id);
	// In case of not perfect fit the last row/column is allowed to exceed the tile region.
	// The return value is the biggest integer-only `(m, n)` satisfying the formula:
	// (m, n) * subtile_size + (m - 1, n - 1) * spacing < region_size + subtile_size
	Vector2 mn = Vector2(1, 1) + (region_size / (subtile_size + Vector2(spacing, spacing)));
	return mn == mn.floor() ? mn.floor() - Vector2(1, 1) : mn.floor();
}

void TileSetEditor::_select_next_subtile() {
	if (get_current_tile() == -1) {
		_select_next_tile();
		return;
	}
	if (tileset->tile_get_tile_mode(get_current_tile()) == TileSet::SINGLE_TILE) {
		_select_next_tile();
	} else if (edit_mode == EDITMODE_REGION || edit_mode == EDITMODE_BITMASK || edit_mode == EDITMODE_ICON) {
		_select_next_tile();
	} else {
		Vector2 cell_count = _get_subtiles_count(get_current_tile());
		if (edited_shape_coord.x >= cell_count.x - 1 && edited_shape_coord.y >= cell_count.y - 1) {
			_select_next_tile();
		} else {
			edited_shape_coord.x++;
			if (edited_shape_coord.x > cell_count.x - 1) {
				edited_shape_coord.x = 0;
				edited_shape_coord.y++;
			}
			_select_edited_shape_coord();
		}
	}
}

void TileSetEditor::_select_previous_subtile() {
	if (get_current_tile() == -1) {
		_select_previous_tile();
		return;
	}
	if (tileset->tile_get_tile_mode(get_current_tile()) == TileSet::SINGLE_TILE) {
		_select_previous_tile();
	} else if (edit_mode == EDITMODE_REGION || edit_mode == EDITMODE_BITMASK || edit_mode == EDITMODE_ICON) {
		_select_previous_tile();
	} else {
		Vector2 cell_count = _get_subtiles_count(get_current_tile());
		if (edited_shape_coord.x <= 0 && edited_shape_coord.y <= 0) {
			_select_previous_tile();
		} else {
			edited_shape_coord.x--;
			if (edited_shape_coord.x < 0) {
				edited_shape_coord.x = cell_count.x - 1;
				edited_shape_coord.y--;
			}
			_select_edited_shape_coord();
		}
	}
}

void TileSetEditor::_select_next_shape() {
	if (get_current_tile() == -1) {
		_select_next_subtile();
	} else if (edit_mode != EDITMODE_COLLISION) {
		_select_next_subtile();
	} else {
		Vector2i edited_coord = Vector2i();
		if (tileset->tile_get_tile_mode(get_current_tile()) != TileSet::SINGLE_TILE) {
			edited_coord = Vector2i(edited_shape_coord);
		}
		SubtileData data = current_tile_data[edited_coord];
		if (data.collisions.size() == 0) {
			_select_next_subtile();
		} else {
			int index = data.collisions.find(edited_collision_shape);
			if (index < 0) {
				_set_edited_collision_shape(data.collisions[0]);
			} else if (index == data.collisions.size() - 1) {
				_select_next_subtile();
			} else {
				_set_edited_collision_shape(data.collisions[index + 1]);
			}
		}
		current_shape.resize(0);
		Rect2 current_tile_region = tileset->tile_get_region(get_current_tile());
		current_tile_region.position += WORKSPACE_MARGIN;

		int spacing = tileset->autotile_get_spacing(get_current_tile());
		Vector2 size = tileset->autotile_get_size(get_current_tile());
		Vector2 shape_anchor = edited_shape_coord;
		shape_anchor.x *= (size.x + spacing);
		shape_anchor.y *= (size.y + spacing);
		current_tile_region.position += shape_anchor;

		if (edited_collision_shape.is_valid()) {
			for (int i = 0; i < _get_edited_shape_points().size(); i++) {
				current_shape.push_back(_get_edited_shape_points()[i] + current_tile_region.position);
			}
		}
		workspace->update();
		workspace_container->update();
		helper->_change_notify("");
	}
}

void TileSetEditor::_select_previous_shape() {
	if (get_current_tile() == -1) {
		_select_previous_subtile();
		if (get_current_tile() != -1 && edit_mode == EDITMODE_COLLISION) {
			SubtileData data = current_tile_data[Vector2i(edited_shape_coord)];
			if (data.collisions.size() > 1) {
				_set_edited_collision_shape(data.collisions[data.collisions.size() - 1]);
			}
		} else {
			return;
		}
	} else if (edit_mode != EDITMODE_COLLISION) {
		_select_previous_subtile();
	} else {
		Vector2i edited_coord = Vector2i();
		if (tileset->tile_get_tile_mode(get_current_tile()) != TileSet::SINGLE_TILE) {
			edited_coord = Vector2i(edited_shape_coord);
		}
		SubtileData data = current_tile_data[edited_coord];
		if (data.collisions.size() == 0) {
			_select_previous_subtile();
			data = current_tile_data[Vector2i(edited_shape_coord)];
			if (data.collisions.size() > 1) {
				_set_edited_collision_shape(data.collisions[data.collisions.size() - 1]);
			}
		} else {
			int index = data.collisions.find(edited_collision_shape);
			if (index < 0) {
				_set_edited_collision_shape(data.collisions[data.collisions.size() - 1]);
			} else if (index == 0) {
				_select_previous_subtile();
				data = current_tile_data[Vector2i(edited_shape_coord)];
				if (data.collisions.size() > 1) {
					_set_edited_collision_shape(data.collisions[data.collisions.size() - 1]);
				}
			} else {
				_set_edited_collision_shape(data.collisions[index - 1]);
			}
		}

		current_shape.resize(0);
		Rect2 current_tile_region = tileset->tile_get_region(get_current_tile());
		current_tile_region.position += WORKSPACE_MARGIN;

		int spacing = tileset->autotile_get_spacing(get_current_tile());
		Vector2 size = tileset->autotile_get_size(get_current_tile());
		Vector2 shape_anchor = edited_shape_coord;
		shape_anchor.x *= (size.x + spacing);
		shape_anchor.y *= (size.y + spacing);
		current_tile_region.position += shape_anchor;

		if (edited_collision_shape.is_valid()) {
			for (int i = 0; i < _get_edited_shape_points().size(); i++) {
				current_shape.push_back(_get_edited_shape_points()[i] + current_tile_region.position);
			}
		}
		workspace->update();
		workspace_container->update();
		helper->_change_notify("");
	}
}

void TileSetEditor::_set_edited_collision_shape(const Ref<Shape2D> &p_shape) {
	edited_collision_shape = p_shape;
	_update_toggle_shape_button();
}

void TileSetEditor::_set_snap_step(Vector2 p_val) {
	snap_step.x = CLAMP(p_val.x, 1, 1024);
	snap_step.y = CLAMP(p_val.y, 1, 1024);
	workspace->update();
}

void TileSetEditor::_set_snap_off(Vector2 p_val) {
	snap_offset.x = CLAMP(p_val.x, 0, 1024 + WORKSPACE_MARGIN.x);
	snap_offset.y = CLAMP(p_val.y, 0, 1024 + WORKSPACE_MARGIN.y);
	workspace->update();
}

void TileSetEditor::_set_snap_sep(Vector2 p_val) {
	snap_separation.x = CLAMP(p_val.x, 0, 1024);
	snap_separation.y = CLAMP(p_val.y, 0, 1024);
	workspace->update();
}

void TileSetEditor::_validate_current_tile_id() {
	if (get_current_tile() >= 0 && !tileset->has_tile(get_current_tile())) {
		set_current_tile(-1);
	}
}

void TileSetEditor::_select_edited_shape_coord() {
	select_coord(edited_shape_coord);
}

void TileSetEditor::_undo_tile_removal(int p_id) {
	undo_redo->add_undo_method(tileset.ptr(), "create_tile", p_id);
	undo_redo->add_undo_method(tileset.ptr(), "tile_set_name", p_id, tileset->tile_get_name(p_id));
	undo_redo->add_undo_method(tileset.ptr(), "tile_set_normal_map", p_id, tileset->tile_get_normal_map(p_id));
	undo_redo->add_undo_method(tileset.ptr(), "tile_set_texture_offset", p_id, tileset->tile_get_texture_offset(p_id));
	undo_redo->add_undo_method(tileset.ptr(), "tile_set_material", p_id, tileset->tile_get_material(p_id));
	undo_redo->add_undo_method(tileset.ptr(), "tile_set_modulate", p_id, tileset->tile_get_modulate(p_id));
	undo_redo->add_undo_method(tileset.ptr(), "tile_set_occluder_offset", p_id, tileset->tile_get_occluder_offset(p_id));
	undo_redo->add_undo_method(tileset.ptr(), "tile_set_navigation_polygon_offset", p_id, tileset->tile_get_navigation_polygon_offset(p_id));
	undo_redo->add_undo_method(tileset.ptr(), "tile_set_shape_offset", p_id, 0, tileset->tile_get_shape_offset(p_id, 0));
	undo_redo->add_undo_method(tileset.ptr(), "tile_set_shape_transform", p_id, 0, tileset->tile_get_shape_transform(p_id, 0));
	undo_redo->add_undo_method(tileset.ptr(), "tile_set_z_index", p_id, tileset->tile_get_z_index(p_id));
	undo_redo->add_undo_method(tileset.ptr(), "tile_set_texture", p_id, tileset->tile_get_texture(p_id));
	undo_redo->add_undo_method(tileset.ptr(), "tile_set_region", p_id, tileset->tile_get_region(p_id));
	// Necessary to get the version that returns a Array instead of a Vector.
	undo_redo->add_undo_method(tileset.ptr(), "tile_set_shapes", p_id, tileset->call("tile_get_shapes", p_id));
	if (tileset->tile_get_tile_mode(p_id) == TileSet::SINGLE_TILE) {
		undo_redo->add_undo_method(tileset.ptr(), "tile_set_light_occluder", p_id, tileset->tile_get_light_occluder(p_id));
		undo_redo->add_undo_method(tileset.ptr(), "tile_set_navigation_polygon", p_id, tileset->tile_get_navigation_polygon(p_id));
	} else {
		Map<Vector2, Ref<OccluderPolygon2D>> oclusion_map = tileset->autotile_get_light_oclusion_map(p_id);
		for (Map<Vector2, Ref<OccluderPolygon2D>>::Element *E = oclusion_map.front(); E; E = E->next()) {
			undo_redo->add_undo_method(tileset.ptr(), "autotile_set_light_occluder", p_id, E->value(), E->key());
		}
		Map<Vector2, Ref<NavigationPolygon>> navigation_map = tileset->autotile_get_navigation_map(p_id);
		for (Map<Vector2, Ref<NavigationPolygon>>::Element *E = navigation_map.front(); E; E = E->next()) {
			undo_redo->add_undo_method(tileset.ptr(), "autotile_set_navigation_polygon", p_id, E->value(), E->key());
		}
		Map<Vector2, uint32_t> bitmask_map = tileset->autotile_get_bitmask_map(p_id);
		for (Map<Vector2, uint32_t>::Element *E = bitmask_map.front(); E; E = E->next()) {
			undo_redo->add_undo_method(tileset.ptr(), "autotile_set_bitmask", p_id, E->key(), E->value());
		}
		Map<Vector2, int> priority_map = tileset->autotile_get_priority_map(p_id);
		for (Map<Vector2, int>::Element *E = priority_map.front(); E; E = E->next()) {
			undo_redo->add_undo_method(tileset.ptr(), "autotile_set_subtile_priority", p_id, E->key(), E->value());
		}
		undo_redo->add_undo_method(tileset.ptr(), "autotile_set_icon_coordinate", p_id, tileset->autotile_get_icon_coordinate(p_id));
		Map<Vector2, int> z_map = tileset->autotile_get_z_index_map(p_id);
		for (Map<Vector2, int>::Element *E = z_map.front(); E; E = E->next()) {
			undo_redo->add_undo_method(tileset.ptr(), "autotile_set_z_index", p_id, E->key(), E->value());
		}
		undo_redo->add_undo_method(tileset.ptr(), "tile_set_tile_mode", p_id, tileset->tile_get_tile_mode(p_id));
		undo_redo->add_undo_method(tileset.ptr(), "autotile_set_size", p_id, tileset->autotile_get_size(p_id));
		undo_redo->add_undo_method(tileset.ptr(), "autotile_set_spacing", p_id, tileset->autotile_get_spacing(p_id));
		undo_redo->add_undo_method(tileset.ptr(), "autotile_set_bitmask_mode", p_id, tileset->autotile_get_bitmask_mode(p_id));
		undo_redo->add_undo_method(tileset.ptr(), "autotile_set_fallback_mode", p_id, tileset->autotile_get_fallback_mode(p_id));
	}
}

void TileSetEditor::_zoom_in() {
	_zoom_on_position(scale_ratio, Vector2());
}

void TileSetEditor::_zoom_out() {
	_zoom_on_position(1 / scale_ratio, Vector2());
}

void TileSetEditor::_zoom_reset() {
	workspace->set_scale(Vector2(1, 1));
	workspace_container->set_custom_minimum_size(workspace->get_rect().size);
	workspace_overlay->set_custom_minimum_size(workspace->get_rect().size);
}

void TileSetEditor::_zoom_on_position(float p_zoom, const Vector2 &p_position) {
	const float old_scale = workspace->get_scale().x;
	const float new_scale = CLAMP(old_scale * p_zoom, min_scale, max_scale);

	workspace->set_scale(Vector2(new_scale, new_scale));
	workspace_container->set_custom_minimum_size(workspace->get_rect().size * new_scale);
	workspace_overlay->set_custom_minimum_size(workspace->get_rect().size * new_scale);

	Vector2 offset = Vector2(scroll->get_h_scroll(), scroll->get_v_scroll());
	offset = (offset + p_position) / old_scale * new_scale - p_position;
	scroll->set_h_scroll(offset.x);
	scroll->set_v_scroll(offset.y);
}

void TileSetEditor::draw_highlight_current_tile() {
	Color shadow_color = Color(0.3, 0.3, 0.3, 0.3);
	if ((workspace_mode == WORKSPACE_EDIT && get_current_tile() >= 0) || !edited_region.has_no_area()) {
		Rect2 region;
		if (edited_region.has_no_area()) {
			region = tileset->tile_get_region(get_current_tile());
			region.position += WORKSPACE_MARGIN;
		} else {
			region = edited_region;
		}

		if (region.position.y >= 0) {
			workspace->draw_rect(Rect2(0, 0, workspace->get_rect().size.x, region.position.y), shadow_color);
		}
		if (region.position.x >= 0) {
			workspace->draw_rect(Rect2(0, MAX(0, region.position.y), region.position.x, MIN(workspace->get_rect().size.y - region.position.y, MIN(region.size.y, region.position.y + region.size.y))), shadow_color);
		}
		if (region.position.x + region.size.x <= workspace->get_rect().size.x) {
			workspace->draw_rect(Rect2(region.position.x + region.size.x, MAX(0, region.position.y), workspace->get_rect().size.x - region.position.x - region.size.x, MIN(workspace->get_rect().size.y - region.position.y, MIN(region.size.y, region.position.y + region.size.y))), shadow_color);
		}
		if (region.position.y + region.size.y <= workspace->get_rect().size.y) {
			workspace->draw_rect(Rect2(0, region.position.y + region.size.y, workspace->get_rect().size.x, workspace->get_rect().size.y - region.size.y - region.position.y), shadow_color);
		}
	} else {
		workspace->draw_rect(Rect2(Point2(0, 0), workspace->get_rect().size), shadow_color);
	}
}

void TileSetEditor::draw_highlight_subtile(Vector2 coord, const Vector<Vector2> &other_highlighted) {
	Color shadow_color = Color(0.3, 0.3, 0.3, 0.3);
	Vector2 size = tileset->autotile_get_size(get_current_tile());
	int spacing = tileset->autotile_get_spacing(get_current_tile());
	Rect2 region = tileset->tile_get_region(get_current_tile());
	coord.x *= (size.x + spacing);
	coord.y *= (size.y + spacing);
	coord += region.position;
	coord += WORKSPACE_MARGIN;

	if (coord.y >= 0) {
		workspace->draw_rect(Rect2(0, 0, workspace->get_rect().size.x, coord.y), shadow_color);
	}
	if (coord.x >= 0) {
		workspace->draw_rect(Rect2(0, MAX(0, coord.y), coord.x, MIN(workspace->get_rect().size.y - coord.y, MIN(size.y, coord.y + size.y))), shadow_color);
	}
	if (coord.x + size.x <= workspace->get_rect().size.x) {
		workspace->draw_rect(Rect2(coord.x + size.x, MAX(0, coord.y), workspace->get_rect().size.x - coord.x - size.x, MIN(workspace->get_rect().size.y - coord.y, MIN(size.y, coord.y + size.y))), shadow_color);
	}
	if (coord.y + size.y <= workspace->get_rect().size.y) {
		workspace->draw_rect(Rect2(0, coord.y + size.y, workspace->get_rect().size.x, workspace->get_rect().size.y - size.y - coord.y), shadow_color);
	}

	coord += Vector2(1, 1) / workspace->get_scale().x;
	workspace->draw_rect(Rect2(coord, size - Vector2(2, 2) / workspace->get_scale().x), Color(1, 0, 0), false);
	for (int i = 0; i < other_highlighted.size(); i++) {
		coord = other_highlighted[i];
		coord.x *= (size.x + spacing);
		coord.y *= (size.y + spacing);
		coord += region.position;
		coord += WORKSPACE_MARGIN;
		coord += Vector2(1, 1) / workspace->get_scale().x;
		workspace->draw_rect(Rect2(coord, size - Vector2(2, 2) / workspace->get_scale().x), Color(1, 0.5, 0.5), false);
	}
}

void TileSetEditor::draw_tile_subdivision(int p_id, Color p_color) const {
	Color c = p_color;
	if (tileset->tile_get_tile_mode(p_id) == TileSet::AUTO_TILE || tileset->tile_get_tile_mode(p_id) == TileSet::ATLAS_TILE) {
		Rect2 region = tileset->tile_get_region(p_id);
		Size2 size = tileset->autotile_get_size(p_id);
		int spacing = tileset->autotile_get_spacing(p_id);
		float j = size.x;

		while (j < region.size.x) {
			if (spacing <= 0) {
				workspace->draw_line(region.position + WORKSPACE_MARGIN + Point2(j, 0), region.position + WORKSPACE_MARGIN + Point2(j, region.size.y), c);
			} else {
				workspace->draw_rect(Rect2(region.position + WORKSPACE_MARGIN + Point2(j, 0), Size2(spacing, region.size.y)), c);
			}
			j += spacing + size.x;
		}
		j = size.y;
		while (j < region.size.y) {
			if (spacing <= 0) {
				workspace->draw_line(region.position + WORKSPACE_MARGIN + Point2(0, j), region.position + WORKSPACE_MARGIN + Point2(region.size.x, j), c);
			} else {
				workspace->draw_rect(Rect2(region.position + WORKSPACE_MARGIN + Point2(0, j), Size2(region.size.x, spacing)), c);
			}
			j += spacing + size.y;
		}
	}
}

void TileSetEditor::draw_edited_region_subdivision() const {
	Color c = Color(0.3, 0.7, 0.6);
	Rect2 region = edited_region;
	Size2 size;
	int spacing;
	bool draw;

	if (workspace_mode == WORKSPACE_EDIT) {
		int p_id = get_current_tile();
		size = tileset->autotile_get_size(p_id);
		spacing = tileset->autotile_get_spacing(p_id);
		draw = tileset->tile_get_tile_mode(p_id) == TileSet::AUTO_TILE || tileset->tile_get_tile_mode(p_id) == TileSet::ATLAS_TILE;
	} else {
		size = snap_step;
		spacing = snap_separation.x;
		draw = workspace_mode != WORKSPACE_CREATE_SINGLE;
	}

	if (draw) {
		float j = size.x;
		while (j < region.size.x) {
			if (spacing <= 0) {
				workspace->draw_line(region.position + Point2(j, 0), region.position + Point2(j, region.size.y), c);
			} else {
				workspace->draw_rect(Rect2(region.position + Point2(j, 0), Size2(spacing, region.size.y)), c);
			}
			j += spacing + size.x;
		}
		j = size.y;
		while (j < region.size.y) {
			if (spacing <= 0) {
				workspace->draw_line(region.position + Point2(0, j), region.position + Point2(region.size.x, j), c);
			} else {
				workspace->draw_rect(Rect2(region.position + Point2(0, j), Size2(region.size.x, spacing)), c);
			}
			j += spacing + size.y;
		}
	}
}

void TileSetEditor::draw_grid_snap() {
	if (tools[TOOL_GRID_SNAP]->is_pressed()) {
		Color grid_color = Color(0.4, 0, 1);
		Size2 s = workspace->get_size();

		int width_count = Math::floor((s.width - WORKSPACE_MARGIN.x) / (snap_step.x + snap_separation.x));
		int height_count = Math::floor((s.height - WORKSPACE_MARGIN.y) / (snap_step.y + snap_separation.y));

		int last_p = 0;
		if (snap_step.x != 0) {
			for (int i = 0; i <= width_count; i++) {
				if (i == 0 && snap_offset.x != 0) {
					last_p = snap_offset.x;
				}
				if (snap_separation.x != 0) {
					if (i != 0) {
						workspace->draw_rect(Rect2(last_p, 0, snap_separation.x, s.height), grid_color);
						last_p += snap_separation.x;
					} else {
						workspace->draw_rect(Rect2(last_p, 0, -snap_separation.x, s.height), grid_color);
					}
				} else {
					workspace->draw_line(Point2(last_p, 0), Point2(last_p, s.height), grid_color);
				}
				last_p += snap_step.x;
			}
		}
		last_p = 0;
		if (snap_step.y != 0) {
			for (int i = 0; i <= height_count; i++) {
				if (i == 0 && snap_offset.y != 0) {
					last_p = snap_offset.y;
				}
				if (snap_separation.y != 0) {
					if (i != 0) {
						workspace->draw_rect(Rect2(0, last_p, s.width, snap_separation.y), grid_color);
						last_p += snap_separation.y;
					} else {
						workspace->draw_rect(Rect2(0, last_p, s.width, -snap_separation.y), grid_color);
					}
				} else {
					workspace->draw_line(Point2(0, last_p), Point2(s.width, last_p), grid_color);
				}
				last_p += snap_step.y;
			}
		}
	}
}

void TileSetEditor::draw_polygon_shapes() {
	int t_id = get_current_tile();
	if (t_id < 0) {
		return;
	}

	switch (edit_mode) {
		case EDITMODE_COLLISION: {
			Vector<TileSet::ShapeData> sd = tileset->tile_get_shapes(t_id);
			for (int i = 0; i < sd.size(); i++) {
				Vector2 coord = Vector2(0, 0);
				Vector2 anchor = Vector2(0, 0);
				if (tileset->tile_get_tile_mode(get_current_tile()) == TileSet::AUTO_TILE || tileset->tile_get_tile_mode(get_current_tile()) == TileSet::ATLAS_TILE) {
					coord = sd[i].autotile_coord;
					anchor = tileset->autotile_get_size(t_id);
					anchor.x += tileset->autotile_get_spacing(t_id);
					anchor.y += tileset->autotile_get_spacing(t_id);
					anchor.x *= coord.x;
					anchor.y *= coord.y;
				}
				anchor += WORKSPACE_MARGIN;
				anchor += tileset->tile_get_region(t_id).position;
				Ref<Shape2D> shape = sd[i].shape;
				if (shape.is_valid()) {
					Color c_bg;
					Color c_border;
					Ref<ConvexPolygonShape2D> convex = shape;
					bool is_convex = convex.is_valid();
					if ((tileset->tile_get_tile_mode(get_current_tile()) == TileSet::SINGLE_TILE || coord == edited_shape_coord) && sd[i].shape == edited_collision_shape) {
						if (is_convex) {
							c_bg = Color(0, 1, 1, 0.5);
							c_border = Color(0, 1, 1);
						} else {
							c_bg = Color(0.8, 0, 1, 0.5);
							c_border = Color(0.8, 0, 1);
						}
					} else {
						if (is_convex) {
							c_bg = Color(0.9, 0.7, 0.07, 0.5);
							c_border = Color(0.9, 0.7, 0.07, 1);

						} else {
							c_bg = Color(0.9, 0.45, 0.075, 0.5);
							c_border = Color(0.9, 0.45, 0.075);
						}
					}
					Vector<Vector2> polygon;
					Vector<Color> colors;
					if (!creating_shape && shape == edited_collision_shape && current_shape.size() > 2) {
						for (int j = 0; j < current_shape.size(); j++) {
							polygon.push_back(current_shape[j]);
							colors.push_back(c_bg);
						}
					} else {
						for (int j = 0; j < _get_collision_shape_points(shape).size(); j++) {
							polygon.push_back(_get_collision_shape_points(shape)[j] + anchor);
							colors.push_back(c_bg);
						}
					}

					if (polygon.size() < 3) {
						continue;
					}

					workspace->draw_polygon(polygon, colors);

					if (coord == edited_shape_coord || tileset->tile_get_tile_mode(get_current_tile()) == TileSet::SINGLE_TILE) {
						if (!creating_shape && polygon.size() > 1) {
							for (int j = 0; j < polygon.size() - 1; j++) {
								workspace->draw_line(polygon[j], polygon[j + 1], c_border, 1, true);
							}
							workspace->draw_line(polygon[polygon.size() - 1], polygon[0], c_border, 1, true);
						}
						if (shape == edited_collision_shape) {
							draw_handles = true;
						}
					}
				}
			}
		} break;
		case EDITMODE_OCCLUSION: {
			if (tileset->tile_get_tile_mode(get_current_tile()) == TileSet::SINGLE_TILE) {
				Ref<OccluderPolygon2D> shape = edited_occlusion_shape;
				if (shape.is_valid()) {
					Color c_bg = Color(0, 1, 1, 0.5);
					Color c_border = Color(0, 1, 1);

					Vector<Vector2> polygon;
					Vector<Color> colors;
					Vector2 anchor = WORKSPACE_MARGIN;
					anchor += tileset->tile_get_region(get_current_tile()).position;
					if (!creating_shape && shape == edited_occlusion_shape && current_shape.size() > 2) {
						for (int j = 0; j < current_shape.size(); j++) {
							polygon.push_back(current_shape[j]);
							colors.push_back(c_bg);
						}
					} else {
						for (int j = 0; j < shape->get_polygon().size(); j++) {
							polygon.push_back(shape->get_polygon()[j] + anchor);
							colors.push_back(c_bg);
						}
					}
					workspace->draw_polygon(polygon, colors);

					if (!creating_shape && polygon.size() > 1) {
						for (int j = 0; j < polygon.size() - 1; j++) {
							workspace->draw_line(polygon[j], polygon[j + 1], c_border, 1, true);
						}
						workspace->draw_line(polygon[polygon.size() - 1], polygon[0], c_border, 1, true);
					}
					if (shape == edited_occlusion_shape) {
						draw_handles = true;
					}
				}
			} else {
				Map<Vector2, Ref<OccluderPolygon2D>> map = tileset->autotile_get_light_oclusion_map(t_id);
				for (Map<Vector2, Ref<OccluderPolygon2D>>::Element *E = map.front(); E; E = E->next()) {
					Vector2 coord = E->key();
					Vector2 anchor = tileset->autotile_get_size(t_id);
					anchor.x += tileset->autotile_get_spacing(t_id);
					anchor.y += tileset->autotile_get_spacing(t_id);
					anchor.x *= coord.x;
					anchor.y *= coord.y;
					anchor += WORKSPACE_MARGIN;
					anchor += tileset->tile_get_region(t_id).position;
					Ref<OccluderPolygon2D> shape = E->value();
					if (shape.is_valid()) {
						Color c_bg;
						Color c_border;
						if (coord == edited_shape_coord && shape == edited_occlusion_shape) {
							c_bg = Color(0, 1, 1, 0.5);
							c_border = Color(0, 1, 1);
						} else {
							c_bg = Color(0.9, 0.7, 0.07, 0.5);
							c_border = Color(0.9, 0.7, 0.07, 1);
						}
						Vector<Vector2> polygon;
						Vector<Color> colors;
						if (!creating_shape && shape == edited_occlusion_shape && current_shape.size() > 2) {
							for (int j = 0; j < current_shape.size(); j++) {
								polygon.push_back(current_shape[j]);
								colors.push_back(c_bg);
							}
						} else {
							for (int j = 0; j < shape->get_polygon().size(); j++) {
								polygon.push_back(shape->get_polygon()[j] + anchor);
								colors.push_back(c_bg);
							}
						}
						workspace->draw_polygon(polygon, colors);

						if (coord == edited_shape_coord) {
							if (!creating_shape && polygon.size() > 1) {
								for (int j = 0; j < polygon.size() - 1; j++) {
									workspace->draw_line(polygon[j], polygon[j + 1], c_border, 1, true);
								}
								workspace->draw_line(polygon[polygon.size() - 1], polygon[0], c_border, 1, true);
							}
							if (shape == edited_occlusion_shape) {
								draw_handles = true;
							}
						}
					}
				}
			}
		} break;
		case EDITMODE_NAVIGATION: {
			if (tileset->tile_get_tile_mode(get_current_tile()) == TileSet::SINGLE_TILE) {
				Ref<NavigationPolygon> shape = edited_navigation_shape;

				if (shape.is_valid()) {
					Color c_bg = Color(0, 1, 1, 0.5);
					Color c_border = Color(0, 1, 1);

					Vector<Vector2> polygon;
					Vector<Color> colors;
					Vector2 anchor = WORKSPACE_MARGIN;
					anchor += tileset->tile_get_region(get_current_tile()).position;
					if (!creating_shape && shape == edited_navigation_shape && current_shape.size() > 2) {
						for (int j = 0; j < current_shape.size(); j++) {
							polygon.push_back(current_shape[j]);
							colors.push_back(c_bg);
						}
					} else {
						for (int outline_idx = 0; outline_idx < shape->get_outline_count(); outline_idx++) {
							PoolVector<Vector2> outline_vertices = shape->get_outline(outline_idx);
							for (int vertex_idx = 0; vertex_idx < outline_vertices.size(); vertex_idx++) {
								polygon.push_back(outline_vertices[vertex_idx] + anchor);
							}
						}
						colors.resize(polygon.size());
						colors.fill(c_bg);
					}
					workspace->draw_polygon(polygon, colors);

					if (!creating_shape && polygon.size() > 1) {
						for (int j = 0; j < polygon.size() - 1; j++) {
							workspace->draw_line(polygon[j], polygon[j + 1], c_border, 1, true);
						}
						workspace->draw_line(polygon[polygon.size() - 1], polygon[0], c_border, 1, true);
					}
					if (shape == edited_navigation_shape) {
						draw_handles = true;
					}
				}
			} else {
				Map<Vector2, Ref<NavigationPolygon>> map = tileset->autotile_get_navigation_map(t_id);
				for (Map<Vector2, Ref<NavigationPolygon>>::Element *E = map.front(); E; E = E->next()) {
					Vector2 coord = E->key();
					Vector2 anchor = tileset->autotile_get_size(t_id);
					anchor.x += tileset->autotile_get_spacing(t_id);
					anchor.y += tileset->autotile_get_spacing(t_id);
					anchor.x *= coord.x;
					anchor.y *= coord.y;
					anchor += WORKSPACE_MARGIN;
					anchor += tileset->tile_get_region(t_id).position;
					Ref<NavigationPolygon> shape = E->value();
					if (shape.is_valid()) {
						Color c_bg;
						Color c_border;
						if (coord == edited_shape_coord && shape == edited_navigation_shape) {
							c_bg = Color(0, 1, 1, 0.5);
							c_border = Color(0, 1, 1);
						} else {
							c_bg = Color(0.9, 0.7, 0.07, 0.5);
							c_border = Color(0.9, 0.7, 0.07, 1);
						}
						Vector<Vector2> polygon;
						Vector<Color> colors;
						if (!creating_shape && shape == edited_navigation_shape && current_shape.size() > 2) {
							for (int j = 0; j < current_shape.size(); j++) {
								polygon.push_back(current_shape[j]);
								colors.push_back(c_bg);
							}
						} else {
							for (int outline_idx = 0; outline_idx < shape->get_outline_count(); outline_idx++) {
								PoolVector<Vector2> outline_vertices = shape->get_outline(outline_idx);
								for (int vertex_idx = 0; vertex_idx < outline_vertices.size(); vertex_idx++) {
									polygon.push_back(outline_vertices[vertex_idx] + anchor);
								}
							}
							colors.resize(polygon.size());
							colors.fill(c_bg);
						}
						workspace->draw_polygon(polygon, colors);

						if (coord == edited_shape_coord) {
							if (!creating_shape && polygon.size() > 1) {
								for (int j = 0; j < polygon.size() - 1; j++) {
									workspace->draw_line(polygon[j], polygon[j + 1], c_border, 1, true);
								}
								workspace->draw_line(polygon[polygon.size() - 1], polygon[0], c_border, 1, true);
							}
							if (shape == edited_navigation_shape) {
								draw_handles = true;
							}
						}
					}
				}
			}
		} break;
		default: {
		}
	}

	if (creating_shape && current_shape.size() > 1) {
		for (int j = 0; j < current_shape.size() - 1; j++) {
			workspace->draw_line(current_shape[j], current_shape[j + 1], Color(0, 1, 1), 1, true);
		}
		workspace->draw_line(current_shape[current_shape.size() - 1], snap_point(workspace->get_local_mouse_position()), Color(0, 1, 1), 1, true);
		draw_handles = true;
	}
}

void TileSetEditor::close_shape(const Vector2 &shape_anchor) {
	creating_shape = false;

	if (edit_mode == EDITMODE_COLLISION) {
		if (current_shape.size() >= 3) {
			Ref<ConvexPolygonShape2D> shape = memnew(ConvexPolygonShape2D);

			Vector<Vector2> points;
			float p_total = 0;

			for (int i = 0; i < current_shape.size(); i++) {
				points.push_back(current_shape[i] - shape_anchor);

				if (i != current_shape.size() - 1) {
					p_total += ((current_shape[i + 1].x - current_shape[i].x) * (-current_shape[i + 1].y + (-current_shape[i].y)));
				} else {
					p_total += ((current_shape[0].x - current_shape[i].x) * (-current_shape[0].y + (-current_shape[i].y)));
				}
			}

			if (p_total < 0) {
				points.invert();
			}

			shape->set_points(points);

			undo_redo->create_action(TTR("Create Collision Polygon"));
			// Necessary to get the version that returns a Array instead of a Vector.
			Array sd = tileset->call("tile_get_shapes", get_current_tile());
			undo_redo->add_undo_method(tileset.ptr(), "tile_set_shapes", get_current_tile(), sd.duplicate());
			for (int i = 0; i < sd.size(); i++) {
				if (sd[i].get("shape") == edited_collision_shape) {
					sd.remove(i);
					break;
				}
			}
			undo_redo->add_do_method(tileset.ptr(), "tile_set_shapes", get_current_tile(), sd);
			if (tileset->tile_get_tile_mode(get_current_tile()) == TileSet::AUTO_TILE || tileset->tile_get_tile_mode(get_current_tile()) == TileSet::ATLAS_TILE) {
				undo_redo->add_do_method(tileset.ptr(), "tile_add_shape", get_current_tile(), shape, Transform2D(), false, edited_shape_coord);
			} else {
				undo_redo->add_do_method(tileset.ptr(), "tile_add_shape", get_current_tile(), shape, Transform2D());
			}
			tools[TOOL_SELECT]->set_pressed(true);
			undo_redo->add_do_method(this, "_select_edited_shape_coord");
			undo_redo->add_undo_method(this, "_select_edited_shape_coord");
			undo_redo->commit_action();
		} else {
			tools[TOOL_SELECT]->set_pressed(true);
			workspace->update();
		}
	} else if (edit_mode == EDITMODE_OCCLUSION) {
		Ref<OccluderPolygon2D> shape = memnew(OccluderPolygon2D);

		PoolVector<Vector2> polygon;
		polygon.resize(current_shape.size());
		PoolVector<Vector2>::Write w = polygon.write();

		for (int i = 0; i < current_shape.size(); i++) {
			w[i] = current_shape[i] - shape_anchor;
		}

		w.release();
		shape->set_polygon(polygon);

		undo_redo->create_action(TTR("Create Occlusion Polygon"));
		if (tileset->tile_get_tile_mode(get_current_tile()) == TileSet::AUTO_TILE || tileset->tile_get_tile_mode(get_current_tile()) == TileSet::ATLAS_TILE) {
			undo_redo->add_do_method(tileset.ptr(), "autotile_set_light_occluder", get_current_tile(), shape, edited_shape_coord);
			undo_redo->add_undo_method(tileset.ptr(), "autotile_set_light_occluder", get_current_tile(), tileset->autotile_get_light_occluder(get_current_tile(), edited_shape_coord), edited_shape_coord);
		} else {
			undo_redo->add_do_method(tileset.ptr(), "tile_set_light_occluder", get_current_tile(), shape);
			undo_redo->add_undo_method(tileset.ptr(), "tile_set_light_occluder", get_current_tile(), tileset->tile_get_light_occluder(get_current_tile()));
		}
		tools[TOOL_SELECT]->set_pressed(true);
		undo_redo->add_do_method(this, "_select_edited_shape_coord");
		undo_redo->add_undo_method(this, "_select_edited_shape_coord");
		undo_redo->commit_action();
	} else if (edit_mode == EDITMODE_NAVIGATION) {
		Ref<NavigationPolygon> shape = memnew(NavigationPolygon);

		PoolVector<Vector2> polygon;
		Vector<int> indices;
		polygon.resize(current_shape.size());
		PoolVector<Vector2>::Write w = polygon.write();

		for (int i = 0; i < current_shape.size(); i++) {
			w[i] = current_shape[i] - shape_anchor;
			indices.push_back(i);
		}

		w.release();
		shape->clear_outlines();
		shape->add_outline(polygon);
		shape->make_polygons_from_outlines();

		undo_redo->create_action(TTR("Create Navigation Polygon"));
		if (tileset->tile_get_tile_mode(get_current_tile()) == TileSet::AUTO_TILE || tileset->tile_get_tile_mode(get_current_tile()) == TileSet::ATLAS_TILE) {
			undo_redo->add_do_method(tileset.ptr(), "autotile_set_navigation_polygon", get_current_tile(), shape, edited_shape_coord);
			undo_redo->add_undo_method(tileset.ptr(), "autotile_set_navigation_polygon", get_current_tile(), tileset->autotile_get_navigation_polygon(get_current_tile(), edited_shape_coord), edited_shape_coord);
		} else {
			undo_redo->add_do_method(tileset.ptr(), "tile_set_navigation_polygon", get_current_tile(), shape);
			undo_redo->add_undo_method(tileset.ptr(), "tile_set_navigation_polygon", get_current_tile(), tileset->tile_get_navigation_polygon(get_current_tile()));
		}
		tools[TOOL_SELECT]->set_pressed(true);
		undo_redo->add_do_method(this, "_select_edited_shape_coord");
		undo_redo->add_undo_method(this, "_select_edited_shape_coord");
		undo_redo->commit_action();
	}
	tileset->_change_notify("");
}

void TileSetEditor::select_coord(const Vector2 &coord) {
	_update_tile_data();
	current_shape = PoolVector2Array();
	if (get_current_tile() == -1) {
		return;
	}
	Rect2 current_tile_region = tileset->tile_get_region(get_current_tile());
	current_tile_region.position += WORKSPACE_MARGIN;
	if (tileset->tile_get_tile_mode(get_current_tile()) == TileSet::SINGLE_TILE) {
		if (edited_collision_shape != tileset->tile_get_shape(get_current_tile(), 0)) {
			_set_edited_collision_shape(tileset->tile_get_shape(get_current_tile(), 0));
		}
		if (edited_occlusion_shape != tileset->tile_get_light_occluder(get_current_tile())) {
			edited_occlusion_shape = tileset->tile_get_light_occluder(get_current_tile());
		}
		if (edited_navigation_shape != tileset->tile_get_navigation_polygon(get_current_tile())) {
			edited_navigation_shape = tileset->tile_get_navigation_polygon(get_current_tile());
		}

		if (edit_mode == EDITMODE_COLLISION) {
			current_shape.resize(0);
			if (edited_collision_shape.is_valid()) {
				for (int i = 0; i < _get_edited_shape_points().size(); i++) {
					current_shape.push_back(_get_edited_shape_points()[i] + current_tile_region.position);
				}
			}
		} else if (edit_mode == EDITMODE_OCCLUSION) {
			current_shape.resize(0);
			if (edited_occlusion_shape.is_valid()) {
				for (int i = 0; i < edited_occlusion_shape->get_polygon().size(); i++) {
					current_shape.push_back(edited_occlusion_shape->get_polygon()[i] + current_tile_region.position);
				}
			}
		} else if (edit_mode == EDITMODE_NAVIGATION) {
			current_shape.resize(0);
			if (edited_navigation_shape.is_valid()) {
				for (int outline_idx = 0; outline_idx < edited_navigation_shape->get_outline_count(); outline_idx++) {
					PoolVector<Vector2> outline_vertices = edited_navigation_shape->get_outline(outline_idx);
					for (int vertex_inx = 0; vertex_inx < outline_vertices.size(); vertex_inx++) {
						current_shape.push_back(outline_vertices[vertex_inx] + current_tile_region.position);
					}
				}
			}
		}
	} else {
		Vector<TileSet::ShapeData> sd = tileset->tile_get_shapes(get_current_tile());
		bool found_collision_shape = false;
		for (int i = 0; i < sd.size(); i++) {
			if (sd[i].autotile_coord == coord) {
				if (edited_collision_shape != sd[i].shape) {
					_set_edited_collision_shape(sd[i].shape);
				}
				found_collision_shape = true;
				break;
			}
		}
		if (!found_collision_shape) {
			_set_edited_collision_shape(Ref<ConvexPolygonShape2D>(nullptr));
		}
		if (edited_occlusion_shape != tileset->autotile_get_light_occluder(get_current_tile(), coord)) {
			edited_occlusion_shape = tileset->autotile_get_light_occluder(get_current_tile(), coord);
		}
		if (edited_navigation_shape != tileset->autotile_get_navigation_polygon(get_current_tile(), coord)) {
			edited_navigation_shape = tileset->autotile_get_navigation_polygon(get_current_tile(), coord);
		}

		int spacing = tileset->autotile_get_spacing(get_current_tile());
		Vector2 size = tileset->autotile_get_size(get_current_tile());
		Vector2 shape_anchor = coord;
		shape_anchor.x *= (size.x + spacing);
		shape_anchor.y *= (size.y + spacing);
		shape_anchor += current_tile_region.position;
		if (edit_mode == EDITMODE_COLLISION) {
			current_shape.resize(0);
			if (edited_collision_shape.is_valid()) {
				for (int j = 0; j < _get_edited_shape_points().size(); j++) {
					current_shape.push_back(_get_edited_shape_points()[j] + shape_anchor);
				}
			}
		} else if (edit_mode == EDITMODE_OCCLUSION) {
			current_shape.resize(0);
			if (edited_occlusion_shape.is_valid()) {
				for (int i = 0; i < edited_occlusion_shape->get_polygon().size(); i++) {
					current_shape.push_back(edited_occlusion_shape->get_polygon()[i] + shape_anchor);
				}
			}
		} else if (edit_mode == EDITMODE_NAVIGATION) {
			current_shape.resize(0);
			if (edited_navigation_shape.is_valid()) {
				for (int outline_idx = 0; outline_idx < edited_navigation_shape->get_outline_count(); outline_idx++) {
					PoolVector<Vector2> outline_vertices = edited_navigation_shape->get_outline(outline_idx);
					for (int vertex_inx = 0; vertex_inx < outline_vertices.size(); vertex_inx++) {
						current_shape.push_back(outline_vertices[vertex_inx] + shape_anchor);
					}
				}
			}
		}
	}
	workspace->update();
	workspace_container->update();
	helper->_change_notify("");
}

Vector2 TileSetEditor::snap_point(const Vector2 &point) {
	Vector2 p = point;
	Vector2 coord = edited_shape_coord;
	Vector2 tile_size = tileset->autotile_get_size(get_current_tile());
	int spacing = tileset->autotile_get_spacing(get_current_tile());
	Vector2 anchor = coord;
	anchor.x *= (tile_size.x + spacing);
	anchor.y *= (tile_size.y + spacing);
	anchor += tileset->tile_get_region(get_current_tile()).position;
	anchor += WORKSPACE_MARGIN;
	Rect2 region(anchor, tile_size);
	Rect2 tile_region(tileset->tile_get_region(get_current_tile()).position + WORKSPACE_MARGIN, tileset->tile_get_region(get_current_tile()).size);
	if (tileset->tile_get_tile_mode(get_current_tile()) == TileSet::SINGLE_TILE) {
		region.position = tileset->tile_get_region(get_current_tile()).position + WORKSPACE_MARGIN;
		region.size = tileset->tile_get_region(get_current_tile()).size;
	}

	if (tools[TOOL_GRID_SNAP]->is_pressed()) {
		p.x = Math::snap_scalar_separation(snap_offset.x, snap_step.x, p.x, snap_separation.x);
		p.y = Math::snap_scalar_separation(snap_offset.y, snap_step.y, p.y, snap_separation.y);
	}

	if (tools[SHAPE_KEEP_INSIDE_TILE]->is_pressed()) {
		if (p.x < region.position.x) {
			p.x = region.position.x;
		}
		if (p.y < region.position.y) {
			p.y = region.position.y;
		}
		if (p.x > region.position.x + region.size.x) {
			p.x = region.position.x + region.size.x;
		}
		if (p.y > region.position.y + region.size.y) {
			p.y = region.position.y + region.size.y;
		}
	}

	if (p.x < tile_region.position.x) {
		p.x = tile_region.position.x;
	}
	if (p.y < tile_region.position.y) {
		p.y = tile_region.position.y;
	}
	if (p.x > (tile_region.position.x + tile_region.size.x)) {
		p.x = (tile_region.position.x + tile_region.size.x);
	}
	if (p.y > (tile_region.position.y + tile_region.size.y)) {
		p.y = (tile_region.position.y + tile_region.size.y);
	}

	return p;
}

void TileSetEditor::add_texture(Ref<Texture> p_texture) {
	texture_list->add_item(p_texture->get_path().get_file());
	texture_map.insert(p_texture->get_path(), p_texture);
	texture_list->set_item_metadata(texture_list->get_item_count() - 1, p_texture->get_path());
}

void TileSetEditor::remove_texture(Ref<Texture> p_texture) {
	texture_list->remove_item(texture_list->find_metadata(p_texture->get_path()));
	texture_map.erase(p_texture->get_path());

	_validate_current_tile_id();

	if (!get_current_texture().is_valid()) {
		_on_texture_list_selected(-1);
		workspace_overlay->update();
	}
}

void TileSetEditor::update_texture_list() {
	Ref<Texture> selected_texture = get_current_texture();

	helper->set_tileset(tileset);

	List<int> ids;
	tileset->get_tile_list(&ids);
	Vector<int> ids_to_remove;
	for (List<int>::Element *E = ids.front(); E; E = E->next()) {
		// Clear tiles referencing gone textures (user has been already given the chance to fix broken deps)
		if (!tileset->tile_get_texture(E->get()).is_valid()) {
			ids_to_remove.push_back(E->get());
			ERR_CONTINUE(!tileset->tile_get_texture(E->get()).is_valid());
		}

		if (!texture_map.has(tileset->tile_get_texture(E->get())->get_path())) {
			add_texture(tileset->tile_get_texture(E->get()));
		}
	}
	for (int i = 0; i < ids_to_remove.size(); i++) {
		tileset->remove_tile(ids_to_remove[i]);
	}

	if (texture_list->get_item_count() > 0 && selected_texture.is_valid()) {
		texture_list->select(texture_list->find_metadata(selected_texture->get_path()));
		if (texture_list->get_selected_items().size() > 0) {
			_on_texture_list_selected(texture_list->get_selected_items()[0]);
		}
	} else if (get_current_texture().is_valid()) {
		_on_texture_list_selected(texture_list->find_metadata(get_current_texture()->get_path()));
	} else {
		_validate_current_tile_id();
		_on_texture_list_selected(-1);
		workspace_overlay->update();
	}
	update_texture_list_icon();
	helper->_change_notify("");
}

void TileSetEditor::update_texture_list_icon() {
	for (int current_idx = 0; current_idx < texture_list->get_item_count(); current_idx++) {
		String path = texture_list->get_item_metadata(current_idx);
		texture_list->set_item_icon(current_idx, texture_map[path]);
		Size2 texture_size = texture_map[path]->get_size();
		texture_list->set_item_icon_region(current_idx, Rect2(0, 0, MIN(texture_size.x, 150), MIN(texture_size.y, 100)));
	}
	texture_list->update();
}

void TileSetEditor::update_workspace_tile_mode() {
	if (!get_current_texture().is_valid()) {
		tool_workspacemode[WORKSPACE_EDIT]->set_pressed(true);
		workspace_mode = WORKSPACE_EDIT;
		for (int i = 1; i < WORKSPACE_MODE_MAX; i++) {
			tool_workspacemode[i]->set_disabled(true);
		}
		tools[SELECT_NEXT]->set_disabled(true);
		tools[SELECT_PREVIOUS]->set_disabled(true);

		tools[ZOOM_OUT]->hide();
		tools[ZOOM_1]->hide();
		tools[ZOOM_IN]->hide();
		tools[VISIBLE_INFO]->hide();

		scroll->hide();
		empty_message->show();
	} else {
		for (int i = 1; i < WORKSPACE_MODE_MAX; i++) {
			tool_workspacemode[i]->set_disabled(false);
		}
		tools[SELECT_NEXT]->set_disabled(false);
		tools[SELECT_PREVIOUS]->set_disabled(false);

		tools[ZOOM_OUT]->show();
		tools[ZOOM_1]->show();
		tools[ZOOM_IN]->show();
		tools[VISIBLE_INFO]->show();

		scroll->show();
		empty_message->hide();
	}

	if (workspace_mode != WORKSPACE_EDIT) {
		for (int i = 0; i < EDITMODE_MAX; i++) {
			tool_editmode[i]->hide();
		}
		tool_editmode[EDITMODE_REGION]->show();
		tool_editmode[EDITMODE_REGION]->set_pressed(true);
		_on_edit_mode_changed(EDITMODE_REGION);
		separator_editmode->show();
		return;
	}

	if (get_current_tile() < 0) {
		for (int i = 0; i < EDITMODE_MAX; i++) {
			tool_editmode[i]->hide();
		}
		for (int i = TOOL_SELECT; i < ZOOM_OUT; i++) {
			tools[i]->hide();
		}

		separator_editmode->hide();
		separator_bitmask->hide();
		separator_delete->hide();
		separator_grid->hide();
		return;
	}

	for (int i = 0; i < EDITMODE_MAX; i++) {
		tool_editmode[i]->show();
	}
	separator_editmode->show();

	if (tileset->tile_get_tile_mode(get_current_tile()) == TileSet::SINGLE_TILE) {
		if (tool_editmode[EDITMODE_ICON]->is_pressed() || tool_editmode[EDITMODE_PRIORITY]->is_pressed() || tool_editmode[EDITMODE_BITMASK]->is_pressed() || tool_editmode[EDITMODE_Z_INDEX]->is_pressed()) {
			tool_editmode[EDITMODE_COLLISION]->set_pressed(true);
			edit_mode = EDITMODE_COLLISION;
		}
		select_coord(Vector2(0, 0));

		tool_editmode[EDITMODE_ICON]->hide();
		tool_editmode[EDITMODE_BITMASK]->hide();
		tool_editmode[EDITMODE_PRIORITY]->hide();
		tool_editmode[EDITMODE_Z_INDEX]->hide();
	} else if (tileset->tile_get_tile_mode(get_current_tile()) == TileSet::AUTO_TILE) {
		if (edit_mode == EDITMODE_ICON) {
			select_coord(tileset->autotile_get_icon_coordinate(get_current_tile()));
		} else {
			_select_edited_shape_coord();
		}
	} else if (tileset->tile_get_tile_mode(get_current_tile()) == TileSet::ATLAS_TILE) {
		if (tool_editmode[EDITMODE_PRIORITY]->is_pressed() || tool_editmode[EDITMODE_BITMASK]->is_pressed()) {
			tool_editmode[EDITMODE_COLLISION]->set_pressed(true);
			edit_mode = EDITMODE_COLLISION;
		}
		if (edit_mode == EDITMODE_ICON) {
			select_coord(tileset->autotile_get_icon_coordinate(get_current_tile()));
		} else {
			_select_edited_shape_coord();
		}

		tool_editmode[EDITMODE_BITMASK]->hide();
	}
	_on_edit_mode_changed(edit_mode);
}

void TileSetEditor::update_workspace_minsize() {
	Size2 workspace_min_size = get_current_texture()->get_size();
	String current_texture_path = get_current_texture()->get_path();
	List<int> tiles;
	tileset->get_tile_list(&tiles);
	for (List<int>::Element *E = tiles.front(); E; E = E->next()) {
		if (tileset->tile_get_texture(E->get())->get_path() != current_texture_path) {
			continue;
		}

		Rect2i region = tileset->tile_get_region(E->get());
		if (region.position.x + region.size.x > workspace_min_size.x) {
			workspace_min_size.x = region.position.x + region.size.x;
		}
		if (region.position.y + region.size.y > workspace_min_size.y) {
			workspace_min_size.y = region.position.y + region.size.y;
		}
	}

	workspace_container->set_custom_minimum_size(workspace_min_size * workspace->get_scale() + WORKSPACE_MARGIN * 2);
	workspace_overlay->set_custom_minimum_size(workspace_min_size * workspace->get_scale() + WORKSPACE_MARGIN * 2);
	// Make sure workspace size is initialized last (otherwise it might be incorrect).
	workspace->call_deferred("set_custom_minimum_size", workspace_min_size + WORKSPACE_MARGIN * 2);
}

void TileSetEditor::update_edited_region(const Vector2 &end_point) {
	edited_region = Rect2(region_from, Size2());
	if (tools[TOOL_GRID_SNAP]->is_pressed()) {
		Vector2 grid_coord;
		grid_coord = ((region_from - snap_offset) / (snap_step + snap_separation)).floor();
		grid_coord *= (snap_step + snap_separation);
		grid_coord += snap_offset;
		edited_region.expand_to(grid_coord);
		grid_coord += snap_step;
		edited_region.expand_to(grid_coord);

		grid_coord = ((end_point - snap_offset) / (snap_step + snap_separation)).floor();
		grid_coord *= (snap_step + snap_separation);
		grid_coord += snap_offset;
		edited_region.expand_to(grid_coord);
		grid_coord += snap_step;
		edited_region.expand_to(grid_coord);
	} else {
		edited_region.expand_to(end_point);
	}
}

int TileSetEditor::get_current_tile() const {
	return current_tile;
}

void TileSetEditor::set_current_tile(int p_id) {
	if (current_tile != p_id) {
		current_tile = p_id;
		helper->_change_notify("");
		select_coord(Vector2(0, 0));
		update_workspace_tile_mode();
		if (p_id == -1) {
			editor->get_inspector()->edit(tileset.ptr());
		} else {
			editor->get_inspector()->edit(helper);
		}
	}
}

Ref<Texture> TileSetEditor::get_current_texture() {
	if (texture_list->get_selected_items().size() == 0) {
		return Ref<Texture>();
	} else {
		return texture_map[texture_list->get_item_metadata(texture_list->get_selected_items()[0])];
	}
}

void TilesetEditorContext::set_tileset(const Ref<TileSet> &p_tileset) {
	tileset = p_tileset;
}

void TilesetEditorContext::set_snap_options_visible(bool p_visible) {
	snap_options_visible = p_visible;
	_change_notify("");
}

bool TilesetEditorContext::_set(const StringName &p_name, const Variant &p_value) {
	String name = p_name.operator String();

	if (name == "options_offset") {
		Vector2 snap = p_value;
		tileset_editor->_set_snap_off(snap + WORKSPACE_MARGIN);
		return true;
	} else if (name == "options_step") {
		Vector2 snap = p_value;
		tileset_editor->_set_snap_step(snap);
		return true;
	} else if (name == "options_separation") {
		Vector2 snap = p_value;
		tileset_editor->_set_snap_sep(snap);
		return true;
	} else if (p_name.operator String().left(5) == "tile_") {
		String name2 = p_name.operator String().right(5);
		bool v = false;

		if (tileset_editor->get_current_tile() < 0 || tileset.is_null()) {
			return false;
		}

		if (name2 == "autotile_bitmask_mode") {
			tileset->set(String::num(tileset_editor->get_current_tile(), 0) + "/autotile/bitmask_mode", p_value, &v);
		} else if (name2 == "subtile_size") {
			tileset->set(String::num(tileset_editor->get_current_tile(), 0) + "/autotile/tile_size", p_value, &v);
		} else if (name2 == "subtile_spacing") {
			tileset->set(String::num(tileset_editor->get_current_tile(), 0) + "/autotile/spacing", p_value, &v);
		} else if (name2 == "autotile_fallback_mode") {
			tileset->set(String::num(tileset_editor->get_current_tile(), 0) + "/autotile/fallback_mode", p_value, &v);
		} else {
			tileset->set(String::num(tileset_editor->get_current_tile(), 0) + "/" + name2, p_value, &v);
		}
		if (v) {
			tileset->_change_notify("");
			tileset_editor->workspace->update();
			tileset_editor->workspace_overlay->update();
		}
		return v;
	} else if (name == "tileset_script") {
		tileset->set_script(p_value);
		return true;
	} else if (name == "selected_collision_one_way") {
		Vector<TileSet::ShapeData> sd = tileset->tile_get_shapes(tileset_editor->get_current_tile());
		for (int index = 0; index < sd.size(); index++) {
			if (sd[index].shape == tileset_editor->edited_collision_shape) {
				tileset->tile_set_shape_one_way(tileset_editor->get_current_tile(), index, p_value);
				return true;
			}
		}
		return false;
	} else if (name == "selected_collision_one_way_margin") {
		Vector<TileSet::ShapeData> sd = tileset->tile_get_shapes(tileset_editor->get_current_tile());
		for (int index = 0; index < sd.size(); index++) {
			if (sd[index].shape == tileset_editor->edited_collision_shape) {
				tileset->tile_set_shape_one_way_margin(tileset_editor->get_current_tile(), index, p_value);
				return true;
			}
		}
		return false;
	}

	tileset_editor->err_dialog->set_text(TTR("This property can't be changed."));
	tileset_editor->err_dialog->popup_centered(Size2(300, 60));
	return false;
}

bool TilesetEditorContext::_get(const StringName &p_name, Variant &r_ret) const {
	String name = p_name.operator String();
	bool v = false;

	if (name == "options_offset") {
		r_ret = tileset_editor->snap_offset - WORKSPACE_MARGIN;
		v = true;
	} else if (name == "options_step") {
		r_ret = tileset_editor->snap_step;
		v = true;
	} else if (name == "options_separation") {
		r_ret = tileset_editor->snap_separation;
		v = true;
	} else if (name.left(5) == "tile_") {
		name = name.right(5);

		if (tileset_editor->get_current_tile() < 0 || tileset.is_null()) {
			return false;
		}
		if (!tileset->has_tile(tileset_editor->get_current_tile())) {
			return false;
		}

		if (name == "autotile_bitmask_mode") {
			r_ret = tileset->get(String::num(tileset_editor->get_current_tile(), 0) + "/autotile/bitmask_mode", &v);
		} else if (name == "subtile_size") {
			r_ret = tileset->get(String::num(tileset_editor->get_current_tile(), 0) + "/autotile/tile_size", &v);
		} else if (name == "subtile_spacing") {
			r_ret = tileset->get(String::num(tileset_editor->get_current_tile(), 0) + "/autotile/spacing", &v);
		} else if (name == "autotile_fallback_mode") {
			r_ret = tileset->get(String::num(tileset_editor->get_current_tile(), 0) + "/autotile/fallback_mode", &v);
		} else {
			r_ret = tileset->get(String::num(tileset_editor->get_current_tile(), 0) + "/" + name, &v);
		}
		return v;
	} else if (name == "selected_collision") {
		r_ret = tileset_editor->edited_collision_shape;
		v = true;
	} else if (name == "selected_collision_one_way") {
		Vector<TileSet::ShapeData> sd = tileset->tile_get_shapes(tileset_editor->get_current_tile());
		for (int index = 0; index < sd.size(); index++) {
			if (sd[index].shape == tileset_editor->edited_collision_shape) {
				r_ret = sd[index].one_way_collision;
				v = true;
				break;
			}
		}
	} else if (name == "selected_collision_one_way_margin") {
		Vector<TileSet::ShapeData> sd = tileset->tile_get_shapes(tileset_editor->get_current_tile());
		for (int index = 0; index < sd.size(); index++) {
			if (sd[index].shape == tileset_editor->edited_collision_shape) {
				r_ret = sd[index].one_way_collision_margin;
				v = true;
				break;
			}
		}
	} else if (name == "selected_navigation") {
		r_ret = tileset_editor->edited_navigation_shape;
		v = true;
	} else if (name == "selected_occlusion") {
		r_ret = tileset_editor->edited_occlusion_shape;
		v = true;
	} else if (name == "tileset_script") {
		r_ret = tileset->get_script();
		v = true;
	}
	return v;
}

void TilesetEditorContext::_get_property_list(List<PropertyInfo> *p_list) const {
	if (snap_options_visible) {
		p_list->push_back(PropertyInfo(Variant::NIL, GNAME("Snap Options", "options_"), PROPERTY_HINT_NONE, "options_", PROPERTY_USAGE_GROUP));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, PNAME("options_offset")));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, PNAME("options_step")));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, PNAME("options_separation")));
	}
	if (tileset_editor->edit_mode == TileSetEditor::EDITMODE_COLLISION && tileset_editor->edited_collision_shape.is_valid()) {
		p_list->push_back(PropertyInfo(Variant::OBJECT, PNAME("selected_collision"), PROPERTY_HINT_RESOURCE_TYPE, tileset_editor->edited_collision_shape->get_class()));
		if (tileset_editor->edited_collision_shape.is_valid()) {
			p_list->push_back(PropertyInfo(Variant::NIL, GNAME("Collision Options", "selected_collision_"), PROPERTY_HINT_NONE, "selected_collision_", PROPERTY_USAGE_GROUP));
			p_list->push_back(PropertyInfo(Variant::BOOL, PNAME("selected_collision_one_way"), PROPERTY_HINT_NONE));
			p_list->push_back(PropertyInfo(Variant::REAL, PNAME("selected_collision_one_way_margin"), PROPERTY_HINT_NONE));
		}
	}
	if (tileset_editor->get_current_tile() >= 0 && !tileset.is_null()) {
		int id = tileset_editor->get_current_tile();
		p_list->push_back(PropertyInfo(Variant::NIL, GNAME("Selected Tile", "tile_"), PROPERTY_HINT_NONE, "tile_", PROPERTY_USAGE_GROUP));
		p_list->push_back(PropertyInfo(Variant::STRING, PNAME("tile_name")));
		p_list->push_back(PropertyInfo(Variant::OBJECT, PNAME("tile_texture"), PROPERTY_HINT_RESOURCE_TYPE, "Texture"));
		p_list->push_back(PropertyInfo(Variant::OBJECT, PNAME("tile_normal_map"), PROPERTY_HINT_RESOURCE_TYPE, "Texture"));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, PNAME("tile_tex_offset")));
		p_list->push_back(PropertyInfo(Variant::OBJECT, PNAME("tile_material"), PROPERTY_HINT_RESOURCE_TYPE, "ShaderMaterial"));
		p_list->push_back(PropertyInfo(Variant::COLOR, PNAME("tile_modulate")));
		p_list->push_back(PropertyInfo(Variant::INT, PNAME("tile_tile_mode"), PROPERTY_HINT_ENUM, "SINGLE_TILE,AUTO_TILE,ATLAS_TILE"));
		if (tileset->tile_get_tile_mode(id) == TileSet::AUTO_TILE) {
			p_list->push_back(PropertyInfo(Variant::INT, PNAME("tile_autotile_bitmask_mode"), PROPERTY_HINT_ENUM, "2x2,3x3 (minimal),3x3"));
			p_list->push_back(PropertyInfo(Variant::VECTOR2, PNAME("tile_subtile_size")));
			p_list->push_back(PropertyInfo(Variant::INT, PNAME("tile_subtile_spacing"), PROPERTY_HINT_RANGE, "0, 1024, 1"));
			p_list->push_back(PropertyInfo(Variant::INT, PNAME("tile_autotile_fallback_mode"), PROPERTY_HINT_ENUM, "Auto,Icon"));
		} else if (tileset->tile_get_tile_mode(id) == TileSet::ATLAS_TILE) {
			p_list->push_back(PropertyInfo(Variant::VECTOR2, PNAME("tile_subtile_size")));
			p_list->push_back(PropertyInfo(Variant::INT, PNAME("tile_subtile_spacing"), PROPERTY_HINT_RANGE, "0, 1024, 1"));
		}
		p_list->push_back(PropertyInfo(Variant::VECTOR2, PNAME("tile_occluder_offset")));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, PNAME("tile_navigation_offset")));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, PNAME("tile_shape_offset"), PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, PNAME("tile_shape_transform"), PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR));
		p_list->push_back(PropertyInfo(Variant::INT, PNAME("tile_z_index"), PROPERTY_HINT_RANGE, itos(VS::CANVAS_ITEM_Z_MIN) + "," + itos(VS::CANVAS_ITEM_Z_MAX) + ",1"));
	}
	if (tileset_editor->edit_mode == TileSetEditor::EDITMODE_NAVIGATION && tileset_editor->edited_navigation_shape.is_valid()) {
		p_list->push_back(PropertyInfo(Variant::OBJECT, PNAME("selected_navigation"), PROPERTY_HINT_RESOURCE_TYPE, tileset_editor->edited_navigation_shape->get_class()));
	}
	if (tileset_editor->edit_mode == TileSetEditor::EDITMODE_OCCLUSION && tileset_editor->edited_occlusion_shape.is_valid()) {
		p_list->push_back(PropertyInfo(Variant::OBJECT, PNAME("selected_occlusion"), PROPERTY_HINT_RESOURCE_TYPE, tileset_editor->edited_occlusion_shape->get_class()));
	}
	if (!tileset.is_null()) {
		p_list->push_back(PropertyInfo(Variant::NIL, GNAME("TileSet", "tileset"), PROPERTY_HINT_NONE, "tileset", PROPERTY_USAGE_CATEGORY));
		p_list->push_back(PropertyInfo(Variant::OBJECT, PNAME("tileset_script"), PROPERTY_HINT_RESOURCE_TYPE, "Script"));
	}
}

void TilesetEditorContext::_bind_methods() {
	ClassDB::bind_method("_hide_script_from_inspector", &TilesetEditorContext::_hide_script_from_inspector);
}

TilesetEditorContext::TilesetEditorContext(TileSetEditor *p_tileset_editor) {
	tileset_editor = p_tileset_editor;
	snap_options_visible = false;
}

void TileSetEditorPlugin::edit(Object *p_node) {
	if (Object::cast_to<TileSet>(p_node)) {
		tileset_editor->edit(Object::cast_to<TileSet>(p_node));
	}
}

bool TileSetEditorPlugin::handles(Object *p_node) const {
	return p_node->is_class("TileSet") || p_node->is_class("TilesetEditorContext");
}

void TileSetEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		tileset_editor_button->show();
		editor->make_bottom_panel_item_visible(tileset_editor);
		if (!get_tree()->is_connected("idle_frame", tileset_editor, "_on_workspace_process")) {
			get_tree()->connect("idle_frame", tileset_editor, "_on_workspace_process");
		}
	} else {
		editor->hide_bottom_panel();
		tileset_editor_button->hide();
		if (get_tree()->is_connected("idle_frame", tileset_editor, "_on_workspace_process")) {
			get_tree()->disconnect("idle_frame", tileset_editor, "_on_workspace_process");
		}
	}
}

Dictionary TileSetEditorPlugin::get_state() const {
	Dictionary state;
	state["snap_offset"] = tileset_editor->snap_offset;
	state["snap_step"] = tileset_editor->snap_step;
	state["snap_separation"] = tileset_editor->snap_separation;
	state["snap_enabled"] = tileset_editor->tools[TileSetEditor::TOOL_GRID_SNAP]->is_pressed();
	state["keep_inside_tile"] = tileset_editor->tools[TileSetEditor::SHAPE_KEEP_INSIDE_TILE]->is_pressed();
	state["show_information"] = tileset_editor->tools[TileSetEditor::VISIBLE_INFO]->is_pressed();
	return state;
}

void TileSetEditorPlugin::set_state(const Dictionary &p_state) {
	Dictionary state = p_state;
	if (state.has("snap_step")) {
		tileset_editor->_set_snap_step(state["snap_step"]);
	}

	if (state.has("snap_offset")) {
		tileset_editor->_set_snap_off(state["snap_offset"]);
	}

	if (state.has("snap_separation")) {
		tileset_editor->_set_snap_sep(state["snap_separation"]);
	}

	if (state.has("snap_enabled")) {
		tileset_editor->tools[TileSetEditor::TOOL_GRID_SNAP]->set_pressed(state["snap_enabled"]);
		if (tileset_editor->helper) {
			tileset_editor->_on_grid_snap_toggled(state["snap_enabled"]);
		}
	}

	if (state.has("keep_inside_tile")) {
		tileset_editor->tools[TileSetEditor::SHAPE_KEEP_INSIDE_TILE]->set_pressed(state["keep_inside_tile"]);
	}

	if (state.has("show_information")) {
		tileset_editor->tools[TileSetEditor::VISIBLE_INFO]->set_pressed(state["show_information"]);
	}
}

TileSetEditorPlugin::TileSetEditorPlugin(EditorNode *p_node) {
	editor = p_node;
	tileset_editor = memnew(TileSetEditor(p_node));

	tileset_editor->set_custom_minimum_size(Size2(0, 200) * EDSCALE);
	tileset_editor->hide();

	tileset_editor_button = p_node->add_bottom_panel_item(TTR("TileSet"), tileset_editor);
	tileset_editor_button->hide();
}

/**************************************************************************/
/*  grid_map_editor_plugin.cpp                                            */
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

#include "grid_map_editor_plugin.h"

#ifdef TOOLS_ENABLED

#include "core/os/keyboard.h"
#include "editor/editor_command_palette.h"
#include "editor/editor_main_screen.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/editor_bottom_panel.h"
#include "editor/gui/editor_zoom_widget.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "editor/themes/editor_scale.h"
#include "scene/3d/camera_3d.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/label.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/separator.h"
#include "scene/main/window.h"

void GridMapEditor::_configure() {
	if (!node) {
		return;
	}

	update_grid();
}

void GridMapEditor::_menu_option(int p_option) {
	switch (p_option) {
		case MENU_OPTION_PREV_LEVEL: {
			floor->set_value(floor->get_value() - 1);
			if (selection.active && input_action == INPUT_SELECT) {
				selection.current[edit_axis]--;
				_validate_selection();
			}
		} break;

		case MENU_OPTION_NEXT_LEVEL: {
			floor->set_value(floor->get_value() + 1);
			if (selection.active && input_action == INPUT_SELECT) {
				selection.current[edit_axis]++;
				_validate_selection();
			}
		} break;

		case MENU_OPTION_X_AXIS:
		case MENU_OPTION_Y_AXIS:
		case MENU_OPTION_Z_AXIS: {
			int new_axis = p_option - MENU_OPTION_X_AXIS;
			for (int i = 0; i < 3; i++) {
				int idx = options->get_popup()->get_item_index(MENU_OPTION_X_AXIS + i);
				options->get_popup()->set_item_checked(idx, i == new_axis);
			}

			if (edit_axis != new_axis) {
				if (edit_axis == Vector3::AXIS_Y) {
					floor->set_tooltip_text("Change Grid Plane");
				} else if (new_axis == Vector3::AXIS_Y) {
					floor->set_tooltip_text("Change Grid Floor");
				}
			}
			edit_axis = Vector3::Axis(new_axis);
			update_grid();

		} break;
		case MENU_OPTION_CURSOR_ROTATE_Y: {
			Basis r;
			if (input_action == INPUT_PASTE) {
				r = node->get_basis_with_orthogonal_index(paste_indicator.orientation);
				r.rotate(Vector3(0, 1, 0), -Math_PI / 2.0);
				paste_indicator.orientation = node->get_orthogonal_index_from_basis(r);
				_update_paste_indicator();
				break;
			}

			r = node->get_basis_with_orthogonal_index(cursor_rot);
			r.rotate(Vector3(0, 1, 0), -Math_PI / 2.0);
			cursor_rot = node->get_orthogonal_index_from_basis(r);
			_update_cursor_transform();
		} break;
		case MENU_OPTION_CURSOR_ROTATE_X: {
			Basis r;
			if (input_action == INPUT_PASTE) {
				r = node->get_basis_with_orthogonal_index(paste_indicator.orientation);
				r.rotate(Vector3(1, 0, 0), -Math_PI / 2.0);
				paste_indicator.orientation = node->get_orthogonal_index_from_basis(r);
				_update_paste_indicator();
				break;
			}

			r = node->get_basis_with_orthogonal_index(cursor_rot);
			r.rotate(Vector3(1, 0, 0), -Math_PI / 2.0);
			cursor_rot = node->get_orthogonal_index_from_basis(r);
			_update_cursor_transform();
		} break;
		case MENU_OPTION_CURSOR_ROTATE_Z: {
			Basis r;
			if (input_action == INPUT_PASTE) {
				r = node->get_basis_with_orthogonal_index(paste_indicator.orientation);
				r.rotate(Vector3(0, 0, 1), -Math_PI / 2.0);
				paste_indicator.orientation = node->get_orthogonal_index_from_basis(r);
				_update_paste_indicator();
				break;
			}

			r = node->get_basis_with_orthogonal_index(cursor_rot);
			r.rotate(Vector3(0, 0, 1), -Math_PI / 2.0);
			cursor_rot = node->get_orthogonal_index_from_basis(r);
			_update_cursor_transform();
		} break;
		case MENU_OPTION_CURSOR_BACK_ROTATE_Y: {
			Basis r;
			if (input_action == INPUT_PASTE) {
				r = node->get_basis_with_orthogonal_index(paste_indicator.orientation);
				r.rotate(Vector3(0, 1, 0), Math_PI / 2.0);
				paste_indicator.orientation = node->get_orthogonal_index_from_basis(r);
				_update_paste_indicator();
				break;
			}

			r = node->get_basis_with_orthogonal_index(cursor_rot);
			r.rotate(Vector3(0, 1, 0), Math_PI / 2.0);
			cursor_rot = node->get_orthogonal_index_from_basis(r);
			_update_cursor_transform();
		} break;
		case MENU_OPTION_CURSOR_BACK_ROTATE_X: {
			Basis r;
			if (input_action == INPUT_PASTE) {
				r = node->get_basis_with_orthogonal_index(paste_indicator.orientation);
				r.rotate(Vector3(1, 0, 0), Math_PI / 2.0);
				paste_indicator.orientation = node->get_orthogonal_index_from_basis(r);
				_update_paste_indicator();
				break;
			}

			r = node->get_basis_with_orthogonal_index(cursor_rot);
			r.rotate(Vector3(1, 0, 0), Math_PI / 2.0);
			cursor_rot = node->get_orthogonal_index_from_basis(r);
			_update_cursor_transform();
		} break;
		case MENU_OPTION_CURSOR_BACK_ROTATE_Z: {
			Basis r;
			if (input_action == INPUT_PASTE) {
				r = node->get_basis_with_orthogonal_index(paste_indicator.orientation);
				r.rotate(Vector3(0, 0, 1), Math_PI / 2.0);
				paste_indicator.orientation = node->get_orthogonal_index_from_basis(r);
				_update_paste_indicator();
				break;
			}

			r = node->get_basis_with_orthogonal_index(cursor_rot);
			r.rotate(Vector3(0, 0, 1), Math_PI / 2.0);
			cursor_rot = node->get_orthogonal_index_from_basis(r);
			_update_cursor_transform();
		} break;
		case MENU_OPTION_CURSOR_CLEAR_ROTATION: {
			if (input_action == INPUT_PASTE) {
				paste_indicator.orientation = 0;
				_update_paste_indicator();
				break;
			}

			cursor_rot = 0;
			_update_cursor_transform();
		} break;

		case MENU_OPTION_PASTE_SELECTS: {
			int idx = options->get_popup()->get_item_index(MENU_OPTION_PASTE_SELECTS);
			options->get_popup()->set_item_checked(idx, !options->get_popup()->is_item_checked(idx));
		} break;

		case MENU_OPTION_SELECTION_DUPLICATE:
		case MENU_OPTION_SELECTION_CUT: {
			if (!(selection.active && input_action == INPUT_NONE)) {
				break;
			}

			_set_clipboard_data();

			if (p_option == MENU_OPTION_SELECTION_CUT) {
				_delete_selection();
			}

			input_action = INPUT_PASTE;
			paste_indicator.click = selection.begin;
			paste_indicator.current = selection.begin;
			paste_indicator.begin = selection.begin;
			paste_indicator.end = selection.end;
			paste_indicator.orientation = 0;
			_update_paste_indicator();
		} break;
		case MENU_OPTION_SELECTION_CLEAR: {
			if (!selection.active) {
				break;
			}

			_delete_selection();

		} break;
		case MENU_OPTION_SELECTION_FILL: {
			if (!selection.active) {
				return;
			}

			_fill_selection();

		} break;
		case MENU_OPTION_GRIDMAP_SETTINGS: {
			settings_dialog->popup_centered(settings_vbc->get_combined_minimum_size() + Size2(50, 50) * EDSCALE);
		} break;
	}
}

void GridMapEditor::_update_cursor_transform() {
	cursor_transform = Transform3D();
	cursor_transform.origin = cursor_origin;
	cursor_transform.basis *= node->get_cell_scale();
	cursor_transform = node->get_global_transform() * cursor_transform;

	if (mode_buttons_group->get_pressed_button() == paint_mode_button) {
		// Rotation is only applied in paint mode, we don't want the cursor box to rotate otherwise.
		cursor_transform.basis = node->get_basis_with_orthogonal_index(cursor_rot);
		if (selected_palette >= 0 && node && node->get_mesh_library().is_valid()) {
			cursor_transform *= node->get_mesh_library()->get_item_mesh_transform(selected_palette);
		}
	} else {
		Transform3D xf;
		xf.scale(node->get_cell_size());
		xf.origin.x = node->get_center_x() ? -node->get_cell_size().x / 2 : 0;
		xf.origin.y = node->get_center_y() ? -node->get_cell_size().y / 2 : 0;
		xf.origin.z = node->get_center_z() ? -node->get_cell_size().z / 2 : 0;
		cursor_transform *= xf;
	}

	if (cursor_instance.is_valid()) {
		RenderingServer::get_singleton()->instance_set_transform(cursor_instance, cursor_transform);
		RenderingServer::get_singleton()->instance_set_visible(cursor_instance, cursor_visible);
	}
}

void GridMapEditor::_update_selection_transform() {
	Transform3D xf_zero;
	xf_zero.basis.set_zero();

	if (!selection.active) {
		RenderingServer::get_singleton()->instance_set_transform(selection_instance, xf_zero);
		for (int i = 0; i < 3; i++) {
			RenderingServer::get_singleton()->instance_set_transform(selection_level_instance[i], xf_zero);
		}
		return;
	}

	Transform3D xf;
	xf.scale((Vector3(1, 1, 1) + (selection.end - selection.begin)) * node->get_cell_size());
	xf.origin = selection.begin * node->get_cell_size();

	RenderingServer::get_singleton()->instance_set_transform(selection_instance, node->get_global_transform() * xf);

	for (int i = 0; i < 3; i++) {
		if (i != edit_axis || (edit_floor[edit_axis] < selection.begin[edit_axis]) || (edit_floor[edit_axis] > selection.end[edit_axis] + 1)) {
			RenderingServer::get_singleton()->instance_set_transform(selection_level_instance[i], xf_zero);
		} else {
			Vector3 scale = (selection.end - selection.begin + Vector3(1, 1, 1));
			scale[edit_axis] = 1.0;
			Vector3 position = selection.begin;
			position[edit_axis] = edit_floor[edit_axis];

			scale *= node->get_cell_size();
			position *= node->get_cell_size();

			Transform3D xf2;
			xf2.basis.scale(scale);
			xf2.origin = position;

			RenderingServer::get_singleton()->instance_set_transform(selection_level_instance[i], node->get_global_transform() * xf2);
		}
	}
}

void GridMapEditor::_validate_selection() {
	if (!selection.active) {
		return;
	}
	selection.begin = selection.click;
	selection.end = selection.current;

	if (selection.begin.x > selection.end.x) {
		SWAP(selection.begin.x, selection.end.x);
	}
	if (selection.begin.y > selection.end.y) {
		SWAP(selection.begin.y, selection.end.y);
	}
	if (selection.begin.z > selection.end.z) {
		SWAP(selection.begin.z, selection.end.z);
	}

	_update_selection_transform();
}

void GridMapEditor::_set_selection(bool p_active, const Vector3 &p_begin, const Vector3 &p_end) {
	selection.active = p_active;
	selection.begin = p_begin;
	selection.end = p_end;
	selection.click = p_begin;
	selection.current = p_end;

	if (is_visible_in_tree()) {
		_update_selection_transform();
	}
}

AABB GridMapEditor::_get_selection() const {
	AABB ret;
	if (selection.active) {
		ret.position = selection.begin;
		ret.size = selection.end - selection.begin;
	} else {
		ret.position.zero();
		ret.size.zero();
	}
	return ret;
}

bool GridMapEditor::_has_selection() const {
	return node != nullptr && selection.active;
}

Array GridMapEditor::_get_selected_cells() const {
	Array ret;
	if (node != nullptr && selection.active) {
		for (int i = selection.begin.x; i <= selection.end.x; i++) {
			for (int j = selection.begin.y; j <= selection.end.y; j++) {
				for (int k = selection.begin.z; k <= selection.end.z; k++) {
					Vector3i selected = Vector3i(i, j, k);
					int itm = node->get_cell_item(selected);
					if (itm == GridMap::INVALID_CELL_ITEM) {
						continue;
					}
					ret.append(selected);
				}
			}
		}
	}
	return ret;
}

bool GridMapEditor::do_input_action(Camera3D *p_camera, const Point2 &p_point, bool p_click) {
	if (!spatial_editor) {
		return false;
	}
	if (input_action == INPUT_TRANSFORM) {
		return false;
	}
	if (selected_palette < 0 && input_action != INPUT_NONE && input_action != INPUT_PICK && input_action != INPUT_SELECT && input_action != INPUT_PASTE) {
		return false;
	}
	if (mesh_library.is_null()) {
		return false;
	}
	if (input_action != INPUT_NONE && input_action != INPUT_PICK && input_action != INPUT_SELECT && input_action != INPUT_PASTE && !mesh_library->has_item(selected_palette)) {
		return false;
	}

	Camera3D *camera = p_camera;
	Vector3 from = camera->project_ray_origin(p_point);
	Vector3 normal = camera->project_ray_normal(p_point);
	Transform3D local_xform = node->get_global_transform().affine_inverse();
	Vector<Plane> planes = camera->get_frustum();
	from = local_xform.xform(from);
	normal = local_xform.basis.xform(normal).normalized();

	Plane p;
	p.normal[edit_axis] = 1.0;
	p.d = edit_floor[edit_axis] * node->get_cell_size()[edit_axis];

	Vector3 inters;
	if (!p.intersects_segment(from, from + normal * settings_pick_distance->get_value(), &inters)) {
		return false;
	}

	// Make sure the intersection is inside the frustum planes, to avoid
	// Painting on invisible regions.
	for (int i = 0; i < planes.size(); i++) {
		Plane fp = local_xform.xform(planes[i]);
		if (fp.is_point_over(inters)) {
			return false;
		}
	}

	int cell[3];
	Vector3 cell_size = node->get_cell_size();

	for (int i = 0; i < 3; i++) {
		if (i == edit_axis) {
			cell[i] = edit_floor[i];
		} else {
			cell[i] = inters[i] / cell_size[i];
			if (inters[i] < 0) {
				cell[i] -= 1; // Compensate negative.
			}
			grid_ofs[i] = cell[i] * cell_size[i];
		}
	}

	RS::get_singleton()->instance_set_transform(grid_instance[edit_axis], node->get_global_transform() * edit_grid_xform);

	if (cursor_instance.is_valid()) {
		cursor_origin = (Vector3(cell[0], cell[1], cell[2]) + Vector3(0.5 * node->get_center_x(), 0.5 * node->get_center_y(), 0.5 * node->get_center_z())) * node->get_cell_size();
		cursor_visible = true;

		if (input_action == INPUT_PASTE) {
			cursor_visible = false;
		}

		_update_cursor_transform();
	}

	if (input_action == INPUT_NONE) {
		return false;
	}

	if (input_action == INPUT_PASTE) {
		paste_indicator.current = Vector3i(cell[0], cell[1], cell[2]);
		_update_paste_indicator();

	} else if (input_action == INPUT_SELECT) {
		selection.current = Vector3i(cell[0], cell[1], cell[2]);
		if (p_click) {
			selection.click = selection.current;
		}
		selection.active = true;
		_validate_selection();

		return true;
	} else if (input_action == INPUT_PICK) {
		int item = node->get_cell_item(Vector3i(cell[0], cell[1], cell[2]));
		if (item >= 0) {
			selected_palette = item;

			// Clear the filter if picked an item that's filtered out.
			int index = mesh_library_palette->find_metadata(item);
			if (index == -1) {
				search_box->clear();
			}

			// This will select `selected_palette` in the ItemList when possible.
			update_palette();

			_update_cursor_instance();
		}
		return true;
	}

	if (input_action == INPUT_PAINT) {
		SetItem si;
		si.position = Vector3i(cell[0], cell[1], cell[2]);
		si.new_value = selected_palette;
		si.new_orientation = cursor_rot;
		si.old_value = node->get_cell_item(Vector3i(cell[0], cell[1], cell[2]));
		si.old_orientation = node->get_cell_item_orientation(Vector3i(cell[0], cell[1], cell[2]));
		set_items.push_back(si);
		node->set_cell_item(Vector3i(cell[0], cell[1], cell[2]), selected_palette, cursor_rot);
		return true;
	} else if (input_action == INPUT_ERASE) {
		SetItem si;
		si.position = Vector3i(cell[0], cell[1], cell[2]);
		si.new_value = -1;
		si.new_orientation = 0;
		si.old_value = node->get_cell_item(Vector3i(cell[0], cell[1], cell[2]));
		si.old_orientation = node->get_cell_item_orientation(Vector3i(cell[0], cell[1], cell[2]));
		set_items.push_back(si);
		node->set_cell_item(Vector3i(cell[0], cell[1], cell[2]), -1);
		return true;
	}

	return false;
}

void GridMapEditor::_delete_selection() {
	if (!selection.active) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("GridMap Delete Selection"));
	for (int i = selection.begin.x; i <= selection.end.x; i++) {
		for (int j = selection.begin.y; j <= selection.end.y; j++) {
			for (int k = selection.begin.z; k <= selection.end.z; k++) {
				Vector3i selected = Vector3i(i, j, k);
				undo_redo->add_do_method(node, "set_cell_item", selected, GridMap::INVALID_CELL_ITEM);
				undo_redo->add_undo_method(node, "set_cell_item", selected, node->get_cell_item(selected), node->get_cell_item_orientation(selected));
			}
		}
	}
	undo_redo->add_do_method(this, "_set_selection", !selection.active, selection.begin, selection.end);
	undo_redo->add_undo_method(this, "_set_selection", selection.active, selection.begin, selection.end);
	undo_redo->commit_action();
}

void GridMapEditor::_fill_selection() {
	if (!selection.active) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("GridMap Fill Selection"));
	for (int i = selection.begin.x; i <= selection.end.x; i++) {
		for (int j = selection.begin.y; j <= selection.end.y; j++) {
			for (int k = selection.begin.z; k <= selection.end.z; k++) {
				Vector3i selected = Vector3i(i, j, k);
				undo_redo->add_do_method(node, "set_cell_item", selected, selected_palette, cursor_rot);
				undo_redo->add_undo_method(node, "set_cell_item", selected, node->get_cell_item(selected), node->get_cell_item_orientation(selected));
			}
		}
	}
	undo_redo->add_do_method(this, "_set_selection", !selection.active, selection.begin, selection.end);
	undo_redo->add_undo_method(this, "_set_selection", selection.active, selection.begin, selection.end);
	undo_redo->commit_action();
}

void GridMapEditor::_clear_clipboard_data() {
	for (const ClipboardItem &E : clipboard_items) {
		RenderingServer::get_singleton()->free(E.instance);
	}

	clipboard_items.clear();
}

void GridMapEditor::_set_clipboard_data() {
	_clear_clipboard_data();

	Ref<MeshLibrary> meshLibrary = node->get_mesh_library();

	for (int i = selection.begin.x; i <= selection.end.x; i++) {
		for (int j = selection.begin.y; j <= selection.end.y; j++) {
			for (int k = selection.begin.z; k <= selection.end.z; k++) {
				Vector3i selected = Vector3i(i, j, k);
				int itm = node->get_cell_item(selected);
				if (itm == GridMap::INVALID_CELL_ITEM) {
					continue;
				}

				Ref<Mesh> mesh = meshLibrary->get_item_mesh(itm);

				ClipboardItem item;
				item.cell_item = itm;
				item.grid_offset = Vector3(selected) - selection.begin;
				item.orientation = node->get_cell_item_orientation(selected);
				item.instance = RenderingServer::get_singleton()->instance_create2(mesh->get_rid(), get_tree()->get_root()->get_world_3d()->get_scenario());

				clipboard_items.push_back(item);
			}
		}
	}
}

void GridMapEditor::_update_paste_indicator() {
	if (input_action != INPUT_PASTE) {
		Transform3D xf;
		xf.basis.set_zero();
		RenderingServer::get_singleton()->instance_set_transform(paste_instance, xf);
		return;
	}

	Vector3 center = 0.5 * Vector3(real_t(node->get_center_x()), real_t(node->get_center_y()), real_t(node->get_center_z()));
	Vector3 scale = (Vector3(1, 1, 1) + (paste_indicator.end - paste_indicator.begin)) * node->get_cell_size();
	Transform3D xf;
	xf.scale(scale);
	xf.origin = (paste_indicator.begin + (paste_indicator.current - paste_indicator.click) + center) * node->get_cell_size();
	Basis rot;
	rot = node->get_basis_with_orthogonal_index(paste_indicator.orientation);
	xf.basis = rot * xf.basis;
	xf.translate_local((-center * node->get_cell_size()) / scale);

	RenderingServer::get_singleton()->instance_set_transform(paste_instance, node->get_global_transform() * xf);

	for (const ClipboardItem &item : clipboard_items) {
		xf = Transform3D();
		xf.origin = (paste_indicator.begin + (paste_indicator.current - paste_indicator.click) + center) * node->get_cell_size();
		xf.basis = rot * xf.basis;
		xf.translate_local(item.grid_offset * node->get_cell_size());

		Basis item_rot;
		item_rot = node->get_basis_with_orthogonal_index(item.orientation);
		xf.basis = item_rot * xf.basis * node->get_cell_scale();

		RenderingServer::get_singleton()->instance_set_transform(item.instance, node->get_global_transform() * xf);
	}
}

void GridMapEditor::_do_paste() {
	int idx = options->get_popup()->get_item_index(MENU_OPTION_PASTE_SELECTS);
	bool reselect = options->get_popup()->is_item_checked(idx);

	Basis rot;
	rot = node->get_basis_with_orthogonal_index(paste_indicator.orientation);

	Vector3 ofs = paste_indicator.current - paste_indicator.click;
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("GridMap Paste Selection"));

	for (const ClipboardItem &item : clipboard_items) {
		Vector3 position = rot.xform(item.grid_offset) + paste_indicator.begin + ofs;

		Basis orm;
		orm = node->get_basis_with_orthogonal_index(item.orientation);
		orm = rot * orm;

		undo_redo->add_do_method(node, "set_cell_item", position, item.cell_item, node->get_orthogonal_index_from_basis(orm));
		undo_redo->add_undo_method(node, "set_cell_item", position, node->get_cell_item(position), node->get_cell_item_orientation(position));
	}

	if (reselect) {
		// We need to rotate the paste_indicator to find the selection begin and end:
		Vector3 temp_end = rot.xform(paste_indicator.end - paste_indicator.begin) + paste_indicator.begin + ofs;
		Vector3 temp_begin = paste_indicator.begin + ofs;
		// _set_selection expects that selection_begin is the corner closer to the origin:
		for (int i = 0; i < 3; ++i) {
			if (temp_begin[i] > temp_end[i]) {
				float p = temp_begin[i];
				temp_begin[i] = temp_end[i];
				temp_end[i] = p;
			}
		}
		undo_redo->add_do_method(this, "_set_selection", true, temp_begin, temp_end);
		undo_redo->add_undo_method(this, "_set_selection", selection.active, selection.begin, selection.end);
	}

	undo_redo->commit_action();

	_clear_clipboard_data();
}

void GridMapEditor::_show_viewports_transform_gizmo(bool p_value) {
	Dictionary new_state;
	new_state["transform_gizmo"] = p_value;
	for (uint32_t i = 0; i < Node3DEditor::VIEWPORTS_COUNT; i++) {
		Node3DEditorViewport *viewport = Node3DEditor::get_singleton()->get_editor_viewport(i);
		viewport->set_state(new_state);
	}
}

EditorPlugin::AfterGUIInput GridMapEditor::forward_spatial_input_event(Camera3D *p_camera, const Ref<InputEvent> &p_event) {
	if (!node) {
		return EditorPlugin::AFTER_GUI_INPUT_PASS;
	}

	Ref<InputEventKey> k = p_event;
	if (k.is_valid() && k->is_pressed() && !k->is_echo()) {
		// Transform mode (toggle button):
		// If we are in Transform mode we pass the events to the 3D editor,
		// but if the Transform mode shortcut is pressed again, we go back to Selection mode.
		if (mode_buttons_group->get_pressed_button() == transform_mode_button) {
			if (transform_mode_button->get_shortcut().is_valid() && transform_mode_button->get_shortcut()->matches_event(p_event)) {
				select_mode_button->set_pressed(true);
				accept_event();
				return EditorPlugin::AFTER_GUI_INPUT_STOP;
			}
			return EditorPlugin::AFTER_GUI_INPUT_PASS;
		}
		// Tool modes and tool actions:
		for (BaseButton *b : viewport_shortcut_buttons) {
			if (b->is_disabled()) {
				continue;
			}

			if (b->get_shortcut().is_valid() && b->get_shortcut()->matches_event(p_event)) {
				if (b->is_toggle_mode()) {
					b->set_pressed(b->get_button_group().is_valid() || !b->is_pressed());
				} else {
					// Can't press a button without toggle mode, so just emit the signal directly.
					b->emit_signal(SceneStringName(pressed));
				}
				accept_event();
				return EditorPlugin::AFTER_GUI_INPUT_STOP;
			}
		}
		// Hard key actions:
		if (k->get_keycode() == Key::ESCAPE) {
			if (input_action == INPUT_PASTE) {
				_clear_clipboard_data();
				input_action = INPUT_NONE;
				_update_paste_indicator();
				return EditorPlugin::AFTER_GUI_INPUT_STOP;
			} else if (selection.active) {
				_set_selection(false);
				return EditorPlugin::AFTER_GUI_INPUT_STOP;
			} else {
				input_action = INPUT_NONE;
				update_palette();
				_update_cursor_instance();
				return EditorPlugin::AFTER_GUI_INPUT_STOP;
			}
		}
		// Options menu shortcuts:
		Ref<Shortcut> ed_shortcut = ED_GET_SHORTCUT("grid_map/previous_floor");
		if (ed_shortcut.is_valid() && ed_shortcut->matches_event(p_event)) {
			accept_event();
			_menu_option(MENU_OPTION_PREV_LEVEL);
			return EditorPlugin::AFTER_GUI_INPUT_STOP;
		}
		ed_shortcut = ED_GET_SHORTCUT("grid_map/next_floor");
		if (ed_shortcut.is_valid() && ed_shortcut->matches_event(p_event)) {
			accept_event();
			_menu_option(MENU_OPTION_NEXT_LEVEL);
			return EditorPlugin::AFTER_GUI_INPUT_STOP;
		}
		for (int i = 0; i < options->get_popup()->get_item_count(); ++i) {
			const Ref<Shortcut> &shortcut = options->get_popup()->get_item_shortcut(i);
			if (shortcut.is_valid() && shortcut->matches_event(p_event)) {
				// Consume input to avoid conflicts with other plugins.
				accept_event();
				_menu_option(options->get_popup()->get_item_id(i));
				return EditorPlugin::AFTER_GUI_INPUT_STOP;
			}
		}
	}

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		if (mb->get_button_index() == MouseButton::WHEEL_UP && (mb->is_command_or_control_pressed())) {
			if (mb->is_pressed()) {
				floor->set_value(floor->get_value() + mb->get_factor());
			}

			return EditorPlugin::AFTER_GUI_INPUT_STOP; // Eaten.
		} else if (mb->get_button_index() == MouseButton::WHEEL_DOWN && (mb->is_command_or_control_pressed())) {
			if (mb->is_pressed()) {
				floor->set_value(floor->get_value() - mb->get_factor());
			}
			return EditorPlugin::AFTER_GUI_INPUT_STOP;
		}

		if (mb->is_pressed()) {
			Node3DEditorViewport::NavigationScheme nav_scheme = (Node3DEditorViewport::NavigationScheme)EDITOR_GET("editors/3d/navigation/navigation_scheme").operator int();
			if ((nav_scheme == Node3DEditorViewport::NAVIGATION_MAYA || nav_scheme == Node3DEditorViewport::NAVIGATION_MODO) && mb->is_alt_pressed()) {
				input_action = INPUT_NONE;
			} else if (mb->get_button_index() == MouseButton::LEFT) {
				bool can_edit = (node && node->get_mesh_library().is_valid());
				if (input_action == INPUT_PASTE) {
					_do_paste();
					input_action = INPUT_NONE;
					_update_paste_indicator();
					return EditorPlugin::AFTER_GUI_INPUT_STOP;
				} else if (mode_buttons_group->get_pressed_button() == select_mode_button && can_edit) {
					input_action = INPUT_SELECT;
					last_selection = selection;
				} else if (mode_buttons_group->get_pressed_button() == pick_mode_button && can_edit) {
					input_action = INPUT_PICK;
				} else if (mode_buttons_group->get_pressed_button() == paint_mode_button && can_edit) {
					input_action = INPUT_PAINT;
					set_items.clear();
				} else if (mode_buttons_group->get_pressed_button() == erase_mode_button && can_edit) {
					input_action = INPUT_ERASE;
					set_items.clear();
				}
			} else if (mb->get_button_index() == MouseButton::RIGHT) {
				if (input_action == INPUT_PASTE) {
					_clear_clipboard_data();
					input_action = INPUT_NONE;
					_update_paste_indicator();
					return EditorPlugin::AFTER_GUI_INPUT_STOP;
				} else if (selection.active) {
					_set_selection(false);
					return EditorPlugin::AFTER_GUI_INPUT_STOP;
				}
			} else {
				return EditorPlugin::AFTER_GUI_INPUT_PASS;
			}

			if (do_input_action(p_camera, Point2(mb->get_position().x, mb->get_position().y), true)) {
				return EditorPlugin::AFTER_GUI_INPUT_STOP;
			}
			return EditorPlugin::AFTER_GUI_INPUT_PASS;
		} else {
			if ((mb->get_button_index() == MouseButton::LEFT && input_action == INPUT_ERASE) || (mb->get_button_index() == MouseButton::LEFT && input_action == INPUT_PAINT)) {
				if (set_items.size()) {
					EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
					undo_redo->create_action(TTR("GridMap Paint"));
					for (const SetItem &si : set_items) {
						undo_redo->add_do_method(node, "set_cell_item", si.position, si.new_value, si.new_orientation);
					}
					for (List<SetItem>::Element *E = set_items.back(); E; E = E->prev()) {
						const SetItem &si = E->get();
						undo_redo->add_undo_method(node, "set_cell_item", si.position, si.old_value, si.old_orientation);
					}

					undo_redo->commit_action();
				}
				set_items.clear();
				input_action = INPUT_NONE;

				if (set_items.size() > 0) {
					return EditorPlugin::AFTER_GUI_INPUT_STOP;
				}
				return EditorPlugin::AFTER_GUI_INPUT_PASS;
			}

			if (mb->get_button_index() == MouseButton::LEFT && input_action == INPUT_SELECT) {
				EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
				undo_redo->create_action(TTR("GridMap Selection"));
				undo_redo->add_do_method(this, "_set_selection", selection.active, selection.begin, selection.end);
				undo_redo->add_undo_method(this, "_set_selection", last_selection.active, last_selection.begin, last_selection.end);
				undo_redo->commit_action();
			}

			if (mb->get_button_index() == MouseButton::LEFT && input_action != INPUT_NONE) {
				set_items.clear();
				input_action = INPUT_NONE;
				return EditorPlugin::AFTER_GUI_INPUT_STOP;
			}
			if (mb->get_button_index() == MouseButton::RIGHT && (input_action == INPUT_ERASE || input_action == INPUT_PASTE)) {
				input_action = INPUT_NONE;
				return EditorPlugin::AFTER_GUI_INPUT_STOP;
			}
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;

	if (mm.is_valid()) {
		// Update the grid, to check if the grid needs to be moved to a tile cursor.
		update_grid();

		if (do_input_action(p_camera, mm->get_position(), false)) {
			return EditorPlugin::AFTER_GUI_INPUT_STOP;
		}
		return EditorPlugin::AFTER_GUI_INPUT_PASS;
	}

	Ref<InputEventPanGesture> pan_gesture = p_event;
	if (pan_gesture.is_valid()) {
		if (pan_gesture->is_alt_pressed() && pan_gesture->is_command_or_control_pressed()) {
			const real_t delta = pan_gesture->get_delta().y * 0.5;
			accumulated_floor_delta += delta;
			int step = 0;
			if (ABS(accumulated_floor_delta) > 1.0) {
				step = SIGN(accumulated_floor_delta);
				accumulated_floor_delta -= step;
			}
			if (step) {
				floor->set_value(floor->get_value() + step);
			}
			return EditorPlugin::AFTER_GUI_INPUT_STOP;
		}
	}
	accumulated_floor_delta = 0.0;

	return EditorPlugin::AFTER_GUI_INPUT_PASS;
}

struct _CGMEItemSort {
	String name;
	int id = 0;
	_FORCE_INLINE_ bool operator<(const _CGMEItemSort &r_it) const { return name < r_it.name; }
};

void GridMapEditor::_set_display_mode(int p_mode) {
	if (display_mode == p_mode) {
		return;
	}

	if (p_mode == DISPLAY_LIST) {
		mode_list->set_pressed(true);
		mode_thumbnail->set_pressed(false);
	} else if (p_mode == DISPLAY_THUMBNAIL) {
		mode_list->set_pressed(false);
		mode_thumbnail->set_pressed(true);
	}

	display_mode = p_mode;

	update_palette();
}

void GridMapEditor::_text_changed(const String &p_text) {
	update_palette();
}

void GridMapEditor::_sbox_input(const Ref<InputEvent> &p_event) {
	// Redirect navigational key events to the item list.
	Ref<InputEventKey> key = p_event;
	if (key.is_valid()) {
		if (key->is_action("ui_up", true) || key->is_action("ui_down", true) || key->is_action("ui_page_up") || key->is_action("ui_page_down")) {
			mesh_library_palette->gui_input(key);
			search_box->accept_event();
		}
	}
}

void GridMapEditor::_mesh_library_palette_input(const Ref<InputEvent> &p_ie) {
	const Ref<InputEventMouseButton> mb = p_ie;

	// Zoom in/out using Ctrl + mouse wheel
	if (mb.is_valid() && mb->is_pressed() && mb->is_command_or_control_pressed()) {
		if (mb->is_pressed() && mb->get_button_index() == MouseButton::WHEEL_UP) {
			zoom_widget->set_zoom(zoom_widget->get_zoom() + 0.2);
			zoom_widget->emit_signal(SNAME("zoom_changed"), zoom_widget->get_zoom());
		}

		if (mb->is_pressed() && mb->get_button_index() == MouseButton::WHEEL_DOWN) {
			zoom_widget->set_zoom(zoom_widget->get_zoom() - 0.2);
			zoom_widget->emit_signal(SNAME("zoom_changed"), zoom_widget->get_zoom());
		}
	}
}

void GridMapEditor::_icon_size_changed(float p_value) {
	mesh_library_palette->set_icon_scale(p_value);
	update_palette();
}

void GridMapEditor::update_palette() {
	float min_size = EDITOR_GET("editors/grid_map/preview_size");
	min_size *= EDSCALE;

	mesh_library_palette->clear();
	if (display_mode == DISPLAY_THUMBNAIL) {
		mesh_library_palette->set_max_columns(0);
		mesh_library_palette->set_icon_mode(ItemList::ICON_MODE_TOP);
		mesh_library_palette->set_fixed_column_width(min_size * MAX(zoom_widget->get_zoom(), 1.5));
	} else if (display_mode == DISPLAY_LIST) {
		mesh_library_palette->set_max_columns(0);
		mesh_library_palette->set_icon_mode(ItemList::ICON_MODE_LEFT);
		mesh_library_palette->set_fixed_column_width(0);
	}

	mesh_library_palette->set_fixed_icon_size(Size2(min_size, min_size));
	mesh_library_palette->set_max_text_lines(2);

	if (mesh_library.is_null()) {
		search_box->set_text("");
		search_box->set_editable(false);
		info_message->show();
		return;
	}

	search_box->set_editable(true);
	info_message->hide();

	Vector<int> ids;
	ids = mesh_library->get_item_list();

	List<_CGMEItemSort> il;
	for (int i = 0; i < ids.size(); i++) {
		_CGMEItemSort is;
		is.id = ids[i];
		is.name = mesh_library->get_item_name(ids[i]);
		il.push_back(is);
	}
	il.sort();

	String filter = search_box->get_text().strip_edges();

	int item = 0;

	for (_CGMEItemSort &E : il) {
		int id = E.id;
		String name = mesh_library->get_item_name(id);
		Ref<Texture2D> preview = mesh_library->get_item_preview(id);

		if (name.is_empty()) {
			name = "#" + itos(id);
		}

		if (!filter.is_empty() && !filter.is_subsequence_ofn(name)) {
			continue;
		}

		mesh_library_palette->add_item("");
		if (preview.is_valid()) {
			mesh_library_palette->set_item_icon(item, preview);
			mesh_library_palette->set_item_tooltip(item, name);
		}
		mesh_library_palette->set_item_text(item, name);
		mesh_library_palette->set_item_metadata(item, id);

		if (selected_palette == id) {
			mesh_library_palette->select(item);
		}

		item++;
	}
}

void GridMapEditor::_update_mesh_library() {
	ERR_FAIL_NULL(node);

	Ref<MeshLibrary> new_mesh_library = node->get_mesh_library();
	if (new_mesh_library != mesh_library) {
		if (mesh_library.is_valid()) {
			mesh_library->disconnect_changed(callable_mp(this, &GridMapEditor::update_palette));
		}
		mesh_library = new_mesh_library;
	} else {
		return;
	}

	if (mesh_library.is_valid()) {
		mesh_library->connect_changed(callable_mp(this, &GridMapEditor::update_palette));
	}

	update_palette();
	// Make sure we select the first tile as default possible.
	if (mesh_library_palette->get_current() == -1 && mesh_library_palette->get_item_count() > 0) {
		mesh_library_palette->set_current(0);
		selected_palette = mesh_library_palette->get_item_metadata(0);
	}
	// Update the cursor and grid in case the library is changed or removed.
	_update_cursor_instance();
	update_grid();
}

void GridMapEditor::edit(GridMap *p_gridmap) {
	if (node) {
		node->disconnect(SNAME("cell_size_changed"), callable_mp(this, &GridMapEditor::_draw_grids));
		node->disconnect(CoreStringName(changed), callable_mp(this, &GridMapEditor::_update_mesh_library));
		if (mesh_library.is_valid()) {
			mesh_library->disconnect_changed(callable_mp(this, &GridMapEditor::update_palette));
			mesh_library = Ref<MeshLibrary>();
		}
	}

	node = p_gridmap;

	input_action = INPUT_NONE;
	selection.active = false;
	_update_selection_transform();
	_update_paste_indicator();

	spatial_editor = Object::cast_to<Node3DEditorPlugin>(EditorNode::get_singleton()->get_editor_main_screen()->get_selected_plugin());

	if (!node) {
		set_process(false);
		for (int i = 0; i < 3; i++) {
			RenderingServer::get_singleton()->instance_set_visible(grid_instance[i], false);
		}

		if (cursor_instance.is_valid()) {
			RenderingServer::get_singleton()->instance_set_visible(cursor_instance, false);
		}

		return;
	}

	update_palette();
	_update_cursor_instance();

	set_process(true);

	_draw_grids(node->get_cell_size());
	update_grid();

	node->connect(SNAME("cell_size_changed"), callable_mp(this, &GridMapEditor::_draw_grids));
	node->connect(CoreStringName(changed), callable_mp(this, &GridMapEditor::_update_mesh_library));
	_update_mesh_library();
}

void GridMapEditor::update_grid() {
	grid_xform.origin.x -= 1; // Force update in hackish way.

	grid_ofs[edit_axis] = edit_floor[edit_axis] * node->get_cell_size()[edit_axis];

	// If there's a valid tile cursor, offset the grid, otherwise move it back to the node.
	edit_grid_xform.origin = cursor_instance.is_valid() ? grid_ofs : Vector3();
	edit_grid_xform.basis = Basis();

	for (int i = 0; i < 3; i++) {
		RenderingServer::get_singleton()->instance_set_visible(grid_instance[i], i == edit_axis);
	}

	updating = true;
	floor->set_value(edit_floor[edit_axis]);
	updating = false;
}

void GridMapEditor::_draw_grids(const Vector3 &cell_size) {
	Vector3 edited_floor = node->get_meta("_editor_floor_", Vector3());

	for (int i = 0; i < 3; i++) {
		RS::get_singleton()->mesh_clear(grid[i]);
		edit_floor[i] = edited_floor[i];
	}

	Vector<Vector3> grid_points[3];
	Vector<Color> grid_colors[3];

	for (int i = 0; i < 3; i++) {
		Vector3 axis;
		axis[i] = 1;
		Vector3 axis_n1;
		axis_n1[(i + 1) % 3] = cell_size[(i + 1) % 3];
		Vector3 axis_n2;
		axis_n2[(i + 2) % 3] = cell_size[(i + 2) % 3];

		for (int j = -GRID_CURSOR_SIZE; j <= GRID_CURSOR_SIZE; j++) {
			for (int k = -GRID_CURSOR_SIZE; k <= GRID_CURSOR_SIZE; k++) {
				Vector3 p = axis_n1 * j + axis_n2 * k;
				float trans = Math::pow(MAX(0, 1.0 - (Vector2(j, k).length() / GRID_CURSOR_SIZE)), 2);

				Vector3 pj = axis_n1 * (j + 1) + axis_n2 * k;
				float transj = Math::pow(MAX(0, 1.0 - (Vector2(j + 1, k).length() / GRID_CURSOR_SIZE)), 2);

				Vector3 pk = axis_n1 * j + axis_n2 * (k + 1);
				float transk = Math::pow(MAX(0, 1.0 - (Vector2(j, k + 1).length() / GRID_CURSOR_SIZE)), 2);

				grid_points[i].push_back(p);
				grid_points[i].push_back(pk);
				grid_colors[i].push_back(Color(1, 1, 1, trans));
				grid_colors[i].push_back(Color(1, 1, 1, transk));

				grid_points[i].push_back(p);
				grid_points[i].push_back(pj);
				grid_colors[i].push_back(Color(1, 1, 1, trans));
				grid_colors[i].push_back(Color(1, 1, 1, transj));
			}
		}

		Array d;
		d.resize(RS::ARRAY_MAX);
		d[RS::ARRAY_VERTEX] = grid_points[i];
		d[RS::ARRAY_COLOR] = grid_colors[i];
		RenderingServer::get_singleton()->mesh_add_surface_from_arrays(grid[i], RenderingServer::PRIMITIVE_LINES, d);
		RenderingServer::get_singleton()->mesh_surface_set_material(grid[i], 0, indicator_mat->get_rid());
	}
}

void GridMapEditor::_update_theme() {
	transform_mode_button->set_button_icon(get_theme_icon(SNAME("ToolMove"), EditorStringName(EditorIcons)));
	select_mode_button->set_button_icon(get_theme_icon(SNAME("ToolSelect"), EditorStringName(EditorIcons)));
	erase_mode_button->set_button_icon(get_theme_icon(SNAME("Eraser"), EditorStringName(EditorIcons)));
	paint_mode_button->set_button_icon(get_theme_icon(SNAME("Paint"), EditorStringName(EditorIcons)));
	pick_mode_button->set_button_icon(get_theme_icon(SNAME("ColorPick"), EditorStringName(EditorIcons)));
	fill_action_button->set_button_icon(get_theme_icon(SNAME("Bucket"), EditorStringName(EditorIcons)));
	move_action_button->set_button_icon(get_theme_icon(SNAME("ActionCut"), EditorStringName(EditorIcons)));
	duplicate_action_button->set_button_icon(get_theme_icon(SNAME("ActionCopy"), EditorStringName(EditorIcons)));
	delete_action_button->set_button_icon(get_theme_icon(SNAME("Clear"), EditorStringName(EditorIcons)));
	rotate_x_button->set_button_icon(get_theme_icon(SNAME("RotateLeft"), EditorStringName(EditorIcons)));
	rotate_y_button->set_button_icon(get_theme_icon(SNAME("ToolRotate"), EditorStringName(EditorIcons)));
	rotate_z_button->set_button_icon(get_theme_icon(SNAME("RotateRight"), EditorStringName(EditorIcons)));
	search_box->set_right_icon(get_theme_icon(SNAME("Search"), EditorStringName(EditorIcons)));
	mode_thumbnail->set_button_icon(get_theme_icon(SNAME("FileThumbnail"), EditorStringName(EditorIcons)));
	mode_list->set_button_icon(get_theme_icon(SNAME("FileList"), EditorStringName(EditorIcons)));
	options->set_button_icon(get_theme_icon(SNAME("Tools"), EditorStringName(EditorIcons)));
}

void GridMapEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			mesh_library_palette->connect(SceneStringName(item_selected), callable_mp(this, &GridMapEditor::_item_selected_cbk));
			for (int i = 0; i < 3; i++) {
				grid[i] = RS::get_singleton()->mesh_create();
				grid_instance[i] = RS::get_singleton()->instance_create2(grid[i], get_tree()->get_root()->get_world_3d()->get_scenario());
				RenderingServer::get_singleton()->instance_set_layer_mask(grid_instance[i], 1 << Node3DEditorViewport::MISC_TOOL_LAYER);
				selection_level_instance[i] = RenderingServer::get_singleton()->instance_create2(selection_level_mesh[i], get_tree()->get_root()->get_world_3d()->get_scenario());
				RenderingServer::get_singleton()->instance_set_layer_mask(selection_level_instance[i], 1 << Node3DEditorViewport::MISC_TOOL_LAYER);
			}

			cursor_instance = RenderingServer::get_singleton()->instance_create2(cursor_mesh, get_tree()->get_root()->get_world_3d()->get_scenario());
			RenderingServer::get_singleton()->instance_set_layer_mask(cursor_instance, 1 << Node3DEditorViewport::MISC_TOOL_LAYER);
			RenderingServer::get_singleton()->instance_set_visible(cursor_instance, false);
			selection_instance = RenderingServer::get_singleton()->instance_create2(selection_mesh, get_tree()->get_root()->get_world_3d()->get_scenario());
			RenderingServer::get_singleton()->instance_set_layer_mask(selection_instance, 1 << Node3DEditorViewport::MISC_TOOL_LAYER);
			paste_instance = RenderingServer::get_singleton()->instance_create2(paste_mesh, get_tree()->get_root()->get_world_3d()->get_scenario());
			RenderingServer::get_singleton()->instance_set_layer_mask(paste_instance, 1 << Node3DEditorViewport::MISC_TOOL_LAYER);

			_update_selection_transform();
			_update_paste_indicator();
			_update_theme();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			_clear_clipboard_data();

			for (int i = 0; i < 3; i++) {
				RS::get_singleton()->free(grid_instance[i]);
				RS::get_singleton()->free(grid[i]);
				grid_instance[i] = RID();
				grid[i] = RID();
				RenderingServer::get_singleton()->free(selection_level_instance[i]);
			}

			RenderingServer::get_singleton()->free(cursor_instance);
			RenderingServer::get_singleton()->free(selection_instance);
			RenderingServer::get_singleton()->free(paste_instance);
			cursor_instance = RID();
			selection_instance = RID();
			paste_instance = RID();
		} break;

		case NOTIFICATION_PROCESS: {
			if (!node) {
				return;
			}

			Transform3D xf = node->get_global_transform();

			if (xf != grid_xform) {
				for (int i = 0; i < 3; i++) {
					RS::get_singleton()->instance_set_transform(grid_instance[i], xf * edit_grid_xform);
				}
				grid_xform = xf;
				_update_cursor_transform();
				_update_selection_transform();
			}
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			_update_theme();
		} break;

		case NOTIFICATION_APPLICATION_FOCUS_OUT: {
			if (input_action == INPUT_PAINT) {
				// Simulate mouse released event to stop drawing when editor focus exists.
				Ref<InputEventMouseButton> release;
				release.instantiate();
				release->set_button_index(MouseButton::LEFT);
				forward_spatial_input_event(nullptr, release);
			}
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			indicator_mat->set_albedo(EDITOR_GET("editors/3d_gizmos/gizmo_colors/gridmap_grid"));

			// Take Preview Size changes into account.
			update_palette();
		} break;
	}
}

void GridMapEditor::_update_cursor_instance() {
	if (!node) {
		return;
	}

	if (cursor_instance.is_valid()) {
		RenderingServer::get_singleton()->free(cursor_instance);
	}
	cursor_instance = RID();

	if (mode_buttons_group->get_pressed_button() == paint_mode_button) {
		if (selected_palette >= 0 && node && node->get_mesh_library().is_valid()) {
			Ref<Mesh> mesh = node->get_mesh_library()->get_item_mesh(selected_palette);
			if (mesh.is_valid() && mesh->get_rid().is_valid()) {
				cursor_instance = RenderingServer::get_singleton()->instance_create2(mesh->get_rid(), get_tree()->get_root()->get_world_3d()->get_scenario());
				RS::ShadowCastingSetting cast_shadows = (RS::ShadowCastingSetting)node->get_mesh_library()->get_item_mesh_cast_shadow(selected_palette);
				RS::get_singleton()->instance_geometry_set_cast_shadows_setting(cursor_instance, cast_shadows);
			}
		}
	} else if (mode_buttons_group->get_pressed_button() == select_mode_button) {
		cursor_inner_mat->set_albedo(Color(default_color, 0.2));
		cursor_outer_mat->set_albedo(Color(default_color, 0.8));
		cursor_instance = RenderingServer::get_singleton()->instance_create2(cursor_mesh, get_tree()->get_root()->get_world_3d()->get_scenario());
	} else if (mode_buttons_group->get_pressed_button() == erase_mode_button) {
		cursor_inner_mat->set_albedo(Color(erase_color, 0.2));
		cursor_outer_mat->set_albedo(Color(erase_color, 0.8));
		cursor_instance = RenderingServer::get_singleton()->instance_create2(cursor_mesh, get_tree()->get_root()->get_world_3d()->get_scenario());
	} else if (mode_buttons_group->get_pressed_button() == pick_mode_button) {
		cursor_inner_mat->set_albedo(Color(pick_color, 0.2));
		cursor_outer_mat->set_albedo(Color(pick_color, 0.8));
		cursor_instance = RenderingServer::get_singleton()->instance_create2(cursor_mesh, get_tree()->get_root()->get_world_3d()->get_scenario());
	}

	// Make the cursor translucent so that it can be distinguished from already-placed tiles.
	RenderingServer::get_singleton()->instance_geometry_set_transparency(cursor_instance, 0.5);

	_update_cursor_transform();
}

void GridMapEditor::_on_tool_mode_changed() {
	_show_viewports_transform_gizmo(mode_buttons_group->get_pressed_button() == transform_mode_button);
	_update_cursor_instance();
}

void GridMapEditor::_item_selected_cbk(int idx) {
	selected_palette = mesh_library_palette->get_item_metadata(idx);

	_update_cursor_instance();
}

void GridMapEditor::_floor_changed(float p_value) {
	if (updating) {
		return;
	}

	edit_floor[edit_axis] = p_value;
	node->set_meta("_editor_floor_", Vector3(edit_floor[0], edit_floor[1], edit_floor[2]));
	update_grid();
	_update_selection_transform();
}

void GridMapEditor::_floor_mouse_exited() {
	floor->get_line_edit()->release_focus();
}

void GridMapEditor::_bind_methods() {
	ClassDB::bind_method("_configure", &GridMapEditor::_configure);
	ClassDB::bind_method("_set_selection", &GridMapEditor::_set_selection);
}

GridMapEditor::GridMapEditor() {
	ED_SHORTCUT("grid_map/previous_floor", TTRC("Previous Floor"), Key::KEY_1, true);
	ED_SHORTCUT("grid_map/next_floor", TTRC("Next Floor"), Key::KEY_3, true);
	ED_SHORTCUT("grid_map/edit_x_axis", TTRC("Edit X Axis"), KeyModifierMask::SHIFT + Key::Z, true);
	ED_SHORTCUT("grid_map/edit_y_axis", TTRC("Edit Y Axis"), KeyModifierMask::SHIFT + Key::X, true);
	ED_SHORTCUT("grid_map/edit_z_axis", TTRC("Edit Z Axis"), KeyModifierMask::SHIFT + Key::C, true);
	ED_SHORTCUT("grid_map/keep_selected", TTRC("Keep Selection"));
	ED_SHORTCUT("grid_map/clear_rotation", TTRC("Clear Rotation"));

	options = memnew(MenuButton);
	options->set_theme_type_variation(SceneStringName(FlatButton));
	options->get_popup()->add_separator();
	options->get_popup()->add_radio_check_shortcut(ED_GET_SHORTCUT("grid_map/edit_x_axis"), MENU_OPTION_X_AXIS);
	options->get_popup()->add_radio_check_shortcut(ED_GET_SHORTCUT("grid_map/edit_y_axis"), MENU_OPTION_Y_AXIS);
	options->get_popup()->add_radio_check_shortcut(ED_GET_SHORTCUT("grid_map/edit_z_axis"), MENU_OPTION_Z_AXIS);
	options->get_popup()->set_item_checked(options->get_popup()->get_item_index(MENU_OPTION_Y_AXIS), true);
	options->get_popup()->add_separator();
	// TRANSLATORS: This is a toggle to select after pasting the new content.
	options->get_popup()->add_shortcut(ED_GET_SHORTCUT("grid_map/clear_rotation"), MENU_OPTION_CURSOR_CLEAR_ROTATION);
	options->get_popup()->add_check_shortcut(ED_GET_SHORTCUT("grid_map/keep_selected"), MENU_OPTION_PASTE_SELECTS);
	options->get_popup()->set_item_checked(options->get_popup()->get_item_index(MENU_OPTION_PASTE_SELECTS), true);
	options->get_popup()->add_separator();
	options->get_popup()->add_item(TTR("Settings..."), MENU_OPTION_GRIDMAP_SETTINGS);

	settings_dialog = memnew(ConfirmationDialog);
	settings_dialog->set_title(TTR("GridMap Settings"));
	add_child(settings_dialog);
	settings_vbc = memnew(VBoxContainer);
	settings_vbc->set_custom_minimum_size(Size2(200, 0) * EDSCALE);
	settings_dialog->add_child(settings_vbc);

	settings_pick_distance = memnew(SpinBox);
	settings_pick_distance->set_max(10000.0f);
	settings_pick_distance->set_min(500.0f);
	settings_pick_distance->set_step(1.0f);
	settings_pick_distance->set_value(EDITOR_GET("editors/grid_map/pick_distance"));
	settings_vbc->add_margin_child(TTR("Pick Distance:"), settings_pick_distance);

	options->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &GridMapEditor::_menu_option));

	toolbar = memnew(HBoxContainer);
	add_child(toolbar);
	toolbar->set_h_size_flags(SIZE_EXPAND_FILL);

	HBoxContainer *mode_buttons = memnew(HBoxContainer);
	toolbar->add_child(mode_buttons);
	mode_buttons_group.instantiate();

	transform_mode_button = memnew(Button);
	transform_mode_button->set_theme_type_variation(SceneStringName(FlatButton));
	transform_mode_button->set_toggle_mode(true);
	transform_mode_button->set_button_group(mode_buttons_group);
	transform_mode_button->set_shortcut(ED_SHORTCUT("grid_map/transform_tool", TTRC("Transform"), Key::T, true));
	transform_mode_button->connect(SceneStringName(toggled),
			callable_mp(this, &GridMapEditor::_on_tool_mode_changed).unbind(1));
	mode_buttons->add_child(transform_mode_button);
	viewport_shortcut_buttons.push_back(transform_mode_button);
	VSeparator *vsep = memnew(VSeparator);
	mode_buttons->add_child(vsep);

	select_mode_button = memnew(Button);
	select_mode_button->set_theme_type_variation(SceneStringName(FlatButton));
	select_mode_button->set_toggle_mode(true);
	select_mode_button->set_button_group(mode_buttons_group);
	select_mode_button->set_shortcut(ED_SHORTCUT("grid_map/selection_tool", TTRC("Selection"), Key::Q, true));
	select_mode_button->connect(SceneStringName(toggled),
			callable_mp(this, &GridMapEditor::_on_tool_mode_changed).unbind(1));
	mode_buttons->add_child(select_mode_button);
	viewport_shortcut_buttons.push_back(select_mode_button);

	erase_mode_button = memnew(Button);
	erase_mode_button->set_theme_type_variation(SceneStringName(FlatButton));
	erase_mode_button->set_toggle_mode(true);
	erase_mode_button->set_button_group(mode_buttons_group);
	erase_mode_button->set_shortcut(ED_SHORTCUT("grid_map/erase_tool", TTRC("Erase"), Key::W, true));
	mode_buttons->add_child(erase_mode_button);
	erase_mode_button->connect(SceneStringName(toggled),
			callable_mp(this, &GridMapEditor::_on_tool_mode_changed).unbind(1));
	viewport_shortcut_buttons.push_back(erase_mode_button);

	paint_mode_button = memnew(Button);
	paint_mode_button->set_theme_type_variation(SceneStringName(FlatButton));
	paint_mode_button->set_toggle_mode(true);
	paint_mode_button->set_button_group(mode_buttons_group);
	paint_mode_button->set_shortcut(ED_SHORTCUT("grid_map/paint_tool", TTRC("Paint"), Key::E, true));
	paint_mode_button->connect(SceneStringName(toggled),
			callable_mp(this, &GridMapEditor::_on_tool_mode_changed).unbind(1));
	mode_buttons->add_child(paint_mode_button);
	viewport_shortcut_buttons.push_back(paint_mode_button);

	pick_mode_button = memnew(Button);
	pick_mode_button->set_theme_type_variation(SceneStringName(FlatButton));
	pick_mode_button->set_toggle_mode(true);
	pick_mode_button->set_button_group(mode_buttons_group);
	pick_mode_button->set_shortcut(ED_SHORTCUT("grid_map/pick_tool", TTRC("Pick"), Key::R, true));
	pick_mode_button->connect(SceneStringName(toggled),
			callable_mp(this, &GridMapEditor::_on_tool_mode_changed).unbind(1));
	mode_buttons->add_child(pick_mode_button);
	viewport_shortcut_buttons.push_back(pick_mode_button);

	vsep = memnew(VSeparator);
	toolbar->add_child(vsep);

	HBoxContainer *action_buttons = memnew(HBoxContainer);
	toolbar->add_child(action_buttons);

	fill_action_button = memnew(Button);
	fill_action_button->set_theme_type_variation(SceneStringName(FlatButton));
	fill_action_button->set_shortcut(ED_SHORTCUT("grid_map/fill_tool", TTRC("Fill"), Key::Z, true));
	fill_action_button->connect(SceneStringName(pressed),
			callable_mp(this, &GridMapEditor::_menu_option).bind(MENU_OPTION_SELECTION_FILL));
	action_buttons->add_child(fill_action_button);
	viewport_shortcut_buttons.push_back(fill_action_button);

	move_action_button = memnew(Button);
	move_action_button->set_theme_type_variation(SceneStringName(FlatButton));
	move_action_button->set_shortcut(ED_SHORTCUT("grid_map/move_tool", TTRC("Move"), Key::X, true));
	move_action_button->connect(SceneStringName(pressed),
			callable_mp(this, &GridMapEditor::_menu_option).bind(MENU_OPTION_SELECTION_CUT));
	action_buttons->add_child(move_action_button);
	viewport_shortcut_buttons.push_back(move_action_button);

	duplicate_action_button = memnew(Button);
	duplicate_action_button->set_theme_type_variation(SceneStringName(FlatButton));
	duplicate_action_button->set_shortcut(ED_SHORTCUT("grid_map/duplicate_tool", TTRC("Duplicate"), Key::C, true));
	duplicate_action_button->connect(SceneStringName(pressed),
			callable_mp(this, &GridMapEditor::_menu_option).bind(MENU_OPTION_SELECTION_DUPLICATE));
	action_buttons->add_child(duplicate_action_button);
	viewport_shortcut_buttons.push_back(duplicate_action_button);

	delete_action_button = memnew(Button);
	delete_action_button->set_theme_type_variation(SceneStringName(FlatButton));
	delete_action_button->set_shortcut(ED_SHORTCUT("grid_map/delete_tool", TTRC("Delete"), Key::V, true));
	delete_action_button->connect(SceneStringName(pressed),
			callable_mp(this, &GridMapEditor::_menu_option).bind(MENU_OPTION_SELECTION_CLEAR));
	action_buttons->add_child(delete_action_button);
	viewport_shortcut_buttons.push_back(delete_action_button);

	vsep = memnew(VSeparator);
	toolbar->add_child(vsep);

	HBoxContainer *rotation_buttons = memnew(HBoxContainer);
	toolbar->add_child(rotation_buttons);

	rotate_x_button = memnew(Button);
	rotate_x_button->set_theme_type_variation(SceneStringName(FlatButton));
	rotate_x_button->set_shortcut(ED_SHORTCUT("grid_map/cursor_rotate_x", TTRC("Cursor Rotate X"), Key::A, true));
	rotate_x_button->connect(SceneStringName(pressed),
			callable_mp(this, &GridMapEditor::_menu_option).bind(MENU_OPTION_CURSOR_ROTATE_X));
	rotation_buttons->add_child(rotate_x_button);
	viewport_shortcut_buttons.push_back(rotate_x_button);

	rotate_y_button = memnew(Button);
	rotate_y_button->set_theme_type_variation(SceneStringName(FlatButton));
	rotate_y_button->set_shortcut(ED_SHORTCUT("grid_map/cursor_rotate_y", TTRC("Cursor Rotate Y"), Key::S, true));
	rotate_y_button->connect(SceneStringName(pressed),
			callable_mp(this, &GridMapEditor::_menu_option).bind(MENU_OPTION_CURSOR_ROTATE_Y));
	rotation_buttons->add_child(rotate_y_button);
	viewport_shortcut_buttons.push_back(rotate_y_button);

	rotate_z_button = memnew(Button);
	rotate_z_button->set_theme_type_variation(SceneStringName(FlatButton));
	rotate_z_button->set_shortcut(ED_SHORTCUT("grid_map/cursor_rotate_z", TTRC("Cursor Rotate Z"), Key::D, true));
	rotate_z_button->connect(SceneStringName(pressed),
			callable_mp(this, &GridMapEditor::_menu_option).bind(MENU_OPTION_CURSOR_ROTATE_Z));
	rotation_buttons->add_child(rotate_z_button);
	viewport_shortcut_buttons.push_back(rotate_z_button);

	// Wide empty separation control. (like BoxContainer::add_spacer())
	Control *c = memnew(Control);
	c->set_mouse_filter(MOUSE_FILTER_PASS);
	c->set_h_size_flags(SIZE_EXPAND_FILL);
	toolbar->add_child(c);

	floor = memnew(SpinBox);
	floor->set_min(-32767);
	floor->set_max(32767);
	floor->set_step(1);
	floor->set_tooltip_text(
			vformat(TTR("Change Grid Floor:\nPrevious Plane (%s)\nNext Plane (%s)"),
					ED_GET_SHORTCUT("grid_map/previous_floor")->get_as_text(),
					ED_GET_SHORTCUT("grid_map/next_floor")->get_as_text()));
	toolbar->add_child(floor);
	floor->get_line_edit()->add_theme_constant_override("minimum_character_width", 2);
	floor->get_line_edit()->set_context_menu_enabled(false);
	floor->connect(SceneStringName(value_changed), callable_mp(this, &GridMapEditor::_floor_changed));
	floor->connect(SceneStringName(mouse_exited), callable_mp(this, &GridMapEditor::_floor_mouse_exited));
	floor->get_line_edit()->connect(SceneStringName(mouse_exited), callable_mp(this, &GridMapEditor::_floor_mouse_exited));

	search_box = memnew(LineEdit);
	search_box->add_theme_constant_override("minimum_character_width", 10);
	search_box->set_placeholder(TTR("Filter Meshes"));
	search_box->set_clear_button_enabled(true);
	toolbar->add_child(search_box);
	search_box->connect(SceneStringName(text_changed), callable_mp(this, &GridMapEditor::_text_changed));
	search_box->connect(SceneStringName(gui_input), callable_mp(this, &GridMapEditor::_sbox_input));

	zoom_widget = memnew(EditorZoomWidget);
	toolbar->add_child(zoom_widget);
	zoom_widget->setup_zoom_limits(0.2, 4);
	zoom_widget->set_zoom(1.0);
	zoom_widget->set_anchors_and_offsets_preset(Control::PRESET_TOP_LEFT, Control::PRESET_MODE_MINSIZE, 2 * EDSCALE);
	zoom_widget->connect("zoom_changed", callable_mp(this, &GridMapEditor::_icon_size_changed));
	zoom_widget->set_shortcut_context(this);

	mode_thumbnail = memnew(Button);
	mode_thumbnail->set_theme_type_variation(SceneStringName(FlatButton));
	mode_thumbnail->set_toggle_mode(true);
	mode_thumbnail->set_pressed(true);
	toolbar->add_child(mode_thumbnail);
	mode_thumbnail->connect(SceneStringName(pressed), callable_mp(this, &GridMapEditor::_set_display_mode).bind(DISPLAY_THUMBNAIL));

	mode_list = memnew(Button);
	mode_list->set_theme_type_variation(SceneStringName(FlatButton));
	mode_list->set_toggle_mode(true);
	mode_list->set_pressed(false);
	toolbar->add_child(mode_list);
	mode_list->connect(SceneStringName(pressed), callable_mp(this, &GridMapEditor::_set_display_mode).bind(DISPLAY_LIST));

	toolbar->add_child(options);

	mesh_library_palette = memnew(ItemList);
	mesh_library_palette->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	add_child(mesh_library_palette);
	mesh_library_palette->set_v_size_flags(SIZE_EXPAND_FILL);
	mesh_library_palette->connect(SceneStringName(gui_input), callable_mp(this, &GridMapEditor::_mesh_library_palette_input));

	info_message = memnew(Label);
	info_message->set_text(TTR("Give a MeshLibrary resource to this GridMap to use its meshes."));
	info_message->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	info_message->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	info_message->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	info_message->set_custom_minimum_size(Size2(100 * EDSCALE, 0));
	info_message->set_anchors_and_offsets_preset(PRESET_FULL_RECT, PRESET_MODE_KEEP_SIZE, 8 * EDSCALE);
	mesh_library_palette->add_child(info_message);

	edit_axis = Vector3::AXIS_Y;
	edit_floor[0] = -1;
	edit_floor[1] = -1;
	edit_floor[2] = -1;

	cursor_mesh = RenderingServer::get_singleton()->mesh_create();
	selection_mesh = RenderingServer::get_singleton()->mesh_create();
	paste_mesh = RenderingServer::get_singleton()->mesh_create();

	{
		// Selection mesh create.

		Vector<Vector3> lines;
		Vector<Vector3> triangles;
		Vector<Vector3> square[3];

		for (int i = 0; i < 6; i++) {
			Vector3 face_points[4];

			for (int j = 0; j < 4; j++) {
				float v[3];
				v[0] = 1.0;
				v[1] = 1 - 2 * ((j >> 1) & 1);
				v[2] = v[1] * (1 - 2 * (j & 1));

				for (int k = 0; k < 3; k++) {
					if (i < 3) {
						face_points[j][(i + k) % 3] = v[k];
					} else {
						face_points[3 - j][(i + k) % 3] = -v[k];
					}
				}
			}

			triangles.push_back(face_points[0] * 0.5 + Vector3(0.5, 0.5, 0.5));
			triangles.push_back(face_points[1] * 0.5 + Vector3(0.5, 0.5, 0.5));
			triangles.push_back(face_points[2] * 0.5 + Vector3(0.5, 0.5, 0.5));

			triangles.push_back(face_points[2] * 0.5 + Vector3(0.5, 0.5, 0.5));
			triangles.push_back(face_points[3] * 0.5 + Vector3(0.5, 0.5, 0.5));
			triangles.push_back(face_points[0] * 0.5 + Vector3(0.5, 0.5, 0.5));
		}

		for (int i = 0; i < 12; i++) {
			AABB base(Vector3(0, 0, 0), Vector3(1, 1, 1));
			Vector3 a, b;
			base.get_edge(i, a, b);
			lines.push_back(a);
			lines.push_back(b);
		}

		for (int i = 0; i < 3; i++) {
			Vector3 points[4];
			for (int j = 0; j < 4; j++) {
				static const bool orderx[4] = { false, true, true, false };
				static const bool ordery[4] = { false, false, true, true };

				Vector3 sp;
				if (orderx[j]) {
					sp[(i + 1) % 3] = 1.0;
				}
				if (ordery[j]) {
					sp[(i + 2) % 3] = 1.0;
				}

				points[j] = sp;
			}

			for (int j = 0; j < 4; j++) {
				Vector3 ofs;
				ofs[i] += 0.01;
				square[i].push_back(points[j] - ofs);
				square[i].push_back(points[(j + 1) % 4] - ofs);
				square[i].push_back(points[j] + ofs);
				square[i].push_back(points[(j + 1) % 4] + ofs);
			}
		}

		Array d;
		d.resize(RS::ARRAY_MAX);

		default_color = Color(0.0, 0.565, 1.0); // blue 0.7, 0.7, 1.0
		erase_color = Color(1.0, 0.2, 0.2); // red
		pick_color = Color(1, 0.7, 0); // orange/yellow

		cursor_inner_mat.instantiate();
		cursor_inner_mat->set_albedo(Color(default_color, 0.2));
		cursor_inner_mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
		cursor_inner_mat->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
		cursor_inner_mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);

		cursor_outer_mat.instantiate();
		cursor_outer_mat->set_albedo(Color(default_color, 0.8));
		cursor_outer_mat->set_on_top_of_alpha();
		cursor_outer_mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
		cursor_outer_mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
		cursor_outer_mat->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);

		inner_mat.instantiate();
		inner_mat->set_albedo(Color(default_color, 0.2));
		inner_mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
		inner_mat->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
		inner_mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);

		outer_mat.instantiate();
		outer_mat->set_albedo(Color(default_color, 0.8));
		outer_mat->set_on_top_of_alpha();
		outer_mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
		outer_mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
		outer_mat->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);

		selection_floor_mat.instantiate();
		selection_floor_mat->set_albedo(Color(0.80, 0.80, 1.0, 1));
		selection_floor_mat->set_on_top_of_alpha();
		selection_floor_mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
		selection_floor_mat->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);

		d[RS::ARRAY_VERTEX] = triangles;
		RenderingServer::get_singleton()->mesh_add_surface_from_arrays(cursor_mesh, RS::PRIMITIVE_TRIANGLES, d);
		RenderingServer::get_singleton()->mesh_surface_set_material(cursor_mesh, 0, cursor_inner_mat->get_rid());

		d[RS::ARRAY_VERTEX] = lines;
		RenderingServer::get_singleton()->mesh_add_surface_from_arrays(cursor_mesh, RS::PRIMITIVE_LINES, d);
		RenderingServer::get_singleton()->mesh_surface_set_material(cursor_mesh, 1, cursor_outer_mat->get_rid());

		d[RS::ARRAY_VERTEX] = triangles;
		RenderingServer::get_singleton()->mesh_add_surface_from_arrays(selection_mesh, RS::PRIMITIVE_TRIANGLES, d);
		RenderingServer::get_singleton()->mesh_surface_set_material(selection_mesh, 0, inner_mat->get_rid());

		d[RS::ARRAY_VERTEX] = lines;
		RenderingServer::get_singleton()->mesh_add_surface_from_arrays(selection_mesh, RS::PRIMITIVE_LINES, d);
		RenderingServer::get_singleton()->mesh_surface_set_material(selection_mesh, 1, outer_mat->get_rid());

		d[RS::ARRAY_VERTEX] = triangles;
		RenderingServer::get_singleton()->mesh_add_surface_from_arrays(paste_mesh, RS::PRIMITIVE_TRIANGLES, d);
		RenderingServer::get_singleton()->mesh_surface_set_material(paste_mesh, 0, inner_mat->get_rid());

		d[RS::ARRAY_VERTEX] = lines;
		RenderingServer::get_singleton()->mesh_add_surface_from_arrays(paste_mesh, RS::PRIMITIVE_LINES, d);
		RenderingServer::get_singleton()->mesh_surface_set_material(paste_mesh, 1, outer_mat->get_rid());

		for (int i = 0; i < 3; i++) {
			d[RS::ARRAY_VERTEX] = square[i];
			selection_level_mesh[i] = RS::get_singleton()->mesh_create();
			RenderingServer::get_singleton()->mesh_add_surface_from_arrays(selection_level_mesh[i], RS::PRIMITIVE_LINES, d);
			RenderingServer::get_singleton()->mesh_surface_set_material(selection_level_mesh[i], 0, selection_floor_mat->get_rid());
		}
	}

	_set_selection(false);

	indicator_mat.instantiate();
	indicator_mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	indicator_mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
	indicator_mat->set_flag(StandardMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
	indicator_mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	indicator_mat->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	indicator_mat->set_albedo(EDITOR_GET("editors/3d_gizmos/gizmo_colors/gridmap_grid"));
}

GridMapEditor::~GridMapEditor() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	_clear_clipboard_data();

	for (int i = 0; i < 3; i++) {
		if (grid[i].is_valid()) {
			RenderingServer::get_singleton()->free(grid[i]);
		}
		if (grid_instance[i].is_valid()) {
			RenderingServer::get_singleton()->free(grid_instance[i]);
		}
		if (selection_level_instance[i].is_valid()) {
			RenderingServer::get_singleton()->free(selection_level_instance[i]);
		}
		if (selection_level_mesh[i].is_valid()) {
			RenderingServer::get_singleton()->free(selection_level_mesh[i]);
		}
	}

	RenderingServer::get_singleton()->free(cursor_mesh);
	if (cursor_instance.is_valid()) {
		RenderingServer::get_singleton()->free(cursor_instance);
	}

	RenderingServer::get_singleton()->free(selection_mesh);
	if (selection_instance.is_valid()) {
		RenderingServer::get_singleton()->free(selection_instance);
	}

	RenderingServer::get_singleton()->free(paste_mesh);
	if (paste_instance.is_valid()) {
		RenderingServer::get_singleton()->free(paste_instance);
	}
}

void GridMapEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			grid_map_editor = memnew(GridMapEditor);
			grid_map_editor->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			grid_map_editor->set_v_size_flags(Control::SIZE_EXPAND_FILL);
			grid_map_editor->set_custom_minimum_size(Size2(0, 200) * EDSCALE);
			grid_map_editor->hide();

			panel_button = EditorNode::get_bottom_panel()->add_item(TTR("GridMap"), grid_map_editor, ED_SHORTCUT_AND_COMMAND("bottom_panels/toggle_grid_map_bottom_panel", TTRC("Toggle GridMap Bottom Panel")));
			panel_button->hide();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			EditorNode::get_bottom_panel()->remove_item(grid_map_editor);
			memdelete_notnull(grid_map_editor);
			grid_map_editor = nullptr;
			panel_button = nullptr;
		} break;
	}
}

void GridMapEditorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_current_grid_map"), &GridMapEditorPlugin::get_current_grid_map);
	ClassDB::bind_method(D_METHOD("set_selection", "begin", "end"), &GridMapEditorPlugin::set_selection);
	ClassDB::bind_method(D_METHOD("clear_selection"), &GridMapEditorPlugin::clear_selection);
	ClassDB::bind_method(D_METHOD("get_selection"), &GridMapEditorPlugin::get_selection);
	ClassDB::bind_method(D_METHOD("has_selection"), &GridMapEditorPlugin::has_selection);
	ClassDB::bind_method(D_METHOD("get_selected_cells"), &GridMapEditorPlugin::get_selected_cells);
	ClassDB::bind_method(D_METHOD("set_selected_palette_item", "item"), &GridMapEditorPlugin::set_selected_palette_item);
	ClassDB::bind_method(D_METHOD("get_selected_palette_item"), &GridMapEditorPlugin::get_selected_palette_item);
}

void GridMapEditorPlugin::edit(Object *p_object) {
	ERR_FAIL_NULL(grid_map_editor);
	grid_map_editor->edit(Object::cast_to<GridMap>(p_object));
}

bool GridMapEditorPlugin::handles(Object *p_object) const {
	return p_object->is_class("GridMap");
}

void GridMapEditorPlugin::make_visible(bool p_visible) {
	ERR_FAIL_NULL(grid_map_editor);
	if (p_visible) {
		BaseButton *button = grid_map_editor->mode_buttons_group->get_pressed_button();
		if (button == nullptr) {
			grid_map_editor->select_mode_button->set_pressed(true);
		}
		grid_map_editor->_on_tool_mode_changed();
		panel_button->show();
		EditorNode::get_bottom_panel()->make_item_visible(grid_map_editor);
		grid_map_editor->set_process(true);
	} else {
		grid_map_editor->_show_viewports_transform_gizmo(true);
		panel_button->hide();
		if (grid_map_editor->is_visible_in_tree()) {
			EditorNode::get_bottom_panel()->hide_bottom_panel();
		}
		grid_map_editor->set_process(false);
	}
}

GridMap *GridMapEditorPlugin::get_current_grid_map() const {
	ERR_FAIL_NULL_V(grid_map_editor, nullptr);
	return grid_map_editor->node;
}

void GridMapEditorPlugin::set_selection(const Vector3i &p_begin, const Vector3i &p_end) {
	ERR_FAIL_NULL(grid_map_editor);
	grid_map_editor->_set_selection(true, p_begin, p_end);
}

void GridMapEditorPlugin::clear_selection() {
	ERR_FAIL_NULL(grid_map_editor);
	grid_map_editor->_set_selection(false);
}

AABB GridMapEditorPlugin::get_selection() const {
	ERR_FAIL_NULL_V(grid_map_editor, AABB());
	return grid_map_editor->_get_selection();
}

bool GridMapEditorPlugin::has_selection() const {
	ERR_FAIL_NULL_V(grid_map_editor, false);
	return grid_map_editor->_has_selection();
}

Array GridMapEditorPlugin::get_selected_cells() const {
	ERR_FAIL_NULL_V(grid_map_editor, Array());
	return grid_map_editor->_get_selected_cells();
}

void GridMapEditorPlugin::set_selected_palette_item(int p_item) const {
	ERR_FAIL_NULL(grid_map_editor);
	if (grid_map_editor->node && grid_map_editor->node->get_mesh_library().is_valid()) {
		if (p_item < -1) {
			p_item = -1;
		} else if (p_item >= grid_map_editor->node->get_mesh_library()->get_item_list().size()) {
			p_item = grid_map_editor->node->get_mesh_library()->get_item_list().size() - 1;
		}
		if (p_item != grid_map_editor->selected_palette) {
			grid_map_editor->selected_palette = p_item;
			grid_map_editor->update_palette();
		}
	}
}

int GridMapEditorPlugin::get_selected_palette_item() const {
	ERR_FAIL_NULL_V(grid_map_editor, 0);
	if (grid_map_editor->selected_palette >= 0 && grid_map_editor->node && grid_map_editor->node->get_mesh_library().is_valid()) {
		return grid_map_editor->selected_palette;
	} else {
		return -1;
	}
}

GridMapEditorPlugin::GridMapEditorPlugin() {
}

GridMapEditorPlugin::~GridMapEditorPlugin() {
}

#endif // TOOLS_ENABLED

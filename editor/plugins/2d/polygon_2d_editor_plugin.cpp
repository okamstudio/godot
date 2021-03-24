/**************************************************************************/
/*  polygon_2d_editor_plugin.cpp                                          */
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

#include "polygon_2d_editor_plugin.h"

#include "core/input/input_event.h"
#include "core/math/geometry_2d.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/editor_zoom_widget.h"
#include "editor/plugins/canvas_item_editor_plugin.h"
#include "editor/themes/editor_scale.h"
#include "scene/2d/skeleton_2d.h"
#include "scene/gui/check_box.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/label.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/panel.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/slider.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/split_container.h"
#include "scene/gui/view_panner.h"

class UVEditDialog : public AcceptDialog {
	GDCLASS(UVEditDialog, AcceptDialog);

	void shortcut_input(const Ref<InputEvent> &p_event) override {
		const Ref<InputEventKey> k = p_event;
		if (k.is_valid() && k->is_pressed()) {
			bool handled = false;

			if (ED_IS_SHORTCUT("ui_undo", p_event)) {
				EditorNode::get_singleton()->undo();
				handled = true;
			}

			if (ED_IS_SHORTCUT("ui_redo", p_event)) {
				EditorNode::get_singleton()->redo();
				handled = true;
			}

			if (handled) {
				set_input_as_handled();
			}
		}
	}
};

Node2D *Polygon2DEditor::_get_node() const {
	return node;
}

void Polygon2DEditor::_set_node(Node *p_polygon) {
	node = Object::cast_to<Polygon2D>(p_polygon);
	_update_polygon_editing_state();
}

Vector2 Polygon2DEditor::_get_offset(int p_idx) const {
	return node->get_offset();
}

int Polygon2DEditor::_get_polygon_count() const {
	if (node->get_internal_vertex_count() > 0) {
		return 0; //do not edit if internal vertices exist
	} else {
		return 1;
	}
}

void Polygon2DEditor::_notification(int p_what) {
	switch (p_what) {
		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (!EditorSettings::get_singleton()->check_changed_settings_in_group("editors/panning")) {
				break;
			}
			[[fallthrough]];
		}
		case NOTIFICATION_ENTER_TREE: {
			uv_panner->setup((ViewPanner::ControlScheme)EDITOR_GET("editors/panning/sub_editors_panning_scheme").operator int(), ED_GET_SHORTCUT("canvas_item_editor/pan_view"), bool(EDITOR_GET("editors/panning/simple_panning")));
			uv_panner->setup_warped_panning(get_viewport(), EDITOR_GET("editors/panning/warped_mouse_panning"));
		} break;

		case NOTIFICATION_READY: {
			button_uv->set_button_icon(get_editor_theme_icon(SNAME("Uv")));

			uv_button[UV_MODE_CREATE]->set_button_icon(get_editor_theme_icon(SNAME("Edit")));
			uv_button[UV_MODE_CREATE_INTERNAL]->set_button_icon(get_editor_theme_icon(SNAME("EditInternal")));
			uv_button[UV_MODE_REMOVE_INTERNAL]->set_button_icon(get_editor_theme_icon(SNAME("RemoveInternal")));
			uv_button[UV_MODE_EDIT_POINT]->set_button_icon(get_editor_theme_icon(SNAME("ToolSelect")));
			uv_button[UV_MODE_MOVE]->set_button_icon(get_editor_theme_icon(SNAME("ToolMove")));
			uv_button[UV_MODE_ROTATE]->set_button_icon(get_editor_theme_icon(SNAME("ToolRotate")));
			uv_button[UV_MODE_SCALE]->set_button_icon(get_editor_theme_icon(SNAME("ToolScale")));
			uv_button[UV_MODE_ADD_POLYGON]->set_button_icon(get_editor_theme_icon(SNAME("Edit")));
			uv_button[UV_MODE_REMOVE_POLYGON]->set_button_icon(get_editor_theme_icon(SNAME("Close")));
			uv_button[UV_MODE_PAINT_WEIGHT]->set_button_icon(get_editor_theme_icon(SNAME("Bucket")));
			uv_button[UV_MODE_CLEAR_WEIGHT]->set_button_icon(get_editor_theme_icon(SNAME("Clear")));

			b_snap_grid->set_button_icon(get_editor_theme_icon(SNAME("Grid")));
			b_snap_enable->set_button_icon(get_editor_theme_icon(SNAME("SnapGrid")));

			uv_vscroll->set_anchors_and_offsets_preset(PRESET_RIGHT_WIDE);
			uv_hscroll->set_anchors_and_offsets_preset(PRESET_BOTTOM_WIDE);
			// Avoid scrollbar overlapping.
			Size2 hmin = uv_hscroll->get_combined_minimum_size();
			Size2 vmin = uv_vscroll->get_combined_minimum_size();
			uv_hscroll->set_anchor_and_offset(SIDE_RIGHT, ANCHOR_END, -vmin.width);
			uv_vscroll->set_anchor_and_offset(SIDE_BOTTOM, ANCHOR_END, -hmin.height);
			[[fallthrough]];
		}
		case NOTIFICATION_THEME_CHANGED: {
			uv_edit_draw->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SceneStringName(panel), SNAME("Tree")));
			bone_scroll->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SceneStringName(panel), SNAME("Tree")));
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_visible()) {
				uv_edit->hide();
			}
		} break;
	}
}

void Polygon2DEditor::_sync_bones() {
	Skeleton2D *skeleton = nullptr;
	if (!node->has_node(node->get_skeleton())) {
		error->set_text(TTR("The skeleton property of the Polygon2D does not point to a Skeleton2D node"));
		error->popup_centered();
	} else {
		Node *sn = node->get_node(node->get_skeleton());
		skeleton = Object::cast_to<Skeleton2D>(sn);
	}

	Array prev_bones = node->call("_get_bones");
	node->clear_bones();

	if (!skeleton) {
		error->set_text(TTR("The skeleton property of the Polygon2D does not point to a Skeleton2D node"));
		error->popup_centered();
	} else {
		for (int i = 0; i < skeleton->get_bone_count(); i++) {
			NodePath path = skeleton->get_path_to(skeleton->get_bone(i));
			Vector<float> weights;
			int wc = node->get_polygon().size();

			for (int j = 0; j < prev_bones.size(); j += 2) {
				NodePath pvp = prev_bones[j];
				Vector<float> pv = prev_bones[j + 1];
				if (pvp == path && pv.size() == wc) {
					weights = pv;
				}
			}

			if (weights.size() == 0) { //create them
				weights.resize(wc);
				float *w = weights.ptrw();
				for (int j = 0; j < wc; j++) {
					w[j] = 0.0;
				}
			}

			node->add_bone(path, weights);
		}
	}

	Array new_bones = node->call("_get_bones");

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Sync Bones"));
	undo_redo->add_do_method(node, "_set_bones", new_bones);
	undo_redo->add_undo_method(node, "_set_bones", prev_bones);
	undo_redo->add_do_method(this, "_update_bone_list");
	undo_redo->add_undo_method(this, "_update_bone_list");
	undo_redo->add_do_method(uv_edit_draw, "queue_redraw");
	undo_redo->add_undo_method(uv_edit_draw, "queue_redraw");
	undo_redo->commit_action();
}

void Polygon2DEditor::_update_bone_list() {
	NodePath selected;
	while (bone_scroll_vb->get_child_count()) {
		CheckBox *cb = Object::cast_to<CheckBox>(bone_scroll_vb->get_child(0));
		if (cb && cb->is_pressed()) {
			selected = cb->get_meta("bone_path");
		}
		memdelete(bone_scroll_vb->get_child(0));
	}

	Ref<ButtonGroup> bg;
	bg.instantiate();
	for (int i = 0; i < node->get_bone_count(); i++) {
		CheckBox *cb = memnew(CheckBox);
		NodePath np = node->get_bone_path(i);
		String name;
		if (np.get_name_count()) {
			name = np.get_name(np.get_name_count() - 1);
		}
		if (name.is_empty()) {
			name = "Bone " + itos(i);
		}
		cb->set_text(name);
		cb->set_button_group(bg);
		cb->set_meta("bone_path", np);
		cb->set_focus_mode(FOCUS_NONE);
		bone_scroll_vb->add_child(cb);

		if (np == selected || bone_scroll_vb->get_child_count() < 2) {
			cb->set_pressed(true);
		}

		cb->connect(SceneStringName(pressed), callable_mp(this, &Polygon2DEditor::_bone_paint_selected).bind(i));
	}

	uv_edit_draw->queue_redraw();
}

void Polygon2DEditor::_bone_paint_selected(int p_index) {
	uv_edit_draw->queue_redraw();
}

void Polygon2DEditor::_uv_edit_mode_select(int p_mode) {
	if (p_mode == 0) { //uv

		uv_button[UV_MODE_CREATE]->hide();
		uv_button[UV_MODE_CREATE_INTERNAL]->hide();
		uv_button[UV_MODE_REMOVE_INTERNAL]->hide();
		for (int i = UV_MODE_EDIT_POINT; i <= UV_MODE_SCALE; i++) {
			uv_button[i]->show();
		}
		uv_button[UV_MODE_ADD_POLYGON]->hide();
		uv_button[UV_MODE_REMOVE_POLYGON]->hide();
		uv_button[UV_MODE_PAINT_WEIGHT]->hide();
		uv_button[UV_MODE_CLEAR_WEIGHT]->hide();
		_uv_mode(UV_MODE_EDIT_POINT);

		bone_scroll_main_vb->hide();
		bone_paint_strength->hide();
		bone_paint_radius->hide();
		bone_paint_radius_label->hide();
	} else if (p_mode == 1) { //poly

		for (int i = 0; i <= UV_MODE_SCALE; i++) {
			uv_button[i]->show();
		}
		uv_button[UV_MODE_ADD_POLYGON]->hide();
		uv_button[UV_MODE_REMOVE_POLYGON]->hide();
		uv_button[UV_MODE_PAINT_WEIGHT]->hide();
		uv_button[UV_MODE_CLEAR_WEIGHT]->hide();
		if (node->get_polygon().is_empty()) {
			_uv_mode(UV_MODE_CREATE);
		} else {
			_uv_mode(UV_MODE_EDIT_POINT);
		}

		bone_scroll_main_vb->hide();
		bone_paint_strength->hide();
		bone_paint_radius->hide();
		bone_paint_radius_label->hide();
	} else if (p_mode == 2) { //splits

		for (int i = 0; i <= UV_MODE_SCALE; i++) {
			uv_button[i]->hide();
		}
		uv_button[UV_MODE_ADD_POLYGON]->show();
		uv_button[UV_MODE_REMOVE_POLYGON]->show();
		uv_button[UV_MODE_PAINT_WEIGHT]->hide();
		uv_button[UV_MODE_CLEAR_WEIGHT]->hide();
		_uv_mode(UV_MODE_ADD_POLYGON);

		bone_scroll_main_vb->hide();
		bone_paint_strength->hide();
		bone_paint_radius->hide();
		bone_paint_radius_label->hide();
	} else if (p_mode == 3) { //bones´

		for (int i = 0; i <= UV_MODE_REMOVE_POLYGON; i++) {
			uv_button[i]->hide();
		}
		uv_button[UV_MODE_PAINT_WEIGHT]->show();
		uv_button[UV_MODE_CLEAR_WEIGHT]->show();
		_uv_mode(UV_MODE_PAINT_WEIGHT);

		bone_scroll_main_vb->show();
		bone_paint_strength->show();
		bone_paint_radius->show();
		bone_paint_radius_label->show();
		_update_bone_list();
		bone_paint_pos = Vector2(-100000, -100000); //send brush away when switching
	}

	uv_edit->set_size(uv_edit->get_size()); // Necessary readjustment of the popup window.
	uv_edit_draw->queue_redraw();
}

void Polygon2DEditor::_uv_edit_popup_show() {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->connect("version_changed", callable_mp(this, &Polygon2DEditor::_update_available_modes));
}

void Polygon2DEditor::_uv_edit_popup_hide() {
	EditorSettings::get_singleton()->set_project_metadata("dialog_bounds", "uv_editor", Rect2(uv_edit->get_position(), uv_edit->get_size()));
	_cancel_editing();
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->disconnect("version_changed", callable_mp(this, &Polygon2DEditor::_update_available_modes));
}

void Polygon2DEditor::_menu_option(int p_option) {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	switch (p_option) {
		case MODE_EDIT_UV: {
			uv_edit_draw->set_texture_filter(node->get_texture_filter_in_tree());

			Vector<Vector2> points = node->get_polygon();
			Vector<Vector2> uvs = node->get_uv();
			if (uvs.size() != points.size()) {
				undo_redo->create_action(TTR("Create UV Map"));
				undo_redo->add_do_method(node, "set_uv", points);
				undo_redo->add_undo_method(node, "set_uv", uvs);
				undo_redo->add_do_method(uv_edit_draw, "queue_redraw");
				undo_redo->add_undo_method(uv_edit_draw, "queue_redraw");
				undo_redo->commit_action();
			}

			const Rect2 bounds = EditorSettings::get_singleton()->get_project_metadata("dialog_bounds", "uv_editor", Rect2());
			if (bounds.has_area()) {
				uv_edit->popup(bounds);
			} else {
				uv_edit->popup_centered_ratio(0.85);
			}
			_update_bone_list();
			_update_available_modes();
			get_tree()->connect("process_frame", callable_mp(this, &Polygon2DEditor::_center_view), CONNECT_ONE_SHOT);
		} break;
		case UVEDIT_POLYGON_TO_UV: {
			Vector<Vector2> points = node->get_polygon();
			if (points.size() == 0) {
				break;
			}
			Vector<Vector2> uvs = node->get_uv();
			undo_redo->create_action(TTR("Create UV Map"));
			undo_redo->add_do_method(node, "set_uv", points);
			undo_redo->add_undo_method(node, "set_uv", uvs);
			undo_redo->add_do_method(uv_edit_draw, "queue_redraw");
			undo_redo->add_undo_method(uv_edit_draw, "queue_redraw");
			undo_redo->commit_action();
		} break;
		case UVEDIT_UV_TO_POLYGON: {
			Vector<Vector2> points = node->get_polygon();
			Vector<Vector2> uvs = node->get_uv();
			if (uvs.size() == 0) {
				break;
			}

			undo_redo->create_action(TTR("Create Polygon"));
			undo_redo->add_do_method(node, "set_polygon", uvs);
			undo_redo->add_undo_method(node, "set_polygon", points);
			undo_redo->add_do_method(uv_edit_draw, "queue_redraw");
			undo_redo->add_undo_method(uv_edit_draw, "queue_redraw");
			undo_redo->commit_action();
		} break;
		case UVEDIT_UV_CLEAR: {
			Vector<Vector2> uvs = node->get_uv();
			if (uvs.size() == 0) {
				break;
			}
			undo_redo->create_action(TTR("Create UV Map"));
			undo_redo->add_do_method(node, "set_uv", Vector<Vector2>());
			undo_redo->add_undo_method(node, "set_uv", uvs);
			undo_redo->add_do_method(uv_edit_draw, "queue_redraw");
			undo_redo->add_undo_method(uv_edit_draw, "queue_redraw");
			undo_redo->commit_action();
		} break;
		case UVEDIT_GRID_SETTINGS: {
			grid_settings->popup_centered();
		} break;
		default: {
			AbstractPolygon2DEditor::_menu_option(p_option);
		} break;
	}
}

void Polygon2DEditor::_cancel_editing() {
	if (uv_create) {
		uv_drag = false;
		uv_create = false;
		node->set_uv(uv_create_uv_prev);
		node->set_polygon(uv_create_poly_prev);
		node->set_internal_vertex_count(uv_create_prev_internal_vertices);
		node->set_vertex_colors(uv_create_colors_prev);
		node->call("_set_bones", uv_create_bones_prev);
		node->set_polygons(polygons_prev);

		_update_polygon_editing_state();
		_update_available_modes();
	} else if (uv_drag) {
		uv_drag = false;
		if (uv_edit_mode[0]->is_pressed()) { // Edit UV.
			node->set_uv(points_prev);
		} else if (uv_edit_mode[1]->is_pressed()) { // Edit polygon.
			node->set_polygon(points_prev);
		}
	}

	polygon_create.clear();
}

void Polygon2DEditor::_update_polygon_editing_state() {
	if (!_get_node()) {
		return;
	}

	if (node->get_internal_vertex_count() > 0) {
		disable_polygon_editing(true, TTR("Polygon 2D has internal vertices, so it can no longer be edited in the viewport."));
	} else {
		disable_polygon_editing(false, String());
	}
}

void Polygon2DEditor::_commit_action() {
	// Makes that undo/redoing actions made outside of the UV editor still affect its polygon.
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->add_do_method(uv_edit_draw, "queue_redraw");
	undo_redo->add_undo_method(uv_edit_draw, "queue_redraw");
	undo_redo->add_do_method(CanvasItemEditor::get_singleton(), "update_viewport");
	undo_redo->add_undo_method(CanvasItemEditor::get_singleton(), "update_viewport");
	undo_redo->commit_action();
}

void Polygon2DEditor::_set_use_snap(bool p_use) {
	use_snap = p_use;
	EditorSettings::get_singleton()->set_project_metadata("polygon_2d_uv_editor", "snap_enabled", p_use);
}

void Polygon2DEditor::_set_show_grid(bool p_show) {
	snap_show_grid = p_show;
	EditorSettings::get_singleton()->set_project_metadata("polygon_2d_uv_editor", "show_grid", p_show);
	uv_edit_draw->queue_redraw();
}

void Polygon2DEditor::_set_snap_off_x(real_t p_val) {
	snap_offset.x = p_val;
	EditorSettings::get_singleton()->set_project_metadata("polygon_2d_uv_editor", "snap_offset", snap_offset);
	uv_edit_draw->queue_redraw();
}

void Polygon2DEditor::_set_snap_off_y(real_t p_val) {
	snap_offset.y = p_val;
	EditorSettings::get_singleton()->set_project_metadata("polygon_2d_uv_editor", "snap_offset", snap_offset);
	uv_edit_draw->queue_redraw();
}

void Polygon2DEditor::_set_snap_step_x(real_t p_val) {
	snap_step.x = p_val;
	EditorSettings::get_singleton()->set_project_metadata("polygon_2d_uv_editor", "snap_step", snap_step);
	uv_edit_draw->queue_redraw();
}

void Polygon2DEditor::_set_snap_step_y(real_t p_val) {
	snap_step.y = p_val;
	EditorSettings::get_singleton()->set_project_metadata("polygon_2d_uv_editor", "snap_step", snap_step);
	uv_edit_draw->queue_redraw();
}

void Polygon2DEditor::_uv_mode(int p_mode) {
	polygon_create.clear();
	uv_drag = false;
	uv_create = false;

	uv_mode = UVMode(p_mode);
	for (int i = 0; i < UV_MODE_MAX; i++) {
		uv_button[i]->set_pressed(p_mode == i);
	}
	uv_edit_draw->queue_redraw();
}

void Polygon2DEditor::_uv_input(const Ref<InputEvent> &p_input) {
	if (!_get_node()) {
		return;
	}

	if (uv_panner->gui_input(p_input, uv_edit_draw->get_global_rect())) {
		accept_event();
		return;
	}

	Transform2D mtx;
	mtx.columns[2] = -uv_draw_ofs * uv_draw_zoom;
	mtx.scale_basis(Vector2(uv_draw_zoom, uv_draw_zoom));

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();

	Ref<InputEventMouseButton> mb = p_input;
	if (mb.is_valid()) {
		if (mb->get_button_index() == MouseButton::LEFT) {
			if (mb->is_pressed()) {
				uv_drag_from = snap_point(mb->get_position());
				uv_drag = true;
				points_prev = node->get_uv();

				if (uv_edit_mode[0]->is_pressed()) { //edit uv
					points_prev = node->get_uv();
				} else { //edit polygon
					points_prev = node->get_polygon();
				}

				uv_move_current = uv_mode;
				if (uv_move_current == UV_MODE_CREATE) {
					if (!uv_create) {
						points_prev.clear();
						Vector2 tuv = mtx.affine_inverse().xform(snap_point(mb->get_position()));
						points_prev.push_back(tuv);
						uv_create_to = tuv;
						point_drag_index = 0;
						uv_drag_from = tuv;
						uv_drag = true;
						uv_create = true;
						uv_create_uv_prev = node->get_uv();
						uv_create_poly_prev = node->get_polygon();
						uv_create_prev_internal_vertices = node->get_internal_vertex_count();
						uv_create_colors_prev = node->get_vertex_colors();
						uv_create_bones_prev = node->call("_get_bones");
						polygons_prev = node->get_polygons();
						disable_polygon_editing(false, String());
						node->set_polygon(points_prev);
						node->set_uv(points_prev);
						node->set_internal_vertex_count(0);

						uv_edit_draw->queue_redraw();
					} else {
						Vector2 tuv = mtx.affine_inverse().xform(snap_point(mb->get_position()));

						// Close the polygon if selected point is near start. Threshold for closing scaled by zoom level
						if (points_prev.size() > 2 && tuv.distance_to(points_prev[0]) < (8 / uv_draw_zoom)) {
							undo_redo->create_action(TTR("Create Polygon & UV"));
							undo_redo->add_do_method(node, "set_uv", node->get_uv());
							undo_redo->add_undo_method(node, "set_uv", uv_create_uv_prev);
							undo_redo->add_do_method(node, "set_polygon", node->get_polygon());
							undo_redo->add_undo_method(node, "set_polygon", uv_create_poly_prev);
							undo_redo->add_do_method(node, "set_internal_vertex_count", 0);
							undo_redo->add_undo_method(node, "set_internal_vertex_count", uv_create_prev_internal_vertices);
							undo_redo->add_do_method(node, "set_vertex_colors", Vector<Color>());
							undo_redo->add_undo_method(node, "set_vertex_colors", uv_create_colors_prev);
							undo_redo->add_do_method(node, "clear_bones");
							undo_redo->add_undo_method(node, "_set_bones", uv_create_bones_prev);
							undo_redo->add_do_method(this, "_update_polygon_editing_state");
							undo_redo->add_undo_method(this, "_update_polygon_editing_state");
							undo_redo->add_do_method(uv_edit_draw, "queue_redraw");
							undo_redo->add_undo_method(uv_edit_draw, "queue_redraw");
							undo_redo->commit_action();
							uv_drag = false;
							uv_create = false;

							_update_available_modes();
							_uv_mode(UV_MODE_EDIT_POINT);
							_menu_option(MODE_EDIT);
						} else {
							points_prev.push_back(tuv);
							point_drag_index = points_prev.size() - 1;
							uv_drag_from = tuv;
						}
						node->set_polygon(points_prev);
						node->set_uv(points_prev);
					}

					CanvasItemEditor::get_singleton()->update_viewport();
				}

				if (uv_move_current == UV_MODE_CREATE_INTERNAL) {
					uv_create_uv_prev = node->get_uv();
					uv_create_poly_prev = node->get_polygon();
					uv_create_colors_prev = node->get_vertex_colors();
					uv_create_bones_prev = node->call("_get_bones");
					int internal_vertices = node->get_internal_vertex_count();

					Vector2 pos = mtx.affine_inverse().xform(snap_point(mb->get_position()));

					uv_create_poly_prev.push_back(pos);
					uv_create_uv_prev.push_back(pos);
					if (uv_create_colors_prev.size()) {
						uv_create_colors_prev.push_back(Color(1, 1, 1));
					}

					undo_redo->create_action(TTR("Create Internal Vertex"));
					undo_redo->add_do_method(node, "set_uv", uv_create_uv_prev);
					undo_redo->add_undo_method(node, "set_uv", node->get_uv());
					undo_redo->add_do_method(node, "set_polygon", uv_create_poly_prev);
					undo_redo->add_undo_method(node, "set_polygon", node->get_polygon());
					undo_redo->add_do_method(node, "set_vertex_colors", uv_create_colors_prev);
					undo_redo->add_undo_method(node, "set_vertex_colors", node->get_vertex_colors());
					for (int i = 0; i < node->get_bone_count(); i++) {
						Vector<float> bonew = node->get_bone_weights(i);
						bonew.push_back(0);
						undo_redo->add_do_method(node, "set_bone_weights", i, bonew);
						undo_redo->add_undo_method(node, "set_bone_weights", i, node->get_bone_weights(i));
					}
					undo_redo->add_do_method(node, "set_internal_vertex_count", internal_vertices + 1);
					undo_redo->add_undo_method(node, "set_internal_vertex_count", internal_vertices);
					undo_redo->add_do_method(this, "_update_polygon_editing_state");
					undo_redo->add_undo_method(this, "_update_polygon_editing_state");
					undo_redo->add_do_method(uv_edit_draw, "queue_redraw");
					undo_redo->add_undo_method(uv_edit_draw, "queue_redraw");
					undo_redo->commit_action();
				}

				if (uv_move_current == UV_MODE_REMOVE_INTERNAL) {
					uv_create_uv_prev = node->get_uv();
					uv_create_poly_prev = node->get_polygon();
					uv_create_colors_prev = node->get_vertex_colors();
					uv_create_bones_prev = node->call("_get_bones");
					int internal_vertices = node->get_internal_vertex_count();

					if (internal_vertices <= 0) {
						return;
					}

					int closest = -1;
					real_t closest_dist = 1e20;

					for (int i = points_prev.size() - internal_vertices; i < points_prev.size(); i++) {
						Vector2 tuv = mtx.xform(uv_create_poly_prev[i]);
						real_t dist = tuv.distance_to(mb->get_position());
						if (dist < 8 && dist < closest_dist) {
							closest = i;
							closest_dist = dist;
						}
					}

					if (closest == -1) {
						return;
					}

					uv_create_poly_prev.remove_at(closest);
					uv_create_uv_prev.remove_at(closest);
					if (uv_create_colors_prev.size()) {
						uv_create_colors_prev.remove_at(closest);
					}

					undo_redo->create_action(TTR("Remove Internal Vertex"));
					undo_redo->add_do_method(node, "set_uv", uv_create_uv_prev);
					undo_redo->add_undo_method(node, "set_uv", node->get_uv());
					undo_redo->add_do_method(node, "set_polygon", uv_create_poly_prev);
					undo_redo->add_undo_method(node, "set_polygon", node->get_polygon());
					undo_redo->add_do_method(node, "set_vertex_colors", uv_create_colors_prev);
					undo_redo->add_undo_method(node, "set_vertex_colors", node->get_vertex_colors());
					for (int i = 0; i < node->get_bone_count(); i++) {
						Vector<float> bonew = node->get_bone_weights(i);
						bonew.remove_at(closest);
						undo_redo->add_do_method(node, "set_bone_weights", i, bonew);
						undo_redo->add_undo_method(node, "set_bone_weights", i, node->get_bone_weights(i));
					}
					undo_redo->add_do_method(node, "set_internal_vertex_count", internal_vertices - 1);
					undo_redo->add_undo_method(node, "set_internal_vertex_count", internal_vertices);
					undo_redo->add_do_method(this, "_update_polygon_editing_state");
					undo_redo->add_undo_method(this, "_update_polygon_editing_state");
					undo_redo->add_do_method(uv_edit_draw, "queue_redraw");
					undo_redo->add_undo_method(uv_edit_draw, "queue_redraw");
					undo_redo->commit_action();
				}

				if (uv_move_current == UV_MODE_EDIT_POINT) {
					if (mb->is_shift_pressed() && mb->is_command_or_control_pressed()) {
						uv_move_current = UV_MODE_SCALE;
					} else if (mb->is_shift_pressed()) {
						uv_move_current = UV_MODE_MOVE;
					} else if (mb->is_command_or_control_pressed()) {
						uv_move_current = UV_MODE_ROTATE;
					}
				}

				if (uv_move_current == UV_MODE_EDIT_POINT) {
					point_drag_index = -1;
					for (int i = 0; i < points_prev.size(); i++) {
						Vector2 tuv = mtx.xform(points_prev[i]);
						if (tuv.distance_to(mb->get_position()) < 8) {
							uv_drag_from = tuv;
							point_drag_index = i;
						}
					}

					if (point_drag_index == -1) {
						uv_drag = false;
					}
				}

				if (uv_move_current == UV_MODE_ADD_POLYGON) {
					int closest = -1;
					real_t closest_dist = 1e20;

					for (int i = 0; i < points_prev.size(); i++) {
						Vector2 tuv = mtx.xform(points_prev[i]);
						real_t dist = tuv.distance_to(mb->get_position());
						if (dist < 8 && dist < closest_dist) {
							closest = i;
							closest_dist = dist;
						}
					}

					if (closest != -1) {
						if (polygon_create.size() && closest == polygon_create[0]) {
							//close
							if (polygon_create.size() < 3) {
								error->set_text(TTR("Invalid Polygon (need 3 different vertices)"));
								error->popup_centered();
							} else {
								Array polygons = node->get_polygons();
								polygons = polygons.duplicate(); //copy because its a reference

								//todo, could check whether it already exists?
								polygons.push_back(polygon_create);
								undo_redo->create_action(TTR("Add Custom Polygon"));
								undo_redo->add_do_method(node, "set_polygons", polygons);
								undo_redo->add_undo_method(node, "set_polygons", node->get_polygons());
								undo_redo->add_do_method(uv_edit_draw, "queue_redraw");
								undo_redo->add_undo_method(uv_edit_draw, "queue_redraw");
								undo_redo->commit_action();
							}

							polygon_create.clear();
						} else if (!polygon_create.has(closest)) {
							//add temporarily if not exists
							polygon_create.push_back(closest);
						}
					}
				}

				if (uv_move_current == UV_MODE_REMOVE_POLYGON) {
					Array polygons = node->get_polygons();
					polygons = polygons.duplicate(); //copy because its a reference

					int erase_index = -1;
					for (int i = polygons.size() - 1; i >= 0; i--) {
						Vector<int> points = polygons[i];
						Vector<Vector2> polys;
						polys.resize(points.size());
						for (int j = 0; j < polys.size(); j++) {
							int idx = points[j];
							if (idx < 0 || idx >= points_prev.size()) {
								continue;
							}
							polys.write[j] = mtx.xform(points_prev[idx]);
						}

						if (Geometry2D::is_point_in_polygon(mb->get_position(), polys)) {
							erase_index = i;
							break;
						}
					}

					if (erase_index != -1) {
						polygons.remove_at(erase_index);
						undo_redo->create_action(TTR("Remove Custom Polygon"));
						undo_redo->add_do_method(node, "set_polygons", polygons);
						undo_redo->add_undo_method(node, "set_polygons", node->get_polygons());
						undo_redo->add_do_method(uv_edit_draw, "queue_redraw");
						undo_redo->add_undo_method(uv_edit_draw, "queue_redraw");
						undo_redo->commit_action();
					}
				}

				if (uv_move_current == UV_MODE_PAINT_WEIGHT || uv_move_current == UV_MODE_CLEAR_WEIGHT) {
					int bone_selected = -1;
					for (int i = 0; i < bone_scroll_vb->get_child_count(); i++) {
						CheckBox *c = Object::cast_to<CheckBox>(bone_scroll_vb->get_child(i));
						if (c && c->is_pressed()) {
							bone_selected = i;
							break;
						}
					}

					if (bone_selected != -1 && node->get_bone_weights(bone_selected).size() == points_prev.size()) {
						prev_weights = node->get_bone_weights(bone_selected);
						bone_painting = true;
						bone_painting_bone = bone_selected;
					}
				}
			} else {
				if (uv_drag && !uv_create) {
					if (uv_edit_mode[0]->is_pressed()) {
						undo_redo->create_action(TTR("Transform UV Map"));
						undo_redo->add_do_method(node, "set_uv", node->get_uv());
						undo_redo->add_undo_method(node, "set_uv", points_prev);
						undo_redo->add_do_method(uv_edit_draw, "queue_redraw");
						undo_redo->add_undo_method(uv_edit_draw, "queue_redraw");
						undo_redo->commit_action();
					} else if (uv_edit_mode[1]->is_pressed()) {
						switch (uv_move_current) {
							case UV_MODE_EDIT_POINT:
							case UV_MODE_MOVE:
							case UV_MODE_ROTATE:
							case UV_MODE_SCALE: {
								undo_redo->create_action(TTR("Transform Polygon"));
								undo_redo->add_do_method(node, "set_polygon", node->get_polygon());
								undo_redo->add_undo_method(node, "set_polygon", points_prev);
								undo_redo->add_do_method(uv_edit_draw, "queue_redraw");
								undo_redo->add_undo_method(uv_edit_draw, "queue_redraw");
								undo_redo->commit_action();
							} break;
							default: {
							} break;
						}
					}

					uv_drag = false;
				}

				if (bone_painting) {
					undo_redo->create_action(TTR("Paint Bone Weights"));
					undo_redo->add_do_method(node, "set_bone_weights", bone_painting_bone, node->get_bone_weights(bone_painting_bone));
					undo_redo->add_undo_method(node, "set_bone_weights", bone_painting_bone, prev_weights);
					undo_redo->add_do_method(uv_edit_draw, "queue_redraw");
					undo_redo->add_undo_method(uv_edit_draw, "queue_redraw");
					undo_redo->commit_action();
					bone_painting = false;
				}
			}
		} else if (mb->get_button_index() == MouseButton::RIGHT && mb->is_pressed()) {
			_cancel_editing();

			if (bone_painting) {
				node->set_bone_weights(bone_painting_bone, prev_weights);
			}

			uv_edit_draw->queue_redraw();
		}
	}

	Ref<InputEventMouseMotion> mm = p_input;

	if (mm.is_valid()) {
		if (uv_drag) {
			Vector2 uv_drag_to = mm->get_position();
			uv_drag_to = snap_point(uv_drag_to);
			Vector2 drag = mtx.affine_inverse().basis_xform(uv_drag_to - uv_drag_from);

			switch (uv_move_current) {
				case UV_MODE_CREATE: {
					if (uv_create) {
						uv_create_to = mtx.affine_inverse().xform(snap_point(mm->get_position()));
					}
				} break;
				case UV_MODE_EDIT_POINT: {
					Vector<Vector2> uv_new = points_prev;
					uv_new.set(point_drag_index, uv_new[point_drag_index] + drag);

					if (uv_edit_mode[0]->is_pressed()) { //edit uv
						node->set_uv(uv_new);
					} else if (uv_edit_mode[1]->is_pressed()) { //edit polygon
						node->set_polygon(uv_new);
					}
				} break;
				case UV_MODE_MOVE: {
					Vector<Vector2> uv_new = points_prev;
					for (int i = 0; i < uv_new.size(); i++) {
						uv_new.set(i, uv_new[i] + drag);
					}

					if (uv_edit_mode[0]->is_pressed()) { //edit uv
						node->set_uv(uv_new);
					} else if (uv_edit_mode[1]->is_pressed()) { //edit polygon
						node->set_polygon(uv_new);
					}
				} break;
				case UV_MODE_ROTATE: {
					Vector2 center;
					Vector<Vector2> uv_new = points_prev;

					for (int i = 0; i < uv_new.size(); i++) {
						center += points_prev[i];
					}
					center /= uv_new.size();

					real_t angle = (uv_drag_from - mtx.xform(center)).normalized().angle_to((uv_drag_to - mtx.xform(center)).normalized());

					for (int i = 0; i < uv_new.size(); i++) {
						Vector2 rel = points_prev[i] - center;
						rel = rel.rotated(angle);
						uv_new.set(i, center + rel);
					}

					if (uv_edit_mode[0]->is_pressed()) { //edit uv
						node->set_uv(uv_new);
					} else if (uv_edit_mode[1]->is_pressed()) { //edit polygon
						node->set_polygon(uv_new);
					}
				} break;
				case UV_MODE_SCALE: {
					Vector2 center;
					Vector<Vector2> uv_new = points_prev;

					for (int i = 0; i < uv_new.size(); i++) {
						center += points_prev[i];
					}
					center /= uv_new.size();

					real_t from_dist = uv_drag_from.distance_to(mtx.xform(center));
					real_t to_dist = uv_drag_to.distance_to(mtx.xform(center));
					if (from_dist < 2) {
						break;
					}

					real_t scale = to_dist / from_dist;

					for (int i = 0; i < uv_new.size(); i++) {
						Vector2 rel = points_prev[i] - center;
						rel = rel * scale;
						uv_new.set(i, center + rel);
					}

					if (uv_edit_mode[0]->is_pressed()) { //edit uv
						node->set_uv(uv_new);
					} else if (uv_edit_mode[1]->is_pressed()) { //edit polygon
						node->set_polygon(uv_new);
					}
				} break;
				case UV_MODE_PAINT_WEIGHT:
				case UV_MODE_CLEAR_WEIGHT: {
					bone_paint_pos = mm->get_position();
				} break;
				default: {
				}
			}

			if (bone_painting) {
				Vector<float> painted_weights = node->get_bone_weights(bone_painting_bone);

				{
					int pc = painted_weights.size();
					real_t amount = bone_paint_strength->get_value();
					real_t radius = bone_paint_radius->get_value() * EDSCALE;

					if (uv_mode == UV_MODE_CLEAR_WEIGHT) {
						amount = -amount;
					}

					float *w = painted_weights.ptrw();
					const float *r = prev_weights.ptr();
					const Vector2 *rv = points_prev.ptr();

					for (int i = 0; i < pc; i++) {
						if (mtx.xform(rv[i]).distance_to(bone_paint_pos) < radius) {
							w[i] = CLAMP(r[i] + amount, 0, 1);
						}
					}
				}

				node->set_bone_weights(bone_painting_bone, painted_weights);
			}

			uv_edit_draw->queue_redraw();
			CanvasItemEditor::get_singleton()->update_viewport();
		} else if (polygon_create.size()) {
			uv_create_to = mtx.affine_inverse().xform(mm->get_position());
			uv_edit_draw->queue_redraw();
		} else if (uv_mode == UV_MODE_PAINT_WEIGHT || uv_mode == UV_MODE_CLEAR_WEIGHT) {
			bone_paint_pos = mm->get_position();
			uv_edit_draw->queue_redraw();
		}
	}
}

void Polygon2DEditor::_update_available_modes() {
	// Force point editing mode if there's no polygon yet.
	if (node->get_polygon().is_empty()) {
		if (!uv_edit_mode[1]->is_pressed()) {
			uv_edit_mode[1]->set_pressed(true);
			_uv_edit_mode_select(1);
		}
		uv_edit_mode[0]->set_disabled(true);
		uv_edit_mode[2]->set_disabled(true);
		uv_edit_mode[3]->set_disabled(true);
	} else {
		uv_edit_mode[0]->set_disabled(false);
		uv_edit_mode[2]->set_disabled(false);
		uv_edit_mode[3]->set_disabled(false);
	}
}

void Polygon2DEditor::_center_view() {
	Size2 texture_size;
	if (node->get_texture().is_valid()) {
		texture_size = node->get_texture()->get_size();
		Vector2 zoom_factor = (uv_edit_draw->get_size() - Vector2(1, 1) * 50 * EDSCALE) / texture_size;
		zoom_widget->set_zoom(MIN(zoom_factor.x, zoom_factor.y));
	} else {
		zoom_widget->set_zoom(EDSCALE);
	}
	// Recalculate scroll limits.
	_update_zoom_and_pan(false);

	Size2 offset = (texture_size - uv_edit_draw->get_size() / uv_draw_zoom) / 2;
	uv_hscroll->set_value_no_signal(offset.x);
	uv_vscroll->set_value_no_signal(offset.y);
	_update_zoom_and_pan(false);
}

void Polygon2DEditor::_uv_pan_callback(Vector2 p_scroll_vec, Ref<InputEvent> p_event) {
	uv_hscroll->set_value_no_signal(uv_hscroll->get_value() - p_scroll_vec.x / uv_draw_zoom);
	uv_vscroll->set_value_no_signal(uv_vscroll->get_value() - p_scroll_vec.y / uv_draw_zoom);
	_update_zoom_and_pan(false);
}

void Polygon2DEditor::_uv_zoom_callback(float p_zoom_factor, Vector2 p_origin, Ref<InputEvent> p_event) {
	zoom_widget->set_zoom(uv_draw_zoom * p_zoom_factor);
	uv_draw_ofs += p_origin / uv_draw_zoom - p_origin / zoom_widget->get_zoom();
	uv_hscroll->set_value_no_signal(uv_draw_ofs.x);
	uv_vscroll->set_value_no_signal(uv_draw_ofs.y);
	_update_zoom_and_pan(false);
}

void Polygon2DEditor::_update_zoom_and_pan(bool p_zoom_at_center) {
	uv_draw_ofs = Vector2(uv_hscroll->get_value(), uv_vscroll->get_value());
	real_t previous_zoom = uv_draw_zoom;
	uv_draw_zoom = zoom_widget->get_zoom();
	if (p_zoom_at_center) {
		Vector2 center = uv_edit_draw->get_size() / 2;
		uv_draw_ofs += center / previous_zoom - center / uv_draw_zoom;
	}

	Point2 min_corner;
	Point2 max_corner;
	if (node->get_texture().is_valid()) {
		max_corner += node->get_texture()->get_size();
	}

	Vector<Vector2> points = uv_edit_mode[0]->is_pressed() ? node->get_uv() : node->get_polygon();
	for (int i = 0; i < points.size(); i++) {
		min_corner = min_corner.min(points[i]);
		max_corner = max_corner.max(points[i]);
	}
	Size2 page_size = uv_edit_draw->get_size() / uv_draw_zoom;
	Vector2 margin = Vector2(50, 50) * EDSCALE / uv_draw_zoom;
	min_corner -= page_size - margin;
	max_corner += page_size - margin;

	uv_hscroll->set_block_signals(true);
	uv_hscroll->set_min(min_corner.x);
	uv_hscroll->set_max(max_corner.x);
	uv_hscroll->set_page(page_size.x);
	uv_hscroll->set_value(uv_draw_ofs.x);
	uv_hscroll->set_block_signals(false);

	uv_vscroll->set_block_signals(true);
	uv_vscroll->set_min(min_corner.y);
	uv_vscroll->set_max(max_corner.y);
	uv_vscroll->set_page(page_size.y);
	uv_vscroll->set_value(uv_draw_ofs.y);
	uv_vscroll->set_block_signals(false);

	uv_edit_draw->queue_redraw();
}

void Polygon2DEditor::_uv_draw() {
	if (!uv_edit->is_visible() || !_get_node()) {
		return;
	}

	Ref<Texture2D> base_tex = node->get_texture();

	String warning;

	Transform2D mtx;
	mtx.columns[2] = -uv_draw_ofs * uv_draw_zoom;
	mtx.scale_basis(Vector2(uv_draw_zoom, uv_draw_zoom));

	// Draw texture as a background if editing uvs or no uv mapping exist.
	if (uv_edit_mode[0]->is_pressed() || uv_mode == UV_MODE_CREATE || node->get_polygon().is_empty() || node->get_uv().size() != node->get_polygon().size()) {
		if (base_tex.is_valid()) {
			Transform2D texture_transform = Transform2D(node->get_texture_rotation(), node->get_texture_offset());
			texture_transform.scale(node->get_texture_scale());
			texture_transform.affine_invert();
			RS::get_singleton()->canvas_item_add_set_transform(uv_edit_draw->get_canvas_item(), mtx * texture_transform);
			uv_edit_draw->draw_texture(base_tex, Point2());
			RS::get_singleton()->canvas_item_add_set_transform(uv_edit_draw->get_canvas_item(), Transform2D());
		}
		preview_polygon->hide();
	} else {
		preview_polygon->set_transform(mtx);
		// Keep in sync with newly added Polygon2D properties (when relevant).
		preview_polygon->set_texture(node->get_texture());
		preview_polygon->set_texture_offset(node->get_texture_offset());
		preview_polygon->set_texture_rotation(node->get_texture_rotation());
		preview_polygon->set_texture_scale(node->get_texture_scale());
		preview_polygon->set_texture_filter(node->get_texture_filter_in_tree());
		preview_polygon->set_texture_repeat(node->get_texture_repeat_in_tree());
		preview_polygon->set_polygon(node->get_polygon());
		preview_polygon->set_uv(node->get_uv());
		preview_polygon->set_invert(node->get_invert());
		preview_polygon->set_invert_border(node->get_invert_border());
		preview_polygon->set_internal_vertex_count(node->get_internal_vertex_count());
		if (uv_mode == UV_MODE_ADD_POLYGON) {
			preview_polygon->set_polygons(Array());
		} else {
			preview_polygon->set_polygons(node->get_polygons());
		}
		preview_polygon->show();
	}

	if (snap_show_grid) {
		Color grid_color = Color(1.0, 1.0, 1.0, 0.15);
		Size2 s = uv_edit_draw->get_size();
		int last_cell = 0;

		if (snap_step.x != 0) {
			for (int i = 0; i < s.width; i++) {
				int cell = Math::fast_ftoi(Math::floor((mtx.affine_inverse().xform(Vector2(i, 0)).x - snap_offset.x) / snap_step.x));
				if (i == 0) {
					last_cell = cell;
				}
				if (last_cell != cell) {
					uv_edit_draw->draw_line(Point2(i, 0), Point2(i, s.height), grid_color, Math::round(EDSCALE));
				}
				last_cell = cell;
			}
		}

		if (snap_step.y != 0) {
			for (int i = 0; i < s.height; i++) {
				int cell = Math::fast_ftoi(Math::floor((mtx.affine_inverse().xform(Vector2(0, i)).y - snap_offset.y) / snap_step.y));
				if (i == 0) {
					last_cell = cell;
				}
				if (last_cell != cell) {
					uv_edit_draw->draw_line(Point2(0, i), Point2(s.width, i), grid_color, Math::round(EDSCALE));
				}
				last_cell = cell;
			}
		}
	}

	Array polygons = node->get_polygons();

	Vector<Vector2> uvs;
	if (uv_edit_mode[0]->is_pressed()) { //edit uv
		uvs = node->get_uv();
	} else { //edit polygon
		uvs = node->get_polygon();
	}

	const float *weight_r = nullptr;

	if (uv_edit_mode[3]->is_pressed()) {
		int bone_selected = -1;
		for (int i = 0; i < bone_scroll_vb->get_child_count(); i++) {
			CheckBox *c = Object::cast_to<CheckBox>(bone_scroll_vb->get_child(i));
			if (c && c->is_pressed()) {
				bone_selected = i;
				break;
			}
		}

		if (bone_selected != -1 && node->get_bone_weights(bone_selected).size() == uvs.size()) {
			weight_r = node->get_bone_weights(bone_selected).ptr();
		}
	}

	// All UV points are sharp, so use the sharp handle icon
	Ref<Texture2D> handle = get_editor_theme_icon(SNAME("EditorPathSharpHandle"));

	Color poly_line_color = Color(0.9, 0.5, 0.5);
	if (polygons.size() || polygon_create.size()) {
		poly_line_color.a *= 0.25;
	}
	Color polygon_line_color = Color(0.5, 0.5, 0.9);
	Color polygon_fill_color = polygon_line_color;
	polygon_fill_color.a *= 0.5;
	Color prev_color = Color(0.5, 0.5, 0.5);

	int uv_draw_max = uvs.size();

	uv_draw_max -= node->get_internal_vertex_count();
	if (uv_draw_max < 0) {
		uv_draw_max = 0;
	}

	for (int i = 0; i < uvs.size(); i++) {
		int next = uv_draw_max > 0 ? (i + 1) % uv_draw_max : 0;

		if (i < uv_draw_max && uv_drag && uv_move_current == UV_MODE_EDIT_POINT && EDITOR_GET("editors/polygon_editor/show_previous_outline")) {
			uv_edit_draw->draw_line(mtx.xform(points_prev[i]), mtx.xform(points_prev[next]), prev_color, Math::round(EDSCALE));
		}

		Vector2 next_point = uvs[next];
		if (uv_create && i == uvs.size() - 1) {
			next_point = uv_create_to;
		}
		if (i < uv_draw_max) { // If using or creating polygons, do not show outline (will show polygons instead).
			uv_edit_draw->draw_line(mtx.xform(uvs[i]), mtx.xform(next_point), poly_line_color, Math::round(EDSCALE));
		}
	}

	for (int i = 0; i < polygons.size(); i++) {
		Vector<int> points = polygons[i];
		Vector<Vector2> polypoints;
		for (int j = 0; j < points.size(); j++) {
			int next = (j + 1) % points.size();

			int idx = points[j];
			int idx_next = points[next];
			if (idx < 0 || idx >= uvs.size()) {
				continue;
			}
			polypoints.push_back(mtx.xform(uvs[idx]));

			if (idx_next < 0 || idx_next >= uvs.size()) {
				continue;
			}
			uv_edit_draw->draw_line(mtx.xform(uvs[idx]), mtx.xform(uvs[idx_next]), polygon_line_color, Math::round(EDSCALE));
		}
		if (points.size() >= 3) {
			uv_edit_draw->draw_colored_polygon(polypoints, polygon_fill_color);
		}
	}

	for (int i = 0; i < uvs.size(); i++) {
		if (weight_r) {
			Vector2 draw_pos = mtx.xform(uvs[i]);
			float weight = weight_r[i];
			uv_edit_draw->draw_rect(Rect2(draw_pos - Vector2(2, 2) * EDSCALE, Vector2(5, 5) * EDSCALE), Color(weight, weight, weight, 1.0), Math::round(EDSCALE));
		} else {
			if (i < uv_draw_max) {
				uv_edit_draw->draw_texture(handle, mtx.xform(uvs[i]) - handle->get_size() * 0.5);
			} else {
				// Internal vertex
				uv_edit_draw->draw_texture(handle, mtx.xform(uvs[i]) - handle->get_size() * 0.5, Color(0.6, 0.8, 1));
			}
		}
	}

	if (polygon_create.size()) {
		for (int i = 0; i < polygon_create.size(); i++) {
			Vector2 from = uvs[polygon_create[i]];
			Vector2 to = (i + 1) < polygon_create.size() ? uvs[polygon_create[i + 1]] : uv_create_to;
			uv_edit_draw->draw_line(mtx.xform(from), mtx.xform(to), polygon_line_color, Math::round(EDSCALE));
		}
	}

	if (uv_mode == UV_MODE_PAINT_WEIGHT || uv_mode == UV_MODE_CLEAR_WEIGHT) {
		NodePath bone_path;
		for (int i = 0; i < bone_scroll_vb->get_child_count(); i++) {
			CheckBox *c = Object::cast_to<CheckBox>(bone_scroll_vb->get_child(i));
			if (c && c->is_pressed()) {
				bone_path = node->get_bone_path(i);
				break;
			}
		}

		//draw skeleton
		NodePath skeleton_path = node->get_skeleton();
		Skeleton2D *skeleton = Object::cast_to<Skeleton2D>(node->get_node_or_null(skeleton_path));
		if (skeleton) {
			Transform2D skeleton_xform = node->get_global_transform().affine_inverse().translated(-node->get_offset()) * skeleton->get_global_transform();
			for (int i = 0; i < skeleton->get_bone_count(); i++) {
				Bone2D *bone = skeleton->get_bone(i);
				if (bone->get_rest() == Transform2D(0, 0, 0, 0, 0, 0)) {
					continue; //not set
				}

				bool current = bone_path == skeleton->get_path_to(bone);

				bool found_child = false;

				for (int j = 0; j < bone->get_child_count(); j++) {
					Bone2D *n = Object::cast_to<Bone2D>(bone->get_child(j));
					if (!n) {
						continue;
					}

					found_child = true;

					Transform2D bone_xform = skeleton_xform * bone->get_skeleton_rest();
					Transform2D endpoint_xform = bone_xform * n->get_transform();

					Color color = current ? Color(1, 1, 1) : Color(0.5, 0.5, 0.5);
					uv_edit_draw->draw_line(mtx.xform(bone_xform.get_origin()), mtx.xform(endpoint_xform.get_origin()), Color(0, 0, 0), Math::round((current ? 5 : 4) * EDSCALE));
					uv_edit_draw->draw_line(mtx.xform(bone_xform.get_origin()), mtx.xform(endpoint_xform.get_origin()), color, Math::round((current ? 3 : 2) * EDSCALE));
				}

				if (!found_child) {
					//draw normally
					Transform2D bone_xform = skeleton_xform * bone->get_skeleton_rest();
					Transform2D endpoint_xform = bone_xform * Transform2D(0, Vector2(bone->get_length(), 0)).rotated(bone->get_bone_angle());

					Color color = current ? Color(1, 1, 1) : Color(0.5, 0.5, 0.5);
					uv_edit_draw->draw_line(mtx.xform(bone_xform.get_origin()), mtx.xform(endpoint_xform.get_origin()), Color(0, 0, 0), Math::round((current ? 5 : 4) * EDSCALE));
					uv_edit_draw->draw_line(mtx.xform(bone_xform.get_origin()), mtx.xform(endpoint_xform.get_origin()), color, Math::round((current ? 3 : 2) * EDSCALE));
				}
			}
		}

		//draw paint circle
		uv_edit_draw->draw_circle(bone_paint_pos, bone_paint_radius->get_value() * EDSCALE, Color(1, 1, 1, 0.1));
	}
}

void Polygon2DEditor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_update_bone_list"), &Polygon2DEditor::_update_bone_list);
	ClassDB::bind_method(D_METHOD("_update_polygon_editing_state"), &Polygon2DEditor::_update_polygon_editing_state);
}

Vector2 Polygon2DEditor::snap_point(Vector2 p_target) const {
	if (use_snap) {
		p_target.x = Math::snap_scalar((snap_offset.x - uv_draw_ofs.x) * uv_draw_zoom, snap_step.x * uv_draw_zoom, p_target.x);
		p_target.y = Math::snap_scalar((snap_offset.y - uv_draw_ofs.y) * uv_draw_zoom, snap_step.y * uv_draw_zoom, p_target.y);
	}

	return p_target;
}

Polygon2DEditor::Polygon2DEditor() {
	snap_offset = EditorSettings::get_singleton()->get_project_metadata("polygon_2d_uv_editor", "snap_offset", Vector2());
	// A power-of-two value works better as a default grid size.
	snap_step = EditorSettings::get_singleton()->get_project_metadata("polygon_2d_uv_editor", "snap_step", Vector2(8, 8));
	use_snap = EditorSettings::get_singleton()->get_project_metadata("polygon_2d_uv_editor", "snap_enabled", false);
	snap_show_grid = EditorSettings::get_singleton()->get_project_metadata("polygon_2d_uv_editor", "show_grid", false);

	button_uv = memnew(Button);
	button_uv->set_theme_type_variation(SceneStringName(FlatButton));
	add_child(button_uv);
	button_uv->set_tooltip_text(TTR("Open Polygon 2D UV editor."));
	button_uv->connect(SceneStringName(pressed), callable_mp(this, &Polygon2DEditor::_menu_option).bind(MODE_EDIT_UV));

	uv_mode = UV_MODE_EDIT_POINT;
	uv_edit = memnew(UVEditDialog);
	uv_edit->set_title(TTR("Polygon 2D UV Editor"));
	uv_edit->set_process_shortcut_input(true);
	add_child(uv_edit);
	uv_edit->connect(SceneStringName(confirmed), callable_mp(this, &Polygon2DEditor::_uv_edit_popup_hide));
	uv_edit->connect("canceled", callable_mp(this, &Polygon2DEditor::_uv_edit_popup_hide));
	uv_edit->connect("about_to_popup", callable_mp(this, &Polygon2DEditor::_uv_edit_popup_show));

	VBoxContainer *uv_main_vb = memnew(VBoxContainer);
	uv_edit->add_child(uv_main_vb);
	HBoxContainer *uv_mode_hb = memnew(HBoxContainer);

	uv_edit_group.instantiate();

	uv_edit_mode[0] = memnew(Button);
	uv_mode_hb->add_child(uv_edit_mode[0]);
	uv_edit_mode[0]->set_toggle_mode(true);
	uv_edit_mode[1] = memnew(Button);
	uv_mode_hb->add_child(uv_edit_mode[1]);
	uv_edit_mode[1]->set_toggle_mode(true);
	uv_edit_mode[2] = memnew(Button);
	uv_mode_hb->add_child(uv_edit_mode[2]);
	uv_edit_mode[2]->set_toggle_mode(true);
	uv_edit_mode[3] = memnew(Button);
	uv_mode_hb->add_child(uv_edit_mode[3]);
	uv_edit_mode[3]->set_toggle_mode(true);

	uv_edit_mode[0]->set_text(TTR("UV"));
	uv_edit_mode[0]->set_pressed(true);
	uv_edit_mode[1]->set_text(TTR("Points"));
	uv_edit_mode[2]->set_text(TTR("Polygons"));
	uv_edit_mode[3]->set_text(TTR("Bones"));

	uv_edit_mode[0]->set_button_group(uv_edit_group);
	uv_edit_mode[1]->set_button_group(uv_edit_group);
	uv_edit_mode[2]->set_button_group(uv_edit_group);
	uv_edit_mode[3]->set_button_group(uv_edit_group);

	uv_edit_mode[0]->connect(SceneStringName(pressed), callable_mp(this, &Polygon2DEditor::_uv_edit_mode_select).bind(0));
	uv_edit_mode[1]->connect(SceneStringName(pressed), callable_mp(this, &Polygon2DEditor::_uv_edit_mode_select).bind(1));
	uv_edit_mode[2]->connect(SceneStringName(pressed), callable_mp(this, &Polygon2DEditor::_uv_edit_mode_select).bind(2));
	uv_edit_mode[3]->connect(SceneStringName(pressed), callable_mp(this, &Polygon2DEditor::_uv_edit_mode_select).bind(3));

	uv_mode_hb->add_child(memnew(VSeparator));

	uv_main_vb->add_child(uv_mode_hb);
	for (int i = 0; i < UV_MODE_MAX; i++) {
		uv_button[i] = memnew(Button);
		uv_button[i]->set_theme_type_variation(SceneStringName(FlatButton));
		uv_button[i]->set_toggle_mode(true);
		uv_mode_hb->add_child(uv_button[i]);
		uv_button[i]->connect(SceneStringName(pressed), callable_mp(this, &Polygon2DEditor::_uv_mode).bind(i));
		uv_button[i]->set_focus_mode(FOCUS_NONE);
	}

	uv_button[UV_MODE_CREATE]->set_tooltip_text(TTR("Create Polygon"));
	uv_button[UV_MODE_CREATE_INTERNAL]->set_tooltip_text(TTR("Create Internal Vertex"));
	uv_button[UV_MODE_REMOVE_INTERNAL]->set_tooltip_text(TTR("Remove Internal Vertex"));
	Key key = (OS::get_singleton()->has_feature("macos") || OS::get_singleton()->has_feature("web_macos") || OS::get_singleton()->has_feature("web_ios")) ? Key::META : Key::CTRL;
	// TRANSLATORS: %s is Control or Command key name.
	uv_button[UV_MODE_EDIT_POINT]->set_tooltip_text(TTR("Move Points") + "\n" + vformat(TTR("%s: Rotate"), find_keycode_name(key)) + "\n" + TTR("Shift: Move All") + "\n" + vformat(TTR("%s + Shift: Scale"), find_keycode_name(key)));
	uv_button[UV_MODE_MOVE]->set_tooltip_text(TTR("Move Polygon"));
	uv_button[UV_MODE_ROTATE]->set_tooltip_text(TTR("Rotate Polygon"));
	uv_button[UV_MODE_SCALE]->set_tooltip_text(TTR("Scale Polygon"));
	uv_button[UV_MODE_ADD_POLYGON]->set_tooltip_text(TTR("Create a custom polygon. Enables custom polygon rendering."));
	uv_button[UV_MODE_REMOVE_POLYGON]->set_tooltip_text(TTR("Remove a custom polygon. If none remain, custom polygon rendering is disabled."));
	uv_button[UV_MODE_PAINT_WEIGHT]->set_tooltip_text(TTR("Paint weights with specified intensity."));
	uv_button[UV_MODE_CLEAR_WEIGHT]->set_tooltip_text(TTR("Unpaint weights with specified intensity."));

	uv_button[UV_MODE_CREATE]->hide();
	uv_button[UV_MODE_CREATE_INTERNAL]->hide();
	uv_button[UV_MODE_REMOVE_INTERNAL]->hide();
	uv_button[UV_MODE_ADD_POLYGON]->hide();
	uv_button[UV_MODE_REMOVE_POLYGON]->hide();
	uv_button[UV_MODE_PAINT_WEIGHT]->hide();
	uv_button[UV_MODE_CLEAR_WEIGHT]->hide();
	uv_button[UV_MODE_EDIT_POINT]->set_pressed(true);

	bone_paint_strength = memnew(HSlider);
	uv_mode_hb->add_child(bone_paint_strength);
	bone_paint_strength->set_custom_minimum_size(Size2(75 * EDSCALE, 0));
	bone_paint_strength->set_v_size_flags(SIZE_SHRINK_CENTER);
	bone_paint_strength->set_min(0);
	bone_paint_strength->set_max(1);
	bone_paint_strength->set_step(0.01);
	bone_paint_strength->set_value(0.5);

	bone_paint_radius_label = memnew(Label(TTR("Radius:")));
	uv_mode_hb->add_child(bone_paint_radius_label);
	bone_paint_radius = memnew(SpinBox);
	uv_mode_hb->add_child(bone_paint_radius);

	bone_paint_strength->hide();
	bone_paint_radius->hide();
	bone_paint_radius_label->hide();
	bone_paint_radius->set_min(1);
	bone_paint_radius->set_max(100);
	bone_paint_radius->set_step(1);
	bone_paint_radius->set_value(32);

	HSplitContainer *uv_main_hsc = memnew(HSplitContainer);
	uv_main_vb->add_child(uv_main_hsc);
	uv_main_hsc->set_v_size_flags(SIZE_EXPAND_FILL);

	uv_edit_background = memnew(Panel);
	uv_main_hsc->add_child(uv_edit_background);
	uv_edit_background->set_h_size_flags(SIZE_EXPAND_FILL);
	uv_edit_background->set_custom_minimum_size(Size2(200, 200) * EDSCALE);
	uv_edit_background->set_clip_contents(true);

	preview_polygon = memnew(Polygon2D);
	uv_edit_background->add_child(preview_polygon);

	uv_edit_draw = memnew(Control);
	uv_edit_background->add_child(uv_edit_draw);
	uv_edit_draw->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);

	Control *space = memnew(Control);
	uv_mode_hb->add_child(space);
	space->set_h_size_flags(SIZE_EXPAND_FILL);

	uv_menu = memnew(MenuButton);
	uv_mode_hb->add_child(uv_menu);
	uv_menu->set_flat(false);
	uv_menu->set_theme_type_variation("FlatMenuButton");
	uv_menu->set_text(TTR("Edit"));
	uv_menu->get_popup()->add_item(TTR("Copy Polygon to UV"), UVEDIT_POLYGON_TO_UV);
	uv_menu->get_popup()->add_item(TTR("Copy UV to Polygon"), UVEDIT_UV_TO_POLYGON);
	uv_menu->get_popup()->add_separator();
	uv_menu->get_popup()->add_item(TTR("Clear UV"), UVEDIT_UV_CLEAR);
	uv_menu->get_popup()->add_separator();
	uv_menu->get_popup()->add_item(TTR("Grid Settings"), UVEDIT_GRID_SETTINGS);
	uv_menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &Polygon2DEditor::_menu_option));

	uv_mode_hb->add_child(memnew(VSeparator));

	b_snap_enable = memnew(Button);
	b_snap_enable->set_theme_type_variation(SceneStringName(FlatButton));
	uv_mode_hb->add_child(b_snap_enable);
	b_snap_enable->set_text(TTR("Snap"));
	b_snap_enable->set_focus_mode(FOCUS_NONE);
	b_snap_enable->set_toggle_mode(true);
	b_snap_enable->set_pressed(use_snap);
	b_snap_enable->set_tooltip_text(TTR("Enable Snap"));
	b_snap_enable->connect(SceneStringName(toggled), callable_mp(this, &Polygon2DEditor::_set_use_snap));

	b_snap_grid = memnew(Button);
	b_snap_grid->set_theme_type_variation(SceneStringName(FlatButton));
	uv_mode_hb->add_child(b_snap_grid);
	b_snap_grid->set_text(TTR("Grid"));
	b_snap_grid->set_focus_mode(FOCUS_NONE);
	b_snap_grid->set_toggle_mode(true);
	b_snap_grid->set_pressed(snap_show_grid);
	b_snap_grid->set_tooltip_text(TTR("Show Grid"));
	b_snap_grid->connect(SceneStringName(toggled), callable_mp(this, &Polygon2DEditor::_set_show_grid));

	grid_settings = memnew(AcceptDialog);
	grid_settings->set_title(TTR("Configure Grid:"));
	uv_edit->add_child(grid_settings);
	VBoxContainer *grid_settings_vb = memnew(VBoxContainer);
	grid_settings->add_child(grid_settings_vb);

	SpinBox *sb_off_x = memnew(SpinBox);
	sb_off_x->set_min(-256);
	sb_off_x->set_max(256);
	sb_off_x->set_step(1);
	sb_off_x->set_value(snap_offset.x);
	sb_off_x->set_suffix("px");
	sb_off_x->connect(SceneStringName(value_changed), callable_mp(this, &Polygon2DEditor::_set_snap_off_x));
	grid_settings_vb->add_margin_child(TTR("Grid Offset X:"), sb_off_x);

	SpinBox *sb_off_y = memnew(SpinBox);
	sb_off_y->set_min(-256);
	sb_off_y->set_max(256);
	sb_off_y->set_step(1);
	sb_off_y->set_value(snap_offset.y);
	sb_off_y->set_suffix("px");
	sb_off_y->connect(SceneStringName(value_changed), callable_mp(this, &Polygon2DEditor::_set_snap_off_y));
	grid_settings_vb->add_margin_child(TTR("Grid Offset Y:"), sb_off_y);

	SpinBox *sb_step_x = memnew(SpinBox);
	sb_step_x->set_min(-256);
	sb_step_x->set_max(256);
	sb_step_x->set_step(1);
	sb_step_x->set_value(snap_step.x);
	sb_step_x->set_suffix("px");
	sb_step_x->connect(SceneStringName(value_changed), callable_mp(this, &Polygon2DEditor::_set_snap_step_x));
	grid_settings_vb->add_margin_child(TTR("Grid Step X:"), sb_step_x);

	SpinBox *sb_step_y = memnew(SpinBox);
	sb_step_y->set_min(-256);
	sb_step_y->set_max(256);
	sb_step_y->set_step(1);
	sb_step_y->set_value(snap_step.y);
	sb_step_y->set_suffix("px");
	sb_step_y->connect(SceneStringName(value_changed), callable_mp(this, &Polygon2DEditor::_set_snap_step_y));
	grid_settings_vb->add_margin_child(TTR("Grid Step Y:"), sb_step_y);

	zoom_widget = memnew(EditorZoomWidget);
	uv_edit_draw->add_child(zoom_widget);
	zoom_widget->set_anchors_and_offsets_preset(Control::PRESET_TOP_LEFT, Control::PRESET_MODE_MINSIZE, 2 * EDSCALE);
	zoom_widget->connect("zoom_changed", callable_mp(this, &Polygon2DEditor::_update_zoom_and_pan).unbind(1).bind(true));
	zoom_widget->set_shortcut_context(nullptr);

	uv_vscroll = memnew(VScrollBar);
	uv_vscroll->set_step(0.001);
	uv_edit_draw->add_child(uv_vscroll);
	uv_vscroll->connect(SceneStringName(value_changed), callable_mp(this, &Polygon2DEditor::_update_zoom_and_pan).unbind(1).bind(false));
	uv_hscroll = memnew(HScrollBar);
	uv_hscroll->set_step(0.001);
	uv_edit_draw->add_child(uv_hscroll);
	uv_hscroll->connect(SceneStringName(value_changed), callable_mp(this, &Polygon2DEditor::_update_zoom_and_pan).unbind(1).bind(false));

	bone_scroll_main_vb = memnew(VBoxContainer);
	bone_scroll_main_vb->hide();
	bone_scroll_main_vb->set_custom_minimum_size(Size2(150 * EDSCALE, 0));
	sync_bones = memnew(Button(TTR("Sync Bones to Polygon")));
	bone_scroll_main_vb->add_child(sync_bones);
	sync_bones->set_h_size_flags(0);
	sync_bones->connect(SceneStringName(pressed), callable_mp(this, &Polygon2DEditor::_sync_bones));
	uv_main_hsc->add_child(bone_scroll_main_vb);
	bone_scroll = memnew(ScrollContainer);
	bone_scroll->set_v_scroll(true);
	bone_scroll->set_h_scroll(false);
	bone_scroll_main_vb->add_child(bone_scroll);
	bone_scroll->set_v_size_flags(SIZE_EXPAND_FILL);
	bone_scroll_vb = memnew(VBoxContainer);
	bone_scroll->add_child(bone_scroll_vb);

	uv_panner.instantiate();
	uv_panner->set_callbacks(callable_mp(this, &Polygon2DEditor::_uv_pan_callback), callable_mp(this, &Polygon2DEditor::_uv_zoom_callback));

	uv_edit_draw->connect(SceneStringName(draw), callable_mp(this, &Polygon2DEditor::_uv_draw));
	uv_edit_draw->connect(SceneStringName(gui_input), callable_mp(this, &Polygon2DEditor::_uv_input));
	uv_edit_draw->connect(SceneStringName(focus_exited), callable_mp(uv_panner.ptr(), &ViewPanner::release_pan_key));
	uv_edit_draw->set_focus_mode(FOCUS_CLICK);
	uv_draw_zoom = 1.0;
	point_drag_index = -1;
	uv_drag = false;
	uv_create = false;
	bone_painting = false;

	error = memnew(AcceptDialog);
	add_child(error);
}

Polygon2DEditorPlugin::Polygon2DEditorPlugin() :
		AbstractPolygon2DEditorPlugin(memnew(Polygon2DEditor), "Polygon2D") {
}

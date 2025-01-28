/**************************************************************************/
/*  abstract_polygon_2d_editor.cpp                                        */
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

#include "abstract_polygon_2d_editor.h"

#include "canvas_item_editor_plugin.h"
#include "core/math/geometry_2d.h"
#include "core/os/keyboard.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/dialogs.h"

bool AbstractPolygon2DEditor::Vertex::operator==(const AbstractPolygon2DEditor::Vertex &p_vertex) const {
	return polygon == p_vertex.polygon && vertex == p_vertex.vertex;
}

bool AbstractPolygon2DEditor::Vertex::operator!=(const AbstractPolygon2DEditor::Vertex &p_vertex) const {
	return !(*this == p_vertex);
}

bool AbstractPolygon2DEditor::Vertex::valid() const {
	return vertex >= 0;
}

bool AbstractPolygon2DEditor::_is_empty() const {
	if (!_get_node()) {
		return true;
	}

	const int n = _get_polygon_count();

	for (int i = 0; i < n; i++) {
		Vector<Vector2> vertices = _get_polygon(i);

		if (vertices.size() != 0) {
			return false;
		}
	}

	return true;
}

bool AbstractPolygon2DEditor::_is_line() const {
	return false;
}

bool AbstractPolygon2DEditor::_has_uv() const {
	return false;
}

int AbstractPolygon2DEditor::_get_polygon_count() const {
	return 1;
}

Variant AbstractPolygon2DEditor::_get_polygon(int p_idx) const {
	return _get_node()->get("polygon");
}

void AbstractPolygon2DEditor::_set_polygon(int p_idx, const Variant &p_polygon) const {
	_get_node()->set("polygon", p_polygon);
}

void AbstractPolygon2DEditor::_action_set_polygon(int p_idx, const Variant &p_previous, const Variant &p_polygon) {
	Node2D *node = _get_node();
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->add_do_method(node, "set_polygon", p_polygon);
	undo_redo->add_undo_method(node, "set_polygon", p_previous);
}

Vector2 AbstractPolygon2DEditor::_get_offset(int p_idx) const {
	return Vector2(0, 0);
}

void AbstractPolygon2DEditor::_commit_action() {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->add_do_method(canvas_item_editor, "update_viewport");
	undo_redo->add_undo_method(canvas_item_editor, "update_viewport");
	undo_redo->commit_action();
}

void AbstractPolygon2DEditor::_action_add_polygon(const Variant &p_polygon) {
	_action_set_polygon(0, p_polygon);
}

void AbstractPolygon2DEditor::_action_remove_polygon(int p_idx) {
	_action_set_polygon(p_idx, _get_polygon(p_idx), Vector<Vector2>());
}

void AbstractPolygon2DEditor::_action_set_polygon(int p_idx, const Variant &p_polygon) {
	_action_set_polygon(p_idx, _get_polygon(p_idx), p_polygon);
}

bool AbstractPolygon2DEditor::_has_resource() const {
	return true;
}

void AbstractPolygon2DEditor::_create_resource() {
}

void AbstractPolygon2DEditor::_menu_option(int p_option) {
	switch (p_option) {
		case MODE_CREATE: {
			mode = MODE_CREATE;
			button_create->set_pressed(true);
			button_edit->set_pressed(false);
			button_delete->set_pressed(false);
		} break;
		case MODE_EDIT: {
			_wip_close();
			mode = MODE_EDIT;
			button_create->set_pressed(false);
			button_edit->set_pressed(true);
			button_delete->set_pressed(false);
		} break;
		case MODE_DELETE: {
			_wip_close();
			mode = MODE_DELETE;
			button_create->set_pressed(false);
			button_edit->set_pressed(false);
			button_delete->set_pressed(true);
		} break;
	}
}

void AbstractPolygon2DEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			button_create->set_button_icon(get_editor_theme_icon(SNAME("CurveCreate")));
			button_edit->set_button_icon(get_editor_theme_icon(SNAME("CurveEdit")));
			button_delete->set_button_icon(get_editor_theme_icon(SNAME("CurveDelete")));
		} break;

		case NOTIFICATION_READY: {
			disable_polygon_editing(false, String());

			button_edit->set_pressed(true);

			get_tree()->connect("node_removed", callable_mp(this, &AbstractPolygon2DEditor::_node_removed));
			create_resource->connect(SceneStringName(confirmed), callable_mp(this, &AbstractPolygon2DEditor::_create_resource));
		} break;
	}
}

void AbstractPolygon2DEditor::_node_removed(Node *p_node) {
	if (p_node == _get_node()) {
		edit(nullptr);
		hide();

		canvas_item_editor->update_viewport();
	}
}

void AbstractPolygon2DEditor::_wip_changed() {
	if (wip_active && _is_line()) {
		_set_polygon(0, wip);
	}
}

void AbstractPolygon2DEditor::_wip_cancel() {
	wip.clear();
	wip_active = false;

	edited_point = PosVertex();
	hover_point = Vertex();
	selected_point = Vertex();

	canvas_item_editor->update_viewport();
}

void AbstractPolygon2DEditor::_wip_close() {
	if (!wip_active) {
		return;
	}

	if (_is_line()) {
		_set_polygon(0, wip);
	} else if (wip.size() >= (_is_line() ? 2 : 3)) {
		EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
		undo_redo->create_action(TTR("Create Polygon"));
		_action_add_polygon(wip);
		if (_has_uv()) {
			undo_redo->add_do_method(_get_node(), "set_uv", Vector<Vector2>());
			undo_redo->add_undo_method(_get_node(), "set_uv", _get_node()->get("uv"));
		}
		_commit_action();
	} else {
		return;
	}

	mode = MODE_EDIT;
	button_edit->set_pressed(true);
	button_create->set_pressed(false);
	button_delete->set_pressed(false);

	wip.clear();
	wip_active = false;

	edited_point = PosVertex();
	hover_point = Vertex();
	selected_point = Vertex();
}

void AbstractPolygon2DEditor::disable_polygon_editing(bool p_disable, const String &p_reason) {
	_polygon_editing_enabled = !p_disable;

	button_create->set_disabled(p_disable);
	button_edit->set_disabled(p_disable);
	button_delete->set_disabled(p_disable);

	if (p_disable) {
		button_create->set_tooltip_text(p_reason);
		button_edit->set_tooltip_text(p_reason);
		button_delete->set_tooltip_text(p_reason);
	} else {
		button_create->set_tooltip_text(TTR("Create points."));
		button_edit->set_tooltip_text(TTR("Edit points.\nLMB: Move Point\nRMB: Erase Point"));
		button_delete->set_tooltip_text(TTR("Erase points."));
	}
}

bool AbstractPolygon2DEditor::forward_gui_input(const Ref<InputEvent> &p_event) {
	if (!_get_node() || !_polygon_editing_enabled) {
		return false;
	}

	if (!_get_node()->is_visible_in_tree()) {
		return false;
	}

	Viewport *vp = _get_node()->get_viewport();
	if (vp && !vp->is_visible_subviewport()) {
		return false;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	Ref<InputEventMouseButton> mb = p_event;

	if (!_has_resource()) {
		if (mb.is_valid() && mb->get_button_index() == MouseButton::LEFT && mb->is_pressed()) {
			create_resource->set_text(String("No polygon resource on this node.\nCreate and assign one?"));
			create_resource->popup_centered();
		}
		return (mb.is_valid() && mb->get_button_index() == MouseButton::LEFT);
	}

	CanvasItemEditor::Tool tool = CanvasItemEditor::get_singleton()->get_current_tool();
	if (tool != CanvasItemEditor::TOOL_SELECT) {
		return false;
	}

	if (mb.is_valid()) {
		Transform2D xform = canvas_item_editor->get_canvas_transform() * _get_node()->get_screen_transform();

		Vector2 gpoint = mb->get_position();
		Vector2 cpoint = canvas_item_editor->snap_point(canvas_item_editor->get_canvas_transform().affine_inverse().xform(gpoint));
		cpoint = _get_node()->get_screen_transform().affine_inverse().xform(cpoint);

		if (mode == MODE_EDIT || (_is_line() && mode == MODE_CREATE)) {
			if (mb->get_button_index() == MouseButton::LEFT) {
				if (mb->is_pressed()) {
					if (mb->is_meta_pressed() || mb->is_ctrl_pressed() || mb->is_shift_pressed() || mb->is_alt_pressed()) {
						return false;
					}

					const PosVertex closest = closest_point(gpoint);
					if (closest.valid()) {
						original_mouse_pos = gpoint;
						pre_move_edit = _get_polygon(closest.polygon);
						edited_point = PosVertex(closest, xform.affine_inverse().xform(closest.pos));
						selected_point = closest;
						edge_point = PosVertex();
						canvas_item_editor->update_viewport();
						return true;
					} else {
						selected_point = Vertex();

						const PosVertex insert = closest_edge_point(gpoint);
						if (insert.valid()) {
							Vector<Vector2> vertices = _get_polygon(insert.polygon);

							if (vertices.size() < (_is_line() ? 2 : 3)) {
								vertices.push_back(cpoint);
								undo_redo->create_action(TTR("Edit Polygon"));
								selected_point = Vertex(insert.polygon, vertices.size());
								_action_set_polygon(insert.polygon, vertices);
								_commit_action();
								return true;
							} else {
								edited_point = PosVertex(insert.polygon, insert.vertex + 1, xform.affine_inverse().xform(insert.pos));
								vertices.insert(edited_point.vertex, edited_point.pos);
								pre_move_edit = vertices;
								selected_point = Vertex(edited_point.polygon, edited_point.vertex);
								edge_point = PosVertex();

								undo_redo->create_action(TTR("Insert Point"));
								_action_set_polygon(insert.polygon, vertices);
								_commit_action();
								return true;
							}
						}
					}
				} else {
					if (edited_point.valid()) {
						if (original_mouse_pos != gpoint) {
							Vector<Vector2> vertices = _get_polygon(edited_point.polygon);
							ERR_FAIL_INDEX_V(edited_point.vertex, vertices.size(), false);
							vertices.write[edited_point.vertex] = edited_point.pos - _get_offset(edited_point.polygon);

							undo_redo->create_action(TTR("Edit Polygon"));
							_action_set_polygon(edited_point.polygon, pre_move_edit, vertices);
							_commit_action();
						}

						edited_point = PosVertex();
						return true;
					}
				}
			} else if (mb->get_button_index() == MouseButton::RIGHT && mb->is_pressed() && !edited_point.valid()) {
				const PosVertex closest = closest_point(gpoint);

				if (closest.valid()) {
					remove_point(closest);
					return true;
				}
			}
		} else if (mode == MODE_DELETE) {
			if (mb->get_button_index() == MouseButton::LEFT && mb->is_pressed()) {
				const PosVertex closest = closest_point(gpoint);

				if (closest.valid()) {
					remove_point(closest);
					return true;
				}
			}
		}

		if (mode == MODE_CREATE) {
			if (mb->get_button_index() == MouseButton::LEFT && mb->is_pressed()) {
				if (_is_line()) {
					// for lines, we don't have a wip mode, and we can undo each single add point.
					Vector<Vector2> vertices = _get_polygon(0);
					vertices.push_back(cpoint);
					undo_redo->create_action(TTR("Insert Point"));
					_action_set_polygon(0, vertices);
					_commit_action();
					return true;
				} else if (!wip_active) {
					wip.clear();
					wip.push_back(cpoint);
					wip_active = true;
					_wip_changed();
					edited_point = PosVertex(-1, 1, cpoint);
					canvas_item_editor->update_viewport();
					hover_point = Vertex();
					selected_point = Vertex(0);
					edge_point = PosVertex();
					return true;
				} else {
					const real_t grab_threshold = EDITOR_GET("editors/polygon_editor/point_grab_radius");

					if (!_is_line() && wip.size() > 1 && xform.xform(wip[0]).distance_to(xform.xform(cpoint)) < grab_threshold) {
						//wip closed
						_wip_close();

						return true;
					} else {
						//add wip point
						wip.push_back(cpoint);
						_wip_changed();
						edited_point = PosVertex(-1, wip.size(), cpoint);
						selected_point = Vertex(wip.size() - 1);
						canvas_item_editor->update_viewport();
						return true;
					}
				}
			} else if (mb->get_button_index() == MouseButton::RIGHT && mb->is_pressed() && wip_active) {
				_wip_cancel();
			}
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;

	if (mm.is_valid()) {
		Vector2 gpoint = mm->get_position();

		if (edited_point.valid() && (wip_active || mm->get_button_mask().has_flag(MouseButtonMask::LEFT))) {
			Vector2 cpoint = canvas_item_editor->snap_point(canvas_item_editor->get_canvas_transform().affine_inverse().xform(gpoint));
			cpoint = _get_node()->get_screen_transform().affine_inverse().xform(cpoint);

			//Move the point in a single axis. Should only work when editing a polygon and while holding shift.
			if (mode == MODE_EDIT && mm->is_shift_pressed()) {
				Vector2 old_point = pre_move_edit.get(selected_point.vertex);
				if (ABS(cpoint.x - old_point.x) > ABS(cpoint.y - old_point.y)) {
					cpoint.y = old_point.y;
				} else {
					cpoint.x = old_point.x;
				}
			}

			edited_point = PosVertex(edited_point, cpoint);

			if (!wip_active) {
				Vector<Vector2> vertices = _get_polygon(edited_point.polygon);
				ERR_FAIL_INDEX_V(edited_point.vertex, vertices.size(), false);
				vertices.write[edited_point.vertex] = cpoint - _get_offset(edited_point.polygon);
				_set_polygon(edited_point.polygon, vertices);
			}

			canvas_item_editor->update_viewport();
		} else if (mode == MODE_EDIT || (_is_line() && mode == MODE_CREATE)) {
			const PosVertex new_hover_point = closest_point(gpoint);
			if (hover_point != new_hover_point) {
				hover_point = new_hover_point;
				canvas_item_editor->update_viewport();
			}

			bool edge_hover = false;
			if (!hover_point.valid()) {
				const PosVertex on_edge_vertex = closest_edge_point(gpoint);

				if (on_edge_vertex.valid()) {
					hover_point = Vertex();
					edge_point = on_edge_vertex;
					canvas_item_editor->update_viewport();
					edge_hover = true;
				}
			}

			if (!edge_hover && edge_point.valid()) {
				edge_point = PosVertex();
				canvas_item_editor->update_viewport();
			}
		}
	}

	Ref<InputEventKey> k = p_event;

	if (k.is_valid() && k->is_pressed()) {
		if (k->get_keycode() == Key::KEY_DELETE || k->get_keycode() == Key::BACKSPACE) {
			if (wip_active && selected_point.polygon == -1) {
				if (wip.size() > selected_point.vertex) {
					wip.remove_at(selected_point.vertex);
					_wip_changed();
					selected_point = wip.size() - 1;
					canvas_item_editor->update_viewport();
					return true;
				}
			} else {
				const Vertex active_point = get_active_point();

				if (active_point.valid()) {
					remove_point(active_point);
					return true;
				}
			}
		} else if (wip_active && k->get_keycode() == Key::ENTER) {
			_wip_close();
		} else if (wip_active && k->get_keycode() == Key::ESCAPE) {
			_wip_cancel();
		}
	}

	return false;
}

void AbstractPolygon2DEditor::forward_canvas_draw_over_viewport(Control *p_overlay) {
	if (!_get_node()) {
		return;
	}

	if (!_get_node()->is_visible_in_tree()) {
		return;
	}

	Viewport *vp = _get_node()->get_viewport();
	if (vp && !vp->is_visible_subviewport()) {
		return;
	}

	Transform2D xform = canvas_item_editor->get_canvas_transform() * _get_node()->get_screen_transform();
	// All polygon points are sharp, so use the sharp handle icon
	const Ref<Texture2D> handle = get_editor_theme_icon(SNAME("EditorPathSharpHandle"));

	const Vertex active_point = get_active_point();
	const int n_polygons = _get_polygon_count();
	const bool is_closed = !_is_line();

	for (int j = -1; j < n_polygons; j++) {
		if (wip_active && wip_destructive && j != -1) {
			continue;
		}

		Vector<Vector2> points;
		Vector2 offset;

		if (wip_active && j == edited_point.polygon) {
			points = Variant(wip);
			offset = Vector2(0, 0);
		} else {
			if (j == -1) {
				continue;
			}
			points = _get_polygon(j);
			offset = _get_offset(j);
		}

		if (!wip_active && j == edited_point.polygon && EDITOR_GET("editors/polygon_editor/show_previous_outline")) {
			const Color col = Color(0.5, 0.5, 0.5); // FIXME polygon->get_outline_color();
			const int n = pre_move_edit.size();
			for (int i = 0; i < n - (is_closed ? 0 : 1); i++) {
				Vector2 p, p2;
				p = pre_move_edit[i] + offset;
				p2 = pre_move_edit[(i + 1) % n] + offset;

				Vector2 point = xform.xform(p);
				Vector2 next_point = xform.xform(p2);

				p_overlay->draw_line(point, next_point, col, Math::round(2 * EDSCALE));
			}
		}

		const int n_points = points.size();
		const Color col = Color(1, 0.3, 0.1, 0.8);

		for (int i = 0; i < n_points; i++) {
			const Vertex vertex(j, i);

			const Vector2 p = (vertex == edited_point) ? edited_point.pos : (points[i] + offset);
			const Vector2 point = xform.xform(p);

			if (is_closed || i < n_points - 1) {
				Vector2 p2;
				if (j == edited_point.polygon &&
						((wip_active && i == n_points - 1) || (((i + 1) % n_points) == edited_point.vertex))) {
					p2 = edited_point.pos;
				} else {
					p2 = points[(i + 1) % n_points] + offset;
				}

				const Vector2 next_point = xform.xform(p2);
				p_overlay->draw_line(point, next_point, col, Math::round(2 * EDSCALE));
			}
		}

		for (int i = 0; i < n_points; i++) {
			const Vertex vertex(j, i);

			const Vector2 p = (vertex == edited_point) ? edited_point.pos : (points[i] + offset);
			const Vector2 point = xform.xform(p);

			const Color overlay_modulate = vertex == active_point ? Color(0.4, 1, 1) : Color(1, 1, 1);
			p_overlay->draw_texture(handle, point - handle->get_size() * 0.5, overlay_modulate);

			if (vertex == hover_point) {
				Ref<Font> font = get_theme_font(SNAME("bold"), EditorStringName(EditorFonts));
				int font_size = 1.3 * get_theme_font_size(SNAME("bold_size"), EditorStringName(EditorFonts));
				String num = String::num(vertex.vertex);
				Size2 num_size = font->get_string_size(num, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size);
				const float outline_size = 4;
				Color font_color = get_theme_color(SceneStringName(font_color), EditorStringName(Editor));
				Color outline_color = font_color.inverted();
				p_overlay->draw_string_outline(font, point - num_size * 0.5, num, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, outline_size, outline_color);
				p_overlay->draw_string(font, point - num_size * 0.5, num, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, font_color);
			}
		}
	}

	if (edge_point.valid()) {
		Ref<Texture2D> add_handle = get_editor_theme_icon(SNAME("EditorHandleAdd"));
		p_overlay->draw_texture(add_handle, edge_point.pos - add_handle->get_size() * 0.5);
	}
}

void AbstractPolygon2DEditor::edit(Node *p_polygon) {
	if (!canvas_item_editor) {
		canvas_item_editor = CanvasItemEditor::get_singleton();
	}

	if (p_polygon) {
		_set_node(p_polygon);

		// Enable the pencil tool if the polygon is empty.
		if (_is_empty()) {
			_menu_option(MODE_CREATE);
		} else {
			_menu_option(MODE_EDIT);
		}

		wip.clear();
		wip_active = false;
		edited_point = PosVertex();
		hover_point = Vertex();
		selected_point = Vertex();
	} else {
		_set_node(nullptr);
	}

	canvas_item_editor->update_viewport();
}

void AbstractPolygon2DEditor::remove_point(const Vertex &p_vertex) {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	Vector<Vector2> vertices = _get_polygon(p_vertex.polygon);

	if (vertices.size() > (_is_line() ? 2 : 3)) {
		vertices.remove_at(p_vertex.vertex);

		undo_redo->create_action(TTR("Edit Polygon (Remove Point)"));
		_action_set_polygon(p_vertex.polygon, vertices);
		_commit_action();
	} else {
		undo_redo->create_action(TTR("Remove Polygon And Point"));
		_action_remove_polygon(p_vertex.polygon);
		_commit_action();
	}

	if (_is_empty()) {
		_menu_option(MODE_CREATE);
	}

	hover_point = Vertex();
	if (selected_point == p_vertex) {
		selected_point = Vertex();
	}
}

AbstractPolygon2DEditor::Vertex AbstractPolygon2DEditor::get_active_point() const {
	return hover_point.valid() ? hover_point : selected_point;
}

AbstractPolygon2DEditor::PosVertex AbstractPolygon2DEditor::closest_point(const Vector2 &p_pos) const {
	const real_t grab_threshold = EDITOR_GET("editors/polygon_editor/point_grab_radius");

	const int n_polygons = _get_polygon_count();
	const Transform2D xform = canvas_item_editor->get_canvas_transform() * _get_node()->get_screen_transform();

	PosVertex closest;
	real_t closest_dist = 1e10;

	for (int j = 0; j < n_polygons; j++) {
		Vector<Vector2> points = _get_polygon(j);
		const Vector2 offset = _get_offset(j);
		const int n_points = points.size();

		for (int i = 0; i < n_points; i++) {
			Vector2 cp = xform.xform(points[i] + offset);

			real_t d = cp.distance_to(p_pos);
			if (d < closest_dist && d < grab_threshold) {
				closest_dist = d;
				closest = PosVertex(j, i, cp);
			}
		}
	}

	return closest;
}

AbstractPolygon2DEditor::PosVertex AbstractPolygon2DEditor::closest_edge_point(const Vector2 &p_pos) const {
	const real_t grab_threshold = EDITOR_GET("editors/polygon_editor/point_grab_radius");
	const real_t eps = grab_threshold * 2;
	const real_t eps2 = eps * eps;

	const int n_polygons = _get_polygon_count();
	const Transform2D xform = canvas_item_editor->get_canvas_transform() * _get_node()->get_screen_transform();

	PosVertex closest;
	real_t closest_dist = 1e10;

	for (int j = 0; j < n_polygons; j++) {
		Vector<Vector2> points = _get_polygon(j);
		const Vector2 offset = _get_offset(j);
		const int n_points = points.size();
		const int n_segments = n_points - (_is_line() ? 1 : 0);

		for (int i = 0; i < n_segments; i++) {
			Vector2 segment[2] = { xform.xform(points[i] + offset),
				xform.xform(points[(i + 1) % n_points] + offset) };

			Vector2 cp = Geometry2D::get_closest_point_to_segment(p_pos, segment);

			if (cp.distance_squared_to(segment[0]) < eps2 || cp.distance_squared_to(segment[1]) < eps2) {
				continue; //not valid to reuse point
			}

			real_t d = cp.distance_to(p_pos);
			if (d < closest_dist && d < grab_threshold) {
				closest_dist = d;
				closest = PosVertex(j, i, cp);
			}
		}
	}

	return closest;
}

AbstractPolygon2DEditor::AbstractPolygon2DEditor(bool p_wip_destructive) {
	edited_point = PosVertex();
	wip_destructive = p_wip_destructive;

	hover_point = Vertex();
	selected_point = Vertex();
	edge_point = PosVertex();

	button_create = memnew(Button);
	button_create->set_theme_type_variation(SceneStringName(FlatButton));
	add_child(button_create);
	button_create->connect(SceneStringName(pressed), callable_mp(this, &AbstractPolygon2DEditor::_menu_option).bind(MODE_CREATE));
	button_create->set_toggle_mode(true);

	button_edit = memnew(Button);
	button_edit->set_theme_type_variation(SceneStringName(FlatButton));
	add_child(button_edit);
	button_edit->connect(SceneStringName(pressed), callable_mp(this, &AbstractPolygon2DEditor::_menu_option).bind(MODE_EDIT));
	button_edit->set_toggle_mode(true);

	button_delete = memnew(Button);
	button_delete->set_theme_type_variation(SceneStringName(FlatButton));
	add_child(button_delete);
	button_delete->connect(SceneStringName(pressed), callable_mp(this, &AbstractPolygon2DEditor::_menu_option).bind(MODE_DELETE));
	button_delete->set_toggle_mode(true);

	create_resource = memnew(ConfirmationDialog);
	add_child(create_resource);
	create_resource->set_ok_button_text(TTR("Create"));
}

void AbstractPolygon2DEditorPlugin::edit(Object *p_object) {
	Node *polygon_node = Object::cast_to<Node>(p_object);
	polygon_editor->edit(polygon_node);
	make_visible(polygon_node != nullptr);
}

bool AbstractPolygon2DEditorPlugin::handles(Object *p_object) const {
	return p_object->is_class(klass);
}

void AbstractPolygon2DEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		polygon_editor->show();
	} else {
		polygon_editor->hide();
		polygon_editor->edit(nullptr);
	}
}

AbstractPolygon2DEditorPlugin::AbstractPolygon2DEditorPlugin(AbstractPolygon2DEditor *p_polygon_editor, const String &p_class) :
		polygon_editor(p_polygon_editor),
		klass(p_class) {
	CanvasItemEditor::get_singleton()->add_control_to_menu_panel(polygon_editor);
	polygon_editor->hide();
}

AbstractPolygon2DEditorPlugin::~AbstractPolygon2DEditorPlugin() {
}

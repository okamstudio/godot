/**************************************************************************/
/*  polygon_3d_editor_plugin.cpp                                          */
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

#include "polygon_3d_editor_plugin.h"

#include "core/input/input.h"
#include "core/math/geometry_2d.h"
#include "core/os/keyboard.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/plugins/canvas_item_editor_plugin.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "scene/3d/camera_3d.h"

void Polygon3DEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			button_create->set_button_icon(get_editor_theme_icon(SNAME("Edit")));
			button_edit->set_button_icon(get_editor_theme_icon(SNAME("MovePoint")));
			button_edit->set_pressed(true);
			get_tree()->connect("node_removed", callable_mp(this, &Polygon3DEditor::_node_removed));

		} break;

		case NOTIFICATION_PROCESS: {
			if (!node) {
				return;
			}

			if (_get_depth() != prev_depth) {
				_polygon_draw();
				prev_depth = _get_depth();
			}

		} break;
	}
}

void Polygon3DEditor::_node_removed(Node *p_node) {
	if (p_node == node) {
		node = nullptr;
		if (imgeom->get_parent() == p_node) {
			p_node->remove_child(imgeom);
		}
		hide();
		set_process(false);
	}
}

void Polygon3DEditor::_menu_option(int p_option) {
	switch (p_option) {
		case MODE_CREATE: {
			mode = MODE_CREATE;
			button_create->set_pressed(true);
			button_edit->set_pressed(false);
		} break;
		case MODE_EDIT: {
			mode = MODE_EDIT;
			button_create->set_pressed(false);
			button_edit->set_pressed(true);
		} break;
	}
}

void Polygon3DEditor::_wip_close() {
	Object *obj = node_resource.is_valid() ? (Object *)node_resource.ptr() : node;
	ERR_FAIL_NULL_MSG(obj, "Edited object is not valid.");
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Create Polygon3D"));
	undo_redo->add_undo_method(obj, "set_polygon", obj->call("get_polygon"));
	undo_redo->add_do_method(obj, "set_polygon", wip);
	undo_redo->add_do_method(this, "_polygon_draw");
	undo_redo->add_undo_method(this, "_polygon_draw");
	wip.clear();
	wip_active = false;
	mode = MODE_EDIT;
	button_edit->set_pressed(true);
	button_create->set_pressed(false);
	edited_point = -1;
	undo_redo->commit_action();
}

EditorPlugin::AfterGUIInput Polygon3DEditor::forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) {
	if (!node) {
		return EditorPlugin::AFTER_GUI_INPUT_PASS;
	}

	Object *obj = node_resource.is_valid() ? (Object *)node_resource.ptr() : node;
	Transform3D gt = node->get_global_transform();
	Transform3D gi = gt.affine_inverse();
	float depth = _get_depth() * 0.5;
	Vector3 n = gt.basis.get_column(2).normalized();
	Plane p(n, gt.origin + n * depth);

	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid()) {
		Vector2 gpoint = mb->get_position();
		Vector3 ray_from = p_camera->project_ray_origin(gpoint);
		Vector3 ray_dir = p_camera->project_ray_normal(gpoint);

		Vector3 spoint;

		if (!p.intersects_ray(ray_from, ray_dir, &spoint)) {
			return EditorPlugin::AFTER_GUI_INPUT_PASS;
		}

		spoint = gi.xform(spoint);

		Vector2 cpoint(spoint.x, spoint.y);

		//DO NOT snap here, it's confusing in 3D for adding points.
		//Let the snap happen when the point is being moved, instead.
		//cpoint = CanvasItemEditor::get_singleton()->snap_point(cpoint);

		PackedVector2Array poly = _get_polygon();

		//first check if a point is to be added (segment split)
		real_t grab_threshold = EDITOR_GET("editors/polygon_editor/point_grab_radius");

		switch (mode) {
			case MODE_CREATE: {
				if (mb->get_button_index() == MouseButton::LEFT && mb->is_pressed()) {
					if (!wip_active) {
						wip.clear();
						wip.push_back(cpoint);
						wip_active = true;
						edited_point_pos = cpoint;
						snap_ignore = false;
						_polygon_draw();
						edited_point = 1;
						return EditorPlugin::AFTER_GUI_INPUT_STOP;
					} else {
						if (wip.size() > 1 && p_camera->unproject_position(gt.xform(Vector3(wip[0].x, wip[0].y, depth))).distance_to(gpoint) < grab_threshold) {
							//wip closed
							_wip_close();

							return EditorPlugin::AFTER_GUI_INPUT_STOP;
						} else {
							wip.push_back(cpoint);
							edited_point = wip.size();
							snap_ignore = false;
							_polygon_draw();
							return EditorPlugin::AFTER_GUI_INPUT_STOP;
						}
					}
				} else if (mb->get_button_index() == MouseButton::RIGHT && mb->is_pressed() && wip_active) {
					_wip_close();
				}

			} break;

			case MODE_EDIT: {
				if (mb->get_button_index() == MouseButton::LEFT) {
					if (mb->is_pressed()) {
						if (mb->is_command_or_control_pressed()) {
							if (poly.size() < 3) {
								EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
								undo_redo->create_action(TTR("Edit Poly"));
								undo_redo->add_undo_method(obj, "set_polygon", poly);
								poly.push_back(cpoint);
								undo_redo->add_do_method(obj, "set_polygon", poly);
								undo_redo->add_do_method(this, "_polygon_draw");
								undo_redo->add_undo_method(this, "_polygon_draw");
								undo_redo->commit_action();
								return EditorPlugin::AFTER_GUI_INPUT_STOP;
							}

							//search edges
							int closest_idx = -1;
							Vector2 closest_pos;
							real_t closest_dist = 1e10;
							for (int i = 0; i < poly.size(); i++) {
								Vector2 points[2] = {
									p_camera->unproject_position(gt.xform(Vector3(poly[i].x, poly[i].y, depth))),
									p_camera->unproject_position(gt.xform(Vector3(poly[(i + 1) % poly.size()].x, poly[(i + 1) % poly.size()].y, depth)))
								};

								Vector2 cp = Geometry2D::get_closest_point_to_segment(gpoint, points);
								if (cp.distance_squared_to(points[0]) < CMP_EPSILON2 || cp.distance_squared_to(points[1]) < CMP_EPSILON2) {
									continue; //not valid to reuse point
								}

								real_t d = cp.distance_to(gpoint);
								if (d < closest_dist && d < grab_threshold) {
									closest_dist = d;
									closest_pos = cp;
									closest_idx = i;
								}
							}

							if (closest_idx >= 0) {
								pre_move_edit = poly;
								poly.insert(closest_idx + 1, cpoint);
								edited_point = closest_idx + 1;
								edited_point_pos = cpoint;
								_set_polygon(poly);
								_polygon_draw();
								snap_ignore = true;

								return EditorPlugin::AFTER_GUI_INPUT_STOP;
							}
						} else {
							//look for points to move

							int closest_idx = -1;
							Vector2 closest_pos;
							real_t closest_dist = 1e10;
							for (int i = 0; i < poly.size(); i++) {
								Vector2 cp = p_camera->unproject_position(gt.xform(Vector3(poly[i].x, poly[i].y, depth)));

								real_t d = cp.distance_to(gpoint);
								if (d < closest_dist && d < grab_threshold) {
									closest_dist = d;
									closest_pos = cp;
									closest_idx = i;
								}
							}

							if (closest_idx >= 0) {
								pre_move_edit = poly;
								edited_point = closest_idx;
								edited_point_pos = poly[closest_idx];
								_polygon_draw();
								snap_ignore = false;
								return EditorPlugin::AFTER_GUI_INPUT_STOP;
							}
						}
					} else {
						snap_ignore = false;

						if (edited_point != -1) {
							//apply

							ERR_FAIL_INDEX_V(edited_point, poly.size(), EditorPlugin::AFTER_GUI_INPUT_PASS);
							poly.write[edited_point] = edited_point_pos;
							EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
							undo_redo->create_action(TTR("Edit Poly"));
							undo_redo->add_do_method(obj, "set_polygon", poly);
							undo_redo->add_undo_method(obj, "set_polygon", pre_move_edit);
							undo_redo->add_do_method(this, "_polygon_draw");
							undo_redo->add_undo_method(this, "_polygon_draw");
							undo_redo->commit_action();

							edited_point = -1;
							return EditorPlugin::AFTER_GUI_INPUT_STOP;
						}
					}
				}
				if (mb->get_button_index() == MouseButton::RIGHT && mb->is_pressed() && edited_point == -1) {
					int closest_idx = -1;
					Vector2 closest_pos;
					real_t closest_dist = 1e10;
					for (int i = 0; i < poly.size(); i++) {
						Vector2 cp = p_camera->unproject_position(gt.xform(Vector3(poly[i].x, poly[i].y, depth)));

						real_t d = cp.distance_to(gpoint);
						if (d < closest_dist && d < grab_threshold) {
							closest_dist = d;
							closest_pos = cp;
							closest_idx = i;
						}
					}

					if (closest_idx >= 0) {
						EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
						undo_redo->create_action(TTR("Edit Poly (Remove Point)"));
						undo_redo->add_undo_method(obj, "set_polygon", poly);
						poly.remove_at(closest_idx);
						undo_redo->add_do_method(obj, "set_polygon", poly);
						undo_redo->add_do_method(this, "_polygon_draw");
						undo_redo->add_undo_method(this, "_polygon_draw");
						undo_redo->commit_action();
						return EditorPlugin::AFTER_GUI_INPUT_STOP;
					}
				}

			} break;
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;

	if (mm.is_valid()) {
		if (edited_point != -1 && (wip_active || mm->get_button_mask().has_flag(MouseButtonMask::LEFT))) {
			Vector2 gpoint = mm->get_position();

			Vector3 ray_from = p_camera->project_ray_origin(gpoint);
			Vector3 ray_dir = p_camera->project_ray_normal(gpoint);

			Vector3 spoint;

			if (!p.intersects_ray(ray_from, ray_dir, &spoint)) {
				return EditorPlugin::AFTER_GUI_INPUT_PASS;
			}

			spoint = gi.xform(spoint);

			Vector2 cpoint(spoint.x, spoint.y);

			if (snap_ignore && !Input::get_singleton()->is_key_pressed(Key::CMD_OR_CTRL)) {
				snap_ignore = false;
			}

			if (!snap_ignore && Node3DEditor::get_singleton()->is_snap_enabled()) {
				cpoint = cpoint.snappedf(Node3DEditor::get_singleton()->get_translate_snap());
			}
			edited_point_pos = cpoint;

			_polygon_draw();
		}
	}

	return EditorPlugin::AFTER_GUI_INPUT_PASS;
}

float Polygon3DEditor::_get_depth() {
	Object *obj = node_resource.is_valid() ? (Object *)node_resource.ptr() : node;
	ERR_FAIL_NULL_V_MSG(obj, 0.0f, "Edited object is not valid.");

	if (bool(obj->call("_has_editable_3d_polygon_no_depth"))) {
		return 0.0f;
	}

	return float(obj->call("get_depth"));
}

PackedVector2Array Polygon3DEditor::_get_polygon() {
	Object *obj = node_resource.is_valid() ? (Object *)node_resource.ptr() : node;
	ERR_FAIL_NULL_V_MSG(obj, PackedVector2Array(), "Edited object is not valid.");
	return PackedVector2Array(obj->call("get_polygon"));
}

void Polygon3DEditor::_set_polygon(const PackedVector2Array &p_poly) {
	Object *obj = node_resource.is_valid() ? (Object *)node_resource.ptr() : node;
	ERR_FAIL_NULL_MSG(obj, "Edited object is not valid.");
	obj->call("set_polygon", p_poly);
}

void Polygon3DEditor::_polygon_draw() {
	if (!node) {
		return;
	}

	PackedVector2Array poly;

	if (wip_active) {
		poly = wip;
	} else {
		poly = _get_polygon();
	}

	float depth = _get_depth() * 0.5;

	m->clear_surfaces();
	imesh->clear_surfaces();
	imgeom->set_material_override(line_material);
	imesh->surface_begin(Mesh::PRIMITIVE_LINES);

	Rect2 rect;

	for (int i = 0; i < poly.size(); i++) {
		Vector2 p, p2;
		p = i == edited_point ? edited_point_pos : poly[i];
		if ((wip_active && i == poly.size() - 1) || (((i + 1) % poly.size()) == edited_point)) {
			p2 = edited_point_pos;
		} else {
			p2 = poly[(i + 1) % poly.size()];
		}

		if (i == 0) {
			rect.position = p;
		} else {
			rect.expand_to(p);
		}

		Vector3 point = Vector3(p.x, p.y, depth);
		Vector3 next_point = Vector3(p2.x, p2.y, depth);

		imesh->surface_set_color(Color(1, 0.3, 0.1, 0.8));
		imesh->surface_add_vertex(point);
		imesh->surface_set_color(Color(1, 0.3, 0.1, 0.8));
		imesh->surface_add_vertex(next_point);

		//Color col=Color(1,0.3,0.1,0.8);
		//vpc->draw_line(point,next_point,col,2);
		//vpc->draw_texture(handle,point-handle->get_size()*0.5);
	}

	rect = rect.grow(1);

	AABB r;
	r.position.x = rect.position.x;
	r.position.y = rect.position.y;
	r.position.z = depth;
	r.size.x = rect.size.x;
	r.size.y = rect.size.y;
	r.size.z = 0;

	imesh->surface_set_color(Color(0.8, 0.8, 0.8, 0.2));
	imesh->surface_add_vertex(r.position);
	imesh->surface_set_color(Color(0.8, 0.8, 0.8, 0.2));
	imesh->surface_add_vertex(r.position + Vector3(0.3, 0, 0));
	imesh->surface_set_color(Color(0.8, 0.8, 0.8, 0.2));
	imesh->surface_add_vertex(r.position);
	imesh->surface_set_color(Color(0.8, 0.8, 0.8, 0.2));
	imesh->surface_add_vertex(r.position + Vector3(0.0, 0.3, 0));

	imesh->surface_set_color(Color(0.8, 0.8, 0.8, 0.2));
	imesh->surface_add_vertex(r.position + Vector3(r.size.x, 0, 0));
	imesh->surface_set_color(Color(0.8, 0.8, 0.8, 0.2));
	imesh->surface_add_vertex(r.position + Vector3(r.size.x, 0, 0) - Vector3(0.3, 0, 0));
	imesh->surface_set_color(Color(0.8, 0.8, 0.8, 0.2));
	imesh->surface_add_vertex(r.position + Vector3(r.size.x, 0, 0));
	imesh->surface_set_color(Color(0.8, 0.8, 0.8, 0.2));
	imesh->surface_add_vertex(r.position + Vector3(r.size.x, 0, 0) + Vector3(0, 0.3, 0));

	imesh->surface_set_color(Color(0.8, 0.8, 0.8, 0.2));
	imesh->surface_add_vertex(r.position + Vector3(0, r.size.y, 0));
	imesh->surface_set_color(Color(0.8, 0.8, 0.8, 0.2));
	imesh->surface_add_vertex(r.position + Vector3(0, r.size.y, 0) - Vector3(0, 0.3, 0));
	imesh->surface_set_color(Color(0.8, 0.8, 0.8, 0.2));
	imesh->surface_add_vertex(r.position + Vector3(0, r.size.y, 0));
	imesh->surface_set_color(Color(0.8, 0.8, 0.8, 0.2));
	imesh->surface_add_vertex(r.position + Vector3(0, r.size.y, 0) + Vector3(0.3, 0, 0));

	imesh->surface_set_color(Color(0.8, 0.8, 0.8, 0.2));
	imesh->surface_add_vertex(r.position + r.size);
	imesh->surface_set_color(Color(0.8, 0.8, 0.8, 0.2));
	imesh->surface_add_vertex(r.position + r.size - Vector3(0.3, 0, 0));
	imesh->surface_set_color(Color(0.8, 0.8, 0.8, 0.2));
	imesh->surface_add_vertex(r.position + r.size);
	imesh->surface_set_color(Color(0.8, 0.8, 0.8, 0.2));
	imesh->surface_add_vertex(r.position + r.size - Vector3(0.0, 0.3, 0));

	imesh->surface_end();

	if (poly.size() == 0) {
		return;
	}

	Array a;
	a.resize(Mesh::ARRAY_MAX);
	Vector<Vector3> va;
	{
		va.resize(poly.size());
		Vector3 *w = va.ptrw();
		for (int i = 0; i < poly.size(); i++) {
			Vector2 p, p2;
			p = i == edited_point ? edited_point_pos : poly[i];

			Vector3 point = Vector3(p.x, p.y, depth);
			w[i] = point;
		}
	}
	a[Mesh::ARRAY_VERTEX] = va;
	m->add_surface_from_arrays(Mesh::PRIMITIVE_POINTS, a);
	m->surface_set_material(0, handle_material);
}

void Polygon3DEditor::edit(Node *p_node) {
	if (p_node) {
		node = Object::cast_to<Node3D>(p_node);
		node_resource = node->call("_get_editable_3d_polygon_resource");

		if (node_resource.is_valid()) {
			node_resource->connect_changed(callable_mp(this, &Polygon3DEditor::_polygon_draw));
		}
		//Enable the pencil tool if the polygon is empty
		if (_get_polygon().is_empty()) {
			_menu_option(MODE_CREATE);
		}
		wip.clear();
		wip_active = false;
		edited_point = -1;
		if (imgeom->get_parent()) {
			imgeom->reparent(p_node, false);
		} else {
			p_node->add_child(imgeom);
		}
		_polygon_draw();
		set_process(true);
		prev_depth = -1;

	} else {
		node = nullptr;
		if (node_resource.is_valid()) {
			node_resource->disconnect_changed(callable_mp(this, &Polygon3DEditor::_polygon_draw));
		}
		node_resource.unref();

		if (imgeom->get_parent()) {
			imgeom->get_parent()->remove_child(imgeom);
		}

		set_process(false);
	}
}

void Polygon3DEditor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_polygon_draw"), &Polygon3DEditor::_polygon_draw);
}

Polygon3DEditor::Polygon3DEditor() {
	node = nullptr;

	button_create = memnew(Button);
	button_create->set_theme_type_variation(SceneStringName(FlatButton));
	add_child(button_create);
	button_create->connect(SceneStringName(pressed), callable_mp(this, &Polygon3DEditor::_menu_option).bind(MODE_CREATE));
	button_create->set_toggle_mode(true);

	button_edit = memnew(Button);
	button_edit->set_theme_type_variation(SceneStringName(FlatButton));
	add_child(button_edit);
	button_edit->connect(SceneStringName(pressed), callable_mp(this, &Polygon3DEditor::_menu_option).bind(MODE_EDIT));
	button_edit->set_toggle_mode(true);

	mode = MODE_EDIT;
	wip_active = false;
	imgeom = memnew(MeshInstance3D);
	imesh.instantiate();
	imgeom->set_mesh(imesh);
	imgeom->set_transform(Transform3D(Basis(), Vector3(0, 0, 0.00001)));

	line_material.instantiate();
	line_material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	line_material->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
	line_material->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	line_material->set_flag(StandardMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
	line_material->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	line_material->set_albedo(Color(1, 1, 1));

	handle_material.instantiate();
	handle_material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	handle_material->set_flag(StandardMaterial3D::FLAG_USE_POINT_SIZE, true);
	handle_material->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
	handle_material->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	handle_material->set_flag(StandardMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
	handle_material->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	Ref<Texture2D> handle = EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("Editor3DHandle"), EditorStringName(EditorIcons));
	handle_material->set_point_size(handle->get_width());
	handle_material->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, handle);

	pointsm = memnew(MeshInstance3D);
	imgeom->add_child(pointsm);
	m.instantiate();
	pointsm->set_mesh(m);
	pointsm->set_transform(Transform3D(Basis(), Vector3(0, 0, 0.00001)));

	snap_ignore = false;
}

Polygon3DEditor::~Polygon3DEditor() {
	memdelete(imgeom);
}

void Polygon3DEditorPlugin::edit(Object *p_object) {
	polygon_editor->edit(Object::cast_to<Node>(p_object));
}

bool Polygon3DEditorPlugin::handles(Object *p_object) const {
	return Object::cast_to<Node3D>(p_object) && bool(p_object->call("_is_editable_3d_polygon"));
}

void Polygon3DEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		polygon_editor->show();
	} else {
		polygon_editor->hide();
		polygon_editor->edit(nullptr);
	}
}

Polygon3DEditorPlugin::Polygon3DEditorPlugin() {
	polygon_editor = memnew(Polygon3DEditor);
	Node3DEditor::get_singleton()->add_control_to_menu_panel(polygon_editor);

	polygon_editor->hide();
}

Polygon3DEditorPlugin::~Polygon3DEditorPlugin() {
}

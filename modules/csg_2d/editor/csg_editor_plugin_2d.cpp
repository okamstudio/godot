/**************************************************************************/
/*  csg_editor_plugin_2d.cpp                                              */
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

#include "csg_editor_plugin_2d.h"

#ifdef TOOLS_ENABLED

#include "../csg_capsule_2d.h"
#include "../csg_circle_2d.h"
#include "../csg_mesh_2d.h"
#include "../csg_polygon_2d.h"
#include "../csg_rectangle_2d.h"

#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/plugins/canvas_item_editor_plugin.h"
#include "editor/themes/editor_scale.h"
#include "scene/2d/camera_2d.h"
#include "scene/2d/light_occluder_2d.h"
#include "scene/2d/mesh_instance_2d.h"
#include "scene/2d/navigation_region_2d.h"
#include "scene/2d/physics/collision_shape_2d.h"
#include "scene/2d/physics/static_body_2d.h"
#include "scene/2d/polygon_2d.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/menu_button.h"

Node2D *CSGPolygon2DEditor::_get_node() const {
	return node;
}

void CSGPolygon2DEditor::_set_node(Node *p_polygon) {
	node = Object::cast_to<CSGPolygon2D>(p_polygon);
}

Variant CSGPolygon2DEditor::_get_polygon(int p_idx) const {
	return node->get_polygon();
}

void CSGPolygon2DEditor::_set_polygon(int p_idx, const Variant &p_polygon) const {
	node->set_polygon(p_polygon);
}

void CSGPolygon2DEditor::_action_add_polygon(const Variant &p_polygon) {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->add_do_method(node, "set_polygon", p_polygon);
	undo_redo->add_undo_method(node, "set_polygon", node->get_polygon());
}

void CSGPolygon2DEditor::_action_remove_polygon(int p_idx) {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->add_do_method(node, "set_polygon", Variant(Vector<Vector2>()));
	undo_redo->add_undo_method(node, "set_polygon", node->get_polygon());
}

void CSGPolygon2DEditor::_action_set_polygon(int p_idx, const Variant &p_previous, const Variant &p_polygon) {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->add_do_method(node, "set_polygon", p_polygon);
	undo_redo->add_undo_method(node, "set_polygon", node->get_polygon());
}

CSGPolygon2DEditor::CSGPolygon2DEditor() {
	node = nullptr;
}

///////////

void CSGShape2DEditor::_node_removed(Node *p_node) {
	if (p_node == node) {
		node = nullptr;
		options->hide();
	}
}

void CSGShape2DEditor::edit(CSGShape2D *p_csg_shape) {
	if (!canvas_item_editor) {
		canvas_item_editor = CanvasItemEditor::get_singleton();
	}

	node = p_csg_shape;
	polygon_editor->edit(p_csg_shape);

	if (node) {
		polygon_editor->hide();
		if (Object::cast_to<CSGCapsule2D>(node)) {
			shape_type = CSGShapeType::CAPSULE_SHAPE;
		} else if (Object::cast_to<CSGCircle2D>(node)) {
			shape_type = CSGShapeType::CIRCLE_SHAPE;
		} else if (Object::cast_to<CSGPolygon2D>(node)) {
			shape_type = CSGShapeType::POLYGON_SHAPE;
			polygon_editor->show();
		} else if (Object::cast_to<CSGMesh2D>(node)) {
			shape_type = CSGShapeType::MESH_SHAPE;
		} else if (Object::cast_to<CSGRectangle2D>(node)) {
			shape_type = CSGShapeType::RECTANGLE_SHAPE;
		} else {
			shape_type = CSGShapeType::NONE;
		}
		if (node->is_root_shape()) {
			options->show();
		}
	} else {
		shape_type = CSGShapeType::NONE;
		options->hide();
		polygon_editor->hide();
		if (pressed) {
			set_handle(edit_handle, original_point);
			pressed = false;
		}
		edit_handle = -1;
	}

	if (options->is_visible() || polygon_editor->is_visible()) {
		toolbar->show();
	} else {
		toolbar->hide();
	}

	canvas_item_editor->update_viewport();
}

Variant CSGShape2DEditor::get_handle_value(int idx) const {
	switch (shape_type) {
		case CSGShapeType::CAPSULE_SHAPE: {
			CSGCapsule2D *shape = Object::cast_to<CSGCapsule2D>(node);
			ERR_FAIL_NULL_V(shape, Variant());

			return Vector2(shape->get_radius(), shape->get_height());
		} break;

		case CSGShapeType::CIRCLE_SHAPE: {
			CSGCircle2D *shape = Object::cast_to<CSGCircle2D>(node);
			ERR_FAIL_NULL_V(shape, Variant());

			if (idx == 0) {
				return shape->get_radius();
			}
		} break;

		case CSGShapeType::POLYGON_SHAPE: {
			CSGPolygon2D *shape = Object::cast_to<CSGPolygon2D>(node);
			ERR_FAIL_NULL_V(shape, Variant());

			const Vector<Vector2> &gpolygon = shape->get_polygon();
			return gpolygon[idx];
		} break;

		case CSGShapeType::RECTANGLE_SHAPE: {
			CSGRectangle2D *shape = Object::cast_to<CSGRectangle2D>(node);
			ERR_FAIL_NULL_V(shape, Variant());

			if (idx < 8) {
				return shape->get_size().abs();
			}
		} break;

		default: {
			break;
		}
	}

	return Variant();
}

void CSGShape2DEditor::set_handle(int idx, Point2 &p_point) {
	switch (shape_type) {
		case CAPSULE_SHAPE: {
			if (idx < 2) {
				CSGCapsule2D *shape = Object::cast_to<CSGCapsule2D>(node);
				ERR_FAIL_NULL(shape);

				real_t parameter = Math::abs(p_point[idx]);

				if (idx == 0) {
					shape->set_radius(parameter);
				} else if (idx == 1) {
					shape->set_height(parameter * 2);
				}
			}
		} break;

		case CIRCLE_SHAPE: {
			CSGCircle2D *shape = Object::cast_to<CSGCircle2D>(node);
			ERR_FAIL_NULL(shape);
			shape->set_radius(p_point.length());
		} break;

		case POLYGON_SHAPE: {
			CSGPolygon2D *shape = Object::cast_to<CSGPolygon2D>(node);
			ERR_FAIL_NULL(shape);

			Vector<Vector2> polygon = shape->get_polygon();

			ERR_FAIL_INDEX(idx, polygon.size());
			polygon.write[idx] = p_point;

			shape->set_polygon(polygon);
		} break;

		case RECTANGLE_SHAPE: {
			if (idx < 8) {
				CSGRectangle2D *shape = Object::cast_to<CSGRectangle2D>(node);
				ERR_FAIL_NULL(shape);

				Vector2 size = (Point2)original;

				if (RECT_HANDLES[idx].x != 0) {
					size.x = p_point.x * RECT_HANDLES[idx].x * 2;
				}
				if (RECT_HANDLES[idx].y != 0) {
					size.y = p_point.y * RECT_HANDLES[idx].y * 2;
				}

				if (Input::get_singleton()->is_key_pressed(Key::ALT)) {
					shape->set_size(size.abs());
					node->set_global_position(original_transform.get_origin());
				} else {
					shape->set_size(((Point2)original + (size - (Point2)original) * 0.5).abs());
					Point2 pos = original_transform.affine_inverse().xform(original_transform.get_origin());
					pos += (size - (Point2)original) * 0.5 * RECT_HANDLES[idx] * 0.5;
					node->set_global_position(original_transform.xform(pos));
				}
			}
		} break;

		default: {
			break;
		}
	}

	canvas_item_editor->update_viewport();
}

void CSGShape2DEditor::commit_handle(int idx, Variant &p_org) {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Set Handle"));

	switch (shape_type) {
		case CSGShapeType::CAPSULE_SHAPE: {
			CSGCapsule2D *shape = Object::cast_to<CSGCapsule2D>(node);
			ERR_FAIL_NULL(shape);

			Vector2 values = p_org;

			if (idx == 0) {
				undo_redo->add_do_method(shape, "set_radius", shape->get_radius());
			} else if (idx == 1) {
				undo_redo->add_do_method(shape, "set_height", shape->get_height());
			}
			undo_redo->add_undo_method(shape, "set_radius", values[0]);
			undo_redo->add_undo_method(shape, "set_height", values[1]);
		} break;

		case CSGShapeType::CIRCLE_SHAPE: {
			CSGCircle2D *shape = Object::cast_to<CSGCircle2D>(node);
			ERR_FAIL_NULL(shape);

			undo_redo->add_do_method(shape, "set_radius", shape->get_radius());
			undo_redo->add_undo_method(shape, "set_radius", p_org);
		} break;

		case CSGShapeType::POLYGON_SHAPE: {
			CSGPolygon2D *shape = Object::cast_to<CSGPolygon2D>(node);
			ERR_FAIL_NULL(shape);

			Vector2 values = p_org;

			Vector<Vector2> undo_polygon = shape->get_polygon();
			ERR_FAIL_INDEX(idx, undo_polygon.size());
			undo_polygon.write[idx] = values;

			undo_redo->add_do_method(shape, "set_polygon", shape->get_polygon());
			undo_redo->add_undo_method(shape, "set_polygon", undo_polygon);
		} break;

		case CSGShapeType::RECTANGLE_SHAPE: {
			CSGRectangle2D *shape = Object::cast_to<CSGRectangle2D>(node);
			ERR_FAIL_NULL(shape);

			undo_redo->add_do_method(shape, "set_size", shape->get_size());
			undo_redo->add_do_method(node, "set_global_transform", node->get_global_transform());

			undo_redo->add_undo_method(shape, "set_size", p_org);
			undo_redo->add_undo_method(node, "set_global_transform", original_transform);
		} break;

		default: {
			break;
		}
	}

	undo_redo->commit_action();

	canvas_item_editor->update_viewport();
}

void CSGShape2DEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			options->set_button_icon(get_editor_theme_icon(SNAME("CSGCombiner2D")));
		} break;

		case NOTIFICATION_ENTER_TREE: {
			get_tree()->connect("node_removed", callable_mp(this, &CSGShape2DEditor::_node_removed));
		} break;

		case NOTIFICATION_EXIT_TREE: {
			get_tree()->disconnect("node_removed", callable_mp(this, &CSGShape2DEditor::_node_removed));
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (EditorSettings::get_singleton()->check_changed_settings_in_group("editors/polygon_editor")) {
				grab_threshold = EDITOR_GET("editors/polygon_editor/point_grab_radius");
			}
		} break;
	}
}

bool CSGShape2DEditor::forward_canvas_gui_input(const Ref<InputEvent> &p_event) {
	if (!node) {
		return false;
	}

	if (!node->is_visible_in_tree()) {
		return false;
	}

	if (shape_type == CSGShapeType::NONE) {
		return false;
	}

	if (shape_type == CSGShapeType::POLYGON_SHAPE) {
		return polygon_editor->forward_gui_input(p_event);
	}

	Ref<InputEventMouseButton> mb = p_event;
	Transform2D xform = canvas_item_editor->get_canvas_transform() * node->get_screen_transform();

	if (mb.is_valid()) {
		Vector2 gpoint = mb->get_position();

		if (mb->get_button_index() == MouseButton::LEFT) {
			if (mb->is_pressed()) {
				for (uint32_t i = 0; i < handles.size(); i++) {
					if (xform.xform(handles[i]).distance_to(gpoint) < grab_threshold) {
						edit_handle = i;

						break;
					}
				}

				if (edit_handle == -1) {
					pressed = false;

					return false;
				}

				original_mouse_pos = gpoint;
				original_point = handles[edit_handle];
				original = get_handle_value(edit_handle);
				original_transform = node->get_global_transform();
				last_point = original;
				pressed = true;

				return true;

			} else {
				if (pressed) {
					if (original_mouse_pos != gpoint) {
						commit_handle(edit_handle, original);
					}

					edit_handle = -1;
					pressed = false;

					return true;
				}
			}
		}

		return false;
	}

	Ref<InputEventMouseMotion> mm = p_event;

	if (mm.is_valid()) {
		if (edit_handle == -1 || !pressed) {
			return false;
		}

		Vector2 cpoint = canvas_item_editor->snap_point(canvas_item_editor->get_canvas_transform().affine_inverse().xform(mm->get_position()));
		cpoint = node->get_viewport()->get_popup_base_transform().affine_inverse().xform(cpoint);
		cpoint = original_transform.affine_inverse().xform(cpoint);
		last_point = cpoint;

		set_handle(edit_handle, cpoint);

		return true;
	}

	Ref<InputEventKey> k = p_event;

	if (k.is_valid()) {
		if (edit_handle == -1 || !pressed || k->is_echo()) {
			return false;
		}

		if (shape_type == CSGShapeType::RECTANGLE_SHAPE && k->get_keycode() == Key::ALT) {
			set_handle(edit_handle, last_point); // Update handle when Alt key is toggled.
		}
	}

	return false;
}

void CSGShape2DEditor::forward_canvas_draw_over_viewport(Control *p_overlay) {
	if (!node) {
		return;
	}

	if (!node->is_visible_in_tree()) {
		return;
	}

	if (shape_type == CSGShapeType::NONE) {
		return;
	}

	if (shape_type == CSGShapeType::POLYGON_SHAPE) {
		polygon_editor->forward_canvas_draw_over_viewport(p_overlay);
		return;
	}

	Transform2D gt = canvas_item_editor->get_canvas_transform() * node->get_screen_transform();

	Ref<Texture2D> h = get_editor_theme_icon(SNAME("EditorHandle"));
	Vector2 size = h->get_size() * 0.5;

	handles.clear();

	switch (shape_type) {
		case CSGShapeType::CAPSULE_SHAPE: {
			CSGCapsule2D *shape = Object::cast_to<CSGCapsule2D>(node);
			ERR_FAIL_NULL(shape);

			handles.resize(2);
			float radius = shape->get_radius();
			float height = shape->get_height() / 2;

			handles[0] = Point2(radius, 0);
			handles[1] = Point2(0, height);

			p_overlay->draw_texture(h, gt.xform(handles[0]) - size);
			p_overlay->draw_texture(h, gt.xform(handles[1]) - size);

		} break;

		case CSGShapeType::CIRCLE_SHAPE: {
			CSGCircle2D *shape = Object::cast_to<CSGCircle2D>(node);
			ERR_FAIL_NULL(shape);

			handles.resize(1);
			handles[0] = Point2(shape->get_radius(), 0);

			p_overlay->draw_texture(h, gt.xform(handles[0]) - size);

		} break;

		case CSGShapeType::MESH_SHAPE: {
		} break;

		case CSGShapeType::POLYGON_SHAPE: {
			CSGPolygon2D *shape = Object::cast_to<CSGPolygon2D>(node);
			ERR_FAIL_NULL(shape);

			const Vector<Vector2> &points = shape->get_polygon();

			handles.resize(points.size());
			for (uint32_t i = 0; i < handles.size(); i++) {
				handles[i] = points[i];
				p_overlay->draw_texture(h, gt.xform(handles[i]) - size);
			}

		} break;

		case CSGShapeType::RECTANGLE_SHAPE: {
			CSGRectangle2D *shape = Object::cast_to<CSGRectangle2D>(node);
			ERR_FAIL_NULL(shape);

			handles.resize(8);
			Vector2 ext = shape->get_size() / 2;
			for (uint32_t i = 0; i < handles.size(); i++) {
				handles[i] = RECT_HANDLES[i] * ext;
				p_overlay->draw_texture(h, gt.xform(handles[i]) - size);
			}

		} break;

		default: {
			break;
		}
	}
}

void CSGShape2DEditor::_menu_option(int p_option) {
	Ref<ArrayMesh> mesh = node->bake_static_mesh();
	if (mesh.is_null()) {
		err_dialog->set_text(TTR("CSG operation returned an empty result."));
		err_dialog->popup_centered();
		return;
	}

	switch (p_option) {
		case MENU_OPTION_BAKE_MESH_INSTANCE: {
			_create_baked_mesh_instance();
		} break;
		case MENU_OPTION_BAKE_COLLISION_SHAPE: {
			_create_baked_collision_shape();
		} break;
		case MENU_OPTION_BAKE_POLYGON_2D: {
			_create_baked_polygon_2d();
		} break;
		case MENU_OPTION_BAKE_LIGHT_OCCLUDER_2D: {
			_create_baked_light_occluder_2d();
		} break;
		case MENU_OPTION_BAKE_NAVIGATION_REGION_2D: {
			_create_baked_navigation_region_2d();
		} break;
	}
}

void CSGShape2DEditor::_create_baked_mesh_instance() {
	if (node == get_tree()->get_edited_scene_root()) {
		err_dialog->set_text(TTR("Can not add a baked mesh as sibling for the scene root.\nMove the CSG root node below a parent node."));
		err_dialog->popup_centered();
		return;
	}

	Ref<ArrayMesh> mesh = node->bake_static_mesh();
	if (mesh.is_null()) {
		err_dialog->set_text(TTR("CSG operation returned an empty mesh."));
		err_dialog->popup_centered();
		return;
	}

	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
	ur->create_action(TTR("Create baked CSGShape2D Mesh Instance"));

	Node *owner = get_tree()->get_edited_scene_root();

	MeshInstance2D *mi = memnew(MeshInstance2D);
	mi->set_mesh(mesh);
	mi->set_name("CSGBakedMeshInstance2D");
	mi->set_transform(node->get_transform());
	ur->add_do_method(node, "add_sibling", mi, true);
	ur->add_do_method(mi, "set_owner", owner);

	ur->add_do_reference(mi);
	ur->add_undo_method(node->get_parent(), "remove_child", mi);

	ur->commit_action();
}

void CSGShape2DEditor::_create_baked_collision_shape() {
	if (node == get_tree()->get_edited_scene_root()) {
		err_dialog->set_text(TTR("Can not add a baked collision shape as sibling for the scene root.\nMove the CSG root node below a parent node."));
		err_dialog->popup_centered();
		return;
	}

	Array shapes = node->bake_collision_shapes();
	if (shapes.is_empty()) {
		err_dialog->set_text(TTR("CSG operation returned an empty result."));
		err_dialog->popup_centered();
		return;
	}

	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
	ur->create_action(TTR("Create baked CSGShape2D Collision Shape"));

	Node *owner = get_tree()->get_edited_scene_root();

	for (int i = 0; i < shapes.size(); i++) {
		Ref<Shape2D> shape = shapes[i];

		CollisionShape2D *cshape = memnew(CollisionShape2D);
		cshape->set_shape(shape);
		cshape->set_name("CSGBakedCollisionShape2D");
		cshape->set_transform(node->get_transform());
		ur->add_do_method(node, "add_sibling", cshape, true);
		ur->add_do_method(cshape, "set_owner", owner);

		ur->add_do_reference(cshape);
		ur->add_undo_method(node->get_parent(), "remove_child", cshape);
	}

	ur->commit_action();
}

void CSGShape2DEditor::_create_baked_polygon_2d() {
	if (node == get_tree()->get_edited_scene_root()) {
		err_dialog->set_text(TTR("Can not add a baked Polygon2D as sibling for the scene root.\nMove the CSG root node below a parent node."));
		err_dialog->popup_centered();
		return;
	}

	Ref<ArrayMesh> mesh = node->bake_static_mesh();
	if (mesh.is_null()) {
		err_dialog->set_text(TTR("CSG operation returned an empty result."));
		err_dialog->popup_centered();
		return;
	}

	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
	ur->create_action(TTR("Create baked CSGShape2D Polygon2D"));

	Node *owner = get_tree()->get_edited_scene_root();

	Polygon2D *polygon_2d = memnew(Polygon2D);
	polygon_2d->set_name("CSGBakedPolygon2D");
	polygon_2d->set_transform(node->get_transform());

	const LocalVector<Vector<int>> &mesh_convex_polygons = node->get_mesh_convex_polygons();
	Array polygons;
	polygons.resize(mesh_convex_polygons.size());
	for (uint32_t i = 0; i < mesh_convex_polygons.size(); i++) {
		polygons[i] = mesh_convex_polygons[i];
	}

	polygon_2d->set_polygon(node->get_mesh_vertices());
	polygon_2d->set_polygons(polygons);

	ur->add_do_method(node, "add_sibling", polygon_2d, true);
	ur->add_do_method(polygon_2d, "set_owner", owner);

	ur->add_do_reference(polygon_2d);
	ur->add_undo_method(node->get_parent(), "remove_child", polygon_2d);

	ur->commit_action();
}

void CSGShape2DEditor::_create_baked_light_occluder_2d() {
	if (node == get_tree()->get_edited_scene_root()) {
		err_dialog->set_text(TTR("Can not add a baked LightOccluder2D as sibling for the scene root.\nMove the CSG root node below a parent node."));
		err_dialog->popup_centered();
		return;
	}

	Ref<ArrayMesh> mesh = node->bake_static_mesh();
	if (mesh.is_null()) {
		err_dialog->set_text(TTR("CSG operation returned an empty result."));
		err_dialog->popup_centered();
		return;
	}

	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
	ur->create_action(TTR("Create baked CSGShape2D LightOccluder2D"));

	Node *owner = get_tree()->get_edited_scene_root();

	const LocalVector<Vector<Vector2>> &mesh_outlines = node->get_mesh_outlines();

	for (uint32_t i = 0; i < mesh_outlines.size(); i++) {
		if (mesh_outlines[i].size() < 3) {
			continue;
		}
		Vector<Vector2> occluder_polygon = mesh_outlines[i];
		occluder_polygon.push_back(occluder_polygon[0]);

		OccluderPolygon2D *occluder_polygon_2d = memnew(OccluderPolygon2D);
		occluder_polygon_2d->set_closed(false);
		occluder_polygon_2d->set_polygon(occluder_polygon);

		LightOccluder2D *light_occluder_2d = memnew(LightOccluder2D);
		light_occluder_2d->set_name("CSGBakedLightOccluder2D");
		light_occluder_2d->set_transform(node->get_transform());
		light_occluder_2d->set_occluder_polygon(occluder_polygon_2d);

		ur->add_do_method(node, "add_sibling", light_occluder_2d, true);
		ur->add_do_method(light_occluder_2d, "set_owner", owner);

		ur->add_do_reference(light_occluder_2d);
		ur->add_undo_method(node->get_parent(), "remove_child", light_occluder_2d);
	}

	ur->commit_action();
}

void CSGShape2DEditor::_create_baked_navigation_region_2d() {
	if (node == get_tree()->get_edited_scene_root()) {
		err_dialog->set_text(TTR("Can not add a baked NavigationRegion2D as sibling for the scene root.\nMove the CSG root node below a parent node."));
		err_dialog->popup_centered();
		return;
	}

	Ref<NavigationPolygon> navigation_mesh = node->bake_navigation_mesh();
	if (navigation_mesh.is_null()) {
		err_dialog->set_text(TTR("CSG operation returned an empty mesh."));
		err_dialog->popup_centered();
		return;
	}

	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
	ur->create_action(TTR("Create baked CSGShape2D NavigationRegion2D"));

	Node *owner = get_tree()->get_edited_scene_root();

	NavigationRegion2D *navigation_region_2d = memnew(NavigationRegion2D);
	navigation_region_2d->set_navigation_polygon(navigation_mesh);
	navigation_region_2d->set_name("CSGBakedNavigationRegion2D");
	navigation_region_2d->set_transform(node->get_transform());
	ur->add_do_method(node, "add_sibling", navigation_region_2d, true);
	ur->add_do_method(navigation_region_2d, "set_owner", owner);

	ur->add_do_reference(navigation_region_2d);
	ur->add_undo_method(node->get_parent(), "remove_child", navigation_region_2d);

	ur->commit_action();
}

CSGShape2DEditor::CSGShape2DEditor() {
	polygon_editor = memnew(CSGPolygon2DEditor);

	toolbar = memnew(HBoxContainer);
	toolbar->hide();
	CanvasItemEditor::get_singleton()->add_control_to_menu_panel(toolbar);

	options = memnew(MenuButton);
	options->hide();
	options->set_text(TTR("CSG2D"));
	options->set_switch_on_hover(true);

	options->get_popup()->add_item(TTR("Bake MeshInstance2D"), MENU_OPTION_BAKE_MESH_INSTANCE);
	options->get_popup()->add_item(TTR("Bake CollisionShape2D"), MENU_OPTION_BAKE_COLLISION_SHAPE);
	options->get_popup()->add_item(TTR("Bake Polygon2D"), MENU_OPTION_BAKE_POLYGON_2D);
	options->get_popup()->add_item(TTR("Bake LightOccluder2D"), MENU_OPTION_BAKE_LIGHT_OCCLUDER_2D);
	options->get_popup()->add_item(TTR("Bake NavigationRegion2D"), MENU_OPTION_BAKE_NAVIGATION_REGION_2D);

	options->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &CSGShape2DEditor::_menu_option));

	toolbar->add_child(options);
	toolbar->add_child(polygon_editor);

	err_dialog = memnew(AcceptDialog);
	add_child(err_dialog);
}

///////////

void EditorPluginCSG2D::edit(Object *p_object) {
	CSGShape2D *csg_shape = Object::cast_to<CSGShape2D>(p_object);
	if (csg_shape) {
		csg_shape_editor->edit(csg_shape);
	} else {
		csg_shape_editor->edit(nullptr);
	}
}

bool EditorPluginCSG2D::handles(Object *p_object) const {
	CSGShape2D *csg_shape = Object::cast_to<CSGShape2D>(p_object);
	return csg_shape;
}

void EditorPluginCSG2D::make_visible(bool p_visible) {
	csg_shape_editor->set_visible(p_visible);
}

EditorPluginCSG2D::EditorPluginCSG2D() {
	csg_shape_editor = memnew(CSGShape2DEditor);
	EditorNode::get_singleton()->get_gui_base()->add_child(csg_shape_editor);
	make_visible(false);
}

#endif // TOOLS_ENABLED

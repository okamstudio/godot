/**************************************************************************/
/*  sprite_2d_editor_plugin.cpp                                           */
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

#include "sprite_2d_editor_plugin.h"

#include "canvas_item_editor_plugin.h"
#include "core/math/geometry_2d.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/editor_zoom_widget.h"
#include "editor/scene_tree_dock.h"
#include "editor/themes/editor_scale.h"
#include "scene/2d/light_occluder_2d.h"
#include "scene/2d/mesh_instance_2d.h"
#include "scene/2d/physics/collision_polygon_2d.h"
#include "scene/2d/polygon_2d.h"
#include "scene/gui/box_container.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/panel.h"
#include "scene/gui/view_panner.h"
#include "thirdparty/clipper2/include/clipper2/clipper.h"

#define PRECISION 1

void Sprite2DEditor::_node_removed(Node *p_node) {
	if (p_node == node) {
		node = nullptr;
		options->hide();
	}
}

void Sprite2DEditor::edit(Sprite2D *p_sprite) {
	node = p_sprite;
}

Vector<Vector2> expand(const Vector<Vector2> &points, const Rect2i &rect, float epsilon = 2.0) {
	int size = points.size();
	ERR_FAIL_COND_V(size < 2, Vector<Vector2>());

	Clipper2Lib::PathD subj(points.size());
	for (int i = 0; i < points.size(); i++) {
		subj[i] = Clipper2Lib::PointD(points[i].x, points[i].y);
	}

	Clipper2Lib::PathsD solution = Clipper2Lib::InflatePaths({ subj }, epsilon, Clipper2Lib::JoinType::Miter, Clipper2Lib::EndType::Polygon, 2.0, PRECISION, 0.0);
	// Here the miter_limit = 2.0 and arc_tolerance = 0.0 are Clipper2 defaults,
	// and PRECISION is used to scale points up internally, to attain the desired precision.

	ERR_FAIL_COND_V(solution.size() == 0, points);

	// Clamp into the specified rect.
	Clipper2Lib::RectD clamp(rect.position.x,
			rect.position.y,
			rect.position.x + rect.size.width,
			rect.position.y + rect.size.height);
	Clipper2Lib::PathsD out = Clipper2Lib::RectClip(clamp, solution[0], PRECISION);
	// Here PRECISION is used to scale points up internally, to attain the desired precision.

	ERR_FAIL_COND_V(out.size() == 0, points);

	const Clipper2Lib::PathD &p2 = out[0];

	Vector<Vector2> outPoints;

	int lasti = p2.size() - 1;
	Vector2 prev = Vector2(p2[lasti].x, p2[lasti].y);
	for (uint64_t i = 0; i < p2.size(); i++) {
		Vector2 cur = Vector2(p2[i].x, p2[i].y);
		if (cur.distance_to(prev) > 0.5) {
			outPoints.push_back(cur);
			prev = cur;
		}
	}
	return outPoints;
}

void Sprite2DEditor::_menu_option(int p_option) {
	if (!node) {
		return;
	}

	selected_menu_item = (Menu)p_option;

	switch (p_option) {
		case MENU_OPTION_CONVERT_TO_MESH_2D: {
			debug_uv_dialog->set_ok_button_text(TTR("Create MeshInstance2D"));
			debug_uv_dialog->set_title(TTR("MeshInstance2D Preview"));

			_popup_debug_uv_dialog();
		} break;
		case MENU_OPTION_CONVERT_TO_POLYGON_2D: {
			debug_uv_dialog->set_ok_button_text(TTR("Create Polygon2D"));
			debug_uv_dialog->set_title(TTR("Polygon2D Preview"));

			_popup_debug_uv_dialog();
		} break;
		case MENU_OPTION_CREATE_COLLISION_POLY_2D: {
			debug_uv_dialog->set_ok_button_text(TTR("Create CollisionPolygon2D"));
			debug_uv_dialog->set_title(TTR("CollisionPolygon2D Preview"));

			_popup_debug_uv_dialog();
		} break;
		case MENU_OPTION_CREATE_LIGHT_OCCLUDER_2D: {
			debug_uv_dialog->set_ok_button_text(TTR("Create LightOccluder2D"));
			debug_uv_dialog->set_title(TTR("LightOccluder2D Preview"));

			_popup_debug_uv_dialog();
		} break;
	}
}

void Sprite2DEditor::_popup_debug_uv_dialog() {
	String error_message;
	if (node->get_owner() != get_tree()->get_edited_scene_root() && node != get_tree()->get_edited_scene_root()) {
		error_message = TTR("Can't convert a sprite from a foreign scene.");
	}
	Ref<Texture2D> texture = node->get_texture();
	if (texture.is_null()) {
		error_message = TTR("Can't convert an empty sprite to mesh.");
	}

	if (!error_message.is_empty()) {
		err_dialog->set_text(error_message);
		err_dialog->popup_centered();
		return;
	}

	_update_mesh_data();
	debug_uv_dialog->popup_centered();
	get_tree()->connect("process_frame", callable_mp(this, &Sprite2DEditor::_center_view), CONNECT_ONE_SHOT);
	debug_uv->set_texture_filter(node->get_texture_filter_in_tree());
	debug_uv->queue_redraw();
}

void Sprite2DEditor::_update_mesh_data() {
	ERR_FAIL_NULL(node);
	Ref<Texture2D> texture = node->get_texture();
	ERR_FAIL_COND(texture.is_null());
	Ref<Image> image = texture->get_image();
	ERR_FAIL_COND(image.is_null());

	if (image->is_compressed()) {
		image->decompress();
	}

	Rect2 rect = node->is_region_enabled() ? node->get_region_rect() : Rect2(Point2(), image->get_size());
	rect.size /= Vector2(node->get_hframes(), node->get_vframes());
	rect.position += node->get_frame_coords() * rect.size;

	Ref<BitMap> bm;
	bm.instantiate();
	bm->create_from_image_alpha(image);

	int shrink = shrink_pixels->get_value();
	if (shrink > 0) {
		bm->shrink_mask(shrink, rect);
	}

	int grow = grow_pixels->get_value();
	if (grow > 0) {
		bm->grow_mask(grow, rect);
	}

	float epsilon = simplification->get_value();

	Vector<Vector<Vector2>> lines = bm->clip_opaque_to_polygons(rect, epsilon);

	uv_lines.clear();

	computed_vertices.clear();
	computed_uv.clear();
	computed_indices.clear();

	Size2 img_size = image->get_size();
	for (int i = 0; i < lines.size(); i++) {
		lines.write[i] = expand(lines[i], rect, epsilon);
	}

	if (selected_menu_item == MENU_OPTION_CONVERT_TO_MESH_2D) {
		for (int j = 0; j < lines.size(); j++) {
			int index_ofs = computed_vertices.size();

			for (int i = 0; i < lines[j].size(); i++) {
				Vector2 vtx = lines[j][i];
				computed_uv.push_back((vtx + rect.position) / img_size);

				if (node->is_flipped_h()) {
					vtx.x = rect.size.x - vtx.x;
				}
				if (node->is_flipped_v()) {
					vtx.y = rect.size.y - vtx.y;
				}
				vtx += node->get_offset();
				if (node->is_centered()) {
					vtx -= rect.size / 2.0;
				}

				computed_vertices.push_back(vtx);
			}

			Vector<int> poly = Geometry2D::triangulate_polygon(lines[j]);

			for (int i = 0; i < poly.size(); i += 3) {
				for (int k = 0; k < 3; k++) {
					int idx = i + k;
					int idxn = i + (k + 1) % 3;
					uv_lines.push_back(lines[j][poly[idx]] + rect.position);
					uv_lines.push_back(lines[j][poly[idxn]] + rect.position);

					computed_indices.push_back(poly[idx] + index_ofs);
				}
			}
		}
	}

	outline_lines.clear();
	computed_outline_lines.clear();

	if (selected_menu_item == MENU_OPTION_CONVERT_TO_POLYGON_2D || selected_menu_item == MENU_OPTION_CREATE_COLLISION_POLY_2D || selected_menu_item == MENU_OPTION_CREATE_LIGHT_OCCLUDER_2D) {
		outline_lines.resize(lines.size());
		computed_outline_lines.resize(lines.size());
		for (int pi = 0; pi < lines.size(); pi++) {
			Vector<Vector2> ol;
			Vector<Vector2> col;

			ol.resize(lines[pi].size());
			col.resize(lines[pi].size());

			for (int i = 0; i < lines[pi].size(); i++) {
				Vector2 vtx = lines[pi][i];
				ol.write[i] = vtx + rect.position;

				if (node->is_flipped_h()) {
					vtx.x = rect.size.x - vtx.x;
				}
				if (node->is_flipped_v()) {
					vtx.y = rect.size.y - vtx.y;
				}
				// Don't bake offset to Polygon2D which has offset property.
				if (selected_menu_item != MENU_OPTION_CONVERT_TO_POLYGON_2D) {
					vtx += node->get_offset();
				}
				if (node->is_centered()) {
					vtx -= rect.size / 2.0;
				}

				col.write[i] = vtx;
			}

			outline_lines.write[pi] = ol;
			computed_outline_lines.write[pi] = col;
		}
	}

	debug_uv->queue_redraw();
}

void Sprite2DEditor::_create_node() {
	switch (selected_menu_item) {
		case MENU_OPTION_CONVERT_TO_MESH_2D: {
			_convert_to_mesh_2d_node();
		} break;
		case MENU_OPTION_CONVERT_TO_POLYGON_2D: {
			_convert_to_polygon_2d_node();
		} break;
		case MENU_OPTION_CREATE_COLLISION_POLY_2D: {
			_create_collision_polygon_2d_node();
		} break;
		case MENU_OPTION_CREATE_LIGHT_OCCLUDER_2D: {
			_create_light_occluder_2d_node();
		} break;
	}
}

void Sprite2DEditor::_convert_to_mesh_2d_node() {
	if (computed_vertices.size() < 3) {
		err_dialog->set_text(TTR("Invalid geometry, can't replace by mesh."));
		err_dialog->popup_centered();
		return;
	}

	Ref<ArrayMesh> mesh;
	mesh.instantiate();

	Array a;
	a.resize(Mesh::ARRAY_MAX);
	a[Mesh::ARRAY_VERTEX] = computed_vertices;
	a[Mesh::ARRAY_TEX_UV] = computed_uv;
	a[Mesh::ARRAY_INDEX] = computed_indices;

	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, a, Array(), Dictionary(), Mesh::ARRAY_FLAG_USE_2D_VERTICES);

	MeshInstance2D *mesh_instance = memnew(MeshInstance2D);
	mesh_instance->set_mesh(mesh);

	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
	ur->create_action(TTR("Convert to MeshInstance2D"), UndoRedo::MERGE_DISABLE, node);
	SceneTreeDock::get_singleton()->replace_node(node, mesh_instance);
	ur->commit_action(false);
}

void Sprite2DEditor::_convert_to_polygon_2d_node() {
	if (computed_outline_lines.is_empty()) {
		err_dialog->set_text(TTR("Invalid geometry, can't create polygon."));
		err_dialog->popup_centered();
		return;
	}

	Polygon2D *polygon_2d_instance = memnew(Polygon2D);

	int total_point_count = 0;
	for (int i = 0; i < computed_outline_lines.size(); i++) {
		total_point_count += computed_outline_lines[i].size();
	}

	PackedVector2Array polygon;
	polygon.resize(total_point_count);
	Vector2 *polygon_write = polygon.ptrw();

	PackedVector2Array uvs;
	uvs.resize(total_point_count);
	Vector2 *uvs_write = uvs.ptrw();

	int current_point_index = 0;

	Array polys;
	polys.resize(computed_outline_lines.size());

	for (int i = 0; i < computed_outline_lines.size(); i++) {
		Vector<Vector2> outline = computed_outline_lines[i];
		Vector<Vector2> uv_outline = outline_lines[i];

		PackedInt32Array pia;
		pia.resize(outline.size());
		int *pia_write = pia.ptrw();

		for (int pi = 0; pi < outline.size(); pi++) {
			polygon_write[current_point_index] = outline[pi];
			uvs_write[current_point_index] = uv_outline[pi];
			pia_write[pi] = current_point_index;
			current_point_index++;
		}

		polys[i] = pia;
	}

	polygon_2d_instance->set_uv(uvs);
	polygon_2d_instance->set_polygon(polygon);
	polygon_2d_instance->set_polygons(polys);

	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
	ur->create_action(TTR("Convert to Polygon2D"), UndoRedo::MERGE_DISABLE, node);
	SceneTreeDock::get_singleton()->replace_node(node, polygon_2d_instance);
	ur->commit_action(false);
}

void Sprite2DEditor::_create_collision_polygon_2d_node() {
	if (computed_outline_lines.is_empty()) {
		err_dialog->set_text(TTR("Invalid geometry, can't create collision polygon."));
		err_dialog->popup_centered();
		return;
	}

	for (int i = 0; i < computed_outline_lines.size(); i++) {
		Vector<Vector2> outline = computed_outline_lines[i];

		CollisionPolygon2D *collision_polygon_2d_instance = memnew(CollisionPolygon2D);
		collision_polygon_2d_instance->set_polygon(outline);

		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action(TTR("Create CollisionPolygon2D Sibling"), UndoRedo::MERGE_DISABLE, node);
		ur->add_do_method(this, "_add_as_sibling_or_child", node, collision_polygon_2d_instance);
		ur->add_do_reference(collision_polygon_2d_instance);
		ur->add_undo_method(node != get_tree()->get_edited_scene_root() ? node->get_parent() : get_tree()->get_edited_scene_root(), "remove_child", collision_polygon_2d_instance);
		ur->commit_action();
	}
}

void Sprite2DEditor::_create_light_occluder_2d_node() {
	if (computed_outline_lines.is_empty()) {
		err_dialog->set_text(TTR("Invalid geometry, can't create light occluder."));
		err_dialog->popup_centered();
		return;
	}

	for (int i = 0; i < computed_outline_lines.size(); i++) {
		Vector<Vector2> outline = computed_outline_lines[i];

		Ref<OccluderPolygon2D> polygon;
		polygon.instantiate();

		PackedVector2Array a;
		a.resize(outline.size());
		Vector2 *aw = a.ptrw();
		for (int io = 0; io < outline.size(); io++) {
			aw[io] = outline[io];
		}
		polygon->set_polygon(a);

		LightOccluder2D *light_occluder_2d_instance = memnew(LightOccluder2D);
		light_occluder_2d_instance->set_occluder_polygon(polygon);

		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action(TTR("Create LightOccluder2D Sibling"), UndoRedo::MERGE_DISABLE, node);
		ur->add_do_method(this, "_add_as_sibling_or_child", node, light_occluder_2d_instance);
		ur->add_do_reference(light_occluder_2d_instance);
		ur->add_undo_method(node != get_tree()->get_edited_scene_root() ? node->get_parent() : get_tree()->get_edited_scene_root(), "remove_child", light_occluder_2d_instance);
		ur->commit_action();
	}
}

void Sprite2DEditor::_add_as_sibling_or_child(Node *p_own_node, Node *p_new_node) {
	// Can't make sibling if own node is scene root
	if (p_own_node != get_tree()->get_edited_scene_root()) {
		p_own_node->get_parent()->add_child(p_new_node, true);
		Object::cast_to<Node2D>(p_new_node)->set_transform(Object::cast_to<Node2D>(p_own_node)->get_transform());
	} else {
		p_own_node->add_child(p_new_node, true);
	}

	p_new_node->set_owner(get_tree()->get_edited_scene_root());
}

void Sprite2DEditor::_debug_uv_input(const Ref<InputEvent> &p_input) {
	if (panner->gui_input(p_input, debug_uv->get_global_rect())) {
		accept_event();
	}
}

void Sprite2DEditor::_debug_uv_draw() {
	debug_uv->draw_set_transform(-draw_offset * draw_zoom, 0, Vector2(draw_zoom, draw_zoom));

	Ref<Texture2D> tex = node->get_texture();
	ERR_FAIL_COND(tex.is_null());

	debug_uv->draw_texture(tex, Point2());

	Color color = Color(1.0, 0.8, 0.7);

	if (selected_menu_item == MENU_OPTION_CONVERT_TO_MESH_2D && uv_lines.size() > 0) {
		debug_uv->draw_multiline(uv_lines, color);

	} else if ((selected_menu_item == MENU_OPTION_CONVERT_TO_POLYGON_2D || selected_menu_item == MENU_OPTION_CREATE_COLLISION_POLY_2D || selected_menu_item == MENU_OPTION_CREATE_LIGHT_OCCLUDER_2D) && outline_lines.size() > 0) {
		for (int i = 0; i < outline_lines.size(); i++) {
			Vector<Vector2> outline = outline_lines[i];

			debug_uv->draw_polyline(outline, color);
			debug_uv->draw_line(outline[0], outline[outline.size() - 1], color);
		}
	}
}

void Sprite2DEditor::_center_view() {
	Ref<Texture2D> tex = node->get_texture();
	ERR_FAIL_COND(tex.is_null());
	Vector2 zoom_factor = (debug_uv->get_size() - Vector2(1, 1) * 50 * EDSCALE) / tex->get_size();
	zoom_widget->set_zoom(MIN(zoom_factor.x, zoom_factor.y));
	// Recalculate scroll limits.
	_update_zoom_and_pan(false);

	Vector2 offset = (tex->get_size() - debug_uv->get_size() / zoom_widget->get_zoom()) / 2;
	h_scroll->set_value_no_signal(offset.x);
	v_scroll->set_value_no_signal(offset.y);
	_update_zoom_and_pan(false);
}

void Sprite2DEditor::_pan_callback(Vector2 p_scroll_vec, Ref<InputEvent> p_event) {
	h_scroll->set_value_no_signal(h_scroll->get_value() - p_scroll_vec.x / draw_zoom);
	v_scroll->set_value_no_signal(v_scroll->get_value() - p_scroll_vec.y / draw_zoom);
	_update_zoom_and_pan(false);
}

void Sprite2DEditor::_zoom_callback(float p_zoom_factor, Vector2 p_origin, Ref<InputEvent> p_event) {
	const real_t prev_zoom = draw_zoom;
	zoom_widget->set_zoom(draw_zoom * p_zoom_factor);
	draw_offset += p_origin / prev_zoom - p_origin / zoom_widget->get_zoom();
	h_scroll->set_value_no_signal(draw_offset.x);
	v_scroll->set_value_no_signal(draw_offset.y);
	_update_zoom_and_pan(false);
}

void Sprite2DEditor::_update_zoom_and_pan(bool p_zoom_at_center) {
	real_t previous_zoom = draw_zoom;
	draw_zoom = zoom_widget->get_zoom();
	draw_offset = Vector2(h_scroll->get_value(), v_scroll->get_value());
	if (p_zoom_at_center) {
		Vector2 center = debug_uv->get_size() / 2;
		draw_offset += center / previous_zoom - center / draw_zoom;
	}

	Ref<Texture2D> tex = node->get_texture();
	ERR_FAIL_COND(tex.is_null());

	Point2 min_corner;
	Point2 max_corner = tex->get_size();
	Size2 page_size = debug_uv->get_size() / draw_zoom;
	Vector2 margin = Vector2(50, 50) * EDSCALE / draw_zoom;
	min_corner -= page_size - margin;
	max_corner += page_size - margin;

	h_scroll->set_block_signals(true);
	h_scroll->set_min(min_corner.x);
	h_scroll->set_max(max_corner.x);
	h_scroll->set_page(page_size.x);
	h_scroll->set_value(draw_offset.x);
	h_scroll->set_block_signals(false);

	v_scroll->set_block_signals(true);
	v_scroll->set_min(min_corner.y);
	v_scroll->set_max(max_corner.y);
	v_scroll->set_page(page_size.y);
	v_scroll->set_value(draw_offset.y);
	v_scroll->set_block_signals(false);

	debug_uv->queue_redraw();
}

void Sprite2DEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			v_scroll->set_anchors_and_offsets_preset(Control::PRESET_RIGHT_WIDE);
			h_scroll->set_anchors_and_offsets_preset(Control::PRESET_BOTTOM_WIDE);
			// Avoid scrollbar overlapping.
			Size2 hmin = h_scroll->get_combined_minimum_size();
			Size2 vmin = v_scroll->get_combined_minimum_size();
			h_scroll->set_anchor_and_offset(SIDE_RIGHT, ANCHOR_END, -vmin.width);
			v_scroll->set_anchor_and_offset(SIDE_BOTTOM, ANCHOR_END, -hmin.height);
			[[fallthrough]];
		}
		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (!EditorSettings::get_singleton()->check_changed_settings_in_group("editors/panning")) {
				break;
			}
			[[fallthrough]];
		}
		case NOTIFICATION_ENTER_TREE: {
			panner->setup((ViewPanner::ControlScheme)EDITOR_GET("editors/panning/sub_editors_panning_scheme").operator int(), ED_GET_SHORTCUT("canvas_item_editor/pan_view"), bool(EDITOR_GET("editors/panning/simple_panning")));
			panner->setup_warped_panning(debug_uv_dialog, EDITOR_GET("editors/panning/warped_mouse_panning"));
		} break;
		case NOTIFICATION_THEME_CHANGED: {
			options->set_button_icon(get_editor_theme_icon(SNAME("Sprite2D")));

			options->get_popup()->set_item_icon(MENU_OPTION_CONVERT_TO_MESH_2D, get_editor_theme_icon(SNAME("MeshInstance2D")));
			options->get_popup()->set_item_icon(MENU_OPTION_CONVERT_TO_POLYGON_2D, get_editor_theme_icon(SNAME("Polygon2D")));
			options->get_popup()->set_item_icon(MENU_OPTION_CREATE_COLLISION_POLY_2D, get_editor_theme_icon(SNAME("CollisionPolygon2D")));
			options->get_popup()->set_item_icon(MENU_OPTION_CREATE_LIGHT_OCCLUDER_2D, get_editor_theme_icon(SNAME("LightOccluder2D")));
		} break;
	}
}

void Sprite2DEditor::_bind_methods() {
	ClassDB::bind_method("_add_as_sibling_or_child", &Sprite2DEditor::_add_as_sibling_or_child);
}

Sprite2DEditor::Sprite2DEditor() {
	options = memnew(MenuButton);

	CanvasItemEditor::get_singleton()->add_control_to_menu_panel(options);

	options->set_text(TTR("Sprite2D"));

	options->get_popup()->add_item(TTR("Convert to MeshInstance2D"), MENU_OPTION_CONVERT_TO_MESH_2D);
	options->get_popup()->add_item(TTR("Convert to Polygon2D"), MENU_OPTION_CONVERT_TO_POLYGON_2D);
	options->get_popup()->add_item(TTR("Create CollisionPolygon2D Sibling"), MENU_OPTION_CREATE_COLLISION_POLY_2D);
	options->get_popup()->add_item(TTR("Create LightOccluder2D Sibling"), MENU_OPTION_CREATE_LIGHT_OCCLUDER_2D);
	options->set_switch_on_hover(true);

	options->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &Sprite2DEditor::_menu_option));

	err_dialog = memnew(AcceptDialog);
	add_child(err_dialog);

	debug_uv_dialog = memnew(ConfirmationDialog);
	debug_uv_dialog->set_size(Size2(960, 540) * EDSCALE);
	VBoxContainer *vb = memnew(VBoxContainer);
	debug_uv_dialog->add_child(vb);
	debug_uv = memnew(Panel);
	debug_uv->connect(SceneStringName(gui_input), callable_mp(this, &Sprite2DEditor::_debug_uv_input));
	debug_uv->connect(SceneStringName(draw), callable_mp(this, &Sprite2DEditor::_debug_uv_draw));
	debug_uv->set_clip_contents(true);
	vb->add_margin_child(TTR("Preview:"), debug_uv, true);

	panner.instantiate();
	panner->set_callbacks(callable_mp(this, &Sprite2DEditor::_pan_callback), callable_mp(this, &Sprite2DEditor::_zoom_callback));

	zoom_widget = memnew(EditorZoomWidget);
	debug_uv->add_child(zoom_widget);
	zoom_widget->set_anchors_and_offsets_preset(Control::PRESET_TOP_LEFT, Control::PRESET_MODE_MINSIZE, 2 * EDSCALE);
	zoom_widget->connect("zoom_changed", callable_mp(this, &Sprite2DEditor::_update_zoom_and_pan).unbind(1).bind(true));
	zoom_widget->set_shortcut_context(nullptr);

	v_scroll = memnew(VScrollBar);
	debug_uv->add_child(v_scroll);
	v_scroll->connect(SceneStringName(value_changed), callable_mp(this, &Sprite2DEditor::_update_zoom_and_pan).unbind(1).bind(false));
	h_scroll = memnew(HScrollBar);
	debug_uv->add_child(h_scroll);
	h_scroll->connect(SceneStringName(value_changed), callable_mp(this, &Sprite2DEditor::_update_zoom_and_pan).unbind(1).bind(false));

	debug_uv_dialog->connect(SceneStringName(confirmed), callable_mp(this, &Sprite2DEditor::_create_node));

	HBoxContainer *hb = memnew(HBoxContainer);
	hb->add_child(memnew(Label(TTR("Simplification:"))));
	simplification = memnew(SpinBox);
	simplification->set_min(0.01);
	simplification->set_max(10.00);
	simplification->set_step(0.01);
	simplification->set_value(2);
	hb->add_child(simplification);
	hb->add_spacer();
	hb->add_child(memnew(Label(TTR("Shrink (Pixels):"))));
	shrink_pixels = memnew(SpinBox);
	shrink_pixels->set_min(0);
	shrink_pixels->set_max(10);
	shrink_pixels->set_step(1);
	shrink_pixels->set_value(0);
	hb->add_child(shrink_pixels);
	hb->add_spacer();
	hb->add_child(memnew(Label(TTR("Grow (Pixels):"))));
	grow_pixels = memnew(SpinBox);
	grow_pixels->set_min(0);
	grow_pixels->set_max(10);
	grow_pixels->set_step(1);
	grow_pixels->set_value(2);
	hb->add_child(grow_pixels);
	hb->add_spacer();
	update_preview = memnew(Button);
	update_preview->set_text(TTR("Update Preview"));
	update_preview->connect(SceneStringName(pressed), callable_mp(this, &Sprite2DEditor::_update_mesh_data));
	hb->add_child(update_preview);
	vb->add_margin_child(TTR("Settings:"), hb);

	add_child(debug_uv_dialog);
}

void Sprite2DEditorPlugin::edit(Object *p_object) {
	sprite_editor->edit(Object::cast_to<Sprite2D>(p_object));
}

bool Sprite2DEditorPlugin::handles(Object *p_object) const {
	return p_object->is_class("Sprite2D");
}

void Sprite2DEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		sprite_editor->options->show();
	} else {
		sprite_editor->options->hide();
		sprite_editor->edit(nullptr);
	}
}

Sprite2DEditorPlugin::Sprite2DEditorPlugin() {
	sprite_editor = memnew(Sprite2DEditor);
	EditorNode::get_singleton()->get_gui_base()->add_child(sprite_editor);
	make_visible(false);

	//sprite_editor->options->hide();
}

Sprite2DEditorPlugin::~Sprite2DEditorPlugin() {
}

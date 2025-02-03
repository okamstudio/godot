/**************************************************************************/
/*  path_editor_plugin.cpp                                                */
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

#include "path_editor_plugin.h"

#include "core/os/keyboard.h"
#include "scene/resources/curve.h"
#include "spatial_editor_plugin.h"

static bool _is_in_handle(int p_id, int p_num_points) {
	int t = (p_id + 1) % 2;
	int idx = (p_id + 1) / 2;
	// order of points is [out_0, out_1, in_1, out_2, in_2, ... out_n-1, in_n-1, in_n]
	if (idx == 0) {
		return false;
	} else if (idx == (p_num_points - 1)) {
		return true;
	} else {
		return (t == 1);
	}
}

String PathSpatialGizmo::get_handle_name(int p_idx) const {
	Ref<Curve3D> c = path->get_curve();
	if (c.is_null()) {
		return "";
	}

	if (p_idx < c->get_point_count()) {
		return TTR("Curve Point #") + itos(p_idx);
	}

	p_idx = p_idx - c->get_point_count();
	int idx = (p_idx + 1) / 2;
	String n = TTR("Curve Point #") + itos(idx);
	if (_is_in_handle(p_idx, c->get_point_count())) {
		n += " In";
	} else {
		n += " Out";
	}

	return n;
}
Variant PathSpatialGizmo::get_handle_value(int p_idx) {
	Ref<Curve3D> c = path->get_curve();
	if (c.is_null()) {
		return Variant();
	}

	if (p_idx < c->get_point_count()) {
		original = c->get_point_position(p_idx);
		return original;
	}

	p_idx = p_idx - c->get_point_count();
	int idx = (p_idx + 1) / 2;

	Vector3 ofs;
	if (_is_in_handle(p_idx, c->get_point_count())) {
		ofs = c->get_point_in(idx);
	} else {
		ofs = c->get_point_out(idx);
	}

	original = ofs + c->get_point_position(idx);

	return ofs;
}
void PathSpatialGizmo::set_handle(int p_idx, Camera *p_camera, const Point2 &p_point) {
	Ref<Curve3D> c = path->get_curve();
	if (c.is_null()) {
		return;
	}

	Transform gt = path->get_global_transform();
	Transform gi = gt.affine_inverse();
	Vector3 ray_from = p_camera->project_ray_origin(p_point);
	Vector3 ray_dir = p_camera->project_ray_normal(p_point);

	// Setting curve point positions
	if (p_idx < c->get_point_count()) {
		Plane p(gt.xform(original), p_camera->get_transform().basis.get_axis(2));

		Vector3 inters;

		if (p.intersects_ray(ray_from, ray_dir, &inters)) {
			if (SpatialEditor::get_singleton()->is_snap_enabled()) {
				float snap = SpatialEditor::get_singleton()->get_translate_snap();
				inters.snap(Vector3(snap, snap, snap));
			}

			Vector3 local = gi.xform(inters);
			c->set_point_position(p_idx, local);
		}

		return;
	}

	p_idx = p_idx - c->get_point_count();
	int idx = (p_idx + 1) / 2;

	Vector3 base = c->get_point_position(idx);

	Plane p(gt.xform(original), p_camera->get_transform().basis.get_axis(2));

	Vector3 inters;

	// Setting curve in/out positions
	if (p.intersects_ray(ray_from, ray_dir, &inters)) {
		if (!PathEditorPlugin::singleton->is_handle_clicked()) {
			orig_in_length = c->get_point_in(idx).length();
			orig_out_length = c->get_point_out(idx).length();
			PathEditorPlugin::singleton->set_handle_clicked(true);
		}

		Vector3 local = gi.xform(inters) - base;
		if (SpatialEditor::get_singleton()->is_snap_enabled()) {
			float snap = SpatialEditor::get_singleton()->get_translate_snap();
			local.snap(Vector3(snap, snap, snap));
		}

		if (_is_in_handle(p_idx, c->get_point_count())) {
			c->set_point_in(idx, local);
			if (PathEditorPlugin::singleton->mirror_angle_enabled()) {
				c->set_point_out(idx, PathEditorPlugin::singleton->mirror_length_enabled() ? -local : (-local.normalized() * orig_out_length));
			}
		} else {
			c->set_point_out(idx, local);
			if (PathEditorPlugin::singleton->mirror_angle_enabled()) {
				c->set_point_in(idx, PathEditorPlugin::singleton->mirror_length_enabled() ? -local : (-local.normalized() * orig_in_length));
			}
		}
	}
}

void PathSpatialGizmo::commit_handle(int p_idx, const Variant &p_restore, bool p_cancel) {
	Ref<Curve3D> c = path->get_curve();
	if (c.is_null()) {
		return;
	}

	UndoRedo *ur = SpatialEditor::get_singleton()->get_undo_redo();

	if (p_idx < c->get_point_count()) {
		if (p_cancel) {
			c->set_point_position(p_idx, p_restore);
			return;
		}
		ur->create_action(TTR("Set Curve Point Position"));
		ur->add_do_method(c.ptr(), "set_point_position", p_idx, c->get_point_position(p_idx));
		ur->add_undo_method(c.ptr(), "set_point_position", p_idx, p_restore);
		ur->commit_action();

		return;
	}

	p_idx = p_idx - c->get_point_count();
	int idx = (p_idx + 1) / 2;

	if (_is_in_handle(p_idx, c->get_point_count())) {
		if (p_cancel) {
			c->set_point_in(p_idx, p_restore);
			return;
		}

		ur->create_action(TTR("Set Curve In Position"));
		ur->add_do_method(c.ptr(), "set_point_in", idx, c->get_point_in(idx));
		ur->add_undo_method(c.ptr(), "set_point_in", idx, p_restore);

		if (PathEditorPlugin::singleton->mirror_angle_enabled()) {
			ur->add_do_method(c.ptr(), "set_point_out", idx, PathEditorPlugin::singleton->mirror_length_enabled() ? -c->get_point_in(idx) : (-c->get_point_in(idx).normalized() * orig_out_length));
			ur->add_undo_method(c.ptr(), "set_point_out", idx, PathEditorPlugin::singleton->mirror_length_enabled() ? -static_cast<Vector3>(p_restore) : (-static_cast<Vector3>(p_restore).normalized() * orig_out_length));
		}
		ur->commit_action();

	} else {
		if (p_cancel) {
			c->set_point_out(idx, p_restore);

			return;
		}

		ur->create_action(TTR("Set Curve Out Position"));
		ur->add_do_method(c.ptr(), "set_point_out", idx, c->get_point_out(idx));
		ur->add_undo_method(c.ptr(), "set_point_out", idx, p_restore);

		if (PathEditorPlugin::singleton->mirror_angle_enabled()) {
			ur->add_do_method(c.ptr(), "set_point_in", idx, PathEditorPlugin::singleton->mirror_length_enabled() ? -c->get_point_out(idx) : (-c->get_point_out(idx).normalized() * orig_in_length));
			ur->add_undo_method(c.ptr(), "set_point_in", idx, PathEditorPlugin::singleton->mirror_length_enabled() ? -static_cast<Vector3>(p_restore) : (-static_cast<Vector3>(p_restore).normalized() * orig_in_length));
		}
		ur->commit_action();
	}
}

void PathSpatialGizmo::redraw() {
	clear();

	Ref<Material3D> path_material = gizmo_plugin->get_material("path_material", this);
	Ref<Material3D> path_thin_material = gizmo_plugin->get_material("path_thin_material", this);
	Ref<Material3D> handles_material = gizmo_plugin->get_material("handles");

	Ref<Curve3D> c = path->get_curve();
	if (c.is_null()) {
		return;
	}

	PoolVector<Vector3> v3a = c->tessellate();
	//PoolVector<Vector3> v3a=c->get_baked_points();

	int v3s = v3a.size();
	if (v3s == 0) {
		return;
	}
	Vector<Vector3> v3p;
	PoolVector<Vector3>::Read r = v3a.read();

	// BUG: the following won't work when v3s, avoid drawing as a temporary workaround.
	for (int i = 0; i < v3s - 1; i++) {
		v3p.push_back(r[i]);
		v3p.push_back(r[i + 1]);
		//v3p.push_back(r[i]);
		//v3p.push_back(r[i]+Vector3(0,0.2,0));
	}

	if (v3p.size() > 1) {
		add_lines(v3p, path_material);
		add_collision_segments(v3p);
	}

	if (PathEditorPlugin::singleton->get_edited_path() == path) {
		v3p.clear();
		Vector<Vector3> handles;
		Vector<Vector3> sec_handles;

		for (int i = 0; i < c->get_point_count(); i++) {
			Vector3 p = c->get_point_position(i);
			handles.push_back(p);
			// push Out points first so they get selected if the In and Out points are on top of each other.
			if (i < c->get_point_count() - 1) {
				v3p.push_back(p);
				v3p.push_back(p + c->get_point_out(i));
				sec_handles.push_back(p + c->get_point_out(i));
			}
			if (i > 0) {
				v3p.push_back(p);
				v3p.push_back(p + c->get_point_in(i));
				sec_handles.push_back(p + c->get_point_in(i));
			}
		}

		if (v3p.size() > 1) {
			add_lines(v3p, path_thin_material);
		}
		if (handles.size()) {
			add_handles(handles, handles_material);
		}
		if (sec_handles.size()) {
			add_handles(sec_handles, handles_material, false, true);
		}
	}
}

PathSpatialGizmo::PathSpatialGizmo(Path *p_path) {
	path = p_path;
	set_spatial_node(p_path);
}

bool PathEditorPlugin::forward_spatial_gui_input(Camera *p_camera, const Ref<InputEvent> &p_event) {
	if (!path) {
		return false;
	}
	Ref<Curve3D> c = path->get_curve();
	if (c.is_null()) {
		return false;
	}
	Transform gt = path->get_global_transform();
	Transform it = gt.affine_inverse();

	static const int click_dist = 10; //should make global

	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid()) {
		Point2 mbpos(mb->get_position().x, mb->get_position().y);

		if (!mb->is_pressed()) {
			set_handle_clicked(false);
		}

		if (mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT && (curve_create->is_pressed() || (curve_edit->is_pressed() && mb->get_control()))) {
			//click into curve, break it down
			PoolVector<Vector3> v3a = c->tessellate();
			int idx = 0;
			int rc = v3a.size();
			int closest_seg = -1;
			Vector3 closest_seg_point;
			float closest_d = 1e20;

			if (rc >= 2) {
				PoolVector<Vector3>::Read r = v3a.read();

				if (p_camera->unproject_position(gt.xform(c->get_point_position(0))).distance_to(mbpos) < click_dist) {
					return false; //nope, existing
				}

				for (int i = 0; i < c->get_point_count() - 1; i++) {
					//find the offset and point index of the place to break up
					int j = idx;
					if (p_camera->unproject_position(gt.xform(c->get_point_position(i + 1))).distance_to(mbpos) < click_dist) {
						return false; //nope, existing
					}

					while (j < rc && c->get_point_position(i + 1) != r[j]) {
						Vector3 from = r[j];
						Vector3 to = r[j + 1];
						real_t cdist = from.distance_to(to);
						from = gt.xform(from);
						to = gt.xform(to);
						if (cdist > 0) {
							Vector2 s[2];
							s[0] = p_camera->unproject_position(from);
							s[1] = p_camera->unproject_position(to);
							Vector2 inters = Geometry::get_closest_point_to_segment_2d(mbpos, s);
							float d = inters.distance_to(mbpos);

							if (d < 10 && d < closest_d) {
								closest_d = d;
								closest_seg = i;
								Vector3 ray_from = p_camera->project_ray_origin(mbpos);
								Vector3 ray_dir = p_camera->project_ray_normal(mbpos);

								Vector3 ra, rb;
								Geometry::get_closest_points_between_segments(ray_from, ray_from + ray_dir * 4096, from, to, ra, rb);

								closest_seg_point = it.xform(rb);
							}
						}
						j++;
					}
					if (idx == j) {
						idx++; //force next
					} else {
						idx = j; //swap
					}

					if (j == rc) {
						break;
					}
				}
			}

			UndoRedo *ur = editor->get_undo_redo();
			if (closest_seg != -1) {
				//subdivide

				ur->create_action(TTR("Split Path"));
				ur->add_do_method(c.ptr(), "add_point", closest_seg_point, Vector3(), Vector3(), closest_seg + 1);
				ur->add_undo_method(c.ptr(), "remove_point", closest_seg + 1);
				ur->commit_action();
				return true;

			} else {
				Vector3 org;
				if (c->get_point_count() == 0) {
					org = path->get_transform().get_origin();
				} else {
					org = gt.xform(c->get_point_position(c->get_point_count() - 1));
				}
				Plane p(org, p_camera->get_transform().basis.get_axis(2));
				Vector3 ray_from = p_camera->project_ray_origin(mbpos);
				Vector3 ray_dir = p_camera->project_ray_normal(mbpos);

				Vector3 inters;
				if (p.intersects_ray(ray_from, ray_dir, &inters)) {
					ur->create_action(TTR("Add Point to Curve"));
					ur->add_do_method(c.ptr(), "add_point", it.xform(inters), Vector3(), Vector3(), -1);
					ur->add_undo_method(c.ptr(), "remove_point", c->get_point_count());
					ur->commit_action();
					return true;
				}

				//add new at pos
			}

		} else if (mb->is_pressed() && ((mb->get_button_index() == BUTTON_LEFT && curve_del->is_pressed()) || (mb->get_button_index() == BUTTON_RIGHT && curve_edit->is_pressed()))) {
			for (int i = 0; i < c->get_point_count(); i++) {
				real_t dist_to_p = p_camera->unproject_position(gt.xform(c->get_point_position(i))).distance_to(mbpos);
				real_t dist_to_p_out = p_camera->unproject_position(gt.xform(c->get_point_position(i) + c->get_point_out(i))).distance_to(mbpos);
				real_t dist_to_p_in = p_camera->unproject_position(gt.xform(c->get_point_position(i) + c->get_point_in(i))).distance_to(mbpos);

				// Find the offset and point index of the place to break up.
				// Also check for the control points.
				if (dist_to_p < click_dist) {
					UndoRedo *ur = editor->get_undo_redo();
					ur->create_action(TTR("Remove Path Point"));
					ur->add_do_method(c.ptr(), "remove_point", i);
					ur->add_undo_method(c.ptr(), "add_point", c->get_point_position(i), c->get_point_in(i), c->get_point_out(i), i);
					ur->commit_action();
					return true;
				} else if (dist_to_p_out < click_dist) {
					UndoRedo *ur = editor->get_undo_redo();
					ur->create_action(TTR("Remove Out-Control Point"));
					ur->add_do_method(c.ptr(), "set_point_out", i, Vector3());
					ur->add_undo_method(c.ptr(), "set_point_out", i, c->get_point_out(i));
					ur->commit_action();
					return true;
				} else if (dist_to_p_in < click_dist) {
					UndoRedo *ur = editor->get_undo_redo();
					ur->create_action(TTR("Remove In-Control Point"));
					ur->add_do_method(c.ptr(), "set_point_in", i, Vector3());
					ur->add_undo_method(c.ptr(), "set_point_in", i, c->get_point_in(i));
					ur->commit_action();
					return true;
				}
			}
		}
	}

	return false;
}

void PathEditorPlugin::edit(Object *p_object) {
	if (p_object) {
		path = Object::cast_to<Path>(p_object);
		if (path) {
			if (path->get_curve().is_valid()) {
				path->get_curve()->emit_signal("changed");
			}
		}
	} else {
		Path *pre = path;
		path = nullptr;
		if (pre) {
			pre->get_curve()->emit_signal("changed");
		}
	}
	//collision_polygon_editor->edit(Object::cast_to<Node>(p_object));
}

bool PathEditorPlugin::handles(Object *p_object) const {
	return p_object->is_class("Path");
}

void PathEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		curve_create->show();
		curve_edit->show();
		curve_del->show();
		curve_close->show();
		handle_menu->show();
		sep->show();
	} else {
		curve_create->hide();
		curve_edit->hide();
		curve_del->hide();
		curve_close->hide();
		handle_menu->hide();
		sep->hide();

		{
			Path *pre = path;
			path = nullptr;
			if (pre && pre->get_curve().is_valid()) {
				pre->get_curve()->emit_signal("changed");
			}
		}
	}
}

void PathEditorPlugin::_mode_changed(int p_idx) {
	curve_create->set_pressed(p_idx == 0);
	curve_edit->set_pressed(p_idx == 1);
	curve_del->set_pressed(p_idx == 2);
}

void PathEditorPlugin::_close_curve() {
	Ref<Curve3D> c = path->get_curve();
	if (c.is_null()) {
		return;
	}
	if (c->get_point_count() < 2) {
		return;
	}
	if (c->get_point_position(0) == c->get_point_position(c->get_point_count() - 1)) {
		return;
	}
	UndoRedo *ur = editor->get_undo_redo();
	ur->create_action(TTR("Close Curve"));
	ur->add_do_method(c.ptr(), "add_point", c->get_point_position(0), c->get_point_in(0), c->get_point_out(0), -1);
	ur->add_undo_method(c.ptr(), "remove_point", c->get_point_count());
	ur->commit_action();
}

void PathEditorPlugin::_handle_option_pressed(int p_option) {
	PopupMenu *pm;
	pm = handle_menu->get_popup();

	switch (p_option) {
		case HANDLE_OPTION_ANGLE: {
			bool is_checked = pm->is_item_checked(HANDLE_OPTION_ANGLE);
			mirror_handle_angle = !is_checked;
			pm->set_item_checked(HANDLE_OPTION_ANGLE, mirror_handle_angle);
			pm->set_item_disabled(HANDLE_OPTION_LENGTH, !mirror_handle_angle);
		} break;
		case HANDLE_OPTION_LENGTH: {
			bool is_checked = pm->is_item_checked(HANDLE_OPTION_LENGTH);
			mirror_handle_length = !is_checked;
			pm->set_item_checked(HANDLE_OPTION_LENGTH, mirror_handle_length);
		} break;
	}
}

void PathEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		curve_create->connect("pressed", this, "_mode_changed", make_binds(0));
		curve_edit->connect("pressed", this, "_mode_changed", make_binds(1));
		curve_del->connect("pressed", this, "_mode_changed", make_binds(2));
		curve_close->connect("pressed", this, "_close_curve");
	}
}

void PathEditorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_mode_changed"), &PathEditorPlugin::_mode_changed);
	ClassDB::bind_method(D_METHOD("_close_curve"), &PathEditorPlugin::_close_curve);
	ClassDB::bind_method(D_METHOD("_handle_option_pressed"), &PathEditorPlugin::_handle_option_pressed);
}

PathEditorPlugin *PathEditorPlugin::singleton = nullptr;

PathEditorPlugin::PathEditorPlugin(EditorNode *p_node) {
	path = nullptr;
	editor = p_node;
	singleton = this;
	mirror_handle_angle = true;
	mirror_handle_length = true;

	Ref<PathSpatialGizmoPlugin> gizmo_plugin;
	gizmo_plugin.instance();
	SpatialEditor::get_singleton()->add_gizmo_plugin(gizmo_plugin);

	sep = memnew(VSeparator);
	sep->hide();
	SpatialEditor::get_singleton()->add_control_to_menu_panel(sep);
	curve_edit = memnew(ToolButton);
	curve_edit->set_icon(EditorNode::get_singleton()->get_gui_base()->get_icon("CurveEdit", "EditorIcons"));
	curve_edit->set_toggle_mode(true);
	curve_edit->hide();
	curve_edit->set_focus_mode(Control::FOCUS_NONE);
	curve_edit->set_tooltip(TTR("Select Points") + "\n" + TTR("Shift+Drag: Select Control Points") + "\n" + keycode_get_string(KEY_MASK_CMD) + TTR("Click: Add Point") + "\n" + TTR("Right Click: Delete Point"));
	SpatialEditor::get_singleton()->add_control_to_menu_panel(curve_edit);
	curve_create = memnew(ToolButton);
	curve_create->set_icon(EditorNode::get_singleton()->get_gui_base()->get_icon("CurveCreate", "EditorIcons"));
	curve_create->set_toggle_mode(true);
	curve_create->hide();
	curve_create->set_focus_mode(Control::FOCUS_NONE);
	curve_create->set_tooltip(TTR("Add Point (in empty space)") + "\n" + TTR("Split Segment (in curve)"));
	SpatialEditor::get_singleton()->add_control_to_menu_panel(curve_create);
	curve_del = memnew(ToolButton);
	curve_del->set_icon(EditorNode::get_singleton()->get_gui_base()->get_icon("CurveDelete", "EditorIcons"));
	curve_del->set_toggle_mode(true);
	curve_del->hide();
	curve_del->set_focus_mode(Control::FOCUS_NONE);
	curve_del->set_tooltip(TTR("Delete Point"));
	SpatialEditor::get_singleton()->add_control_to_menu_panel(curve_del);
	curve_close = memnew(ToolButton);
	curve_close->set_icon(EditorNode::get_singleton()->get_gui_base()->get_icon("CurveClose", "EditorIcons"));
	curve_close->hide();
	curve_close->set_focus_mode(Control::FOCUS_NONE);
	curve_close->set_tooltip(TTR("Close Curve"));
	SpatialEditor::get_singleton()->add_control_to_menu_panel(curve_close);

	PopupMenu *menu;

	handle_menu = memnew(MenuButton);
	handle_menu->set_text(TTR("Options"));
	handle_menu->hide();
	SpatialEditor::get_singleton()->add_control_to_menu_panel(handle_menu);

	menu = handle_menu->get_popup();
	menu->add_check_item(TTR("Mirror Handle Angles"));
	menu->set_item_checked(HANDLE_OPTION_ANGLE, mirror_handle_angle);
	menu->add_check_item(TTR("Mirror Handle Lengths"));
	menu->set_item_checked(HANDLE_OPTION_LENGTH, mirror_handle_length);
	menu->connect("id_pressed", this, "_handle_option_pressed");

	curve_edit->set_pressed(true);
}

PathEditorPlugin::~PathEditorPlugin() {
}

Ref<EditorSpatialGizmo> PathSpatialGizmoPlugin::create_gizmo(Spatial *p_spatial) {
	Ref<PathSpatialGizmo> ref;

	Path *path = Object::cast_to<Path>(p_spatial);
	if (path) {
		ref = Ref<PathSpatialGizmo>(memnew(PathSpatialGizmo(path)));
	}

	return ref;
}

String PathSpatialGizmoPlugin::get_name() const {
	return "Path";
}

int PathSpatialGizmoPlugin::get_priority() const {
	return -1;
}

PathSpatialGizmoPlugin::PathSpatialGizmoPlugin() {
	Color path_color = EDITOR_DEF("editors/3d_gizmos/gizmo_colors/path", Color(0.5, 0.5, 1.0, 0.8));
	create_material("path_material", path_color);
	create_material("path_thin_material", Color(0.5, 0.5, 0.5));
	create_handle_material("handles");
}

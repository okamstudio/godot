/**************************************************************************/
/*  viewport.cpp                                                          */
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

#include "viewport.h"

#include "core/config/project_settings.h"
#include "core/debugger/engine_debugger.h"
#include "core/string/translation.h"
#include "core/templates/pair.h"
#include "core/templates/sort_array.h"
#include "scene/2d/audio_listener_2d.h"
#include "scene/2d/camera_2d.h"
#include "scene/2d/physics/collision_object_2d.h"
#ifndef _3D_DISABLED
#include "scene/3d/audio_listener_3d.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/physics/collision_object_3d.h"
#include "scene/3d/world_environment.h"
#endif // _3D_DISABLED
#include "scene/gui/control.h"
#include "scene/gui/label.h"
#include "scene/gui/popup.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/subviewport_container.h"
#include "scene/main/canvas_layer.h"
#include "scene/main/window.h"
#include "scene/resources/mesh.h"
#include "scene/resources/text_line.h"
#include "scene/resources/world_2d.h"
#include "servers/audio_server.h"
#include "servers/rendering/rendering_server_globals.h"

void ViewportTexture::setup_local_to_scene() {
	// For the same target viewport, setup is only allowed once to prevent multiple free or multiple creations.
	if (!vp_changed) {
		return;
	}

	if (vp_pending) {
		return;
	}

	Node *loc_scene = get_local_scene();
	if (!loc_scene) {
		return;
	}

	if (vp) {
		vp->viewport_textures.erase(this);
		vp = nullptr;
	}

	if (loc_scene->is_ready()) {
		_setup_local_to_scene(loc_scene);
	} else {
		loc_scene->connect(SceneStringName(ready), callable_mp(this, &ViewportTexture::_setup_local_to_scene).bind(loc_scene), CONNECT_ONE_SHOT);
		vp_pending = true;
	}
}

void ViewportTexture::reset_local_to_scene() {
	vp_changed = true;

	if (vp) {
		vp->viewport_textures.erase(this);
		vp = nullptr;
	}

	if (proxy.is_valid() && proxy_ph.is_null()) {
		proxy_ph = RS::get_singleton()->texture_2d_placeholder_create();
		RS::get_singleton()->texture_proxy_update(proxy, proxy_ph);
	}
}

void ViewportTexture::set_viewport_path_in_scene(const NodePath &p_path) {
	if (path == p_path) {
		return;
	}

	path = p_path;

	reset_local_to_scene();

	if (get_local_scene() && !path.is_empty()) {
		setup_local_to_scene();
	} else {
		if (path.is_empty()) {
			vp_changed = false;
		}
		emit_changed();
	}
}

NodePath ViewportTexture::get_viewport_path_in_scene() const {
	return path;
}

int ViewportTexture::get_width() const {
	if (!vp) {
		_err_print_viewport_not_set();
		return 0;
	}
	return vp->size.width;
}

int ViewportTexture::get_height() const {
	if (!vp) {
		_err_print_viewport_not_set();
		return 0;
	}
	return vp->size.height;
}

Size2 ViewportTexture::get_size() const {
	if (!vp) {
		_err_print_viewport_not_set();
		return Size2();
	}
	return vp->size;
}

RID ViewportTexture::get_rid() const {
	if (proxy.is_null()) {
		proxy_ph = RS::get_singleton()->texture_2d_placeholder_create();
		proxy = RS::get_singleton()->texture_proxy_create(proxy_ph);
	}
	return proxy;
}

bool ViewportTexture::has_alpha() const {
	return false;
}

Ref<Image> ViewportTexture::get_image() const {
	if (!vp) {
		_err_print_viewport_not_set();
		return Ref<Image>();
	}
	return RS::get_singleton()->texture_2d_get(vp->texture_rid);
}

void ViewportTexture::_err_print_viewport_not_set() const {
	if (!vp_pending && !vp_changed) {
		ERR_PRINT("Viewport Texture must be set to use it.");
	}
}

void ViewportTexture::_setup_local_to_scene(const Node *p_loc_scene) {
	// Always reset this, even if this call fails with an error.
	vp_pending = false;

	Node *vpn = p_loc_scene->get_node_or_null(path);
	ERR_FAIL_NULL_MSG(vpn, "Path to node is invalid: '" + path + "'.");
	vp = Object::cast_to<Viewport>(vpn);
	ERR_FAIL_NULL_MSG(vp, "Path to node does not point to a viewport: '" + path + "'.");

	vp->viewport_textures.insert(this);

	ERR_FAIL_NULL(RenderingServer::get_singleton());
	if (proxy_ph.is_valid()) {
		RS::get_singleton()->texture_proxy_update(proxy, vp->texture_rid);
		RS::get_singleton()->free(proxy_ph);
		proxy_ph = RID();
	} else {
		ERR_FAIL_COND(proxy.is_valid()); // Should be invalid.
		proxy = RS::get_singleton()->texture_proxy_create(vp->texture_rid);
	}
	vp_changed = false;

	emit_changed();
}

void ViewportTexture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_viewport_path_in_scene", "path"), &ViewportTexture::set_viewport_path_in_scene);
	ClassDB::bind_method(D_METHOD("get_viewport_path_in_scene"), &ViewportTexture::get_viewport_path_in_scene);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "viewport_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Viewport", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NODE_PATH_FROM_SCENE_ROOT), "set_viewport_path_in_scene", "get_viewport_path_in_scene");
}

ViewportTexture::ViewportTexture() {
	set_local_to_scene(true);
}

ViewportTexture::~ViewportTexture() {
	if (vp) {
		vp->viewport_textures.erase(this);
	}

	ERR_FAIL_NULL(RenderingServer::get_singleton());

	if (proxy_ph.is_valid()) {
		RS::get_singleton()->free(proxy_ph);
	}
	if (proxy.is_valid()) {
		RS::get_singleton()->free(proxy);
	}
}

void Viewport::_process_dirty_canvas_parent_orders() {
	for (const ObjectID &id : gui.canvas_parents_with_dirty_order) {
		Object *obj = ObjectDB::get_instance(id);
		if (!obj) {
			continue; // May have been deleted.
		}

		Node *n = static_cast<Node *>(obj);
		for (int i = 0; i < n->get_child_count(); i++) {
			Node *c = n->get_child(i);
			CanvasItem *ci = Object::cast_to<CanvasItem>(c);
			if (ci) {
				ci->update_draw_order();
				continue;
			}
			CanvasLayer *cl = Object::cast_to<CanvasLayer>(c);
			if (cl) {
				cl->update_draw_order();
			}
		}
	}

	gui.canvas_parents_with_dirty_order.clear();
}

void Viewport::_sub_window_update_order() {
	if (gui.sub_windows.size() < 2) {
		return;
	}

	if (!gui.sub_windows[gui.sub_windows.size() - 1].window->get_flag(Window::FLAG_ALWAYS_ON_TOP)) {
		int index = gui.sub_windows.size() - 1;

		while (index > 0 && gui.sub_windows[index - 1].window->get_flag(Window::FLAG_ALWAYS_ON_TOP)) {
			--index;
		}

		if (index != (gui.sub_windows.size() - 1)) {
			SubWindow sw = gui.sub_windows[gui.sub_windows.size() - 1];
			gui.sub_windows.remove_at(gui.sub_windows.size() - 1);
			gui.sub_windows.insert(index, sw);
		}
	}

	for (int i = 0; i < gui.sub_windows.size(); i++) {
		RS::get_singleton()->canvas_item_set_draw_index(gui.sub_windows[i].canvas_item, i);
	}
}

void Viewport::_sub_window_register(Window *p_window) {
	ERR_FAIL_COND(!is_inside_tree());
	for (int i = 0; i < gui.sub_windows.size(); i++) {
		ERR_FAIL_COND(gui.sub_windows[i].window == p_window);
	}

	if (gui.sub_windows.size() == 0) {
		subwindow_canvas = RS::get_singleton()->canvas_create();
		RS::get_singleton()->viewport_attach_canvas(viewport, subwindow_canvas);
		RS::get_singleton()->viewport_set_canvas_stacking(viewport, subwindow_canvas, SUBWINDOW_CANVAS_LAYER, 0);
	}
	SubWindow sw;
	sw.canvas_item = RS::get_singleton()->canvas_item_create();
	RS::get_singleton()->canvas_item_set_parent(sw.canvas_item, subwindow_canvas);
	sw.window = p_window;
	gui.sub_windows.push_back(sw);

	if (gui.subwindow_drag == SUB_WINDOW_DRAG_DISABLED) {
		if (p_window->get_flag(Window::FLAG_NO_FOCUS)) {
			_sub_window_update_order();
		} else {
			_sub_window_grab_focus(p_window);
		}
	} else {
		int index = _sub_window_find(gui.currently_dragged_subwindow);
		sw = gui.sub_windows[index];
		gui.sub_windows.remove_at(index);
		gui.sub_windows.push_back(sw);
		_sub_window_update_order();
	}

	RenderingServer::get_singleton()->viewport_set_parent_viewport(p_window->viewport, viewport);
}

void Viewport::_sub_window_update(Window *p_window) {
	int index = _sub_window_find(p_window);
	ERR_FAIL_COND(index == -1);

	SubWindow &sw = gui.sub_windows.write[index];
	sw.pending_window_update = false;

	Transform2D pos;
	pos.set_origin(p_window->get_position());
	RS::get_singleton()->canvas_item_clear(sw.canvas_item);
	Rect2i r = Rect2i(p_window->get_position(), sw.window->get_size());

	if (!p_window->get_flag(Window::FLAG_BORDERLESS)) {
		Ref<StyleBox> panel = gui.subwindow_focused == p_window ? p_window->theme_cache.embedded_border : p_window->theme_cache.embedded_unfocused_border;
		panel->draw(sw.canvas_item, r);

		// Draw the title bar text.
		Ref<Font> title_font = p_window->theme_cache.title_font;
		int font_size = p_window->theme_cache.title_font_size;
		Color title_color = p_window->theme_cache.title_color;
		int title_height = p_window->theme_cache.title_height;
		int close_h_ofs = p_window->theme_cache.close_h_offset;
		int close_v_ofs = p_window->theme_cache.close_v_offset;

		TextLine title_text = TextLine(p_window->get_translated_title(), title_font, font_size);
		title_text.set_width(r.size.width - panel->get_minimum_size().x - close_h_ofs);
		title_text.set_direction(p_window->is_layout_rtl() ? TextServer::DIRECTION_RTL : TextServer::DIRECTION_LTR);
		int x = (r.size.width - title_text.get_size().x) / 2;
		int y = (-title_height - title_text.get_size().y) / 2;

		Color font_outline_color = p_window->theme_cache.title_outline_modulate;
		int outline_size = p_window->theme_cache.title_outline_size;
		if (outline_size > 0 && font_outline_color.a > 0) {
			title_text.draw_outline(sw.canvas_item, r.position + Point2(x, y), outline_size, font_outline_color);
		}
		title_text.draw(sw.canvas_item, r.position + Point2(x, y), title_color);

		bool pressed = gui.subwindow_focused == sw.window && gui.subwindow_drag == SUB_WINDOW_DRAG_CLOSE && gui.subwindow_drag_close_inside;
		Ref<Texture2D> close_icon = pressed ? p_window->theme_cache.close_pressed : p_window->theme_cache.close;
		close_icon->draw(sw.canvas_item, r.position + Vector2(r.size.width - close_h_ofs, -close_v_ofs));
	}

	RS::get_singleton()->canvas_item_add_texture_rect(sw.canvas_item, r, sw.window->get_texture()->get_rid());
}

void Viewport::_sub_window_grab_focus(Window *p_window) {
	if (p_window == nullptr) {
		// Release current focus.
		if (gui.subwindow_focused) {
			gui.subwindow_focused->_event_callback(DisplayServer::WINDOW_EVENT_FOCUS_OUT);
			gui.subwindow_focused = nullptr;
			gui.subwindow_drag = SUB_WINDOW_DRAG_DISABLED;
		}

		Window *this_window = Object::cast_to<Window>(this);
		if (this_window) {
			this_window->_event_callback(DisplayServer::WINDOW_EVENT_FOCUS_IN);
		}

		return;
	}

	// The index needs to be update before every usage in case an event callback changed the window list.
	int index = _sub_window_find(p_window);
	ERR_FAIL_COND(index == -1);

	if (p_window->get_flag(Window::FLAG_NO_FOCUS)) {
		// Release current focus.
		if (gui.subwindow_focused) {
			gui.subwindow_focused->_event_callback(DisplayServer::WINDOW_EVENT_FOCUS_OUT);
			gui.subwindow_focused = nullptr;
			gui.subwindow_drag = SUB_WINDOW_DRAG_DISABLED;
		}
		// Can only move to foreground, but no focus granted.
		index = _sub_window_find(p_window);
		ERR_FAIL_COND(index == -1);
		SubWindow sw = gui.sub_windows[index];
		gui.sub_windows.remove_at(index);
		gui.sub_windows.push_back(sw);
		_sub_window_update_order();
		return;
	}

	if (gui.subwindow_focused) {
		if (gui.subwindow_focused == p_window) {
			return; // Nothing to do.
		}
		gui.subwindow_focused->_event_callback(DisplayServer::WINDOW_EVENT_FOCUS_OUT);
		gui.subwindow_drag = SUB_WINDOW_DRAG_DISABLED;
	} else {
		Window *this_window = Object::cast_to<Window>(this);
		if (this_window) {
			this_window->_event_callback(DisplayServer::WINDOW_EVENT_FOCUS_OUT);
		}
	}

	Window *old_focus = gui.subwindow_focused;

	gui.subwindow_focused = p_window;

	gui.subwindow_focused->_event_callback(DisplayServer::WINDOW_EVENT_FOCUS_IN);

	{ // Move to foreground.
		index = _sub_window_find(p_window);
		ERR_FAIL_COND(index == -1);
		SubWindow sw = gui.sub_windows[index];
		gui.sub_windows.remove_at(index);
		gui.sub_windows.push_back(sw);
		index = gui.sub_windows.size() - 1;
		_sub_window_update_order();
	}

	if (old_focus) {
		_sub_window_update(old_focus);
	}

	_sub_window_update(p_window);
}

void Viewport::_sub_window_remove(Window *p_window) {
	int index = _sub_window_find(p_window);
	ERR_FAIL_COND(index == -1);

	ERR_FAIL_NULL(RenderingServer::get_singleton());

	SubWindow sw = gui.sub_windows[index];
	if (gui.subwindow_over == sw.window) {
		sw.window->_mouse_leave_viewport();
		gui.subwindow_over = nullptr;
	}
	RS::get_singleton()->free(sw.canvas_item);
	gui.sub_windows.remove_at(index);

	if (gui.sub_windows.size() == 0) {
		RS::get_singleton()->free(subwindow_canvas);
		subwindow_canvas = RID();
	}

	if (gui.currently_dragged_subwindow == p_window) {
		gui.subwindow_drag = SUB_WINDOW_DRAG_DISABLED;
		gui.currently_dragged_subwindow = nullptr;
	}

	if (gui.subwindow_focused == p_window) {
		Window *new_focused_window;
		Window *parent_visible = p_window->get_parent_visible_window();

		gui.subwindow_focused->_event_callback(DisplayServer::WINDOW_EVENT_FOCUS_OUT);

		if (parent_visible) {
			new_focused_window = parent_visible;
		} else {
			new_focused_window = Object::cast_to<Window>(this);
		}

		if (new_focused_window) {
			int new_focused_index = _sub_window_find(new_focused_window);
			if (new_focused_index != -1) {
				gui.subwindow_focused = new_focused_window;
			} else {
				gui.subwindow_focused = nullptr;
			}

			new_focused_window->_event_callback(DisplayServer::WINDOW_EVENT_FOCUS_IN);
		} else {
			gui.subwindow_focused = nullptr;
		}
	}

	RenderingServer::get_singleton()->viewport_set_parent_viewport(p_window->viewport, p_window->parent ? p_window->parent->viewport : RID());
}

int Viewport::_sub_window_find(Window *p_window) const {
	for (int i = 0; i < gui.sub_windows.size(); i++) {
		if (gui.sub_windows[i].window == p_window) {
			return i;
		}
	}

	return -1;
}

void Viewport::_update_viewport_path() {
	if (viewport_textures.is_empty()) {
		return;
	}

	Node *scene_root = get_scene_file_path().is_empty() ? get_owner() : this;
	if (!scene_root && is_inside_tree()) {
		scene_root = get_tree()->get_edited_scene_root();
	}
	if (scene_root && (scene_root == this || scene_root->is_ancestor_of(this))) {
		NodePath path_in_scene = scene_root->get_path_to(this);
		for (ViewportTexture *E : viewport_textures) {
			E->path = path_in_scene;
		}
	}
}

void Viewport::_notification(int p_what) {
	ERR_MAIN_THREAD_GUARD;

	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_update_viewport_path();

			if (get_parent()) {
				parent = get_parent()->get_viewport();
				RenderingServer::get_singleton()->viewport_set_parent_viewport(viewport, parent->get_viewport_rid());
			} else {
				parent = nullptr;
			}

			current_canvas = find_world_2d()->get_canvas();
			RenderingServer::get_singleton()->viewport_attach_canvas(viewport, current_canvas);
			RenderingServer::get_singleton()->viewport_set_canvas_transform(viewport, current_canvas, canvas_transform);
			RenderingServer::get_singleton()->viewport_set_canvas_cull_mask(viewport, canvas_cull_mask);
			_update_audio_listener_2d();
#ifndef _3D_DISABLED
			RenderingServer::get_singleton()->viewport_set_scenario(viewport, find_world_3d()->get_scenario());
			_update_audio_listener_3d();
#endif // _3D_DISABLED

			add_to_group("_viewports");
			if (get_tree()->is_debugging_collisions_hint()) {
				PhysicsServer2D::get_singleton()->space_set_debug_contacts(find_world_2d()->get_space(), get_tree()->get_collision_debug_contact_count());
				contact_2d_debug = RenderingServer::get_singleton()->canvas_item_create();
				RenderingServer::get_singleton()->canvas_item_set_parent(contact_2d_debug, current_canvas);
#ifndef _3D_DISABLED
				PhysicsServer3D::get_singleton()->space_set_debug_contacts(find_world_3d()->get_space(), get_tree()->get_collision_debug_contact_count());
				contact_3d_debug_multimesh = RenderingServer::get_singleton()->multimesh_create();
				RenderingServer::get_singleton()->multimesh_allocate_data(contact_3d_debug_multimesh, get_tree()->get_collision_debug_contact_count(), RS::MULTIMESH_TRANSFORM_3D, false);
				RenderingServer::get_singleton()->multimesh_set_visible_instances(contact_3d_debug_multimesh, 0);
				RenderingServer::get_singleton()->multimesh_set_mesh(contact_3d_debug_multimesh, get_tree()->get_debug_contact_mesh()->get_rid());
				contact_3d_debug_instance = RenderingServer::get_singleton()->instance_create();
				RenderingServer::get_singleton()->instance_set_base(contact_3d_debug_instance, contact_3d_debug_multimesh);
				RenderingServer::get_singleton()->instance_set_scenario(contact_3d_debug_instance, find_world_3d()->get_scenario());
				RenderingServer::get_singleton()->instance_geometry_set_flag(contact_3d_debug_instance, RS::INSTANCE_FLAG_DRAW_NEXT_FRAME_IF_VISIBLE, true);
#endif // _3D_DISABLED
				set_physics_process_internal(true);
			}
		} break;

		case NOTIFICATION_READY: {
#ifndef _3D_DISABLED
			if (audio_listener_3d_set.size() && !audio_listener_3d) {
				AudioListener3D *first = nullptr;
				for (AudioListener3D *E : audio_listener_3d_set) {
					if (first == nullptr || first->is_greater_than(E)) {
						first = E;
					}
				}

				if (first) {
					first->make_current();
				}
			}

			if (camera_3d_set.size() && !camera_3d) {
				// There are cameras but no current camera, pick first in tree and make it current.
				Camera3D *first = nullptr;
				for (Camera3D *E : camera_3d_set) {
					if (first == nullptr || first->is_greater_than(E)) {
						first = E;
					}
				}

				if (first) {
					first->make_current();
				}
			}
#endif // _3D_DISABLED
		} break;

		case NOTIFICATION_EXIT_TREE: {
			_gui_cancel_tooltip();

			RenderingServer::get_singleton()->viewport_set_scenario(viewport, RID());
			RenderingServer::get_singleton()->viewport_remove_canvas(viewport, current_canvas);
			if (contact_2d_debug.is_valid()) {
				RenderingServer::get_singleton()->free(contact_2d_debug);
				contact_2d_debug = RID();
			}

			if (contact_3d_debug_multimesh.is_valid()) {
				RenderingServer::get_singleton()->free(contact_3d_debug_multimesh);
				RenderingServer::get_singleton()->free(contact_3d_debug_instance);
				contact_3d_debug_instance = RID();
				contact_3d_debug_multimesh = RID();
			}

			remove_from_group("_viewports");
			set_physics_process_internal(false);

			RS::get_singleton()->viewport_set_active(viewport, false);
			RenderingServer::get_singleton()->viewport_set_parent_viewport(viewport, RID());
		} break;

		case NOTIFICATION_PATH_RENAMED: {
			_update_viewport_path();
		} break;

		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (!get_tree()) {
				return;
			}

			if (get_tree()->is_debugging_collisions_hint() && contact_2d_debug.is_valid()) {
				RenderingServer::get_singleton()->canvas_item_clear(contact_2d_debug);
				RenderingServer::get_singleton()->canvas_item_set_draw_index(contact_2d_debug, 0xFFFFF); //very high index

				Vector<Vector2> points = PhysicsServer2D::get_singleton()->space_get_contacts(find_world_2d()->get_space());
				int point_count = PhysicsServer2D::get_singleton()->space_get_contact_count(find_world_2d()->get_space());
				Color ccol = get_tree()->get_debug_collision_contact_color();

				for (int i = 0; i < point_count; i++) {
					RenderingServer::get_singleton()->canvas_item_add_rect(contact_2d_debug, Rect2(points[i] - Vector2(2, 2), Vector2(5, 5)), ccol);
				}
			}
#ifndef _3D_DISABLED
			if (get_tree()->is_debugging_collisions_hint() && contact_3d_debug_multimesh.is_valid()) {
				Vector<Vector3> points = PhysicsServer3D::get_singleton()->space_get_contacts(find_world_3d()->get_space());
				int point_count = PhysicsServer3D::get_singleton()->space_get_contact_count(find_world_3d()->get_space());

				RS::get_singleton()->multimesh_set_visible_instances(contact_3d_debug_multimesh, point_count);

				for (int i = 0; i < point_count; i++) {
					Transform3D point_transform;
					point_transform.origin = points[i];
					RS::get_singleton()->multimesh_instance_set_transform(contact_3d_debug_multimesh, i, point_transform);
				}
			}
#endif // _3D_DISABLED
		} break;

		case NOTIFICATION_VP_MOUSE_ENTER: {
			gui.mouse_in_viewport = true;
		} break;

		case NOTIFICATION_VP_MOUSE_EXIT: {
			gui.mouse_in_viewport = false;
			_drop_physics_mouseover();
			// When the mouse exits the viewport, we don't want to end
			// mouse_focus, because, for example, we want to continue
			// dragging a scrollbar even if the mouse has left the viewport.
		} break;

		case NOTIFICATION_WM_WINDOW_FOCUS_OUT: {
			_gui_cancel_tooltip();
			_drop_physics_mouseover();
			_drop_mouse_focus();
			// When the window focus changes, we want to end mouse_focus, but
			// not the mouse_over. Note: The OS will trigger a separate mouse
			// exit event if the change in focus results in the mouse exiting
			// the window.
		} break;

		case NOTIFICATION_PREDELETE: {
			if (gui_parent) {
				gui_parent->gui.tooltip_popup = nullptr;
				gui_parent->gui.tooltip_label = nullptr;
			}
		} break;
	}
}

void Viewport::_process_picking() {
	if (!is_inside_tree()) {
		return;
	}
	if (!physics_object_picking) {
		return;
	}
	if (Object::cast_to<Window>(this) && Input::get_singleton()->get_mouse_mode() == Input::MOUSE_MODE_CAPTURED) {
		return;
	}
	if (!gui.mouse_in_viewport || gui.subwindow_over) {
		// Clear picking events if the mouse has left the viewport or is over an embedded window.
		// These are locations, that are expected to not trigger physics picking.
		physics_picking_events.clear();
		return;
	}
#ifndef _3D_DISABLED
	if (use_xr) {
		if (XRServer::get_singleton() != nullptr) {
			Ref<XRInterface> xr_interface = XRServer::get_singleton()->get_primary_interface();
			if (xr_interface.is_valid() && xr_interface->is_initialized() && xr_interface->get_view_count() > 1) {
				WARN_PRINT_ONCE("Object picking can't be used when stereo rendering, this will be turned off!");
				physics_object_picking = false; // don't try again.
				return;
			}
		}
	}
#endif

	_drop_physics_mouseover(true);

#ifndef _3D_DISABLED
	Vector2 last_pos(1e20, 1e20);
	CollisionObject3D *last_object = nullptr;
	ObjectID last_id;
	PhysicsDirectSpaceState3D::RayResult result;
#endif // _3D_DISABLED

	PhysicsDirectSpaceState2D *ss2d = PhysicsServer2D::get_singleton()->space_get_direct_state(find_world_2d()->get_space());

	SubViewportContainer *parent_svc = Object::cast_to<SubViewportContainer>(get_parent());
	bool parent_ignore_mouse = (parent_svc && parent_svc->get_mouse_filter() == Control::MOUSE_FILTER_IGNORE);
	bool create_passive_hover_event = true;
	if (gui.mouse_over || parent_ignore_mouse) {
		// When the mouse is over a Control node, passive hovering would cause input events for Colliders, that are behind Control nodes.
		// When parent SubViewportContainer ignores mouse, that setting should be respected.
		create_passive_hover_event = false;
	} else {
		for (const Ref<InputEvent> &e : physics_picking_events) {
			Ref<InputEventMouse> m = e;
			if (m.is_valid()) {
				// A mouse event exists, so passive hovering isn't necessary.
				create_passive_hover_event = false;
				break;
			}
		}
	}

	if (create_passive_hover_event) {
		// Create a mouse motion event. This is necessary because objects or camera may have moved.
		// While this extra event is sent, it is checked if both camera and last object and last ID did not move.
		// If nothing changed, the event is discarded to avoid flooding with unnecessary motion events every frame.
		Ref<InputEventMouseMotion> mm;
		mm.instantiate();

		mm->set_device(InputEvent::DEVICE_ID_INTERNAL);
		mm->set_position(get_mouse_position());
		mm->set_global_position(mm->get_position());
		mm->set_alt_pressed(Input::get_singleton()->is_key_pressed(Key::ALT));
		mm->set_shift_pressed(Input::get_singleton()->is_key_pressed(Key::SHIFT));
		mm->set_ctrl_pressed(Input::get_singleton()->is_key_pressed(Key::CTRL));
		mm->set_meta_pressed(Input::get_singleton()->is_key_pressed(Key::META));
		mm->set_button_mask(Input::get_singleton()->get_mouse_button_mask());
		physics_picking_events.push_back(mm);
	}

	while (physics_picking_events.size()) {
		local_input_handled = false;
		if (!handle_input_locally) {
			Viewport *vp = this;
			while (!Object::cast_to<Window>(vp) && vp->get_parent()) {
				vp = vp->get_parent()->get_viewport();
			}
			vp->local_input_handled = false;
		}

		Ref<InputEvent> ev = physics_picking_events.front()->get();
		physics_picking_events.pop_front();

		Vector2 pos;
		bool is_mouse = false;

		Ref<InputEventMouseMotion> mm = ev;

		if (mm.is_valid()) {
			pos = mm->get_position();
			is_mouse = true;
		}

		Ref<InputEventMouseButton> mb = ev;

		if (mb.is_valid()) {
			pos = mb->get_position();
			is_mouse = true;
		}

		Ref<InputEventScreenDrag> sd = ev;

		if (sd.is_valid()) {
			pos = sd->get_position();
		}

		Ref<InputEventScreenTouch> st = ev;

		if (st.is_valid()) {
			pos = st->get_position();
		}

		if (ss2d) {
			// Send to 2D.

			uint64_t frame = get_tree()->get_frame();

			PhysicsDirectSpaceState2D::ShapeResult res[64];
			for (const CanvasLayer *E : canvas_layers) {
				Transform2D canvas_layer_transform;
				ObjectID canvas_layer_id;
				if (E) {
					// A descendant CanvasLayer.
					canvas_layer_transform = E->get_final_transform();
					canvas_layer_id = E->get_instance_id();
				} else {
					// This Viewport's builtin canvas.
					canvas_layer_transform = get_canvas_transform();
					canvas_layer_id = ObjectID();
				}

				Vector2 point = canvas_layer_transform.affine_inverse().xform(pos);

				PhysicsDirectSpaceState2D::PointParameters point_params;
				point_params.position = point;
				point_params.canvas_instance_id = canvas_layer_id;
				point_params.collide_with_areas = true;
				point_params.pick_point = true;

				int rc = ss2d->intersect_point(point_params, res, 64);
				if (physics_object_picking_sort) {
					struct ComparatorCollisionObjects {
						bool operator()(const PhysicsDirectSpaceState2D::ShapeResult &p_a, const PhysicsDirectSpaceState2D::ShapeResult &p_b) const {
							CollisionObject2D *a = Object::cast_to<CollisionObject2D>(p_a.collider);
							CollisionObject2D *b = Object::cast_to<CollisionObject2D>(p_b.collider);
							if (!a || !b) {
								return false;
							}
							int za = a->get_effective_z_index();
							int zb = b->get_effective_z_index();
							if (za != zb) {
								return zb < za;
							}
							return a->is_greater_than(b);
						}
					};
					SortArray<PhysicsDirectSpaceState2D::ShapeResult, ComparatorCollisionObjects> sorter;
					sorter.sort(res, rc);
				}
				for (int i = 0; i < rc; i++) {
					if (is_input_handled()) {
						break;
					}
					if (res[i].collider_id.is_valid() && res[i].collider) {
						CollisionObject2D *co = Object::cast_to<CollisionObject2D>(res[i].collider);
						if (co && co->can_process()) {
							bool send_event = true;
							if (is_mouse) {
								HashMap<ObjectID, uint64_t>::Iterator F = physics_2d_mouseover.find(res[i].collider_id);
								if (!F) {
									physics_2d_mouseover.insert(res[i].collider_id, frame);
									co->_mouse_enter();
								} else {
									F->value = frame;
									// It was already hovered, so don't send the event if it's faked.
									if (mm.is_valid() && mm->get_device() == InputEvent::DEVICE_ID_INTERNAL) {
										send_event = false;
									}
								}
								HashMap<Pair<ObjectID, int>, uint64_t, PairHash<ObjectID, int>>::Iterator SF = physics_2d_shape_mouseover.find(Pair(res[i].collider_id, res[i].shape));
								if (!SF) {
									physics_2d_shape_mouseover.insert(Pair(res[i].collider_id, res[i].shape), frame);
									co->_mouse_shape_enter(res[i].shape);
								} else {
									SF->value = frame;
								}
							}

							if (send_event) {
								co->_input_event_call(this, ev, res[i].shape);
							}

							if (physics_object_picking_first_only) {
								break;
							}
						}
					}
				}
			}

			if (is_mouse) {
				_cleanup_mouseover_colliders(false, false, frame);
			}
		}

#ifndef _3D_DISABLED
		if (physics_object_picking_first_only && is_input_handled()) {
			continue;
		}

		CollisionObject3D *capture_object = nullptr;
		if (physics_object_capture.is_valid()) {
			capture_object = Object::cast_to<CollisionObject3D>(ObjectDB::get_instance(physics_object_capture));
			if (!capture_object || !camera_3d || (mb.is_valid() && mb->get_button_index() == MouseButton::LEFT && !mb->is_pressed())) {
				physics_object_capture = ObjectID();
			} else {
				last_id = physics_object_capture;
				last_object = capture_object;
			}
		}

		if (pos == last_pos) {
			if (last_id.is_valid()) {
				if (ObjectDB::get_instance(last_id) && last_object) {
					// Good, exists.
					_collision_object_3d_input_event(last_object, camera_3d, ev, result.position, result.normal, result.shape);
					if (last_object->get_capture_input_on_drag() && mb.is_valid() && mb->get_button_index() == MouseButton::LEFT && mb->is_pressed()) {
						physics_object_capture = last_id;
					}
				}
			}
		} else {
			if (camera_3d) {
				Vector3 from = camera_3d->project_ray_origin(pos);
				Vector3 dir = camera_3d->project_ray_normal(pos);
				real_t depth_far = camera_3d->get_far();

				PhysicsDirectSpaceState3D *space = PhysicsServer3D::get_singleton()->space_get_direct_state(find_world_3d()->get_space());
				if (space) {
					PhysicsDirectSpaceState3D::RayParameters ray_params;
					ray_params.from = from;
					ray_params.to = from + dir * depth_far;
					ray_params.collide_with_areas = true;
					ray_params.pick_ray = true;

					bool col = space->intersect_ray(ray_params, result);
					ObjectID new_collider;
					CollisionObject3D *co = col ? Object::cast_to<CollisionObject3D>(result.collider) : nullptr;
					if (co && co->can_process()) {
						new_collider = result.collider_id;
						if (!capture_object) {
							last_object = co;
							last_id = result.collider_id;
							if (co->get_capture_input_on_drag() && mb.is_valid() && mb->get_button_index() == MouseButton::LEFT && mb->is_pressed()) {
								physics_object_capture = last_id;
							}
						}
					}

					if (is_mouse && new_collider != physics_object_over) {
						if (physics_object_over.is_valid()) {
							CollisionObject3D *previous_co = Object::cast_to<CollisionObject3D>(ObjectDB::get_instance(physics_object_over));
							if (previous_co) {
								previous_co->_mouse_exit();
							}
						}

						if (new_collider.is_valid()) {
							DEV_ASSERT(co);
							co->_mouse_enter();
						}

						physics_object_over = new_collider;
					}
					if (capture_object) {
						_collision_object_3d_input_event(capture_object, camera_3d, ev, result.position, result.normal, result.shape);
					} else if (new_collider.is_valid()) {
						_collision_object_3d_input_event(co, camera_3d, ev, result.position, result.normal, result.shape);
					}
				}

				last_pos = pos;
			}
		}
#endif // _3D_DISABLED
	}
}

RID Viewport::get_viewport_rid() const {
	ERR_READ_THREAD_GUARD_V(RID());
	return viewport;
}

void Viewport::update_canvas_items() {
	ERR_MAIN_THREAD_GUARD;
	if (!is_inside_tree()) {
		return;
	}

	if (is_embedding_subwindows()) {
		for (Viewport::SubWindow w : gui.sub_windows) {
			if (w.window && !w.pending_window_update) {
				w.pending_window_update = true;
				callable_mp(this, &Viewport::_sub_window_update).call_deferred(w.window);
			}
		}
	}
	_update_canvas_items(this);
}

bool Viewport::_set_size(const Size2i &p_size, const Size2i &p_size_2d_override, bool p_allocated) {
	Transform2D stretch_transform_new = Transform2D();
	if (is_size_2d_override_stretch_enabled() && p_size_2d_override.width > 0 && p_size_2d_override.height > 0) {
		Size2 scale = Size2(p_size) / Size2(p_size_2d_override);
		stretch_transform_new.scale(scale);
	}

	Size2i new_size = p_size.maxi(2);
	if (size == new_size && size_allocated == p_allocated && stretch_transform == stretch_transform_new && p_size_2d_override == size_2d_override) {
		return false;
	}

	size = new_size;
	size_allocated = p_allocated;
	size_2d_override = p_size_2d_override;
	stretch_transform = stretch_transform_new;

#ifndef _3D_DISABLED
	if (!use_xr) {
#endif

		if (p_allocated) {
			RS::get_singleton()->viewport_set_size(viewport, size.width, size.height);
		} else {
			RS::get_singleton()->viewport_set_size(viewport, 0, 0);
		}

#ifndef _3D_DISABLED
	} // if (!use_xr)
#endif

	_update_global_transform();
	update_configuration_warnings();

	update_canvas_items();

	for (ViewportTexture *E : viewport_textures) {
		E->emit_changed();
	}

	emit_signal(SNAME("size_changed"));

	Rect2i limit = get_visible_rect();
	for (int i = 0; i < gui.sub_windows.size(); ++i) {
		Window *sw = gui.sub_windows[i].window;
		Rect2i rect = Rect2i(sw->position, sw->size);
		Rect2i new_rect = sw->fit_rect_in_parent(rect, limit);
		if (new_rect != rect) {
			sw->set_position(new_rect.position);
			sw->set_size(new_rect.size);
		}
	}
	return true;
}

Size2i Viewport::_get_size() const {
#ifndef _3D_DISABLED
	if (use_xr) {
		if (XRServer::get_singleton() != nullptr) {
			Ref<XRInterface> xr_interface = XRServer::get_singleton()->get_primary_interface();
			if (xr_interface.is_valid() && xr_interface->is_initialized()) {
				Size2 xr_size = xr_interface->get_render_target_size();
				return (Size2i)xr_size;
			}
		}
		return Size2i();
	}
#endif // _3D_DISABLED

	return size;
}

Size2i Viewport::_get_size_2d_override() const {
	return size_2d_override;
}

bool Viewport::_is_size_allocated() const {
	return size_allocated;
}

Rect2 Viewport::get_visible_rect() const {
	ERR_READ_THREAD_GUARD_V(Rect2());
	Rect2 r;

	if (size == Size2()) {
		r = Rect2(Point2(), DisplayServer::get_singleton()->window_get_size());
	} else {
		r = Rect2(Point2(), size);
	}

	if (size_2d_override != Size2i()) {
		r.size = size_2d_override;
	}

	return r;
}

void Viewport::canvas_parent_mark_dirty(Node *p_node) {
	ERR_MAIN_THREAD_GUARD;
	bool request_update = gui.canvas_parents_with_dirty_order.is_empty();
	gui.canvas_parents_with_dirty_order.insert(p_node->get_instance_id());
	if (request_update) {
		callable_mp(this, &Viewport::_process_dirty_canvas_parent_orders).call_deferred();
	}
}

void Viewport::enable_canvas_transform_override(bool p_enable) {
	ERR_MAIN_THREAD_GUARD;
	if (override_canvas_transform == p_enable) {
		return;
	}

	override_canvas_transform = p_enable;
	if (p_enable) {
		RenderingServer::get_singleton()->viewport_set_canvas_transform(viewport, find_world_2d()->get_canvas(), canvas_transform_override);
	} else {
		RenderingServer::get_singleton()->viewport_set_canvas_transform(viewport, find_world_2d()->get_canvas(), canvas_transform);
	}
}

bool Viewport::is_canvas_transform_override_enabled() const {
	ERR_READ_THREAD_GUARD_V(false);
	return override_canvas_transform;
}

void Viewport::set_canvas_transform_override(const Transform2D &p_transform) {
	ERR_MAIN_THREAD_GUARD;
	if (canvas_transform_override == p_transform) {
		return;
	}

	canvas_transform_override = p_transform;
	if (override_canvas_transform) {
		RenderingServer::get_singleton()->viewport_set_canvas_transform(viewport, find_world_2d()->get_canvas(), canvas_transform_override);
	}
}

Transform2D Viewport::get_canvas_transform_override() const {
	ERR_READ_THREAD_GUARD_V(Transform2D());
	return canvas_transform_override;
}

void Viewport::set_canvas_transform(const Transform2D &p_transform) {
	ERR_MAIN_THREAD_GUARD;
	canvas_transform = p_transform;

	if (!override_canvas_transform) {
		RenderingServer::get_singleton()->viewport_set_canvas_transform(viewport, find_world_2d()->get_canvas(), canvas_transform);
	}
}

Transform2D Viewport::get_canvas_transform() const {
	ERR_READ_THREAD_GUARD_V(Transform2D());
	return canvas_transform;
}

void Viewport::_update_global_transform() {
	Transform2D sxform = stretch_transform * global_canvas_transform;

	RenderingServer::get_singleton()->viewport_set_global_canvas_transform(viewport, sxform);
}

void Viewport::set_global_canvas_transform(const Transform2D &p_transform) {
	ERR_MAIN_THREAD_GUARD;
	global_canvas_transform = p_transform;

	_update_global_transform();
}

Transform2D Viewport::get_global_canvas_transform() const {
	ERR_READ_THREAD_GUARD_V(Transform2D());
	return global_canvas_transform;
}

void Viewport::_canvas_layer_add(CanvasLayer *p_canvas_layer) {
	canvas_layers.insert(p_canvas_layer);
}

void Viewport::_canvas_layer_remove(CanvasLayer *p_canvas_layer) {
	canvas_layers.erase(p_canvas_layer);
}

void Viewport::set_transparent_background(bool p_enable) {
	ERR_MAIN_THREAD_GUARD;
	transparent_bg = p_enable;
	RS::get_singleton()->viewport_set_transparent_background(viewport, p_enable);
}

bool Viewport::has_transparent_background() const {
	ERR_READ_THREAD_GUARD_V(false);
	return transparent_bg;
}

void Viewport::set_use_hdr_2d(bool p_enable) {
	ERR_MAIN_THREAD_GUARD;
	use_hdr_2d = p_enable;
	RS::get_singleton()->viewport_set_use_hdr_2d(viewport, p_enable);
}

bool Viewport::is_using_hdr_2d() const {
	ERR_READ_THREAD_GUARD_V(false);
	return use_hdr_2d;
}

void Viewport::set_world_2d(const Ref<World2D> &p_world_2d) {
	ERR_MAIN_THREAD_GUARD;
	if (world_2d == p_world_2d) {
		return;
	}

	if (is_inside_tree()) {
		RenderingServer::get_singleton()->viewport_remove_canvas(viewport, current_canvas);
	}

	if (world_2d.is_valid()) {
		world_2d->remove_viewport(this);
	}

	if (p_world_2d.is_valid()) {
		bool do_propagate = world_2d.is_valid() && is_inside_tree();
		world_2d = p_world_2d;
		if (do_propagate) {
			_propagate_world_2d_changed(this);
		}
	} else {
		WARN_PRINT("Invalid world_2d");
		world_2d.instantiate();
	}

	world_2d->register_viewport(this);
	_update_audio_listener_2d();

	if (is_inside_tree()) {
		current_canvas = find_world_2d()->get_canvas();
		RenderingServer::get_singleton()->viewport_attach_canvas(viewport, current_canvas);
	}
}

Ref<World2D> Viewport::find_world_2d() const {
	ERR_READ_THREAD_GUARD_V(Ref<World2D>());
	if (world_2d.is_valid()) {
		return world_2d;
	} else if (parent) {
		return parent->find_world_2d();
	} else {
		return Ref<World2D>();
	}
}

void Viewport::_propagate_drag_notification(Node *p_node, int p_what) {
	// Send notification to p_node and all children and descendant nodes of p_node, except to SubViewports which are not children of a SubViewportContainer.
	p_node->notification(p_what);
	bool is_svc = Object::cast_to<SubViewportContainer>(p_node);
	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *c = p_node->get_child(i);
		if (!is_svc && Object::cast_to<SubViewport>(c)) {
			continue;
		}
		Viewport::_propagate_drag_notification(c, p_what);
	}
}

Ref<World2D> Viewport::get_world_2d() const {
	ERR_READ_THREAD_GUARD_V(Ref<World2D>());
	return world_2d;
}

Transform2D Viewport::get_stretch_transform() const {
	return stretch_transform;
}

Transform2D Viewport::get_final_transform() const {
	ERR_READ_THREAD_GUARD_V(Transform2D());
	return stretch_transform * global_canvas_transform;
}

void Viewport::_update_canvas_items(Node *p_node) {
	if (p_node != this) {
		Window *w = Object::cast_to<Window>(p_node);
		if (w && (!w->is_inside_tree() || !w->is_embedded())) {
			return;
		}

		CanvasItem *ci = Object::cast_to<CanvasItem>(p_node);
		if (ci) {
			ci->queue_redraw();
		}
	}

	int cc = p_node->get_child_count();

	for (int i = 0; i < cc; i++) {
		_update_canvas_items(p_node->get_child(i));
	}
}

Ref<ViewportTexture> Viewport::get_texture() const {
	ERR_READ_THREAD_GUARD_V(Ref<ViewportTexture>());
	return default_texture;
}

void Viewport::set_positional_shadow_atlas_size(int p_size) {
	ERR_MAIN_THREAD_GUARD;
	positional_shadow_atlas_size = p_size;
	RS::get_singleton()->viewport_set_positional_shadow_atlas_size(viewport, p_size, positional_shadow_atlas_16_bits);
}

int Viewport::get_positional_shadow_atlas_size() const {
	ERR_READ_THREAD_GUARD_V(0);
	return positional_shadow_atlas_size;
}

void Viewport::set_positional_shadow_atlas_16_bits(bool p_16_bits) {
	ERR_MAIN_THREAD_GUARD;
	if (positional_shadow_atlas_16_bits == p_16_bits) {
		return;
	}

	positional_shadow_atlas_16_bits = p_16_bits;
	RS::get_singleton()->viewport_set_positional_shadow_atlas_size(viewport, positional_shadow_atlas_size, positional_shadow_atlas_16_bits);
}

bool Viewport::get_positional_shadow_atlas_16_bits() const {
	ERR_READ_THREAD_GUARD_V(false);
	return positional_shadow_atlas_16_bits;
}
void Viewport::set_positional_shadow_atlas_quadrant_subdiv(int p_quadrant, PositionalShadowAtlasQuadrantSubdiv p_subdiv) {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_INDEX(p_quadrant, 4);
	ERR_FAIL_INDEX(p_subdiv, SHADOW_ATLAS_QUADRANT_SUBDIV_MAX);

	if (positional_shadow_atlas_quadrant_subdiv[p_quadrant] == p_subdiv) {
		return;
	}

	positional_shadow_atlas_quadrant_subdiv[p_quadrant] = p_subdiv;
	static const int subdiv[SHADOW_ATLAS_QUADRANT_SUBDIV_MAX] = { 0, 1, 4, 16, 64, 256, 1024 };

	RS::get_singleton()->viewport_set_positional_shadow_atlas_quadrant_subdivision(viewport, p_quadrant, subdiv[p_subdiv]);
}

Viewport::PositionalShadowAtlasQuadrantSubdiv Viewport::get_positional_shadow_atlas_quadrant_subdiv(int p_quadrant) const {
	ERR_READ_THREAD_GUARD_V(SHADOW_ATLAS_QUADRANT_SUBDIV_DISABLED);
	ERR_FAIL_INDEX_V(p_quadrant, 4, SHADOW_ATLAS_QUADRANT_SUBDIV_DISABLED);
	return positional_shadow_atlas_quadrant_subdiv[p_quadrant];
}

Ref<InputEvent> Viewport::_make_input_local(const Ref<InputEvent> &ev) {
	if (ev.is_null()) {
		return ev; // No transformation defined for null event
	}

	Transform2D ai = get_final_transform().affine_inverse();
	Ref<InputEventMouse> me = ev;
	if (me.is_valid()) {
		me = me->xformed_by(ai);
		// For InputEventMouse, the global position is not adjusted by ev->xformed_by() and needs to be set separately.
		me->set_global_position(me->get_position());
		return me;
	}
	return ev->xformed_by(ai);
}

Vector2 Viewport::get_mouse_position() const {
	ERR_READ_THREAD_GUARD_V(Vector2());
	if (get_section_root_viewport() != SceneTree::get_singleton()->get_root()) {
		// Rely on the most recent mouse coordinate from an InputEventMouse in push_input.
		// In this case get_screen_transform is not applicable, because it is ambiguous.
		return gui.last_mouse_pos;
	} else if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_MOUSE)) {
		Transform2D xform = get_screen_transform_internal(true);
		if (xform.determinant() == 0) {
			// Screen transform can be non-invertible when the Window is minimized.
			return Vector2();
		}
		return xform.affine_inverse().xform(DisplayServer::get_singleton()->mouse_get_position());
	} else {
		// Fallback to Input for getting mouse position in case of emulated mouse.
		return get_screen_transform_internal().affine_inverse().xform(Input::get_singleton()->get_mouse_position());
	}
}

void Viewport::warp_mouse(const Vector2 &p_position) {
	ERR_MAIN_THREAD_GUARD;
	Transform2D xform = get_screen_transform_internal();
	Vector2 gpos = xform.xform(p_position);
	Input::get_singleton()->warp_mouse(gpos);
}

void Viewport::_gui_sort_roots() {
	if (!gui.roots_order_dirty) {
		return;
	}

	gui.roots.sort_custom<Control::CComparator>();

	gui.roots_order_dirty = false;
}

void Viewport::_gui_cancel_tooltip() {
	gui.tooltip_control = nullptr;
	gui.tooltip_text = "";

	if (gui.tooltip_timer.is_valid()) {
		gui.tooltip_timer->release_connections();
		gui.tooltip_timer = Ref<SceneTreeTimer>();
	}
	if (gui.tooltip_popup) {
		gui.tooltip_popup->queue_free();
	}
}

String Viewport::_gui_get_tooltip(Control *p_control, const Vector2 &p_pos, Control **r_tooltip_owner) {
	Vector2 pos = p_pos;
	String tooltip;

	while (p_control) {
		tooltip = p_control->get_tooltip(pos);

		// Temporary solution for PopupMenus.
		PopupMenu *menu = Object::cast_to<PopupMenu>(this);
		if (menu) {
			tooltip = menu->get_tooltip(pos);
		}

		if (r_tooltip_owner) {
			*r_tooltip_owner = p_control;
		}

		// If we found a tooltip, we stop here.
		if (!tooltip.is_empty()) {
			break;
		}

		// Otherwise, we check parent controls unless some conditions prevent it.

		if (p_control->data.mouse_filter == Control::MOUSE_FILTER_STOP) {
			break;
		}
		if (p_control->is_set_as_top_level()) {
			break;
		}

		// Transform cursor pos for parent control.
		pos = p_control->get_transform().xform(pos);

		p_control = p_control->get_parent_control();
	}

	return tooltip;
}

void Viewport::_gui_show_tooltip() {
	if (!gui.tooltip_control) {
		return;
	}

	// Get the Control under cursor and the relevant tooltip text, if any.
	Control *tooltip_owner = nullptr;
	gui.tooltip_text = _gui_get_tooltip(
			gui.tooltip_control,
			gui.tooltip_control->get_global_transform_with_canvas().affine_inverse().xform(gui.last_mouse_pos),
			&tooltip_owner);
	gui.tooltip_text = gui.tooltip_text.strip_edges();

	// Controls can implement `make_custom_tooltip` to provide their own tooltip.
	// This should be a Control node which will be added as child to a TooltipPanel.
	Control *base_tooltip = tooltip_owner->make_custom_tooltip(gui.tooltip_text);

	// When the custom control is not visible, don't show any tooltip.
	// This way, the custom tooltip from `ConnectionsDockTree` can create
	// its own tooltip without conflicting with the default one, even an empty tooltip.
	if (base_tooltip && !base_tooltip->is_visible()) {
		return;
	}

	if (gui.tooltip_text.is_empty() && !base_tooltip) {
		return; // Nothing to show.
	}

	// Remove previous popup if we change something.
	if (gui.tooltip_popup) {
		memdelete(gui.tooltip_popup);
		gui.tooltip_popup = nullptr;
	}

	if (!tooltip_owner) {
		return;
	}

	// Popup window which houses the tooltip content.
	PopupPanel *panel = memnew(PopupPanel);
	panel->set_theme_type_variation(SNAME("TooltipPanel"));

	// Ensure no opaque background behind the panel as its StyleBox can be partially transparent (e.g. corners).
	panel->set_transparent_background(true);

	// If no custom tooltip is given, use a default implementation.
	if (!base_tooltip) {
		gui.tooltip_label = memnew(Label);
		gui.tooltip_label->set_theme_type_variation(SNAME("TooltipLabel"));
		gui.tooltip_label->set_text(gui.tooltip_text);
		gui.tooltip_label->set_auto_translate_mode(tooltip_owner->get_tooltip_auto_translate_mode());
		base_tooltip = gui.tooltip_label;
		panel->connect(SceneStringName(mouse_entered), callable_mp(this, &Viewport::_gui_cancel_tooltip));
	}

	base_tooltip->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);

	panel->set_transient(true);
	panel->set_flag(Window::FLAG_NO_FOCUS, true);
	panel->set_flag(Window::FLAG_POPUP, false);
	panel->set_flag(Window::FLAG_MOUSE_PASSTHROUGH, true);
	// A non-embedded tooltip window will only be transparent if per_pixel_transparency is allowed in the main Viewport.
	panel->set_flag(Window::FLAG_TRANSPARENT, true);
	panel->set_wrap_controls(true);
	panel->add_child(base_tooltip);
	panel->gui_parent = this;

	gui.tooltip_popup = panel;

	tooltip_owner->add_child(gui.tooltip_popup);

	Window *window = Object::cast_to<Window>(gui.tooltip_popup->get_embedder());
	if (!window) { // Not embedded.
		window = gui.tooltip_popup->get_parent_visible_window();
	}
	float win_scale = window->content_scale_factor;
	Point2 tooltip_offset = GLOBAL_GET("display/mouse_cursor/tooltip_position_offset");
	if (!gui.tooltip_popup->is_embedded()) {
		tooltip_offset *= win_scale;
	}
	Rect2 r(gui.tooltip_pos + tooltip_offset, gui.tooltip_popup->get_contents_minimum_size());
	Rect2i vr;
	if (gui.tooltip_popup->is_embedded()) {
		vr = gui.tooltip_popup->get_embedder()->get_visible_rect();
	} else {
		panel->content_scale_factor = win_scale;
		r.size *= win_scale;
		vr = window->get_usable_parent_rect();
	}
	r.size = r.size.ceil();
	r.size = r.size.min(panel->get_max_size());

	if (r.size.x + r.position.x > vr.size.x + vr.position.x) {
		// Place it in the opposite direction. If it fails, just hug the border.
		r.position.x = gui.tooltip_pos.x - r.size.x - tooltip_offset.x;

		if (r.position.x < vr.position.x) {
			r.position.x = vr.position.x + vr.size.x - r.size.x;
		}
	} else if (r.position.x < vr.position.x) {
		r.position.x = vr.position.x;
	}

	if (r.size.y + r.position.y > vr.size.y + vr.position.y) {
		// Same as above.
		r.position.y = gui.tooltip_pos.y - r.size.y - tooltip_offset.y;

		if (r.position.y < vr.position.y) {
			r.position.y = vr.position.y + vr.size.y - r.size.y;
		}
	} else if (r.position.y < vr.position.y) {
		r.position.y = vr.position.y;
	}

	gui.tooltip_popup->set_position(r.position);
	gui.tooltip_popup->set_size(r.size);

	DisplayServer::WindowID active_popup = DisplayServer::get_singleton()->window_get_active_popup();
	if (active_popup == DisplayServer::INVALID_WINDOW_ID || active_popup == window->get_window_id()) {
		gui.tooltip_popup->show();
	}
	gui.tooltip_popup->child_controls_changed();
}

void Viewport::_gui_call_input(Control *p_control, const Ref<InputEvent> &p_input) {
	Ref<InputEvent> ev = p_input;

	// Returns true if an event should be impacted by a control's mouse filter.
	bool is_pointer_event = Ref<InputEventMouse>(p_input).is_valid() || Ref<InputEventScreenDrag>(p_input).is_valid() || Ref<InputEventScreenTouch>(p_input).is_valid();

	Ref<InputEventMouseButton> mb = p_input;
	bool is_scroll_event = mb.is_valid() &&
			(mb->get_button_index() == MouseButton::WHEEL_DOWN ||
					mb->get_button_index() == MouseButton::WHEEL_UP ||
					mb->get_button_index() == MouseButton::WHEEL_LEFT ||
					mb->get_button_index() == MouseButton::WHEEL_RIGHT);

	CanvasItem *ci = p_control;
	while (ci) {
		Control *control = Object::cast_to<Control>(ci);
		if (control) {
			if (control->data.mouse_filter != Control::MOUSE_FILTER_IGNORE) {
				control->_call_gui_input(ev);
			}

			if (!control->is_inside_tree() || control->is_set_as_top_level()) {
				break;
			}
			if (control->data.mouse_filter == Control::MOUSE_FILTER_STOP && is_pointer_event && !(is_scroll_event && control->data.force_pass_scroll_events)) {
				// Mouse, ScreenDrag and ScreenTouch events are stopped by default with MOUSE_FILTER_STOP, unless we have a scroll event and force_pass_scroll_events set to true
				set_input_as_handled();
				break;
			}
		}

		if (is_input_handled()) {
			// Break when the event is set to handled in a child Control node or after physics picking in SubViewport.
			break;
		}

		if (ci->is_set_as_top_level()) {
			break;
		}

		ev = ev->xformed_by(ci->get_transform()); // Transform event upwards.
		ci = ci->get_parent_item();
	}
}

void Viewport::_gui_call_notification(Control *p_control, int p_what) {
	CanvasItem *ci = p_control;
	while (ci) {
		Control *control = Object::cast_to<Control>(ci);
		if (control) {
			if (control->data.mouse_filter != Control::MOUSE_FILTER_IGNORE) {
				control->notification(p_what);
			}

			if (!control->is_inside_tree()) {
				break;
			}

			if (!control->is_inside_tree() || control->is_set_as_top_level()) {
				break;
			}
			if (control->data.mouse_filter == Control::MOUSE_FILTER_STOP) {
				break;
			}
		}

		if (ci->is_set_as_top_level()) {
			break;
		}

		ci = ci->get_parent_item();
	}
}

Control *Viewport::gui_find_control(const Point2 &p_global) {
	ERR_MAIN_THREAD_GUARD_V(nullptr);
	// Handle subwindows.
	_gui_sort_roots();

	for (List<Control *>::Element *E = gui.roots.back(); E; E = E->prev()) {
		Control *sw = E->get();
		if (!sw->is_visible_in_tree()) {
			continue;
		}

		Transform2D xform;
		CanvasItem *pci = sw->get_parent_item();
		if (pci) {
			xform = pci->get_global_transform_with_canvas();
		} else {
			xform = sw->get_canvas_transform();
		}

		Control *ret = _gui_find_control_at_pos(sw, p_global, xform);
		if (ret) {
			return ret;
		}
	}

	return nullptr;
}

Control *Viewport::_gui_find_control_at_pos(CanvasItem *p_node, const Point2 &p_global, const Transform2D &p_xform) {
	if (!p_node->is_visible()) {
		return nullptr; // Canvas item hidden, discard.
	}

	Transform2D matrix = p_xform * p_node->get_transform();
	// matrix.determinant() == 0.0f implies that node does not exist on scene
	if (matrix.determinant() == 0.0f) {
		return nullptr;
	}

	Control *c = Object::cast_to<Control>(p_node);

	if (!c || !c->is_clipping_contents() || c->has_point(matrix.affine_inverse().xform(p_global))) {
		for (int i = p_node->get_child_count() - 1; i >= 0; i--) {
			CanvasItem *ci = Object::cast_to<CanvasItem>(p_node->get_child(i));
			if (!ci || ci->is_set_as_top_level()) {
				continue;
			}

			Control *ret = _gui_find_control_at_pos(ci, p_global, matrix);
			if (ret) {
				return ret;
			}
		}
	}

	if (!c || c->data.mouse_filter == Control::MOUSE_FILTER_IGNORE) {
		return nullptr;
	}

	matrix.affine_invert();
	if (!c->has_point(matrix.xform(p_global))) {
		return nullptr;
	}

	Control *drag_preview = _gui_get_drag_preview();
	if (!drag_preview || (c != drag_preview && !drag_preview->is_ancestor_of(c))) {
		return c;
	}

	return nullptr;
}

bool Viewport::_gui_drop(Control *p_at_control, Point2 p_at_pos, bool p_just_check) {
	// Attempt drop, try parent controls too.
	CanvasItem *ci = p_at_control;
	Viewport *section_root = get_section_root_viewport();
	while (ci) {
		Control *control = Object::cast_to<Control>(ci);
		if (control) {
			if (control->can_drop_data(p_at_pos, section_root->gui.drag_data)) {
				if (!p_just_check) {
					control->drop_data(p_at_pos, section_root->gui.drag_data);
				}

				return true;
			}

			if (control->data.mouse_filter == Control::MOUSE_FILTER_STOP) {
				break;
			}
		}

		p_at_pos = ci->get_transform().xform(p_at_pos);

		if (ci->is_set_as_top_level()) {
			break;
		}

		ci = ci->get_parent_item();
	}

	return false;
}

void Viewport::_gui_input_event(Ref<InputEvent> p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		Point2 mpos = mb->get_position();
		if (mb->is_pressed()) {
			if (gui.dragging && mb->get_button_index() == MouseButton::RIGHT) {
				_perform_drop();
				set_input_as_handled();
				return;
			}
			MouseButtonMask button_mask = mouse_button_to_mask(mb->get_button_index());
			if (!gui.mouse_focus_mask.is_empty() && !gui.mouse_focus_mask.has_flag(button_mask)) {
				// Do not steal mouse focus and stuff while a focus mask without the current mouse button exists.
				gui.mouse_focus_mask.set_flag(button_mask);
			} else {
				gui.mouse_focus = gui_find_control(mpos);

				if (!gui.mouse_focus) {
					return;
				}

				gui.mouse_focus_mask.set_flag(mouse_button_to_mask(mb->get_button_index()));

				if (mb->get_button_index() == MouseButton::LEFT) {
					gui.drag_accum = Vector2();
					gui.drag_attempted = false;
				}
			}
			DEV_ASSERT(gui.mouse_focus);

			mb = mb->xformed_by(Transform2D()); // Make a copy of the event.

			Point2 pos = gui.mouse_focus->get_global_transform_with_canvas().affine_inverse().xform(mpos);
			mb->set_position(pos);

#ifdef DEBUG_ENABLED
			if (EngineDebugger::get_singleton()) {
				Array arr;
				arr.push_back(gui.mouse_focus->get_path());
				arr.push_back(gui.mouse_focus->get_class());
				EngineDebugger::get_singleton()->send_message("scene:click_ctrl", arr);
			}
#endif

			if (mb->get_button_index() == MouseButton::LEFT) { // Assign focus.
				CanvasItem *ci = gui.mouse_focus;
				while (ci) {
					Control *control = Object::cast_to<Control>(ci);
					if (control) {
						if (control->get_focus_mode() != Control::FOCUS_NONE) {
							// Grabbing unhovered focus can cause issues when mouse is dragged
							// with another button held down.
							if (control != gui.key_focus && gui.mouse_over_hierarchy.has(control)) {
								control->grab_focus();
							}
							break;
						}

						if (control->data.mouse_filter == Control::MOUSE_FILTER_STOP) {
							break;
						}
					}

					if (ci->is_set_as_top_level()) {
						break;
					}

					ci = ci->get_parent_item();
				}
			}

			if (gui.mouse_focus && gui.mouse_focus->can_process()) {
				_gui_call_input(gui.mouse_focus, mb);
			}

			if (gui.dragging && mb->get_button_index() == MouseButton::LEFT) {
				// Alternate drop use (when using force_drag(), as proposed by #5342).
				_perform_drop(gui.mouse_focus);
			}

			_gui_cancel_tooltip();
		} else {
			if (gui.dragging && mb->get_button_index() == MouseButton::LEFT) {
				_perform_drop(gui.drag_mouse_over);
			}

			gui.mouse_focus_mask.clear_flag(mouse_button_to_mask(mb->get_button_index())); // Remove from mask.

			if (!gui.mouse_focus) {
				// Release event is only sent if a mouse focus (previously pressed button) exists.
				return;
			}

			mb = mb->xformed_by(Transform2D()); // Make a copy.
			Point2 pos = gui.mouse_focus->get_global_transform_with_canvas().affine_inverse().xform(mpos);
			mb->set_position(pos);

			Control *mouse_focus = gui.mouse_focus;

			// Disable mouse focus if needed before calling input,
			// this makes popups on mouse press event work better,
			// as the release will never be received otherwise.
			if (gui.mouse_focus_mask.is_empty()) {
				gui.mouse_focus = nullptr;
			}

			if (mouse_focus && mouse_focus->can_process()) {
				_gui_call_input(mouse_focus, mb);
			}
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		Point2 mpos = mm->get_position();

		// Drag & drop.
		Viewport *section_root = get_section_root_viewport();
		if (!gui.drag_attempted && gui.mouse_focus && section_root && !section_root->gui.global_dragging && (mm->get_button_mask().has_flag(MouseButtonMask::LEFT))) {
			gui.drag_accum += mm->get_relative();
			float len = gui.drag_accum.length();
			if (len > 10) {
				{ // Attempt grab, try parent controls too.
					CanvasItem *ci = gui.mouse_focus;
					while (ci) {
						Control *control = Object::cast_to<Control>(ci);
						if (control) {
							section_root->gui.global_dragging = true;
							section_root->gui.drag_data = control->get_drag_data(control->get_global_transform_with_canvas().affine_inverse().xform(mpos - gui.drag_accum));
							if (section_root->gui.drag_data.get_type() != Variant::NIL) {
								gui.mouse_focus = nullptr;
								gui.mouse_focus_mask.clear();
								gui.dragging = true;
								break;
							} else {
								Control *drag_preview = _gui_get_drag_preview();
								if (drag_preview) {
									ERR_PRINT("Don't set a drag preview and return null data. Preview was deleted and drag request ignored.");
									memdelete(drag_preview);
									gui.drag_preview_id = ObjectID();
								}
								section_root->gui.global_dragging = false;
							}

							if (control->data.mouse_filter == Control::MOUSE_FILTER_STOP) {
								break;
							}
						}

						if (ci->is_set_as_top_level()) {
							break;
						}

						ci = ci->get_parent_item();
					}
				}

				gui.drag_attempted = true;
				if (gui.dragging) {
					Viewport::_propagate_drag_notification(section_root, NOTIFICATION_DRAG_BEGIN);
				}
			}
		}

		Control *over = nullptr;
		if (gui.mouse_focus) {
			over = gui.mouse_focus;
		} else if (gui.mouse_in_viewport) {
			over = gui_find_control(mpos);
		}

		DisplayServer::CursorShape ds_cursor_shape = (DisplayServer::CursorShape)Input::get_singleton()->get_default_cursor_shape();

		if (over) {
			Transform2D localizer = over->get_global_transform_with_canvas().affine_inverse();
			Size2 pos = localizer.xform(mpos);
			Vector2 velocity = localizer.basis_xform(mm->get_velocity());
			Vector2 rel = localizer.basis_xform(mm->get_relative());

			mm = mm->xformed_by(Transform2D()); // Make a copy.

			mm->set_global_position(mpos);
			mm->set_velocity(velocity);
			mm->set_relative(rel);

			// Nothing pressed.
			if (mm->get_button_mask().is_empty()) {
				bool is_tooltip_shown = false;

				if (gui.tooltip_popup) {
					if (gui.tooltip_control) {
						String tooltip = _gui_get_tooltip(over, gui.tooltip_control->get_global_transform_with_canvas().affine_inverse().xform(mpos));
						tooltip = tooltip.strip_edges();

						if (tooltip != gui.tooltip_text) {
							_gui_cancel_tooltip();
						} else {
							is_tooltip_shown = true;
						}
					} else {
						_gui_cancel_tooltip();
					}
				}

				// Reset the timer if the mouse has moved more than 5 pixels or has entered a new control.
				if (!is_tooltip_shown && over->can_process()) {
					Vector2 new_tooltip_pos = over->get_screen_transform().xform(pos);
					if (over != gui.tooltip_control || gui.tooltip_pos.distance_squared_to(new_tooltip_pos) > 25) {
						if (gui.tooltip_timer.is_valid()) {
							gui.tooltip_timer->release_connections();
						}
						gui.tooltip_control = over;
						gui.tooltip_pos = new_tooltip_pos;
						gui.tooltip_timer = get_tree()->create_timer(gui.tooltip_delay);
						gui.tooltip_timer->set_ignore_time_scale(true);
						gui.tooltip_timer->connect("timeout", callable_mp(this, &Viewport::_gui_show_tooltip));
					}
				}
			}

			mm->set_position(pos);

			Control::CursorShape cursor_shape = Control::CURSOR_ARROW;
			{
				Control *c = over;
				Vector2 cpos = pos;
				while (c) {
					if (!gui.mouse_focus_mask.is_empty() || c->has_point(cpos)) {
						cursor_shape = c->get_cursor_shape(cpos);
					} else {
						cursor_shape = Control::CURSOR_ARROW;
					}
					cpos = c->get_transform().xform(cpos);
					if (cursor_shape != Control::CURSOR_ARROW) {
						break;
					}
					if (c->data.mouse_filter == Control::MOUSE_FILTER_STOP) {
						break;
					}
					if (c->is_set_as_top_level()) {
						break;
					}
					c = c->get_parent_control();
				}
			}

			ds_cursor_shape = (DisplayServer::CursorShape)cursor_shape;

			if (over->can_process()) {
				_gui_call_input(over, mm);
			}
		}

		if (gui.dragging) {
			// Handle drag & drop. This happens in the viewport where dragging started.

			Control *drag_preview = _gui_get_drag_preview();
			if (drag_preview) {
				Vector2 pos = drag_preview->get_canvas_transform().affine_inverse().xform(mpos);
				drag_preview->set_position(pos);
			}

			gui.drag_mouse_over = section_root->gui.target_control;
			if (gui.drag_mouse_over) {
				if (!_gui_drop(gui.drag_mouse_over, gui.drag_mouse_over->get_local_mouse_position(), true)) {
					gui.drag_mouse_over = nullptr;
				}
				if (gui.drag_mouse_over) {
					ds_cursor_shape = DisplayServer::CURSOR_CAN_DROP;
				} else {
					ds_cursor_shape = DisplayServer::CURSOR_FORBIDDEN;
				}
			}
		}

		if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_CURSOR_SHAPE) && (gui.dragging || (!section_root->gui.global_dragging && !Object::cast_to<SubViewportContainer>(over)))) {
			// If dragging is active, then set the cursor shape only from the Viewport where dragging started.
			// If dragging is inactive, then set the cursor shape only when not over a SubViewportContainer.
			DisplayServer::get_singleton()->cursor_set_shape(ds_cursor_shape);
		}
	}

	Ref<InputEventScreenTouch> touch_event = p_event;
	if (touch_event.is_valid()) {
		Size2 pos = touch_event->get_position();
		const int touch_index = touch_event->get_index();
		if (touch_event->is_pressed()) {
			Control *over = gui_find_control(pos);
			if (over) {
				gui.touch_focus[touch_index] = over->get_instance_id();
				if (over->can_process()) {
					touch_event = touch_event->xformed_by(Transform2D()); // Make a copy.
					pos = over->get_global_transform_with_canvas().affine_inverse().xform(pos);
					touch_event->set_position(pos);
					_gui_call_input(over, touch_event);
				}
				return;
			}
		} else {
			ObjectID control_id = gui.touch_focus[touch_index];
			Control *over = control_id.is_valid() ? Object::cast_to<Control>(ObjectDB::get_instance(control_id)) : nullptr;
			if (over && over->can_process()) {
				touch_event = touch_event->xformed_by(Transform2D()); // Make a copy.
				pos = over->get_global_transform_with_canvas().affine_inverse().xform(pos);
				touch_event->set_position(pos);

				_gui_call_input(over, touch_event);
			}
			gui.touch_focus.erase(touch_index);
			return;
		}
	}

	Ref<InputEventGesture> gesture_event = p_event;
	if (gesture_event.is_valid()) {
		_gui_cancel_tooltip();

		Size2 pos = gesture_event->get_position();

		Control *over = gui_find_control(pos);
		if (over) {
			if (over->can_process()) {
				gesture_event = gesture_event->xformed_by(Transform2D()); // Make a copy.
				pos = over->get_global_transform_with_canvas().affine_inverse().xform(pos);
				gesture_event->set_position(pos);
				_gui_call_input(over, gesture_event);
			}
			return;
		}
	}

	Ref<InputEventScreenDrag> drag_event = p_event;
	if (drag_event.is_valid()) {
		const int drag_event_index = drag_event->get_index();
		ObjectID control_id = gui.touch_focus[drag_event_index];
		Control *over = control_id.is_valid() ? Object::cast_to<Control>(ObjectDB::get_instance(control_id)) : nullptr;
		if (!over) {
			over = gui_find_control(drag_event->get_position());
		}
		if (over) {
			if (over->can_process()) {
				Transform2D localizer = over->get_global_transform_with_canvas().affine_inverse();
				Size2 pos = localizer.xform(drag_event->get_position());
				Vector2 velocity = localizer.basis_xform(drag_event->get_velocity());
				Vector2 rel = localizer.basis_xform(drag_event->get_relative());

				drag_event = drag_event->xformed_by(Transform2D()); // Make a copy.

				drag_event->set_velocity(velocity);
				drag_event->set_relative(rel);
				drag_event->set_position(pos);

				_gui_call_input(over, drag_event);
			}

			return;
		}
	}

	if (mm.is_null() && mb.is_null() && p_event->is_action_type()) {
		if (gui.dragging && p_event->is_action_pressed(SNAME("ui_cancel")) && Input::get_singleton()->is_action_just_pressed(SNAME("ui_cancel"))) {
			_perform_drop();
			set_input_as_handled();
			return;
		}

		if (p_event->is_action_pressed(SNAME("ui_cancel"))) {
			// Cancel tooltip timer or hide tooltip when pressing Escape (this is standard behavior in most applications).
			_gui_cancel_tooltip();
			if (gui.tooltip_popup) {
				// If a tooltip was hidden, prevent other actions associated with `ui_cancel` from occurring.
				// For instance, this prevents the node from being deselected when pressing Escape
				// to hide a documentation tooltip in the inspector.
				set_input_as_handled();
				return;
			}
		}

		if (gui.key_focus && !gui.key_focus->is_visible_in_tree()) {
			gui.key_focus->release_focus();
		}

		if (gui.key_focus) {
			if (gui.key_focus->can_process()) {
				gui.key_focus->_call_gui_input(p_event);
			}

			if (is_input_handled()) {
				return;
			}
		}

		Control *from = gui.key_focus ? gui.key_focus : nullptr;

		if (from && p_event->is_pressed()) {
			Control *next = nullptr;

			Ref<InputEventJoypadMotion> joypadmotion_event = p_event;
			if (joypadmotion_event.is_valid()) {
				Input *input = Input::get_singleton();

				if (p_event->is_action_pressed(SNAME("ui_focus_next")) && input->is_action_just_pressed(SNAME("ui_focus_next"))) {
					next = from->find_next_valid_focus();
				}

				if (p_event->is_action_pressed(SNAME("ui_focus_prev")) && input->is_action_just_pressed(SNAME("ui_focus_prev"))) {
					next = from->find_prev_valid_focus();
				}

				if (p_event->is_action_pressed(SNAME("ui_up")) && input->is_action_just_pressed(SNAME("ui_up"))) {
					next = from->_get_focus_neighbor(SIDE_TOP);
				}

				if (p_event->is_action_pressed(SNAME("ui_left")) && input->is_action_just_pressed(SNAME("ui_left"))) {
					next = from->_get_focus_neighbor(SIDE_LEFT);
				}

				if (p_event->is_action_pressed(SNAME("ui_right")) && input->is_action_just_pressed(SNAME("ui_right"))) {
					next = from->_get_focus_neighbor(SIDE_RIGHT);
				}

				if (p_event->is_action_pressed(SNAME("ui_down")) && input->is_action_just_pressed(SNAME("ui_down"))) {
					next = from->_get_focus_neighbor(SIDE_BOTTOM);
				}
			} else {
				if (p_event->is_action_pressed(SNAME("ui_focus_next"), true, true)) {
					next = from->find_next_valid_focus();
				}

				if (p_event->is_action_pressed(SNAME("ui_focus_prev"), true, true)) {
					next = from->find_prev_valid_focus();
				}

				if (p_event->is_action_pressed(SNAME("ui_up"), true, true)) {
					next = from->_get_focus_neighbor(SIDE_TOP);
				}

				if (p_event->is_action_pressed(SNAME("ui_left"), true, true)) {
					next = from->_get_focus_neighbor(SIDE_LEFT);
				}

				if (p_event->is_action_pressed(SNAME("ui_right"), true, true)) {
					next = from->_get_focus_neighbor(SIDE_RIGHT);
				}

				if (p_event->is_action_pressed(SNAME("ui_down"), true, true)) {
					next = from->_get_focus_neighbor(SIDE_BOTTOM);
				}
			}
			if (next) {
				next->grab_focus();
				set_input_as_handled();
			}
		}
	}
}

void Viewport::_perform_drop(Control *p_control) {
	// Without any arguments, simply cancel Drag and Drop.
	if (p_control) {
		gui.drag_successful = _gui_drop(p_control, p_control->get_local_mouse_position(), false);
	} else {
		gui.drag_successful = false;
	}

	Control *drag_preview = _gui_get_drag_preview();
	if (drag_preview) {
		memdelete(drag_preview);
		gui.drag_preview_id = ObjectID();
	}

	Viewport *section_root = get_section_root_viewport();
	section_root->gui.drag_data = Variant();
	gui.dragging = false;
	section_root->gui.global_dragging = false;
	gui.drag_mouse_over = nullptr;
	Viewport::_propagate_drag_notification(section_root, NOTIFICATION_DRAG_END);
	// Display the new cursor shape instantly.
	update_mouse_cursor_state();
}

void Viewport::_gui_cleanup_internal_state(Ref<InputEvent> p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		if (!mb->is_pressed()) {
			gui.mouse_focus_mask.clear_flag(mouse_button_to_mask(mb->get_button_index())); // Remove from mask.
		}
	}
}

List<Control *>::Element *Viewport::_gui_add_root_control(Control *p_control) {
	gui.roots_order_dirty = true;
	return gui.roots.push_back(p_control);
}

void Viewport::gui_set_root_order_dirty() {
	ERR_MAIN_THREAD_GUARD;
	gui.roots_order_dirty = true;
}

void Viewport::_gui_force_drag(Control *p_base, const Variant &p_data, Control *p_control) {
	ERR_FAIL_COND_MSG(p_data.get_type() == Variant::NIL, "Drag data must be a value.");

	gui.dragging = true;
	Viewport *section_root = get_section_root_viewport();
	section_root->gui.global_dragging = true;
	section_root->gui.drag_data = p_data;
	gui.mouse_focus = nullptr;
	gui.mouse_focus_mask.clear();

	if (p_control) {
		_gui_set_drag_preview(p_base, p_control);
	}
	Viewport::_propagate_drag_notification(section_root, NOTIFICATION_DRAG_BEGIN);
}

void Viewport::_gui_set_drag_preview(Control *p_base, Control *p_control) {
	ERR_FAIL_NULL(p_control);
	ERR_FAIL_COND(p_control->is_inside_tree());
	ERR_FAIL_COND(p_control->get_parent() != nullptr);

	Control *drag_preview = _gui_get_drag_preview();
	if (drag_preview) {
		memdelete(drag_preview);
	}
	p_control->set_as_top_level(true);
	p_control->set_position(gui.last_mouse_pos);
	p_base->get_root_parent_control()->add_child(p_control); // Add as child of viewport.
	p_control->move_to_front();

	gui.drag_preview_id = p_control->get_instance_id();
}

Control *Viewport::_gui_get_drag_preview() {
	if (gui.drag_preview_id.is_null()) {
		return nullptr;
	} else {
		Control *drag_preview = Object::cast_to<Control>(ObjectDB::get_instance(gui.drag_preview_id));
		if (!drag_preview) {
			ERR_PRINT("Don't free the control set as drag preview.");
			gui.drag_preview_id = ObjectID();
		}
		return drag_preview;
	}
}

void Viewport::_gui_remove_root_control(List<Control *>::Element *RI) {
	gui.roots.erase(RI);
}

void Viewport::_gui_unfocus_control(Control *p_control) {
	if (gui.key_focus == p_control) {
		gui.key_focus->release_focus();
	}
}

void Viewport::_gui_hide_control(Control *p_control) {
	if (gui.mouse_focus == p_control) {
		_drop_mouse_focus();
	}

	if (gui.key_focus == p_control) {
		gui_release_focus();
	}
	if (gui.mouse_over == p_control || gui.mouse_over_hierarchy.has(p_control)) {
		_drop_mouse_over(p_control->get_parent_control());
	}
	if (gui.drag_mouse_over == p_control) {
		gui.drag_mouse_over = nullptr;
	}
	if (gui.tooltip_control == p_control) {
		_gui_cancel_tooltip();
	}
}

void Viewport::_gui_remove_control(Control *p_control) {
	if (gui.mouse_focus == p_control) {
		gui.mouse_focus = nullptr;
		gui.mouse_focus_mask.clear();
	}
	if (gui.key_focus == p_control) {
		gui.key_focus = nullptr;
	}
	if (gui.mouse_over == p_control || gui.mouse_over_hierarchy.has(p_control)) {
		_drop_mouse_over(p_control->get_parent_control());
	}
	if (gui.drag_mouse_over == p_control) {
		gui.drag_mouse_over = nullptr;
	}
	if (gui.tooltip_control == p_control) {
		gui.tooltip_control = nullptr;
	}
}

void Viewport::canvas_item_top_level_changed() {
	_gui_update_mouse_over();
}

void Viewport::_gui_update_mouse_over() {
	if (gui.mouse_over == nullptr || gui.mouse_over_hierarchy.is_empty()) {
		return;
	}

	if (gui.sending_mouse_enter_exit_notifications) {
		// If notifications are already being sent, delay call to next frame.
		if (get_tree() && !get_tree()->is_connected(SNAME("process_frame"), callable_mp(this, &Viewport::_gui_update_mouse_over))) {
			get_tree()->connect(SNAME("process_frame"), callable_mp(this, &Viewport::_gui_update_mouse_over), CONNECT_ONE_SHOT);
		}
		return;
	}

	// Rebuild the mouse over hierarchy.
	LocalVector<Control *> new_mouse_over_hierarchy;
	LocalVector<Control *> needs_enter;
	LocalVector<int> needs_exit;

	CanvasItem *ancestor = gui.mouse_over;
	bool removing = false;
	bool reached_top = false;
	while (ancestor) {
		Control *ancestor_control = Object::cast_to<Control>(ancestor);
		if (ancestor_control) {
			int found = gui.mouse_over_hierarchy.find(ancestor_control);
			if (found >= 0) {
				// Remove the node if the propagation chain has been broken or it is now MOUSE_FILTER_IGNORE.
				if (removing || ancestor_control->get_mouse_filter() == Control::MOUSE_FILTER_IGNORE) {
					needs_exit.push_back(found);
				}
			}
			if (found == 0) {
				if (removing) {
					// Stop if the chain has been broken and the top of the hierarchy has been reached.
					break;
				}
				reached_top = true;
			}
			if (!removing && ancestor_control->get_mouse_filter() != Control::MOUSE_FILTER_IGNORE) {
				new_mouse_over_hierarchy.push_back(ancestor_control);
				// Add the node if it was not found and it is now not MOUSE_FILTER_IGNORE.
				if (found < 0) {
					needs_enter.push_back(ancestor_control);
				}
			}
			if (ancestor_control->get_mouse_filter() == Control::MOUSE_FILTER_STOP) {
				// MOUSE_FILTER_STOP breaks the propagation chain.
				if (reached_top) {
					break;
				}
				removing = true;
			}
		}
		if (ancestor->is_set_as_top_level()) {
			// Top level breaks the propagation chain.
			if (reached_top) {
				break;
			} else {
				removing = true;
				ancestor = Object::cast_to<CanvasItem>(ancestor->get_parent());
				continue;
			}
		}
		ancestor = ancestor->get_parent_item();
	}
	if (needs_exit.is_empty() && needs_enter.is_empty()) {
		return;
	}

	gui.sending_mouse_enter_exit_notifications = true;

	// Send Mouse Exit Self notification.
	if (gui.mouse_over && !needs_exit.is_empty() && needs_exit[0] == (int)gui.mouse_over_hierarchy.size() - 1) {
		gui.mouse_over->notification(Control::NOTIFICATION_MOUSE_EXIT_SELF);
		gui.mouse_over = nullptr;
	}

	// Send Mouse Exit notifications.
	for (int exit_control_index : needs_exit) {
		gui.mouse_over_hierarchy[exit_control_index]->notification(Control::NOTIFICATION_MOUSE_EXIT);
	}

	// Update the mouse over hierarchy.
	gui.mouse_over_hierarchy.resize(new_mouse_over_hierarchy.size());
	for (int i = 0; i < (int)new_mouse_over_hierarchy.size(); i++) {
		gui.mouse_over_hierarchy[i] = new_mouse_over_hierarchy[new_mouse_over_hierarchy.size() - 1 - i];
	}

	// Send Mouse Enter notifications.
	for (int i = needs_enter.size() - 1; i >= 0; i--) {
		needs_enter[i]->notification(Control::NOTIFICATION_MOUSE_ENTER);
	}

	gui.sending_mouse_enter_exit_notifications = false;
}

Window *Viewport::get_base_window() {
	ERR_READ_THREAD_GUARD_V(nullptr);
	ERR_FAIL_COND_V(!is_inside_tree(), nullptr);

	Viewport *v = this;
	Window *w = Object::cast_to<Window>(v);
	while (!w) {
		v = v->get_parent_viewport();
		w = Object::cast_to<Window>(v);
	}

	return w;
}

void Viewport::_gui_remove_focus_for_window(Node *p_window) {
	if (get_base_window() == p_window) {
		gui_release_focus();
	}
}

bool Viewport::_gui_control_has_focus(const Control *p_control) {
	return gui.key_focus == p_control;
}

void Viewport::_gui_control_grab_focus(Control *p_control) {
	if (gui.key_focus && gui.key_focus == p_control) {
		// No need for change.
		return;
	}
	get_tree()->call_group("_viewports", "_gui_remove_focus_for_window", get_base_window());
	if (p_control->is_inside_tree() && p_control->get_viewport() == this) {
		gui.key_focus = p_control;
		emit_signal(SNAME("gui_focus_changed"), p_control);
		p_control->notification(Control::NOTIFICATION_FOCUS_ENTER);
		p_control->queue_redraw();
	}
}

void Viewport::_gui_accept_event() {
	if (is_inside_tree()) {
		set_input_as_handled();
	}
}

void Viewport::_drop_mouse_focus() {
	Control *c = gui.mouse_focus;
	BitField<MouseButtonMask> mask = gui.mouse_focus_mask;
	gui.mouse_focus = nullptr;
	gui.mouse_focus_mask.clear();

	if (!c) {
		return;
	}

	for (int i = 0; i < 3; i++) {
		if ((int)mask & (1 << i)) {
			Ref<InputEventMouseButton> mb;
			mb.instantiate();
			mb->set_position(c->get_local_mouse_position());
			mb->set_global_position(c->get_local_mouse_position());
			mb->set_button_index(MouseButton(i + 1));
			mb->set_pressed(false);
			mb->set_device(InputEvent::DEVICE_ID_INTERNAL);
			c->_call_gui_input(mb);
		}
	}
}

void Viewport::_drop_physics_mouseover(bool p_paused_only) {
	_cleanup_mouseover_colliders(true, p_paused_only);

#ifndef _3D_DISABLED
	if (physics_object_over.is_valid()) {
		CollisionObject3D *co = Object::cast_to<CollisionObject3D>(ObjectDB::get_instance(physics_object_over));
		if (co) {
			if (!co->is_inside_tree()) {
				physics_object_over = ObjectID();
				physics_object_capture = ObjectID();
			} else if (!(p_paused_only && co->can_process())) {
				co->_mouse_exit();
				physics_object_over = ObjectID();
				physics_object_capture = ObjectID();
			}
		}
	}
#endif // _3D_DISABLED
}

void Viewport::_gui_grab_click_focus(Control *p_control) {
	gui.mouse_click_grabber = p_control;
	callable_mp(this, &Viewport::_post_gui_grab_click_focus).call_deferred();
}

void Viewport::_post_gui_grab_click_focus() {
	Control *focus_grabber = gui.mouse_click_grabber;
	if (!focus_grabber) {
		// Redundant grab requests were made.
		return;
	}
	gui.mouse_click_grabber = nullptr;

	if (gui.mouse_focus) {
		if (gui.mouse_focus == focus_grabber) {
			return;
		}

		BitField<MouseButtonMask> mask = gui.mouse_focus_mask;
		Point2 click = gui.mouse_focus->get_global_transform_with_canvas().affine_inverse().xform(gui.last_mouse_pos);

		for (int i = 0; i < 3; i++) {
			if ((int)mask & (1 << i)) {
				Ref<InputEventMouseButton> mb;
				mb.instantiate();

				// Send unclick.

				mb->set_position(click);
				mb->set_button_index(MouseButton(i + 1));
				mb->set_pressed(false);
				mb->set_device(InputEvent::DEVICE_ID_INTERNAL);
				gui.mouse_focus->_call_gui_input(mb);
			}
		}

		gui.mouse_focus = focus_grabber;
		click = gui.mouse_focus->get_global_transform_with_canvas().affine_inverse().xform(gui.last_mouse_pos);

		for (int i = 0; i < 3; i++) {
			if ((int)mask & (1 << i)) {
				Ref<InputEventMouseButton> mb;
				mb.instantiate();

				// Send click.

				mb->set_position(click);
				mb->set_button_index(MouseButton(i + 1));
				mb->set_pressed(true);
				mb->set_device(InputEvent::DEVICE_ID_INTERNAL);
				callable_mp(gui.mouse_focus, &Control::_call_gui_input).call_deferred(mb);
			}
		}
	}
}

///////////////////////////////

void Viewport::push_text_input(const String &p_text) {
	ERR_MAIN_THREAD_GUARD;
	if (gui.subwindow_focused) {
		gui.subwindow_focused->push_text_input(p_text);
		return;
	}

	if (gui.key_focus) {
		gui.key_focus->call("set_text", p_text);
	}
}

Viewport::SubWindowResize Viewport::_sub_window_get_resize_margin(Window *p_subwindow, const Point2 &p_point) {
	if (p_subwindow->get_flag(Window::FLAG_BORDERLESS) || p_subwindow->get_flag(Window::FLAG_RESIZE_DISABLED)) {
		return SUB_WINDOW_RESIZE_DISABLED;
	}

	Rect2i r = Rect2i(p_subwindow->get_position(), p_subwindow->get_size());

	int title_height = p_subwindow->theme_cache.title_height;

	r.position.y -= title_height;
	r.size.y += title_height;

	if (r.has_point(p_point)) {
		return SUB_WINDOW_RESIZE_DISABLED; // It's inside, so no resize.
	}

	int dist_x = p_point.x < r.position.x ? (p_point.x - r.position.x) : (p_point.x > (r.position.x + r.size.x) ? (p_point.x - (r.position.x + r.size.x)) : 0);
	int dist_y = p_point.y < r.position.y ? (p_point.y - r.position.y) : (p_point.y > (r.position.y + r.size.y) ? (p_point.y - (r.position.y + r.size.y)) : 0);

	int limit = p_subwindow->theme_cache.resize_margin;

	if (ABS(dist_x) > limit) {
		return SUB_WINDOW_RESIZE_DISABLED;
	}

	if (ABS(dist_y) > limit) {
		return SUB_WINDOW_RESIZE_DISABLED;
	}

	if (dist_x < 0 && dist_y < 0) {
		return SUB_WINDOW_RESIZE_TOP_LEFT;
	}

	if (dist_x == 0 && dist_y < 0) {
		return SUB_WINDOW_RESIZE_TOP;
	}

	if (dist_x > 0 && dist_y < 0) {
		return SUB_WINDOW_RESIZE_TOP_RIGHT;
	}

	if (dist_x < 0 && dist_y == 0) {
		return SUB_WINDOW_RESIZE_LEFT;
	}

	if (dist_x > 0 && dist_y == 0) {
		return SUB_WINDOW_RESIZE_RIGHT;
	}

	if (dist_x < 0 && dist_y > 0) {
		return SUB_WINDOW_RESIZE_BOTTOM_LEFT;
	}

	if (dist_x == 0 && dist_y > 0) {
		return SUB_WINDOW_RESIZE_BOTTOM;
	}

	if (dist_x > 0 && dist_y > 0) {
		return SUB_WINDOW_RESIZE_BOTTOM_RIGHT;
	}

	return SUB_WINDOW_RESIZE_DISABLED;
}

bool Viewport::_sub_windows_forward_input(const Ref<InputEvent> &p_event) {
	if (gui.subwindow_drag != SUB_WINDOW_DRAG_DISABLED) {
		ERR_FAIL_NULL_V(gui.currently_dragged_subwindow, false);

		Ref<InputEventMouseButton> mb = p_event;
		if (mb.is_valid() && !mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
			if (gui.subwindow_drag == SUB_WINDOW_DRAG_CLOSE) {
				if (gui.subwindow_drag_close_rect.has_point(mb->get_position())) {
					// Close window.
					gui.currently_dragged_subwindow->_event_callback(DisplayServer::WINDOW_EVENT_CLOSE_REQUEST);
				}
			}
			gui.subwindow_drag = SUB_WINDOW_DRAG_DISABLED;
			if (gui.currently_dragged_subwindow != nullptr) { // May have been erased.
				_sub_window_update(gui.currently_dragged_subwindow);
				gui.currently_dragged_subwindow = nullptr;
			}
		}

		Ref<InputEventMouseMotion> mm = p_event;
		if (mm.is_valid()) {
			if (gui.subwindow_drag == SUB_WINDOW_DRAG_MOVE) {
				Vector2 diff = mm->get_position() - gui.subwindow_drag_from;
				Rect2i new_rect(gui.subwindow_drag_pos + diff, gui.currently_dragged_subwindow->get_size());

				if (gui.currently_dragged_subwindow->is_clamped_to_embedder()) {
					new_rect = gui.currently_dragged_subwindow->fit_rect_in_parent(new_rect, get_visible_rect());
				}

				gui.currently_dragged_subwindow->_rect_changed_callback(new_rect);

				if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_CURSOR_SHAPE)) {
					DisplayServer::get_singleton()->cursor_set_shape(DisplayServer::CURSOR_MOVE);
				}
			}
			if (gui.subwindow_drag == SUB_WINDOW_DRAG_CLOSE) {
				gui.subwindow_drag_close_inside = gui.subwindow_drag_close_rect.has_point(mm->get_position());
			}
			if (gui.subwindow_drag == SUB_WINDOW_DRAG_RESIZE) {
				Vector2i diff = mm->get_position() - gui.subwindow_drag_from;
				Size2i min_size = gui.currently_dragged_subwindow->get_min_size();
				Size2i min_size_clamped = gui.currently_dragged_subwindow->get_clamped_minimum_size();

				min_size_clamped = min_size_clamped.maxi(1);

				Rect2i r = gui.subwindow_resize_from_rect;

				Size2i limit = r.size - min_size_clamped;

				switch (gui.subwindow_resize_mode) {
					case SUB_WINDOW_RESIZE_TOP_LEFT: {
						diff.x = MIN(diff.x, limit.x);
						diff.y = MIN(diff.y, limit.y);
						r.position += diff;
						r.size -= diff;
					} break;
					case SUB_WINDOW_RESIZE_TOP: {
						diff.x = 0;
						diff.y = MIN(diff.y, limit.y);
						r.position += diff;
						r.size -= diff;
					} break;
					case SUB_WINDOW_RESIZE_TOP_RIGHT: {
						diff.x = MAX(diff.x, -limit.x);
						diff.y = MIN(diff.y, limit.y);
						r.position.y += diff.y;
						r.size.y -= diff.y;
						r.size.x += diff.x;
					} break;
					case SUB_WINDOW_RESIZE_LEFT: {
						diff.x = MIN(diff.x, limit.x);
						diff.y = 0;
						r.position += diff;
						r.size -= diff;

					} break;
					case SUB_WINDOW_RESIZE_RIGHT: {
						diff.x = MAX(diff.x, -limit.x);
						r.size.x += diff.x;
					} break;
					case SUB_WINDOW_RESIZE_BOTTOM_LEFT: {
						diff.x = MIN(diff.x, limit.x);
						diff.y = MAX(diff.y, -limit.y);
						r.position.x += diff.x;
						r.size.x -= diff.x;
						r.size.y += diff.y;

					} break;
					case SUB_WINDOW_RESIZE_BOTTOM: {
						diff.y = MAX(diff.y, -limit.y);
						r.size.y += diff.y;
					} break;
					case SUB_WINDOW_RESIZE_BOTTOM_RIGHT: {
						diff.x = MAX(diff.x, -limit.x);
						diff.y = MAX(diff.y, -limit.y);
						r.size += diff;

					} break;
					default: {
					}
				}

				Size2i max_size = gui.currently_dragged_subwindow->get_max_size();
				if ((max_size.x > 0 || max_size.y > 0) && (max_size.x >= min_size.x && max_size.y >= min_size.y)) {
					max_size = max_size.maxi(1);

					if (r.size.x > max_size.x) {
						r.size.x = max_size.x;
					}
					if (r.size.y > max_size.y) {
						r.size.y = max_size.y;
					}
				}

				gui.currently_dragged_subwindow->_rect_changed_callback(r);
			}

			if (gui.currently_dragged_subwindow) { // May have been erased.
				_sub_window_update(gui.currently_dragged_subwindow);
			}
		}

		return true; // Handled.
	}
	Ref<InputEventMouseButton> mb = p_event;
	// If the event is a mouse button, we need to check whether another window was clicked.

	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		Window *click_on_window = nullptr;
		for (int i = gui.sub_windows.size() - 1; i >= 0; i--) {
			SubWindow sw = gui.sub_windows.write[i];

			// Clicked inside window?

			Rect2i r = Rect2i(sw.window->get_position(), sw.window->get_size());

			if (!sw.window->get_flag(Window::FLAG_BORDERLESS)) {
				// Check top bar.
				int title_height = sw.window->theme_cache.title_height;
				Rect2i title_bar = r;
				title_bar.position.y -= title_height;
				title_bar.size.y = title_height;

				if (title_bar.size.y > 0 && title_bar.has_point(mb->get_position())) {
					click_on_window = sw.window;

					int close_h_ofs = sw.window->theme_cache.close_h_offset;
					int close_v_ofs = sw.window->theme_cache.close_v_offset;
					Ref<Texture2D> close_icon = sw.window->theme_cache.close;

					Rect2 close_rect;
					close_rect.position = Vector2(r.position.x + r.size.x - close_h_ofs, r.position.y - close_v_ofs);
					close_rect.size = close_icon->get_size();

					if (gui.subwindow_focused != sw.window) {
						// Refocus.
						_sub_window_grab_focus(sw.window);
					}

					if (close_rect.has_point(mb->get_position())) {
						gui.subwindow_drag = SUB_WINDOW_DRAG_CLOSE;
						gui.subwindow_drag_close_inside = true; // Starts inside.
						gui.subwindow_drag_close_rect = close_rect;
					} else {
						gui.subwindow_drag = SUB_WINDOW_DRAG_MOVE;
					}

					gui.subwindow_drag_from = mb->get_position();
					gui.subwindow_drag_pos = sw.window->get_position();

					_sub_window_update(sw.window);
				} else {
					gui.subwindow_resize_mode = _sub_window_get_resize_margin(sw.window, mb->get_position());
					if (gui.subwindow_resize_mode != SUB_WINDOW_RESIZE_DISABLED) {
						if (gui.subwindow_focused != sw.window) {
							// Refocus.
							_sub_window_grab_focus(sw.window);
						}

						gui.subwindow_resize_from_rect = r;
						gui.subwindow_drag_from = mb->get_position();
						gui.subwindow_drag = SUB_WINDOW_DRAG_RESIZE;
						click_on_window = sw.window;
					}
				}
			}
			if (!click_on_window && r.has_point(mb->get_position())) {
				// Clicked, see if it needs to fetch focus.
				if (gui.subwindow_focused != sw.window) {
					// Refocus.
					_sub_window_grab_focus(sw.window);
				}

				click_on_window = sw.window;
			}

			if (click_on_window) {
				break;
			}
		}

		gui.currently_dragged_subwindow = click_on_window;

		if (!click_on_window && gui.subwindow_focused) {
			// No window found and clicked, remove focus.
			_sub_window_grab_focus(nullptr);
		}
	}

	if (gui.subwindow_focused) {
		Ref<InputEventMouseMotion> mm = p_event;
		if (mm.is_valid()) {
			SubWindowResize resize = _sub_window_get_resize_margin(gui.subwindow_focused, mm->get_position());
			if (resize != SUB_WINDOW_RESIZE_DISABLED) {
				DisplayServer::CursorShape shapes[SUB_WINDOW_RESIZE_MAX] = {
					DisplayServer::CURSOR_ARROW,
					DisplayServer::CURSOR_FDIAGSIZE,
					DisplayServer::CURSOR_VSIZE,
					DisplayServer::CURSOR_BDIAGSIZE,
					DisplayServer::CURSOR_HSIZE,
					DisplayServer::CURSOR_HSIZE,
					DisplayServer::CURSOR_BDIAGSIZE,
					DisplayServer::CURSOR_VSIZE,
					DisplayServer::CURSOR_FDIAGSIZE
				};

				if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_CURSOR_SHAPE)) {
					DisplayServer::get_singleton()->cursor_set_shape(shapes[resize]);
				}

				return true; // Reserved for showing the resize cursor.
			}
		}
	}

	if (gui.subwindow_drag != SUB_WINDOW_DRAG_DISABLED) {
		return true; // Dragging, don't pass the event.
	}

	if (!gui.subwindow_focused) {
		return false;
	}

	Transform2D window_ofs;
	window_ofs.set_origin(-gui.subwindow_focused->get_position());

	Ref<InputEvent> ev = p_event->xformed_by(window_ofs);

	gui.subwindow_focused->_window_input(ev);

	return true;
}

void Viewport::_update_mouse_over() {
	// Update gui.mouse_over and gui.subwindow_over in all Viewports.
	// Send necessary mouse_enter/mouse_exit signals and the MOUSE_ENTER/MOUSE_EXIT notifications for every Viewport in the SceneTree.

	if (is_attached_in_viewport()) {
		// Execute this function only, when it is processed by a native Window or a SubViewport, that has no SubViewportContainer as parent.
		return;
	}

	if (get_tree()->get_root()->is_embedding_subwindows() || is_sub_viewport()) {
		// Use embedder logic for calculating mouse position.
		_update_mouse_over(gui.last_mouse_pos);
	} else {
		// Native Window: Use DisplayServer logic for calculating mouse position.
		Window *receiving_window = get_tree()->get_root()->gui.windowmanager_window_over;
		if (!receiving_window) {
			return;
		}

		Vector2 pos = DisplayServer::get_singleton()->mouse_get_position() - receiving_window->get_position();
		pos = receiving_window->get_final_transform().affine_inverse().xform(pos);

		receiving_window->_update_mouse_over(pos);
	}
}

void Viewport::_update_mouse_over(Vector2 p_pos) {
	gui.last_mouse_pos = p_pos; // Necessary, because mouse cursor can be over Viewports that are not reached by the InputEvent.
	// Look for embedded windows at mouse position.
	if (is_embedding_subwindows()) {
		for (int i = gui.sub_windows.size() - 1; i >= 0; i--) {
			Window *sw = gui.sub_windows[i].window;
			Rect2 swrect = Rect2(sw->get_position(), sw->get_size());
			Rect2 swrect_border = swrect;

			if (!sw->get_flag(Window::FLAG_BORDERLESS)) {
				int title_height = sw->theme_cache.title_height;
				int margin = sw->theme_cache.resize_margin;
				swrect_border.position.y -= title_height + margin;
				swrect_border.size.y += title_height + margin * 2;
				swrect_border.position.x -= margin;
				swrect_border.size.x += margin * 2;
			}

			if (swrect_border.has_point(p_pos)) {
				if (gui.mouse_over) {
					_drop_mouse_over();
				} else if (!gui.subwindow_over) {
					_drop_physics_mouseover();
				}
				if (swrect.has_point(p_pos)) {
					if (sw != gui.subwindow_over) {
						if (gui.subwindow_over) {
							gui.subwindow_over->_mouse_leave_viewport();
						}
						gui.subwindow_over = sw;
					}
					if (!sw->is_input_disabled()) {
						sw->_update_mouse_over(sw->get_final_transform().affine_inverse().xform(p_pos - sw->get_position()));
					}
				} else {
					if (gui.subwindow_over) {
						gui.subwindow_over->_mouse_leave_viewport();
						gui.subwindow_over = nullptr;
					}
				}
				return;
			}
		}

		if (gui.subwindow_over) {
			// Take care of moving mouse out of any embedded Window.
			gui.subwindow_over->_mouse_leave_viewport();
			gui.subwindow_over = nullptr;
		}
	}

	// Look for Controls at mouse position.
	Control *over = gui_find_control(p_pos);
	get_section_root_viewport()->gui.target_control = over;
	bool notify_embedded_viewports = false;
	if (over != gui.mouse_over || (!over && !gui.mouse_over_hierarchy.is_empty())) {
		// Find the common ancestor of `gui.mouse_over` and `over`.
		Control *common_ancestor = nullptr;
		LocalVector<Control *> over_ancestors;

		if (over) {
			// Get all ancestors that the mouse is currently over and need an enter signal.
			CanvasItem *ancestor = over;
			while (ancestor) {
				Control *ancestor_control = Object::cast_to<Control>(ancestor);
				if (ancestor_control) {
					if (ancestor_control->get_mouse_filter() != Control::MOUSE_FILTER_IGNORE) {
						int found = gui.mouse_over_hierarchy.find(ancestor_control);
						if (found >= 0) {
							common_ancestor = gui.mouse_over_hierarchy[found];
							break;
						}
						over_ancestors.push_back(ancestor_control);
					}
					if (ancestor_control->get_mouse_filter() == Control::MOUSE_FILTER_STOP) {
						// MOUSE_FILTER_STOP breaks the propagation chain.
						break;
					}
				}
				if (ancestor->is_set_as_top_level()) {
					// Top level breaks the propagation chain.
					break;
				}
				ancestor = ancestor->get_parent_item();
			}
		}

		if (gui.mouse_over || !gui.mouse_over_hierarchy.is_empty()) {
			// Send Mouse Exit Self and Mouse Exit notifications.
			_drop_mouse_over(common_ancestor);
		} else {
			_drop_physics_mouseover();
		}

		if (over) {
			gui.mouse_over = over;
			gui.mouse_over_hierarchy.reserve(gui.mouse_over_hierarchy.size() + over_ancestors.size());

			gui.sending_mouse_enter_exit_notifications = true;

			// Send Mouse Enter notifications to parents first.
			for (int i = over_ancestors.size() - 1; i >= 0; i--) {
				gui.mouse_over_hierarchy.push_back(over_ancestors[i]);
				over_ancestors[i]->notification(Control::NOTIFICATION_MOUSE_ENTER);
			}

			// Send Mouse Enter Self notification.
			if (gui.mouse_over) {
				gui.mouse_over->notification(Control::NOTIFICATION_MOUSE_ENTER_SELF);
			}

			gui.sending_mouse_enter_exit_notifications = false;

			notify_embedded_viewports = true;
		}
	}

	if (over) {
		SubViewportContainer *c = Object::cast_to<SubViewportContainer>(over);
		if (!c) {
			return;
		}
		Vector2 pos = c->get_global_transform_with_canvas().affine_inverse().xform(p_pos);
		if (c->is_stretch_enabled()) {
			pos /= c->get_stretch_shrink();
		}

		for (int i = 0; i < c->get_child_count(); i++) {
			SubViewport *v = Object::cast_to<SubViewport>(c->get_child(i));
			if (!v || v->is_input_disabled()) {
				continue;
			}
			if (notify_embedded_viewports) {
				v->notification(NOTIFICATION_VP_MOUSE_ENTER);
			}
			v->_update_mouse_over(v->get_final_transform().affine_inverse().xform(pos));
		}

		Viewport *section_root = get_section_root_viewport();
		if (section_root && c->is_mouse_target_enabled()) {
			// Evaluating `mouse_target` and adjusting target_control needs to happen
			// after `_update_mouse_over` in the SubViewports, because otherwise physics picking
			// would not work inside SubViewports.
			section_root->gui.target_control = over;
		}
	}
}

void Viewport::_mouse_leave_viewport() {
	if (!is_inside_tree() || is_input_disabled()) {
		return;
	}
	if (gui.subwindow_over) {
		gui.subwindow_over->_mouse_leave_viewport();
		gui.subwindow_over = nullptr;
	} else if (gui.mouse_over) {
		_drop_mouse_over();
	}
	notification(NOTIFICATION_VP_MOUSE_EXIT);
}

void Viewport::_drop_mouse_over(Control *p_until_control) {
	if (gui.sending_mouse_enter_exit_notifications) {
		// If notifications are already being sent, defer call.
		callable_mp(this, &Viewport::_drop_mouse_over).call_deferred(p_until_control);
		return;
	}

	_gui_cancel_tooltip();
	SubViewportContainer *c = Object::cast_to<SubViewportContainer>(gui.mouse_over);
	if (c) {
		for (int i = 0; i < c->get_child_count(); i++) {
			SubViewport *v = Object::cast_to<SubViewport>(c->get_child(i));
			if (!v) {
				continue;
			}
			v->_mouse_leave_viewport();
		}
	}

	gui.sending_mouse_enter_exit_notifications = true;
	if (gui.mouse_over && gui.mouse_over->is_inside_tree()) {
		gui.mouse_over->notification(Control::NOTIFICATION_MOUSE_EXIT_SELF);
	}
	Viewport *section_root = get_section_root_viewport();
	if (section_root && section_root->gui.target_control == gui.mouse_over) {
		section_root->gui.target_control = nullptr;
	}
	gui.mouse_over = nullptr;

	// Send Mouse Exit notifications to children first. Don't send to p_until_control or above.
	int notification_until = p_until_control ? gui.mouse_over_hierarchy.find(p_until_control) + 1 : 0;
	for (int i = gui.mouse_over_hierarchy.size() - 1; i >= notification_until; i--) {
		if (gui.mouse_over_hierarchy[i]->is_inside_tree()) {
			gui.mouse_over_hierarchy[i]->notification(Control::NOTIFICATION_MOUSE_EXIT);
		}
	}
	gui.mouse_over_hierarchy.resize(notification_until);
	gui.sending_mouse_enter_exit_notifications = false;
}

void Viewport::push_input(const Ref<InputEvent> &p_event, bool p_local_coords) {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_COND(!is_inside_tree());
	ERR_FAIL_COND(p_event.is_null());

	if (disable_input || disable_input_override) {
		return;
	}

	if (Engine::get_singleton()->is_editor_hint() && get_tree()->get_edited_scene_root() && get_tree()->get_edited_scene_root()->is_ancestor_of(this)) {
		return;
	}

	local_input_handled = false;
	if (!handle_input_locally) {
		Viewport *vp = this;
		while (true) {
			if (Object::cast_to<Window>(vp) || !vp->get_parent()) {
				break;
			}
			vp = vp->get_parent()->get_viewport();
		}
		vp->local_input_handled = false;
	}

	Ref<InputEvent> ev;
	if (!p_local_coords) {
		ev = _make_input_local(p_event);
	} else {
		ev = p_event;
	}

	Ref<InputEventMouse> me = ev;
	if (me.is_valid()) {
		gui.last_mouse_pos = me->get_position();

		_update_mouse_over();
	}

	if (is_embedding_subwindows() && _sub_windows_forward_input(ev)) {
		set_input_as_handled();
		return;
	}

	if (!_can_consume_input_events()) {
		return;
	}

	if (!is_input_handled()) {
		ERR_FAIL_COND(!is_inside_tree());
		get_tree()->_call_input_pause(input_group, SceneTree::CALL_INPUT_TYPE_INPUT, ev, this); //not a bug, must happen before GUI, order is _input -> gui input -> _unhandled input
	}

	if (!is_input_handled()) {
		ERR_FAIL_COND(!is_inside_tree());
		_gui_input_event(ev);
	} else {
		// Cleanup internal GUI state after accepting event during _input().
		_gui_cleanup_internal_state(ev);
	}

	if (!is_input_handled()) {
		_push_unhandled_input_internal(ev);
	}

	event_count++;
}

#ifndef DISABLE_DEPRECATED
void Viewport::push_unhandled_input(const Ref<InputEvent> &p_event, bool p_local_coords) {
	ERR_MAIN_THREAD_GUARD;
	WARN_DEPRECATED_MSG(R"*(The "push_unhandled_input()" method is deprecated, use "push_input()" instead.)*");
	ERR_FAIL_COND(!is_inside_tree());
	ERR_FAIL_COND(p_event.is_null());

	local_input_handled = false;

	if (disable_input || disable_input_override || !_can_consume_input_events()) {
		return;
	}

	if (Engine::get_singleton()->is_editor_hint() && get_tree()->get_edited_scene_root() && get_tree()->get_edited_scene_root()->is_ancestor_of(this)) {
		return;
	}

	Ref<InputEvent> ev;
	if (!p_local_coords) {
		ev = _make_input_local(p_event);
	} else {
		ev = p_event;
	}

	_push_unhandled_input_internal(ev);
}
#endif // DISABLE_DEPRECATED

void Viewport::_push_unhandled_input_internal(const Ref<InputEvent> &p_event) {
	// Shortcut Input.
	if (Object::cast_to<InputEventKey>(*p_event) != nullptr || Object::cast_to<InputEventShortcut>(*p_event) != nullptr || Object::cast_to<InputEventJoypadButton>(*p_event) != nullptr) {
		ERR_FAIL_COND(!is_inside_tree());
		get_tree()->_call_input_pause(shortcut_input_group, SceneTree::CALL_INPUT_TYPE_SHORTCUT_INPUT, p_event, this);
	}

	// Unhandled key Input - Used for performance reasons - This is called a lot less than _unhandled_input since it ignores MouseMotion, and to handle Unicode input with Alt / Ctrl modifiers after handling shortcuts.
	if (!is_input_handled() && (Object::cast_to<InputEventKey>(*p_event) != nullptr)) {
		ERR_FAIL_COND(!is_inside_tree());
		get_tree()->_call_input_pause(unhandled_key_input_group, SceneTree::CALL_INPUT_TYPE_UNHANDLED_KEY_INPUT, p_event, this);
	}

	// Unhandled Input.
	if (!is_input_handled()) {
		ERR_FAIL_COND(!is_inside_tree());
		get_tree()->_call_input_pause(unhandled_input_group, SceneTree::CALL_INPUT_TYPE_UNHANDLED_INPUT, p_event, this);
	}

	if (physics_object_picking && !is_input_handled()) {
		if (Input::get_singleton()->get_mouse_mode() != Input::MOUSE_MODE_CAPTURED &&
				(Object::cast_to<InputEventMouse>(*p_event) ||
						Object::cast_to<InputEventScreenDrag>(*p_event) ||
						Object::cast_to<InputEventScreenTouch>(*p_event)

								)) {
			physics_picking_events.push_back(p_event);
			set_input_as_handled();
		}
	}
}

void Viewport::set_physics_object_picking(bool p_enable) {
	ERR_MAIN_THREAD_GUARD;
	physics_object_picking = p_enable;
	if (physics_object_picking) {
		add_to_group("_picking_viewports");
	} else {
		physics_picking_events.clear();
		if (is_in_group("_picking_viewports")) {
			remove_from_group("_picking_viewports");
		}
	}
}

bool Viewport::get_physics_object_picking() {
	ERR_READ_THREAD_GUARD_V(false);
	return physics_object_picking;
}

void Viewport::set_physics_object_picking_sort(bool p_enable) {
	ERR_MAIN_THREAD_GUARD;
	physics_object_picking_sort = p_enable;
}

bool Viewport::get_physics_object_picking_sort() {
	ERR_READ_THREAD_GUARD_V(false);
	return physics_object_picking_sort;
}

void Viewport::set_physics_object_picking_first_only(bool p_enable) {
	physics_object_picking_first_only = p_enable;
}

bool Viewport::get_physics_object_picking_first_only() {
	return physics_object_picking_first_only;
}

Vector2 Viewport::get_camera_coords(const Vector2 &p_viewport_coords) const {
	ERR_READ_THREAD_GUARD_V(Vector2());
	Transform2D xf = stretch_transform * global_canvas_transform;
	return xf.xform(p_viewport_coords);
}

Vector2 Viewport::get_camera_rect_size() const {
	ERR_READ_THREAD_GUARD_V(Vector2());
	return size;
}

void Viewport::set_disable_input(bool p_disable) {
	ERR_MAIN_THREAD_GUARD;
	if (p_disable == disable_input) {
		return;
	}
	if (p_disable && !disable_input_override) {
		_drop_mouse_focus();
		_mouse_leave_viewport();
		_gui_cancel_tooltip();
	}
	disable_input = p_disable;
}

bool Viewport::is_input_disabled() const {
	ERR_READ_THREAD_GUARD_V(false);
	return disable_input;
}

void Viewport::set_disable_input_override(bool p_disable) {
	ERR_MAIN_THREAD_GUARD;
	if (p_disable == disable_input_override) {
		return;
	}
	if (p_disable && !disable_input) {
		_drop_mouse_focus();
		_mouse_leave_viewport();
		_gui_cancel_tooltip();
	}
	disable_input_override = p_disable;
}

Variant Viewport::gui_get_drag_data() const {
	ERR_READ_THREAD_GUARD_V(Variant());
	return get_section_root_viewport()->gui.drag_data;
}

PackedStringArray Viewport::get_configuration_warnings() const {
	ERR_MAIN_THREAD_GUARD_V(PackedStringArray());
	PackedStringArray warnings = Node::get_configuration_warnings();

	if (size.x <= 1 || size.y <= 1) {
		warnings.push_back(RTR("The Viewport size must be greater than or equal to 2 pixels on both dimensions to render anything."));
	}
	return warnings;
}

void Viewport::gui_reset_canvas_sort_index() {
	ERR_MAIN_THREAD_GUARD;
	gui.canvas_sort_index = 0;
}

int Viewport::gui_get_canvas_sort_index() {
	ERR_MAIN_THREAD_GUARD_V(0);
	return gui.canvas_sort_index++;
}

void Viewport::gui_release_focus() {
	ERR_MAIN_THREAD_GUARD;
	if (gui.key_focus) {
		Control *f = gui.key_focus;
		gui.key_focus = nullptr;
		f->notification(Control::NOTIFICATION_FOCUS_EXIT, true);
		f->queue_redraw();
	}
}

Control *Viewport::gui_get_focus_owner() const {
	ERR_READ_THREAD_GUARD_V(nullptr);
	return gui.key_focus;
}

Control *Viewport::gui_get_hovered_control() const {
	ERR_READ_THREAD_GUARD_V(nullptr);
	return gui.mouse_over;
}

void Viewport::set_msaa_2d(MSAA p_msaa) {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_INDEX(p_msaa, MSAA_MAX);
	if (msaa_2d == p_msaa) {
		return;
	}
	msaa_2d = p_msaa;
	RS::get_singleton()->viewport_set_msaa_2d(viewport, RS::ViewportMSAA(p_msaa));
}

Viewport::MSAA Viewport::get_msaa_2d() const {
	ERR_READ_THREAD_GUARD_V(MSAA_DISABLED);
	return msaa_2d;
}

void Viewport::set_msaa_3d(MSAA p_msaa) {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_INDEX(p_msaa, MSAA_MAX);
	if (msaa_3d == p_msaa) {
		return;
	}
	msaa_3d = p_msaa;
	RS::get_singleton()->viewport_set_msaa_3d(viewport, RS::ViewportMSAA(p_msaa));
}

Viewport::MSAA Viewport::get_msaa_3d() const {
	ERR_READ_THREAD_GUARD_V(MSAA_DISABLED);
	return msaa_3d;
}

void Viewport::set_screen_space_aa(ScreenSpaceAA p_screen_space_aa) {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_INDEX(p_screen_space_aa, SCREEN_SPACE_AA_MAX);
	if (screen_space_aa == p_screen_space_aa) {
		return;
	}
	screen_space_aa = p_screen_space_aa;
	RS::get_singleton()->viewport_set_screen_space_aa(viewport, RS::ViewportScreenSpaceAA(p_screen_space_aa));
}

Viewport::ScreenSpaceAA Viewport::get_screen_space_aa() const {
	ERR_READ_THREAD_GUARD_V(SCREEN_SPACE_AA_DISABLED);
	return screen_space_aa;
}

void Viewport::set_use_taa(bool p_use_taa) {
	ERR_MAIN_THREAD_GUARD;
	if (use_taa == p_use_taa) {
		return;
	}
	use_taa = p_use_taa;
	RS::get_singleton()->viewport_set_use_taa(viewport, p_use_taa);
}

bool Viewport::is_using_taa() const {
	ERR_READ_THREAD_GUARD_V(false);
	return use_taa;
}

void Viewport::set_use_debanding(bool p_use_debanding) {
	ERR_MAIN_THREAD_GUARD;
	if (use_debanding == p_use_debanding) {
		return;
	}
	use_debanding = p_use_debanding;
	RS::get_singleton()->viewport_set_use_debanding(viewport, p_use_debanding);
}

bool Viewport::is_using_debanding() const {
	ERR_READ_THREAD_GUARD_V(false);
	return use_debanding;
}

void Viewport::set_mesh_lod_threshold(float p_pixels) {
	ERR_MAIN_THREAD_GUARD;
	mesh_lod_threshold = p_pixels;
	RS::get_singleton()->viewport_set_mesh_lod_threshold(viewport, mesh_lod_threshold);
}

float Viewport::get_mesh_lod_threshold() const {
	ERR_READ_THREAD_GUARD_V(0);
	return mesh_lod_threshold;
}

void Viewport::set_use_occlusion_culling(bool p_use_occlusion_culling) {
	ERR_MAIN_THREAD_GUARD;
	if (use_occlusion_culling == p_use_occlusion_culling) {
		return;
	}

	use_occlusion_culling = p_use_occlusion_culling;
	RS::get_singleton()->viewport_set_use_occlusion_culling(viewport, p_use_occlusion_culling);

	notify_property_list_changed();
}

bool Viewport::is_using_occlusion_culling() const {
	ERR_READ_THREAD_GUARD_V(false);
	return use_occlusion_culling;
}

void Viewport::set_debug_draw(DebugDraw p_debug_draw) {
	ERR_MAIN_THREAD_GUARD;
	debug_draw = p_debug_draw;
	RS::get_singleton()->viewport_set_debug_draw(viewport, RS::ViewportDebugDraw(p_debug_draw));
}

Viewport::DebugDraw Viewport::get_debug_draw() const {
	ERR_READ_THREAD_GUARD_V(DEBUG_DRAW_DISABLED);
	return debug_draw;
}

int Viewport::get_render_info(RenderInfoType p_type, RenderInfo p_info) {
	ERR_READ_THREAD_GUARD_V(0);
	return RS::get_singleton()->viewport_get_render_info(viewport, RS::ViewportRenderInfoType(p_type), RS::ViewportRenderInfo(p_info));
}

void Viewport::set_snap_controls_to_pixels(bool p_enable) {
	ERR_MAIN_THREAD_GUARD;
	snap_controls_to_pixels = p_enable;
}

bool Viewport::is_snap_controls_to_pixels_enabled() const {
	ERR_READ_THREAD_GUARD_V(false);
	return snap_controls_to_pixels;
}

void Viewport::set_snap_2d_transforms_to_pixel(bool p_enable) {
	ERR_MAIN_THREAD_GUARD;
	snap_2d_transforms_to_pixel = p_enable;
	RS::get_singleton()->viewport_set_snap_2d_transforms_to_pixel(viewport, snap_2d_transforms_to_pixel);
}

bool Viewport::is_snap_2d_transforms_to_pixel_enabled() const {
	ERR_READ_THREAD_GUARD_V(false);
	return snap_2d_transforms_to_pixel;
}

void Viewport::set_snap_2d_vertices_to_pixel(bool p_enable) {
	ERR_MAIN_THREAD_GUARD;
	snap_2d_vertices_to_pixel = p_enable;
	RS::get_singleton()->viewport_set_snap_2d_vertices_to_pixel(viewport, snap_2d_vertices_to_pixel);
}

bool Viewport::is_snap_2d_vertices_to_pixel_enabled() const {
	ERR_READ_THREAD_GUARD_V(false);
	return snap_2d_vertices_to_pixel;
}

bool Viewport::gui_is_dragging() const {
	ERR_READ_THREAD_GUARD_V(false);
	return get_section_root_viewport()->gui.global_dragging;
}

bool Viewport::gui_is_drag_successful() const {
	ERR_READ_THREAD_GUARD_V(false);
	return gui.drag_successful;
}

void Viewport::gui_cancel_drag() {
	ERR_MAIN_THREAD_GUARD;
	if (gui_is_dragging()) {
		_perform_drop();
	}
}

void Viewport::set_input_as_handled() {
	ERR_MAIN_THREAD_GUARD;
	if (!handle_input_locally) {
		ERR_FAIL_COND(!is_inside_tree());
		Viewport *vp = this;
		while (true) {
			if (Object::cast_to<Window>(vp)) {
				break;
			}
			if (!vp->get_parent()) {
				break;
			}
			vp = vp->get_parent()->get_viewport();
		}
		if (vp != this) {
			vp->set_input_as_handled();
			return;
		}
	}

	local_input_handled = true;
}

bool Viewport::is_input_handled() const {
	ERR_READ_THREAD_GUARD_V(false);
	if (!handle_input_locally) {
		ERR_FAIL_COND_V(!is_inside_tree(), false);
		const Viewport *vp = this;
		while (true) {
			if (Object::cast_to<Window>(vp)) {
				break;
			}
			if (!vp->get_parent()) {
				break;
			}
			vp = vp->get_parent()->get_viewport();
		}
		if (vp != this) {
			return vp->is_input_handled();
		}
	}
	return local_input_handled;
}

void Viewport::set_handle_input_locally(bool p_enable) {
	ERR_MAIN_THREAD_GUARD;
	handle_input_locally = p_enable;
}

bool Viewport::is_handling_input_locally() const {
	ERR_READ_THREAD_GUARD_V(false);
	return handle_input_locally;
}

void Viewport::set_default_canvas_item_texture_filter(DefaultCanvasItemTextureFilter p_filter) {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_INDEX(p_filter, DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_MAX);

	if (default_canvas_item_texture_filter == p_filter) {
		return;
	}
	default_canvas_item_texture_filter = p_filter;
	switch (default_canvas_item_texture_filter) {
		case DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_NEAREST:
			RS::get_singleton()->viewport_set_default_canvas_item_texture_filter(viewport, RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
			break;
		case DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_LINEAR:
			RS::get_singleton()->viewport_set_default_canvas_item_texture_filter(viewport, RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR);
			break;
		case DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS:
			RS::get_singleton()->viewport_set_default_canvas_item_texture_filter(viewport, RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS);
			break;
		case DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS:
			RS::get_singleton()->viewport_set_default_canvas_item_texture_filter(viewport, RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS);
			break;
		default: {
		}
	}
}

Viewport::DefaultCanvasItemTextureFilter Viewport::get_default_canvas_item_texture_filter() const {
	ERR_READ_THREAD_GUARD_V(DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
	return default_canvas_item_texture_filter;
}

void Viewport::set_default_canvas_item_texture_repeat(DefaultCanvasItemTextureRepeat p_repeat) {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_INDEX(p_repeat, DEFAULT_CANVAS_ITEM_TEXTURE_REPEAT_MAX);

	if (default_canvas_item_texture_repeat == p_repeat) {
		return;
	}

	default_canvas_item_texture_repeat = p_repeat;

	switch (default_canvas_item_texture_repeat) {
		case DEFAULT_CANVAS_ITEM_TEXTURE_REPEAT_DISABLED:
			RS::get_singleton()->viewport_set_default_canvas_item_texture_repeat(viewport, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
			break;
		case DEFAULT_CANVAS_ITEM_TEXTURE_REPEAT_ENABLED:
			RS::get_singleton()->viewport_set_default_canvas_item_texture_repeat(viewport, RS::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED);
			break;
		case DEFAULT_CANVAS_ITEM_TEXTURE_REPEAT_MIRROR:
			RS::get_singleton()->viewport_set_default_canvas_item_texture_repeat(viewport, RS::CANVAS_ITEM_TEXTURE_REPEAT_MIRROR);
			break;
		default: {
		}
	}
}

Viewport::DefaultCanvasItemTextureRepeat Viewport::get_default_canvas_item_texture_repeat() const {
	ERR_READ_THREAD_GUARD_V(DEFAULT_CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
	return default_canvas_item_texture_repeat;
}

void Viewport::set_vrs_mode(Viewport::VRSMode p_vrs_mode) {
	ERR_MAIN_THREAD_GUARD;
	// Note, set this even if not supported on this hardware, it will only be used if it is but we want to save the value as set by the user.
	vrs_mode = p_vrs_mode;

	switch (p_vrs_mode) {
		case VRS_TEXTURE: {
			RS::get_singleton()->viewport_set_vrs_mode(viewport, RS::VIEWPORT_VRS_TEXTURE);
		} break;
		case VRS_XR: {
			RS::get_singleton()->viewport_set_vrs_mode(viewport, RS::VIEWPORT_VRS_XR);
		} break;
		default: {
			RS::get_singleton()->viewport_set_vrs_mode(viewport, RS::VIEWPORT_VRS_DISABLED);
		} break;
	}

	notify_property_list_changed();
}

Viewport::VRSMode Viewport::get_vrs_mode() const {
	ERR_READ_THREAD_GUARD_V(VRS_DISABLED);
	return vrs_mode;
}

void Viewport::set_vrs_update_mode(VRSUpdateMode p_vrs_update_mode) {
	ERR_MAIN_THREAD_GUARD;

	vrs_update_mode = p_vrs_update_mode;
	switch (p_vrs_update_mode) {
		case VRS_UPDATE_ONCE: {
			RS::get_singleton()->viewport_set_vrs_update_mode(viewport, RS::VIEWPORT_VRS_UPDATE_ONCE);
		} break;
		case VRS_UPDATE_ALWAYS: {
			RS::get_singleton()->viewport_set_vrs_update_mode(viewport, RS::VIEWPORT_VRS_UPDATE_ALWAYS);
		} break;
		default: {
			RS::get_singleton()->viewport_set_vrs_update_mode(viewport, RS::VIEWPORT_VRS_UPDATE_DISABLED);
		} break;
	}
}

Viewport::VRSUpdateMode Viewport::get_vrs_update_mode() const {
	ERR_READ_THREAD_GUARD_V(VRS_UPDATE_DISABLED);
	return vrs_update_mode;
}

void Viewport::set_vrs_texture(Ref<Texture2D> p_texture) {
	ERR_MAIN_THREAD_GUARD;
	vrs_texture = p_texture;

	// TODO need to add something here in case the RID changes
	RID tex = p_texture.is_valid() ? p_texture->get_rid() : RID();
	RS::get_singleton()->viewport_set_vrs_texture(viewport, tex);
}

Ref<Texture2D> Viewport::get_vrs_texture() const {
	ERR_READ_THREAD_GUARD_V(Ref<Texture2D>());
	return vrs_texture;
}

DisplayServer::WindowID Viewport::get_window_id() const {
	ERR_READ_THREAD_GUARD_V(DisplayServer::INVALID_WINDOW_ID);
	return DisplayServer::MAIN_WINDOW_ID;
}

Viewport *Viewport::get_parent_viewport() const {
	ERR_READ_THREAD_GUARD_V(nullptr);
	ERR_FAIL_COND_V(!is_inside_tree(), nullptr);
	if (!get_parent()) {
		return nullptr; //root viewport
	}

	return get_parent()->get_viewport();
}

void Viewport::set_embedding_subwindows(bool p_embed) {
	ERR_THREAD_GUARD;
	if (gui.embed_subwindows_hint == p_embed) {
		return;
	}

	bool allow_change = true;

	if (!is_inside_tree()) {
		// Change can happen since no child window is displayed.
	} else if (gui.embed_subwindows_hint) {
		if (!gui.sub_windows.is_empty()) {
			// Prevent change when this viewport has embedded windows.
			allow_change = false;
		}
	} else {
		Viewport *vp = this;
		while (true) {
			if (!vp->get_parent()) {
				// Root window reached.
				break;
			}
			vp = vp->get_parent()->get_viewport();
			if (vp->is_embedding_subwindows()) {
				for (int i = 0; i < vp->gui.sub_windows.size(); i++) {
					if (is_ancestor_of(vp->gui.sub_windows[i].window)) {
						// Prevent change when this viewport has child windows that are displayed in an ancestor viewport.
						allow_change = false;
						break;
					}
				}
			}
		}

		if (allow_change) {
			Vector<DisplayServer::WindowID> wl = DisplayServer::get_singleton()->get_window_list();
			for (const DisplayServer::WindowID &window_id : wl) {
				const Window *w = Window::get_from_id(window_id);
				if (w && is_ancestor_of(w)) {
					// Prevent change when this viewport has child windows that are displayed as native windows.
					allow_change = false;
					break;
				}
			}
		}
	}

	if (allow_change) {
		gui.embed_subwindows_hint = p_embed;
	} else {
		WARN_PRINT("Can't change \"gui_embed_subwindows\" while a child window is displayed. Consider hiding all child windows before changing this value.");
	}
}

bool Viewport::is_embedding_subwindows() const {
	ERR_READ_THREAD_GUARD_V(false);
	return gui.embed_subwindows_hint;
}

TypedArray<Window> Viewport::get_embedded_subwindows() const {
	TypedArray<Window> windows;
	for (int i = 0; i < gui.sub_windows.size(); i++) {
		windows.append(gui.sub_windows[i].window);
	}

	return windows;
}

void Viewport::subwindow_set_popup_safe_rect(Window *p_window, const Rect2i &p_rect) {
	int index = _sub_window_find(p_window);
	ERR_FAIL_COND(index == -1);

	gui.sub_windows.write[index].parent_safe_rect = p_rect;
}

Rect2i Viewport::subwindow_get_popup_safe_rect(Window *p_window) const {
	int index = _sub_window_find(p_window);
	// FIXME: Re-enable ERR_FAIL_COND after rewriting embedded window popup closing.
	// Currently it is expected, that index == -1 can happen.
	if (index == -1) {
		return Rect2i();
	}
	// ERR_FAIL_COND_V(index == -1, Rect2i());

	return gui.sub_windows[index].parent_safe_rect;
}

void Viewport::set_sdf_oversize(SDFOversize p_sdf_oversize) {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_INDEX(p_sdf_oversize, SDF_OVERSIZE_MAX);
	sdf_oversize = p_sdf_oversize;
	RS::get_singleton()->viewport_set_sdf_oversize_and_scale(viewport, RS::ViewportSDFOversize(sdf_oversize), RS::ViewportSDFScale(sdf_scale));
}

Viewport::SDFOversize Viewport::get_sdf_oversize() const {
	ERR_READ_THREAD_GUARD_V(SDF_OVERSIZE_100_PERCENT);
	return sdf_oversize;
}

void Viewport::set_sdf_scale(SDFScale p_sdf_scale) {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_INDEX(p_sdf_scale, SDF_SCALE_MAX);
	sdf_scale = p_sdf_scale;
	RS::get_singleton()->viewport_set_sdf_oversize_and_scale(viewport, RS::ViewportSDFOversize(sdf_oversize), RS::ViewportSDFScale(sdf_scale));
}

Viewport::SDFScale Viewport::get_sdf_scale() const {
	ERR_READ_THREAD_GUARD_V(SDF_SCALE_100_PERCENT);
	return sdf_scale;
}

Transform2D Viewport::get_screen_transform() const {
	ERR_READ_THREAD_GUARD_V(Transform2D());
	return get_screen_transform_internal();
}

Transform2D Viewport::get_screen_transform_internal(bool p_absolute_position) const {
	ERR_READ_THREAD_GUARD_V(Transform2D());
	return get_final_transform();
}

void Viewport::update_mouse_cursor_state() {
	// Updates need to happen in Window, because SubViewportContainers might be hidden behind other Controls.
	Window *base_window = get_base_window();
	if (base_window) {
		base_window->update_mouse_cursor_state();
	}
}

void Viewport::set_canvas_cull_mask(uint32_t p_canvas_cull_mask) {
	ERR_MAIN_THREAD_GUARD;
	canvas_cull_mask = p_canvas_cull_mask;
	RenderingServer::get_singleton()->viewport_set_canvas_cull_mask(viewport, canvas_cull_mask);
}

uint32_t Viewport::get_canvas_cull_mask() const {
	ERR_READ_THREAD_GUARD_V(0);
	return canvas_cull_mask;
}

void Viewport::set_canvas_cull_mask_bit(uint32_t p_layer, bool p_enable) {
	ERR_MAIN_THREAD_GUARD;
	ERR_FAIL_UNSIGNED_INDEX(p_layer, 32);
	if (p_enable) {
		set_canvas_cull_mask(canvas_cull_mask | (1 << p_layer));
	} else {
		set_canvas_cull_mask(canvas_cull_mask & (~(1 << p_layer)));
	}
}

bool Viewport::get_canvas_cull_mask_bit(uint32_t p_layer) const {
	ERR_READ_THREAD_GUARD_V(false);
	ERR_FAIL_UNSIGNED_INDEX_V(p_layer, 32, false);
	return (canvas_cull_mask & (1 << p_layer));
}

#ifdef TOOLS_ENABLED
bool Viewport::is_visible_subviewport() const {
	if (!is_sub_viewport()) {
		return true;
	}
	SubViewportContainer *container = Object::cast_to<SubViewportContainer>(get_parent());
	return container && container->is_visible_in_tree();
}
#endif // TOOLS_ENABLED

void Viewport::_update_audio_listener_2d() {
	if (AudioServer::get_singleton()) {
		AudioServer::get_singleton()->notify_listener_changed();
	}
}

void Viewport::_audio_listener_2d_set(AudioListener2D *p_audio_listener) {
	if (audio_listener_2d == p_audio_listener) {
		return;
	} else if (audio_listener_2d) {
		audio_listener_2d->clear_current();
	}
	audio_listener_2d = p_audio_listener;
}

void Viewport::_audio_listener_2d_remove(AudioListener2D *p_audio_listener) {
	if (audio_listener_2d == p_audio_listener) {
		audio_listener_2d = nullptr;
	}
}

void Viewport::_camera_2d_set(Camera2D *p_camera_2d) {
	camera_2d = p_camera_2d;
}

void Viewport::_cleanup_mouseover_colliders(bool p_clean_all_frames, bool p_paused_only, uint64_t p_frame_reference) {
	List<ObjectID> to_erase;
	List<ObjectID> to_mouse_exit;

	for (const KeyValue<ObjectID, uint64_t> &E : physics_2d_mouseover) {
		if (!p_clean_all_frames && E.value == p_frame_reference) {
			continue;
		}

		Object *o = ObjectDB::get_instance(E.key);
		if (o) {
			CollisionObject2D *co = Object::cast_to<CollisionObject2D>(o);
			if (co && co->is_inside_tree()) {
				if (p_clean_all_frames && p_paused_only && co->can_process()) {
					continue;
				}
				to_mouse_exit.push_back(E.key);
			}
		}
		to_erase.push_back(E.key);
	}

	while (to_erase.size()) {
		physics_2d_mouseover.erase(to_erase.front()->get());
		to_erase.pop_front();
	}

	// Per-shape.
	List<Pair<ObjectID, int>> shapes_to_erase;
	List<Pair<ObjectID, int>> shapes_to_mouse_exit;

	for (KeyValue<Pair<ObjectID, int>, uint64_t> &E : physics_2d_shape_mouseover) {
		if (!p_clean_all_frames && E.value == p_frame_reference) {
			continue;
		}

		Object *o = ObjectDB::get_instance(E.key.first);
		if (o) {
			CollisionObject2D *co = Object::cast_to<CollisionObject2D>(o);
			if (co && co->is_inside_tree()) {
				if (p_clean_all_frames && p_paused_only && co->can_process()) {
					continue;
				}
				shapes_to_mouse_exit.push_back(E.key);
			}
		}
		shapes_to_erase.push_back(E.key);
	}

	while (shapes_to_erase.size()) {
		physics_2d_shape_mouseover.erase(shapes_to_erase.front()->get());
		shapes_to_erase.pop_front();
	}

	while (to_mouse_exit.size()) {
		Object *o = ObjectDB::get_instance(to_mouse_exit.front()->get());
		CollisionObject2D *co = Object::cast_to<CollisionObject2D>(o);
		co->_mouse_exit();
		to_mouse_exit.pop_front();
	}

	while (shapes_to_mouse_exit.size()) {
		Pair<ObjectID, int> e = shapes_to_mouse_exit.front()->get();
		Object *o = ObjectDB::get_instance(e.first);
		CollisionObject2D *co = Object::cast_to<CollisionObject2D>(o);
		co->_mouse_shape_exit(e.second);
		shapes_to_mouse_exit.pop_front();
	}
}

AudioListener2D *Viewport::get_audio_listener_2d() const {
	ERR_READ_THREAD_GUARD_V(nullptr);
	return audio_listener_2d;
}

void Viewport::set_as_audio_listener_2d(bool p_enable) {
	ERR_MAIN_THREAD_GUARD;
	if (p_enable == is_audio_listener_2d_enabled) {
		return;
	}

	is_audio_listener_2d_enabled = p_enable;
	_update_audio_listener_2d();
}

bool Viewport::is_audio_listener_2d() const {
	ERR_READ_THREAD_GUARD_V(false);
	return is_audio_listener_2d_enabled;
}

Camera2D *Viewport::get_camera_2d() const {
	ERR_READ_THREAD_GUARD_V(nullptr);
	return camera_2d;
}

void Viewport::assign_next_enabled_camera_2d(const StringName &p_camera_group) {
	ERR_MAIN_THREAD_GUARD;
	List<Node *> camera_list;
	get_tree()->get_nodes_in_group(p_camera_group, &camera_list);

	Camera2D *new_camera = nullptr;
	for (Node *E : camera_list) {
		Camera2D *cam = Object::cast_to<Camera2D>(E);
		if (!cam) {
			continue; // Non-camera node (e.g. ParallaxBackground).
		}

		if (cam->is_enabled()) {
			new_camera = cam;
			break;
		}
	}

	_camera_2d_set(new_camera);
	if (!camera_2d) {
		set_canvas_transform(Transform2D());
	}
}

#ifndef _3D_DISABLED
AudioListener3D *Viewport::get_audio_listener_3d() const {
	ERR_READ_THREAD_GUARD_V(nullptr);
	return audio_listener_3d;
}

void Viewport::set_as_audio_listener_3d(bool p_enable) {
	ERR_MAIN_THREAD_GUARD;
	if (p_enable == is_audio_listener_3d_enabled) {
		return;
	}

	is_audio_listener_3d_enabled = p_enable;
	_update_audio_listener_3d();
}

bool Viewport::is_audio_listener_3d() const {
	ERR_READ_THREAD_GUARD_V(false);
	return is_audio_listener_3d_enabled;
}

void Viewport::_update_audio_listener_3d() {
	if (AudioServer::get_singleton()) {
		AudioServer::get_singleton()->notify_listener_changed();
	}
}

void Viewport::_listener_transform_3d_changed_notify() {
}

void Viewport::_audio_listener_3d_set(AudioListener3D *p_listener) {
	if (audio_listener_3d == p_listener) {
		return;
	}

	audio_listener_3d = p_listener;

	_update_audio_listener_3d();
	_listener_transform_3d_changed_notify();
}

bool Viewport::_audio_listener_3d_add(AudioListener3D *p_listener) {
	audio_listener_3d_set.insert(p_listener);
	return audio_listener_3d_set.size() == 1;
}

void Viewport::_audio_listener_3d_remove(AudioListener3D *p_listener) {
	audio_listener_3d_set.erase(p_listener);
	if (audio_listener_3d == p_listener) {
		audio_listener_3d = nullptr;
	}
}

void Viewport::_audio_listener_3d_make_next_current(AudioListener3D *p_exclude) {
	if (audio_listener_3d_set.size() > 0) {
		for (AudioListener3D *E : audio_listener_3d_set) {
			if (p_exclude == E) {
				continue;
			}
			if (!E->is_inside_tree()) {
				continue;
			}
			if (audio_listener_3d != nullptr) {
				return;
			}

			E->make_current();
		}
	} else {
		// Attempt to reset listener to the camera position.
		if (camera_3d != nullptr) {
			_update_audio_listener_3d();
			_camera_3d_transform_changed_notify();
		}
	}
}

void Viewport::_collision_object_3d_input_event(CollisionObject3D *p_object, Camera3D *p_camera, const Ref<InputEvent> &p_input_event, const Vector3 &p_pos, const Vector3 &p_normal, int p_shape) {
	Transform3D object_transform = p_object->get_global_transform();
	Transform3D camera_transform = p_camera->get_global_transform();
	ObjectID id = p_object->get_instance_id();

	// Avoid sending the fake event unnecessarily if nothing really changed in the context.
	if (object_transform == physics_last_object_transform && camera_transform == physics_last_camera_transform && physics_last_id == id) {
		Ref<InputEventMouseMotion> mm = p_input_event;
		if (mm.is_valid() && mm->get_device() == InputEvent::DEVICE_ID_INTERNAL) {
			return; // Discarded.
		}
	}
	p_object->_input_event_call(camera_3d, p_input_event, p_pos, p_normal, p_shape);
	physics_last_object_transform = object_transform;
	physics_last_camera_transform = camera_transform;
	physics_last_id = id;
}

Camera3D *Viewport::get_camera_3d() const {
	ERR_READ_THREAD_GUARD_V(nullptr);
	return camera_3d;
}

void Viewport::_camera_3d_transform_changed_notify() {
}

void Viewport::_camera_3d_set(Camera3D *p_camera) {
	if (camera_3d == p_camera) {
		return;
	}

	if (camera_3d) {
		camera_3d->notification(Camera3D::NOTIFICATION_LOST_CURRENT);
	}

	camera_3d = p_camera;

	if (!camera_3d_override) {
		if (camera_3d) {
			RenderingServer::get_singleton()->viewport_attach_camera(viewport, camera_3d->get_camera());
		} else {
			RenderingServer::get_singleton()->viewport_attach_camera(viewport, RID());
		}
	}

	if (camera_3d) {
		camera_3d->notification(Camera3D::NOTIFICATION_BECAME_CURRENT);
	}

	_update_audio_listener_3d();
	_camera_3d_transform_changed_notify();
}

bool Viewport::_camera_3d_add(Camera3D *p_camera) {
	camera_3d_set.insert(p_camera);
	return camera_3d_set.size() == 1;
}

void Viewport::_camera_3d_remove(Camera3D *p_camera) {
	camera_3d_set.erase(p_camera);
	if (camera_3d == p_camera) {
		_camera_3d_set(nullptr);
	}
}

void Viewport::_camera_3d_make_next_current(Camera3D *p_exclude) {
	for (Camera3D *E : camera_3d_set) {
		if (p_exclude == E) {
			continue;
		}
		if (!E->is_inside_tree()) {
			continue;
		}
		if (camera_3d != nullptr) {
			return;
		}

		E->make_current();
	}
}

void Viewport::enable_camera_3d_override(bool p_enable) {
	ERR_MAIN_THREAD_GUARD;
	if (p_enable == camera_3d_override) {
		return;
	}

	if (p_enable) {
		camera_3d_override.rid = RenderingServer::get_singleton()->camera_create();
	} else {
		RenderingServer::get_singleton()->free(camera_3d_override.rid);
		camera_3d_override.rid = RID();
	}

	if (p_enable) {
		RenderingServer::get_singleton()->viewport_attach_camera(viewport, camera_3d_override.rid);
	} else if (camera_3d) {
		RenderingServer::get_singleton()->viewport_attach_camera(viewport, camera_3d->get_camera());
	} else {
		RenderingServer::get_singleton()->viewport_attach_camera(viewport, RID());
	}
}

void Viewport::set_camera_3d_override_perspective(real_t p_fovy_degrees, real_t p_z_near, real_t p_z_far) {
	ERR_MAIN_THREAD_GUARD;
	if (camera_3d_override) {
		if (camera_3d_override.fov == p_fovy_degrees && camera_3d_override.z_near == p_z_near &&
				camera_3d_override.z_far == p_z_far && camera_3d_override.projection == Camera3DOverrideData::PROJECTION_PERSPECTIVE) {
			return;
		}

		camera_3d_override.fov = p_fovy_degrees;
		camera_3d_override.z_near = p_z_near;
		camera_3d_override.z_far = p_z_far;
		camera_3d_override.projection = Camera3DOverrideData::PROJECTION_PERSPECTIVE;

		RenderingServer::get_singleton()->camera_set_perspective(camera_3d_override.rid, camera_3d_override.fov, camera_3d_override.z_near, camera_3d_override.z_far);
	}
}

void Viewport::set_camera_3d_override_orthogonal(real_t p_size, real_t p_z_near, real_t p_z_far) {
	ERR_MAIN_THREAD_GUARD;
	if (camera_3d_override) {
		if (camera_3d_override.size == p_size && camera_3d_override.z_near == p_z_near &&
				camera_3d_override.z_far == p_z_far && camera_3d_override.projection == Camera3DOverrideData::PROJECTION_ORTHOGONAL) {
			return;
		}

		camera_3d_override.size = p_size;
		camera_3d_override.z_near = p_z_near;
		camera_3d_override.z_far = p_z_far;
		camera_3d_override.projection = Camera3DOverrideData::PROJECTION_ORTHOGONAL;

		RenderingServer::get_singleton()->camera_set_orthogonal(camera_3d_override.rid, camera_3d_override.size, camera_3d_override.z_near, camera_3d_override.z_far);
	}
}

HashMap<StringName, real_t> Viewport::get_camera_3d_override_properties() const {
	HashMap<StringName, real_t> props;

	props["size"] = 0;
	props["fov"] = 0;
	props["z_near"] = 0;
	props["z_far"] = 0;
	ERR_READ_THREAD_GUARD_V(props);

	props["size"] = camera_3d_override.size;
	props["fov"] = camera_3d_override.fov;
	props["z_near"] = camera_3d_override.z_near;
	props["z_far"] = camera_3d_override.z_far;
	return props;
}

void Viewport::set_disable_3d(bool p_disable) {
	ERR_MAIN_THREAD_GUARD;
	disable_3d = p_disable;
	RenderingServer::get_singleton()->viewport_set_disable_3d(viewport, disable_3d);
}

bool Viewport::is_3d_disabled() const {
	ERR_READ_THREAD_GUARD_V(false);
	return disable_3d;
}

bool Viewport::is_camera_3d_override_enabled() const {
	ERR_READ_THREAD_GUARD_V(false);
	return camera_3d_override;
}

void Viewport::set_camera_3d_override_transform(const Transform3D &p_transform) {
	ERR_MAIN_THREAD_GUARD;
	if (camera_3d_override) {
		camera_3d_override.transform = p_transform;
		RenderingServer::get_singleton()->camera_set_transform(camera_3d_override.rid, p_transform);
	}
}

Transform3D Viewport::get_camera_3d_override_transform() const {
	ERR_READ_THREAD_GUARD_V(Transform3D());
	if (camera_3d_override) {
		return camera_3d_override.transform;
	}

	return Transform3D();
}

Vector3 Viewport::camera_3d_override_project_ray_normal(const Point2 &p_pos) const {
	ERR_READ_THREAD_GUARD_V(Vector3());
	Vector3 ray = camera_3d_override_project_local_ray_normal(p_pos);
	return camera_3d_override.transform.basis.xform(ray).normalized();
}

Vector3 Viewport::camera_3d_override_project_local_ray_normal(const Point2 &p_pos) const {
	ERR_READ_THREAD_GUARD_V(Vector3());
	Size2 viewport_size = get_camera_rect_size();
	Vector2 cpos = get_camera_coords(p_pos);
	Vector3 ray;

	if (camera_3d_override.projection == Camera3DOverrideData::PROJECTION_ORTHOGONAL) {
		ray = Vector3(0, 0, -1);
	} else {
		Projection cm;
		cm.set_perspective(camera_3d_override.fov, get_visible_rect().size.aspect(), camera_3d_override.z_near, camera_3d_override.z_far, false);

		Vector2 screen_he = cm.get_viewport_half_extents();
		ray = Vector3(((cpos.x / viewport_size.width) * 2.0 - 1.0) * screen_he.x, ((1.0 - (cpos.y / viewport_size.height)) * 2.0 - 1.0) * screen_he.y, -camera_3d_override.z_near).normalized();
	}

	return ray;
}

Vector3 Viewport::camera_3d_override_project_ray_origin(const Point2 &p_pos) const {
	ERR_READ_THREAD_GUARD_V(Vector3());
	Size2 viewport_size = get_camera_rect_size();
	Vector2 cpos = get_camera_coords(p_pos);
	ERR_FAIL_COND_V(viewport_size.y == 0, Vector3());

	if (camera_3d_override.projection == Camera3DOverrideData::PROJECTION_ORTHOGONAL) {
		Vector2 pos = cpos / viewport_size;
		real_t vsize, hsize;
		hsize = camera_3d_override.size * viewport_size.aspect();
		vsize = camera_3d_override.size;

		Vector3 ray;
		ray.x = pos.x * (hsize)-hsize / 2;
		ray.y = (1.0 - pos.y) * (vsize)-vsize / 2;
		ray.z = -camera_3d_override.z_near;
		ray = camera_3d_override.transform.xform(ray);
		return ray;
	} else {
		return camera_3d_override.transform.origin;
	};
}

Ref<World3D> Viewport::get_world_3d() const {
	ERR_READ_THREAD_GUARD_V(Ref<World3D>());
	return world_3d;
}

Ref<World3D> Viewport::find_world_3d() const {
	ERR_READ_THREAD_GUARD_V(Ref<World3D>());
	if (own_world_3d.is_valid()) {
		return own_world_3d;
	} else if (world_3d.is_valid()) {
		return world_3d;
	} else if (parent) {
		return parent->find_world_3d();
	} else {
		return Ref<World3D>();
	}
}

void Viewport::set_world_3d(const Ref<World3D> &p_world_3d) {
	ERR_MAIN_THREAD_GUARD;
	if (world_3d == p_world_3d) {
		return;
	}

	if (is_inside_tree()) {
		_propagate_exit_world_3d(this);
	}

	if (own_world_3d.is_valid() && world_3d.is_valid()) {
		world_3d->disconnect_changed(callable_mp(this, &Viewport::_own_world_3d_changed));
	}

	world_3d = p_world_3d;

	if (own_world_3d.is_valid()) {
		if (world_3d.is_valid()) {
			own_world_3d = world_3d->duplicate();
			world_3d->connect_changed(callable_mp(this, &Viewport::_own_world_3d_changed));
		} else {
			own_world_3d.instantiate();
		}
	}

	if (is_inside_tree()) {
		_propagate_enter_world_3d(this);
	}

	if (is_inside_tree()) {
		RenderingServer::get_singleton()->viewport_set_scenario(viewport, find_world_3d()->get_scenario());
	}

	_update_audio_listener_3d();
}

void Viewport::_own_world_3d_changed() {
	ERR_FAIL_COND(world_3d.is_null());
	ERR_FAIL_COND(own_world_3d.is_null());

	if (is_inside_tree()) {
		_propagate_exit_world_3d(this);
	}

	own_world_3d = world_3d->duplicate();

	if (is_inside_tree()) {
		_propagate_enter_world_3d(this);
	}

	if (is_inside_tree()) {
		RenderingServer::get_singleton()->viewport_set_scenario(viewport, find_world_3d()->get_scenario());
	}

	_update_audio_listener_3d();
}

void Viewport::set_use_own_world_3d(bool p_use_own_world_3d) {
	ERR_MAIN_THREAD_GUARD;
	if (p_use_own_world_3d == own_world_3d.is_valid()) {
		return;
	}

	if (is_inside_tree()) {
		_propagate_exit_world_3d(this);
	}

	if (p_use_own_world_3d) {
		if (world_3d.is_valid()) {
			own_world_3d = world_3d->duplicate();
			world_3d->connect_changed(callable_mp(this, &Viewport::_own_world_3d_changed));
		} else {
			own_world_3d.instantiate();
		}
	} else {
		own_world_3d = Ref<World3D>();
		if (world_3d.is_valid()) {
			world_3d->disconnect_changed(callable_mp(this, &Viewport::_own_world_3d_changed));
		}
	}

	if (is_inside_tree()) {
		_propagate_enter_world_3d(this);
	}

	if (is_inside_tree()) {
		RenderingServer::get_singleton()->viewport_set_scenario(viewport, find_world_3d()->get_scenario());
	}

	_update_audio_listener_3d();
}

bool Viewport::is_using_own_world_3d() const {
	ERR_READ_THREAD_GUARD_V(false);
	return own_world_3d.is_valid();
}

void Viewport::_propagate_enter_world_3d(Node *p_node) {
	if (p_node != this) {
		if (!p_node->is_inside_tree()) { //may not have entered scene yet
			return;
		}

		if (Object::cast_to<Node3D>(p_node) || Object::cast_to<WorldEnvironment>(p_node)) {
			p_node->notification(Node3D::NOTIFICATION_ENTER_WORLD);
		} else {
			Viewport *v = Object::cast_to<Viewport>(p_node);
			if (v) {
				if (v->world_3d.is_valid() || v->own_world_3d.is_valid()) {
					return;
				}
			}
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_propagate_enter_world_3d(p_node->get_child(i));
	}
}

void Viewport::_propagate_exit_world_3d(Node *p_node) {
	if (p_node != this) {
		if (!p_node->is_inside_tree()) { //may have exited scene already
			return;
		}

		if (Object::cast_to<Node3D>(p_node) || Object::cast_to<WorldEnvironment>(p_node)) {
			p_node->notification(Node3D::NOTIFICATION_EXIT_WORLD);
		} else {
			Viewport *v = Object::cast_to<Viewport>(p_node);
			if (v) {
				if (v->world_3d.is_valid() || v->own_world_3d.is_valid()) {
					return;
				}
			}
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_propagate_exit_world_3d(p_node->get_child(i));
	}
}

void Viewport::set_use_xr(bool p_use_xr) {
	ERR_MAIN_THREAD_GUARD;
	if (use_xr != p_use_xr) {
		use_xr = p_use_xr;

		RS::get_singleton()->viewport_set_use_xr(viewport, use_xr);

		if (!use_xr) {
			// Set viewport to previous size when exiting XR.
			if (size_allocated) {
				RS::get_singleton()->viewport_set_size(viewport, size.width, size.height);
			} else {
				RS::get_singleton()->viewport_set_size(viewport, 0, 0);
			}

			// Reset render target override textures.
			RID rt = RS::get_singleton()->viewport_get_render_target(viewport);
			RSG::texture_storage->render_target_set_override(rt, RID(), RID(), RID(), RID());
		}
	}
}

bool Viewport::is_using_xr() {
	ERR_READ_THREAD_GUARD_V(false);
	return use_xr;
}

void Viewport::set_scaling_3d_mode(Scaling3DMode p_scaling_3d_mode) {
	ERR_MAIN_THREAD_GUARD;
	if (scaling_3d_mode == p_scaling_3d_mode) {
		return;
	}

	scaling_3d_mode = p_scaling_3d_mode;
	RS::get_singleton()->viewport_set_scaling_3d_mode(viewport, (RS::ViewportScaling3DMode)(int)p_scaling_3d_mode);
}

Viewport::Scaling3DMode Viewport::get_scaling_3d_mode() const {
	ERR_READ_THREAD_GUARD_V(SCALING_3D_MODE_BILINEAR);
	return scaling_3d_mode;
}

void Viewport::set_scaling_3d_scale(float p_scaling_3d_scale) {
	ERR_MAIN_THREAD_GUARD;
	// Clamp to reasonable values that are actually useful.
	// Values above 2.0 don't serve a practical purpose since the viewport
	// isn't displayed with mipmaps.
	scaling_3d_scale = CLAMP(p_scaling_3d_scale, 0.1, 2.0);

	RS::get_singleton()->viewport_set_scaling_3d_scale(viewport, scaling_3d_scale);
}

float Viewport::get_scaling_3d_scale() const {
	ERR_READ_THREAD_GUARD_V(0);
	return scaling_3d_scale;
}

void Viewport::set_fsr_sharpness(float p_fsr_sharpness) {
	ERR_MAIN_THREAD_GUARD;
	if (fsr_sharpness == p_fsr_sharpness) {
		return;
	}

	if (p_fsr_sharpness < 0.0f) {
		p_fsr_sharpness = 0.0f;
	}

	fsr_sharpness = p_fsr_sharpness;
	RS::get_singleton()->viewport_set_fsr_sharpness(viewport, p_fsr_sharpness);
}

float Viewport::get_fsr_sharpness() const {
	ERR_READ_THREAD_GUARD_V(0);
	return fsr_sharpness;
}

void Viewport::set_texture_mipmap_bias(float p_texture_mipmap_bias) {
	ERR_MAIN_THREAD_GUARD;
	if (texture_mipmap_bias == p_texture_mipmap_bias) {
		return;
	}

	texture_mipmap_bias = p_texture_mipmap_bias;
	RS::get_singleton()->viewport_set_texture_mipmap_bias(viewport, p_texture_mipmap_bias);
}

float Viewport::get_texture_mipmap_bias() const {
	ERR_READ_THREAD_GUARD_V(0);
	return texture_mipmap_bias;
}

#endif // _3D_DISABLED

void Viewport::_propagate_world_2d_changed(Node *p_node) {
	if (p_node != this) {
		if (Object::cast_to<CanvasItem>(p_node)) {
			p_node->notification(CanvasItem::NOTIFICATION_WORLD_2D_CHANGED);
		} else {
			Viewport *v = Object::cast_to<Viewport>(p_node);
			if (v) {
				if (v->world_2d.is_valid()) {
					return;
				}
			}
		}
	}

	for (int i = 0; i < p_node->get_child_count(); ++i) {
		_propagate_world_2d_changed(p_node->get_child(i));
	}
}

void Viewport::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_world_2d", "world_2d"), &Viewport::set_world_2d);
	ClassDB::bind_method(D_METHOD("get_world_2d"), &Viewport::get_world_2d);
	ClassDB::bind_method(D_METHOD("find_world_2d"), &Viewport::find_world_2d);

	ClassDB::bind_method(D_METHOD("set_canvas_transform", "xform"), &Viewport::set_canvas_transform);
	ClassDB::bind_method(D_METHOD("get_canvas_transform"), &Viewport::get_canvas_transform);

	ClassDB::bind_method(D_METHOD("set_global_canvas_transform", "xform"), &Viewport::set_global_canvas_transform);
	ClassDB::bind_method(D_METHOD("get_global_canvas_transform"), &Viewport::get_global_canvas_transform);
	ClassDB::bind_method(D_METHOD("get_stretch_transform"), &Viewport::get_stretch_transform);
	ClassDB::bind_method(D_METHOD("get_final_transform"), &Viewport::get_final_transform);
	ClassDB::bind_method(D_METHOD("get_screen_transform"), &Viewport::get_screen_transform);

	ClassDB::bind_method(D_METHOD("get_visible_rect"), &Viewport::get_visible_rect);
	ClassDB::bind_method(D_METHOD("set_transparent_background", "enable"), &Viewport::set_transparent_background);
	ClassDB::bind_method(D_METHOD("has_transparent_background"), &Viewport::has_transparent_background);
	ClassDB::bind_method(D_METHOD("set_use_hdr_2d", "enable"), &Viewport::set_use_hdr_2d);
	ClassDB::bind_method(D_METHOD("is_using_hdr_2d"), &Viewport::is_using_hdr_2d);

	ClassDB::bind_method(D_METHOD("set_msaa_2d", "msaa"), &Viewport::set_msaa_2d);
	ClassDB::bind_method(D_METHOD("get_msaa_2d"), &Viewport::get_msaa_2d);

	ClassDB::bind_method(D_METHOD("set_msaa_3d", "msaa"), &Viewport::set_msaa_3d);
	ClassDB::bind_method(D_METHOD("get_msaa_3d"), &Viewport::get_msaa_3d);

	ClassDB::bind_method(D_METHOD("set_screen_space_aa", "screen_space_aa"), &Viewport::set_screen_space_aa);
	ClassDB::bind_method(D_METHOD("get_screen_space_aa"), &Viewport::get_screen_space_aa);

	ClassDB::bind_method(D_METHOD("set_use_taa", "enable"), &Viewport::set_use_taa);
	ClassDB::bind_method(D_METHOD("is_using_taa"), &Viewport::is_using_taa);

	ClassDB::bind_method(D_METHOD("set_use_debanding", "enable"), &Viewport::set_use_debanding);
	ClassDB::bind_method(D_METHOD("is_using_debanding"), &Viewport::is_using_debanding);

	ClassDB::bind_method(D_METHOD("set_use_occlusion_culling", "enable"), &Viewport::set_use_occlusion_culling);
	ClassDB::bind_method(D_METHOD("is_using_occlusion_culling"), &Viewport::is_using_occlusion_culling);

	ClassDB::bind_method(D_METHOD("set_debug_draw", "debug_draw"), &Viewport::set_debug_draw);
	ClassDB::bind_method(D_METHOD("get_debug_draw"), &Viewport::get_debug_draw);

	ClassDB::bind_method(D_METHOD("get_render_info", "type", "info"), &Viewport::get_render_info);

	ClassDB::bind_method(D_METHOD("get_texture"), &Viewport::get_texture);

	ClassDB::bind_method(D_METHOD("set_physics_object_picking", "enable"), &Viewport::set_physics_object_picking);
	ClassDB::bind_method(D_METHOD("get_physics_object_picking"), &Viewport::get_physics_object_picking);
	ClassDB::bind_method(D_METHOD("set_physics_object_picking_sort", "enable"), &Viewport::set_physics_object_picking_sort);
	ClassDB::bind_method(D_METHOD("get_physics_object_picking_sort"), &Viewport::get_physics_object_picking_sort);
	ClassDB::bind_method(D_METHOD("set_physics_object_picking_first_only", "enable"), &Viewport::set_physics_object_picking_first_only);
	ClassDB::bind_method(D_METHOD("get_physics_object_picking_first_only"), &Viewport::get_physics_object_picking_first_only);

	ClassDB::bind_method(D_METHOD("get_viewport_rid"), &Viewport::get_viewport_rid);
	ClassDB::bind_method(D_METHOD("push_text_input", "text"), &Viewport::push_text_input);
	ClassDB::bind_method(D_METHOD("push_input", "event", "in_local_coords"), &Viewport::push_input, DEFVAL(false));
#ifndef DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("push_unhandled_input", "event", "in_local_coords"), &Viewport::push_unhandled_input, DEFVAL(false));
#endif // DISABLE_DEPRECATED

	ClassDB::bind_method(D_METHOD("get_mouse_position"), &Viewport::get_mouse_position);
	ClassDB::bind_method(D_METHOD("warp_mouse", "position"), &Viewport::warp_mouse);
	ClassDB::bind_method(D_METHOD("update_mouse_cursor_state"), &Viewport::update_mouse_cursor_state);

	ClassDB::bind_method(D_METHOD("gui_cancel_drag"), &Viewport::gui_cancel_drag);
	ClassDB::bind_method(D_METHOD("gui_get_drag_data"), &Viewport::gui_get_drag_data);
	ClassDB::bind_method(D_METHOD("gui_is_dragging"), &Viewport::gui_is_dragging);
	ClassDB::bind_method(D_METHOD("gui_is_drag_successful"), &Viewport::gui_is_drag_successful);

	ClassDB::bind_method(D_METHOD("gui_release_focus"), &Viewport::gui_release_focus);
	ClassDB::bind_method(D_METHOD("gui_get_focus_owner"), &Viewport::gui_get_focus_owner);
	ClassDB::bind_method(D_METHOD("gui_get_hovered_control"), &Viewport::gui_get_hovered_control);

	ClassDB::bind_method(D_METHOD("set_disable_input", "disable"), &Viewport::set_disable_input);
	ClassDB::bind_method(D_METHOD("is_input_disabled"), &Viewport::is_input_disabled);

	ClassDB::bind_method(D_METHOD("_gui_remove_focus_for_window"), &Viewport::_gui_remove_focus_for_window);

	ClassDB::bind_method(D_METHOD("set_positional_shadow_atlas_size", "size"), &Viewport::set_positional_shadow_atlas_size);
	ClassDB::bind_method(D_METHOD("get_positional_shadow_atlas_size"), &Viewport::get_positional_shadow_atlas_size);

	ClassDB::bind_method(D_METHOD("set_positional_shadow_atlas_16_bits", "enable"), &Viewport::set_positional_shadow_atlas_16_bits);
	ClassDB::bind_method(D_METHOD("get_positional_shadow_atlas_16_bits"), &Viewport::get_positional_shadow_atlas_16_bits);

	ClassDB::bind_method(D_METHOD("set_snap_controls_to_pixels", "enabled"), &Viewport::set_snap_controls_to_pixels);
	ClassDB::bind_method(D_METHOD("is_snap_controls_to_pixels_enabled"), &Viewport::is_snap_controls_to_pixels_enabled);

	ClassDB::bind_method(D_METHOD("set_snap_2d_transforms_to_pixel", "enabled"), &Viewport::set_snap_2d_transforms_to_pixel);
	ClassDB::bind_method(D_METHOD("is_snap_2d_transforms_to_pixel_enabled"), &Viewport::is_snap_2d_transforms_to_pixel_enabled);

	ClassDB::bind_method(D_METHOD("set_snap_2d_vertices_to_pixel", "enabled"), &Viewport::set_snap_2d_vertices_to_pixel);
	ClassDB::bind_method(D_METHOD("is_snap_2d_vertices_to_pixel_enabled"), &Viewport::is_snap_2d_vertices_to_pixel_enabled);

	ClassDB::bind_method(D_METHOD("set_positional_shadow_atlas_quadrant_subdiv", "quadrant", "subdiv"), &Viewport::set_positional_shadow_atlas_quadrant_subdiv);
	ClassDB::bind_method(D_METHOD("get_positional_shadow_atlas_quadrant_subdiv", "quadrant"), &Viewport::get_positional_shadow_atlas_quadrant_subdiv);

	ClassDB::bind_method(D_METHOD("set_input_as_handled"), &Viewport::set_input_as_handled);
	ClassDB::bind_method(D_METHOD("is_input_handled"), &Viewport::is_input_handled);

	ClassDB::bind_method(D_METHOD("set_handle_input_locally", "enable"), &Viewport::set_handle_input_locally);
	ClassDB::bind_method(D_METHOD("is_handling_input_locally"), &Viewport::is_handling_input_locally);

	ClassDB::bind_method(D_METHOD("set_default_canvas_item_texture_filter", "mode"), &Viewport::set_default_canvas_item_texture_filter);
	ClassDB::bind_method(D_METHOD("get_default_canvas_item_texture_filter"), &Viewport::get_default_canvas_item_texture_filter);

	ClassDB::bind_method(D_METHOD("set_embedding_subwindows", "enable"), &Viewport::set_embedding_subwindows);
	ClassDB::bind_method(D_METHOD("is_embedding_subwindows"), &Viewport::is_embedding_subwindows);
	ClassDB::bind_method(D_METHOD("get_embedded_subwindows"), &Viewport::get_embedded_subwindows);

	ClassDB::bind_method(D_METHOD("set_canvas_cull_mask", "mask"), &Viewport::set_canvas_cull_mask);
	ClassDB::bind_method(D_METHOD("get_canvas_cull_mask"), &Viewport::get_canvas_cull_mask);

	ClassDB::bind_method(D_METHOD("set_canvas_cull_mask_bit", "layer", "enable"), &Viewport::set_canvas_cull_mask_bit);
	ClassDB::bind_method(D_METHOD("get_canvas_cull_mask_bit", "layer"), &Viewport::get_canvas_cull_mask_bit);

	ClassDB::bind_method(D_METHOD("set_default_canvas_item_texture_repeat", "mode"), &Viewport::set_default_canvas_item_texture_repeat);
	ClassDB::bind_method(D_METHOD("get_default_canvas_item_texture_repeat"), &Viewport::get_default_canvas_item_texture_repeat);

	ClassDB::bind_method(D_METHOD("set_sdf_oversize", "oversize"), &Viewport::set_sdf_oversize);
	ClassDB::bind_method(D_METHOD("get_sdf_oversize"), &Viewport::get_sdf_oversize);

	ClassDB::bind_method(D_METHOD("set_sdf_scale", "scale"), &Viewport::set_sdf_scale);
	ClassDB::bind_method(D_METHOD("get_sdf_scale"), &Viewport::get_sdf_scale);

	ClassDB::bind_method(D_METHOD("set_mesh_lod_threshold", "pixels"), &Viewport::set_mesh_lod_threshold);
	ClassDB::bind_method(D_METHOD("get_mesh_lod_threshold"), &Viewport::get_mesh_lod_threshold);

	ClassDB::bind_method(D_METHOD("_process_picking"), &Viewport::_process_picking);

	ClassDB::bind_method(D_METHOD("set_as_audio_listener_2d", "enable"), &Viewport::set_as_audio_listener_2d);
	ClassDB::bind_method(D_METHOD("is_audio_listener_2d"), &Viewport::is_audio_listener_2d);
	ClassDB::bind_method(D_METHOD("get_audio_listener_2d"), &Viewport::get_audio_listener_2d);
	ClassDB::bind_method(D_METHOD("get_camera_2d"), &Viewport::get_camera_2d);

#ifndef _3D_DISABLED
	ClassDB::bind_method(D_METHOD("set_world_3d", "world_3d"), &Viewport::set_world_3d);
	ClassDB::bind_method(D_METHOD("get_world_3d"), &Viewport::get_world_3d);
	ClassDB::bind_method(D_METHOD("find_world_3d"), &Viewport::find_world_3d);

	ClassDB::bind_method(D_METHOD("set_use_own_world_3d", "enable"), &Viewport::set_use_own_world_3d);
	ClassDB::bind_method(D_METHOD("is_using_own_world_3d"), &Viewport::is_using_own_world_3d);

	ClassDB::bind_method(D_METHOD("get_audio_listener_3d"), &Viewport::get_audio_listener_3d);
	ClassDB::bind_method(D_METHOD("get_camera_3d"), &Viewport::get_camera_3d);
	ClassDB::bind_method(D_METHOD("set_as_audio_listener_3d", "enable"), &Viewport::set_as_audio_listener_3d);
	ClassDB::bind_method(D_METHOD("is_audio_listener_3d"), &Viewport::is_audio_listener_3d);

	ClassDB::bind_method(D_METHOD("set_disable_3d", "disable"), &Viewport::set_disable_3d);
	ClassDB::bind_method(D_METHOD("is_3d_disabled"), &Viewport::is_3d_disabled);

	ClassDB::bind_method(D_METHOD("set_use_xr", "use"), &Viewport::set_use_xr);
	ClassDB::bind_method(D_METHOD("is_using_xr"), &Viewport::is_using_xr);

	ClassDB::bind_method(D_METHOD("set_scaling_3d_mode", "scaling_3d_mode"), &Viewport::set_scaling_3d_mode);
	ClassDB::bind_method(D_METHOD("get_scaling_3d_mode"), &Viewport::get_scaling_3d_mode);

	ClassDB::bind_method(D_METHOD("set_scaling_3d_scale", "scale"), &Viewport::set_scaling_3d_scale);
	ClassDB::bind_method(D_METHOD("get_scaling_3d_scale"), &Viewport::get_scaling_3d_scale);

	ClassDB::bind_method(D_METHOD("set_fsr_sharpness", "fsr_sharpness"), &Viewport::set_fsr_sharpness);
	ClassDB::bind_method(D_METHOD("get_fsr_sharpness"), &Viewport::get_fsr_sharpness);

	ClassDB::bind_method(D_METHOD("set_texture_mipmap_bias", "texture_mipmap_bias"), &Viewport::set_texture_mipmap_bias);
	ClassDB::bind_method(D_METHOD("get_texture_mipmap_bias"), &Viewport::get_texture_mipmap_bias);

	ClassDB::bind_method(D_METHOD("set_vrs_mode", "mode"), &Viewport::set_vrs_mode);
	ClassDB::bind_method(D_METHOD("get_vrs_mode"), &Viewport::get_vrs_mode);

	ClassDB::bind_method(D_METHOD("set_vrs_update_mode", "mode"), &Viewport::set_vrs_update_mode);
	ClassDB::bind_method(D_METHOD("get_vrs_update_mode"), &Viewport::get_vrs_update_mode);

	ClassDB::bind_method(D_METHOD("set_vrs_texture", "texture"), &Viewport::set_vrs_texture);
	ClassDB::bind_method(D_METHOD("get_vrs_texture"), &Viewport::get_vrs_texture);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disable_3d"), "set_disable_3d", "is_3d_disabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_xr"), "set_use_xr", "is_using_xr");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "own_world_3d"), "set_use_own_world_3d", "is_using_own_world_3d");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "world_3d", PROPERTY_HINT_RESOURCE_TYPE, "World3D"), "set_world_3d", "get_world_3d");
#endif // _3D_DISABLED
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "world_2d", PROPERTY_HINT_RESOURCE_TYPE, "World2D", PROPERTY_USAGE_NONE), "set_world_2d", "get_world_2d");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "transparent_bg"), "set_transparent_background", "has_transparent_background");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "handle_input_locally"), "set_handle_input_locally", "is_handling_input_locally");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "snap_2d_transforms_to_pixel"), "set_snap_2d_transforms_to_pixel", "is_snap_2d_transforms_to_pixel_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "snap_2d_vertices_to_pixel"), "set_snap_2d_vertices_to_pixel", "is_snap_2d_vertices_to_pixel_enabled");
	ADD_GROUP("Rendering", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "msaa_2d", PROPERTY_HINT_ENUM, String::utf8("Disabled (Fastest),2× (Average),4× (Slow),8× (Slowest)")), "set_msaa_2d", "get_msaa_2d");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "msaa_3d", PROPERTY_HINT_ENUM, String::utf8("Disabled (Fastest),2× (Average),4× (Slow),8× (Slowest)")), "set_msaa_3d", "get_msaa_3d");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "screen_space_aa", PROPERTY_HINT_ENUM, "Disabled (Fastest),FXAA (Fast)"), "set_screen_space_aa", "get_screen_space_aa");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_taa"), "set_use_taa", "is_using_taa");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_debanding"), "set_use_debanding", "is_using_debanding");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_occlusion_culling"), "set_use_occlusion_culling", "is_using_occlusion_culling");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mesh_lod_threshold", PROPERTY_HINT_RANGE, "0,1024,0.1"), "set_mesh_lod_threshold", "get_mesh_lod_threshold");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "debug_draw", PROPERTY_HINT_ENUM, "Disabled,Unshaded,Lighting,Overdraw,Wireframe,Normal Buffer,VoxelGI Albedo,VoxelGI Lighting,VoxelGI Emission,Shadow Atlas,Directional Shadow Map,Scene Luminance,SSAO,SSIL,Directional Shadow Splits,Decal Atlas,SDFGI Cascades,SDFGI Probes,VoxelGI/SDFGI Buffer,Disable Mesh LOD,OmniLight3D Cluster,SpotLight3D Cluster,Decal Cluster,ReflectionProbe Cluster,Occlusion Culling Buffer,Motion Vectors,Internal Buffer"), "set_debug_draw", "get_debug_draw");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_hdr_2d"), "set_use_hdr_2d", "is_using_hdr_2d");

#ifndef _3D_DISABLED
	ADD_GROUP("Scaling 3D", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "scaling_3d_mode", PROPERTY_HINT_ENUM, "Bilinear (Fastest),FSR 1.0 (Fast),FSR 2.2 (Slow)"), "set_scaling_3d_mode", "get_scaling_3d_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scaling_3d_scale", PROPERTY_HINT_RANGE, "0.25,2.0,0.01"), "set_scaling_3d_scale", "get_scaling_3d_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "texture_mipmap_bias", PROPERTY_HINT_RANGE, "-2,2,0.001"), "set_texture_mipmap_bias", "get_texture_mipmap_bias");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fsr_sharpness", PROPERTY_HINT_RANGE, "0,2,0.1"), "set_fsr_sharpness", "get_fsr_sharpness");
	ADD_GROUP("Variable Rate Shading", "vrs_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "vrs_mode", PROPERTY_HINT_ENUM, "Disabled,Texture,Depth buffer,XR"), "set_vrs_mode", "get_vrs_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "vrs_update_mode", PROPERTY_HINT_ENUM, "Disabled,Once,Always"), "set_vrs_update_mode", "get_vrs_update_mode");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "vrs_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_vrs_texture", "get_vrs_texture");
#endif
	ADD_GROUP("Canvas Items", "canvas_item_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "canvas_item_default_texture_filter", PROPERTY_HINT_ENUM, "Nearest,Linear,Linear Mipmap,Nearest Mipmap"), "set_default_canvas_item_texture_filter", "get_default_canvas_item_texture_filter");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "canvas_item_default_texture_repeat", PROPERTY_HINT_ENUM, "Disabled,Enabled,Mirror"), "set_default_canvas_item_texture_repeat", "get_default_canvas_item_texture_repeat");
	ADD_GROUP("Audio Listener", "audio_listener_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "audio_listener_enable_2d"), "set_as_audio_listener_2d", "is_audio_listener_2d");
#ifndef _3D_DISABLED
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "audio_listener_enable_3d"), "set_as_audio_listener_3d", "is_audio_listener_3d");
#endif // _3D_DISABLED
	ADD_GROUP("Physics", "physics_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "physics_object_picking"), "set_physics_object_picking", "get_physics_object_picking");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "physics_object_picking_sort"), "set_physics_object_picking_sort", "get_physics_object_picking_sort");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "physics_object_picking_first_only"), "set_physics_object_picking_first_only", "get_physics_object_picking_first_only");
	ADD_GROUP("GUI", "gui_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "gui_disable_input"), "set_disable_input", "is_input_disabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "gui_snap_controls_to_pixels"), "set_snap_controls_to_pixels", "is_snap_controls_to_pixels_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "gui_embed_subwindows"), "set_embedding_subwindows", "is_embedding_subwindows");
	ADD_GROUP("SDF", "sdf_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sdf_oversize", PROPERTY_HINT_ENUM, "100%,120%,150%,200%"), "set_sdf_oversize", "get_sdf_oversize");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sdf_scale", PROPERTY_HINT_ENUM, "100%,50%,25%"), "set_sdf_scale", "get_sdf_scale");
	ADD_GROUP("Positional Shadow Atlas", "positional_shadow_atlas_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "positional_shadow_atlas_size"), "set_positional_shadow_atlas_size", "get_positional_shadow_atlas_size");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "positional_shadow_atlas_16_bits"), "set_positional_shadow_atlas_16_bits", "get_positional_shadow_atlas_16_bits");
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "positional_shadow_atlas_quad_0", PROPERTY_HINT_ENUM, "Disabled,1 Shadow,4 Shadows,16 Shadows,64 Shadows,256 Shadows,1024 Shadows"), "set_positional_shadow_atlas_quadrant_subdiv", "get_positional_shadow_atlas_quadrant_subdiv", 0);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "positional_shadow_atlas_quad_1", PROPERTY_HINT_ENUM, "Disabled,1 Shadow,4 Shadows,16 Shadows,64 Shadows,256 Shadows,1024 Shadows"), "set_positional_shadow_atlas_quadrant_subdiv", "get_positional_shadow_atlas_quadrant_subdiv", 1);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "positional_shadow_atlas_quad_2", PROPERTY_HINT_ENUM, "Disabled,1 Shadow,4 Shadows,16 Shadows,64 Shadows,256 Shadows,1024 Shadows"), "set_positional_shadow_atlas_quadrant_subdiv", "get_positional_shadow_atlas_quadrant_subdiv", 2);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "positional_shadow_atlas_quad_3", PROPERTY_HINT_ENUM, "Disabled,1 Shadow,4 Shadows,16 Shadows,64 Shadows,256 Shadows,1024 Shadows"), "set_positional_shadow_atlas_quadrant_subdiv", "get_positional_shadow_atlas_quadrant_subdiv", 3);
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM2D, "canvas_transform", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_canvas_transform", "get_canvas_transform");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM2D, "global_canvas_transform", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_global_canvas_transform", "get_global_canvas_transform");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "canvas_cull_mask", PROPERTY_HINT_LAYERS_2D_RENDER), "set_canvas_cull_mask", "get_canvas_cull_mask");

	ADD_SIGNAL(MethodInfo("size_changed"));
	ADD_SIGNAL(MethodInfo("gui_focus_changed", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Control")));

	BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_DISABLED);
	BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_1);
	BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_4);
	BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_16);
	BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_64);
	BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_256);
	BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_1024);
	BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_MAX);

	BIND_ENUM_CONSTANT(SCALING_3D_MODE_BILINEAR);
	BIND_ENUM_CONSTANT(SCALING_3D_MODE_FSR);
	BIND_ENUM_CONSTANT(SCALING_3D_MODE_FSR2);
	BIND_ENUM_CONSTANT(SCALING_3D_MODE_MAX);

	BIND_ENUM_CONSTANT(MSAA_DISABLED);
	BIND_ENUM_CONSTANT(MSAA_2X);
	BIND_ENUM_CONSTANT(MSAA_4X);
	BIND_ENUM_CONSTANT(MSAA_8X);
	BIND_ENUM_CONSTANT(MSAA_MAX);

	BIND_ENUM_CONSTANT(SCREEN_SPACE_AA_DISABLED);
	BIND_ENUM_CONSTANT(SCREEN_SPACE_AA_FXAA);
	BIND_ENUM_CONSTANT(SCREEN_SPACE_AA_MAX);

	BIND_ENUM_CONSTANT(RENDER_INFO_OBJECTS_IN_FRAME);
	BIND_ENUM_CONSTANT(RENDER_INFO_PRIMITIVES_IN_FRAME);
	BIND_ENUM_CONSTANT(RENDER_INFO_DRAW_CALLS_IN_FRAME);
	BIND_ENUM_CONSTANT(RENDER_INFO_MAX);

	BIND_ENUM_CONSTANT(RENDER_INFO_TYPE_VISIBLE);
	BIND_ENUM_CONSTANT(RENDER_INFO_TYPE_SHADOW);
	BIND_ENUM_CONSTANT(RENDER_INFO_TYPE_CANVAS);
	BIND_ENUM_CONSTANT(RENDER_INFO_TYPE_MAX);

	BIND_ENUM_CONSTANT(DEBUG_DRAW_DISABLED);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_UNSHADED);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_LIGHTING);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_OVERDRAW);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_WIREFRAME);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_NORMAL_BUFFER);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_VOXEL_GI_ALBEDO);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_VOXEL_GI_LIGHTING);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_VOXEL_GI_EMISSION);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_SHADOW_ATLAS);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_DIRECTIONAL_SHADOW_ATLAS);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_SCENE_LUMINANCE);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_SSAO);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_SSIL);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_PSSM_SPLITS);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_DECAL_ATLAS);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_SDFGI);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_SDFGI_PROBES);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_GI_BUFFER);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_DISABLE_LOD);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_CLUSTER_OMNI_LIGHTS);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_CLUSTER_SPOT_LIGHTS);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_CLUSTER_DECALS);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_CLUSTER_REFLECTION_PROBES);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_OCCLUDERS)
	BIND_ENUM_CONSTANT(DEBUG_DRAW_MOTION_VECTORS)
	BIND_ENUM_CONSTANT(DEBUG_DRAW_INTERNAL_BUFFER);

	BIND_ENUM_CONSTANT(DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
	BIND_ENUM_CONSTANT(DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_LINEAR);
	BIND_ENUM_CONSTANT(DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS);
	BIND_ENUM_CONSTANT(DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS);
	BIND_ENUM_CONSTANT(DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_MAX);

	BIND_ENUM_CONSTANT(DEFAULT_CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
	BIND_ENUM_CONSTANT(DEFAULT_CANVAS_ITEM_TEXTURE_REPEAT_ENABLED);
	BIND_ENUM_CONSTANT(DEFAULT_CANVAS_ITEM_TEXTURE_REPEAT_MIRROR);
	BIND_ENUM_CONSTANT(DEFAULT_CANVAS_ITEM_TEXTURE_REPEAT_MAX);

	BIND_ENUM_CONSTANT(SDF_OVERSIZE_100_PERCENT);
	BIND_ENUM_CONSTANT(SDF_OVERSIZE_120_PERCENT);
	BIND_ENUM_CONSTANT(SDF_OVERSIZE_150_PERCENT);
	BIND_ENUM_CONSTANT(SDF_OVERSIZE_200_PERCENT);
	BIND_ENUM_CONSTANT(SDF_OVERSIZE_MAX);

	BIND_ENUM_CONSTANT(SDF_SCALE_100_PERCENT);
	BIND_ENUM_CONSTANT(SDF_SCALE_50_PERCENT);
	BIND_ENUM_CONSTANT(SDF_SCALE_25_PERCENT);
	BIND_ENUM_CONSTANT(SDF_SCALE_MAX);

	BIND_ENUM_CONSTANT(VRS_DISABLED);
	BIND_ENUM_CONSTANT(VRS_TEXTURE);
	BIND_ENUM_CONSTANT(VRS_XR);
	BIND_ENUM_CONSTANT(VRS_MAX);

	BIND_ENUM_CONSTANT(VRS_UPDATE_DISABLED);
	BIND_ENUM_CONSTANT(VRS_UPDATE_ONCE);
	BIND_ENUM_CONSTANT(VRS_UPDATE_ALWAYS);
	BIND_ENUM_CONSTANT(VRS_UPDATE_MAX);
}

void Viewport::_validate_property(PropertyInfo &p_property) const {
	if (vrs_mode != VRS_TEXTURE && (p_property.name == "vrs_texture")) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}

	if (vrs_mode == VRS_DISABLED && (p_property.name == "vrs_update_mode")) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

Viewport::Viewport() {
	world_2d.instantiate();
	world_2d->register_viewport(this);

	viewport = RenderingServer::get_singleton()->viewport_create();
	texture_rid = RenderingServer::get_singleton()->viewport_get_texture(viewport);

	default_texture.instantiate();
	default_texture->vp = this;
	viewport_textures.insert(default_texture.ptr());
	default_texture->proxy = RS::get_singleton()->texture_proxy_create(texture_rid);

	canvas_layers.insert(nullptr); // This eases picking code (interpreted as the canvas of the Viewport).

	set_positional_shadow_atlas_size(positional_shadow_atlas_size);

	for (int i = 0; i < 4; i++) {
		positional_shadow_atlas_quadrant_subdiv[i] = SHADOW_ATLAS_QUADRANT_SUBDIV_MAX;
	}
	set_positional_shadow_atlas_quadrant_subdiv(0, SHADOW_ATLAS_QUADRANT_SUBDIV_4);
	set_positional_shadow_atlas_quadrant_subdiv(1, SHADOW_ATLAS_QUADRANT_SUBDIV_4);
	set_positional_shadow_atlas_quadrant_subdiv(2, SHADOW_ATLAS_QUADRANT_SUBDIV_16);
	set_positional_shadow_atlas_quadrant_subdiv(3, SHADOW_ATLAS_QUADRANT_SUBDIV_64);

	set_mesh_lod_threshold(mesh_lod_threshold);

	String id = itos(get_instance_id());
	input_group = "_vp_input" + id;
	unhandled_input_group = "_vp_unhandled_input" + id;
	shortcut_input_group = "_vp_shortcut_input" + id;
	unhandled_key_input_group = "_vp_unhandled_key_input" + id;

	// Window tooltip.
	gui.tooltip_delay = GLOBAL_GET("gui/timers/tooltip_delay_sec");

#ifndef _3D_DISABLED
	set_scaling_3d_mode((Viewport::Scaling3DMode)(int)GLOBAL_GET("rendering/scaling_3d/mode"));
	set_scaling_3d_scale(GLOBAL_GET("rendering/scaling_3d/scale"));
	set_fsr_sharpness((float)GLOBAL_GET("rendering/scaling_3d/fsr_sharpness"));
	set_texture_mipmap_bias((float)GLOBAL_GET("rendering/textures/default_filters/texture_mipmap_bias"));
#endif // _3D_DISABLED

	set_sdf_oversize(sdf_oversize); // Set to server.

	// Physics interpolation mode for viewports is a special case.
	// Typically viewports will be housed within Controls,
	// and Controls default to PHYSICS_INTERPOLATION_MODE_OFF.
	// Viewports can thus inherit physics interpolation OFF, which is unexpected.
	// Setting to ON allows each viewport to have a fresh interpolation state.
	set_physics_interpolation_mode(Node::PHYSICS_INTERPOLATION_MODE_ON);
}

Viewport::~Viewport() {
	// Erase itself from viewport textures.
	for (ViewportTexture *E : viewport_textures) {
		E->vp = nullptr;
	}
	if (world_2d.is_valid()) {
		world_2d->remove_viewport(this);
	}
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RenderingServer::get_singleton()->free(viewport);
}

/////////////////////////////////

void SubViewport::set_size(const Size2i &p_size) {
	ERR_MAIN_THREAD_GUARD;
	_internal_set_size(p_size);
}

void SubViewport::set_size_force(const Size2i &p_size) {
	ERR_MAIN_THREAD_GUARD;
	// Use only for setting the size from the parent SubViewportContainer with enabled stretch mode.
	// Don't expose function to scripting.
	_internal_set_size(p_size, true);
}

void SubViewport::_internal_set_size(const Size2i &p_size, bool p_force) {
	SubViewportContainer *c = Object::cast_to<SubViewportContainer>(get_parent());
	if (!p_force && c && c->is_stretch_enabled()) {
#ifdef DEBUG_ENABLED
		WARN_PRINT("Can't change the size of a `SubViewport` with a `SubViewportContainer` parent that has `stretch` enabled. Set `SubViewportContainer.stretch` to `false` to allow changing the size manually.");
#endif // DEBUG_ENABLED
		return;
	}

	_set_size(p_size, _get_size_2d_override(), true);

	if (c) {
		c->update_minimum_size();
		c->queue_redraw();
	}
}

Size2i SubViewport::get_size() const {
	ERR_READ_THREAD_GUARD_V(Size2());
	return _get_size();
}

void SubViewport::set_size_2d_override(const Size2i &p_size) {
	ERR_MAIN_THREAD_GUARD;
	_set_size(_get_size(), p_size, true);
}

Size2i SubViewport::get_size_2d_override() const {
	ERR_READ_THREAD_GUARD_V(Size2i());
	return _get_size_2d_override();
}

void SubViewport::set_size_2d_override_stretch(bool p_enable) {
	ERR_MAIN_THREAD_GUARD;
	if (p_enable == size_2d_override_stretch) {
		return;
	}

	size_2d_override_stretch = p_enable;
	_set_size(_get_size(), _get_size_2d_override(), true);
}

bool SubViewport::is_size_2d_override_stretch_enabled() const {
	ERR_READ_THREAD_GUARD_V(false);
	return size_2d_override_stretch;
}

void SubViewport::set_update_mode(UpdateMode p_mode) {
	ERR_MAIN_THREAD_GUARD;
	update_mode = p_mode;
	RS::get_singleton()->viewport_set_update_mode(get_viewport_rid(), RS::ViewportUpdateMode(p_mode));
}

SubViewport::UpdateMode SubViewport::get_update_mode() const {
	ERR_READ_THREAD_GUARD_V(UPDATE_DISABLED);
	return update_mode;
}

void SubViewport::set_clear_mode(ClearMode p_mode) {
	ERR_MAIN_THREAD_GUARD;
	clear_mode = p_mode;
	RS::get_singleton()->viewport_set_clear_mode(get_viewport_rid(), RS::ViewportClearMode(p_mode));
}

SubViewport::ClearMode SubViewport::get_clear_mode() const {
	ERR_READ_THREAD_GUARD_V(CLEAR_MODE_ALWAYS);
	return clear_mode;
}

DisplayServer::WindowID SubViewport::get_window_id() const {
	ERR_READ_THREAD_GUARD_V(DisplayServer::INVALID_WINDOW_ID);
	return DisplayServer::INVALID_WINDOW_ID;
}

Transform2D SubViewport::get_screen_transform_internal(bool p_absolute_position) const {
	ERR_READ_THREAD_GUARD_V(Transform2D());
	Transform2D container_transform;
	SubViewportContainer *c = Object::cast_to<SubViewportContainer>(get_parent());
	if (c) {
		if (c->is_stretch_enabled()) {
			container_transform.scale(Vector2(c->get_stretch_shrink(), c->get_stretch_shrink()));
		}
		container_transform = c->get_viewport()->get_screen_transform_internal(p_absolute_position) * c->get_global_transform_with_canvas() * container_transform;
	} else {
		WARN_PRINT_ONCE("SubViewport is not a child of a SubViewportContainer. get_screen_transform doesn't return the actual screen position.");
	}
	return container_transform * get_final_transform();
}

Transform2D SubViewport::get_popup_base_transform() const {
	ERR_READ_THREAD_GUARD_V(Transform2D());
	if (is_embedding_subwindows()) {
		return Transform2D();
	}
	SubViewportContainer *c = Object::cast_to<SubViewportContainer>(get_parent());
	if (!c) {
		return get_final_transform();
	}
	Transform2D container_transform;
	if (c->is_stretch_enabled()) {
		container_transform.scale(Vector2(c->get_stretch_shrink(), c->get_stretch_shrink()));
	}
	return c->get_screen_transform() * container_transform * get_final_transform();
}

Viewport *SubViewport::get_section_root_viewport() const {
	if (Object::cast_to<SubViewportContainer>(get_parent()) && get_parent()->get_viewport()) {
		return get_parent()->get_viewport()->get_section_root_viewport();
	}
	SubViewport *vp = const_cast<SubViewport *>(this);
	return vp;
}

bool SubViewport::is_attached_in_viewport() const {
	return Object::cast_to<SubViewportContainer>(get_parent());
}

void SubViewport::_notification(int p_what) {
	ERR_MAIN_THREAD_GUARD;
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			RS::get_singleton()->viewport_set_active(get_viewport_rid(), true);

			SubViewportContainer *parent_svc = Object::cast_to<SubViewportContainer>(get_parent());
			if (parent_svc) {
				parent_svc->recalc_force_viewport_sizes();
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			RS::get_singleton()->viewport_set_active(get_viewport_rid(), false);
		} break;
	}
}

void SubViewport::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_size", "size"), &SubViewport::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &SubViewport::get_size);

	ClassDB::bind_method(D_METHOD("set_size_2d_override", "size"), &SubViewport::set_size_2d_override);
	ClassDB::bind_method(D_METHOD("get_size_2d_override"), &SubViewport::get_size_2d_override);

	ClassDB::bind_method(D_METHOD("set_size_2d_override_stretch", "enable"), &SubViewport::set_size_2d_override_stretch);
	ClassDB::bind_method(D_METHOD("is_size_2d_override_stretch_enabled"), &SubViewport::is_size_2d_override_stretch_enabled);

	ClassDB::bind_method(D_METHOD("set_update_mode", "mode"), &SubViewport::set_update_mode);
	ClassDB::bind_method(D_METHOD("get_update_mode"), &SubViewport::get_update_mode);

	ClassDB::bind_method(D_METHOD("set_clear_mode", "mode"), &SubViewport::set_clear_mode);
	ClassDB::bind_method(D_METHOD("get_clear_mode"), &SubViewport::get_clear_mode);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "size", PROPERTY_HINT_NONE, "suffix:px"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "size_2d_override", PROPERTY_HINT_NONE, "suffix:px"), "set_size_2d_override", "get_size_2d_override");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "size_2d_override_stretch"), "set_size_2d_override_stretch", "is_size_2d_override_stretch_enabled");
	ADD_GROUP("Render Target", "render_target_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "render_target_clear_mode", PROPERTY_HINT_ENUM, "Always,Never,Next Frame"), "set_clear_mode", "get_clear_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "render_target_update_mode", PROPERTY_HINT_ENUM, "Disabled,Once,When Visible,When Parent Visible,Always"), "set_update_mode", "get_update_mode");

	BIND_ENUM_CONSTANT(CLEAR_MODE_ALWAYS);
	BIND_ENUM_CONSTANT(CLEAR_MODE_NEVER);
	BIND_ENUM_CONSTANT(CLEAR_MODE_ONCE);

	BIND_ENUM_CONSTANT(UPDATE_DISABLED);
	BIND_ENUM_CONSTANT(UPDATE_ONCE);
	BIND_ENUM_CONSTANT(UPDATE_WHEN_VISIBLE);
	BIND_ENUM_CONSTANT(UPDATE_WHEN_PARENT_VISIBLE);
	BIND_ENUM_CONSTANT(UPDATE_ALWAYS);
}

void SubViewport::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "size") {
		SubViewportContainer *parent_svc = Object::cast_to<SubViewportContainer>(get_parent());
		if (parent_svc && parent_svc->is_stretch_enabled()) {
			p_property.usage = PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_READ_ONLY;
		} else {
			p_property.usage = PROPERTY_USAGE_DEFAULT;
		}
	}
}

SubViewport::SubViewport() {
	RS::get_singleton()->viewport_set_size(get_viewport_rid(), get_size().width, get_size().height);
}

SubViewport::~SubViewport() {}

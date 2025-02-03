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

#include "core/core_string_names.h"
#include "core/os/input.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "scene/2d/collision_object_2d.h"
#include "scene/2d/listener_2d.h"
#include "scene/3d/camera.h"
#include "scene/3d/collision_object.h"
#include "scene/3d/listener.h"
#include "scene/3d/spatial.h"
#include "scene/3d/world_environment.h"
#include "scene/gui/control.h"
#include "scene/gui/label.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/panel.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/viewport_container.h"
#include "scene/main/canvas_layer.h"
#include "scene/main/timer.h"
#include "scene/resources/mesh.h"
#include "scene/scene_string_names.h"
#include "servers/physics_2d_server.h"

LocalVector<Viewport::GUI::CanvasParent> Viewport::GUI::canvas_parents_dirty_order;

void ViewportTexture::setup_local_to_scene() {
	Node *local_scene = get_local_scene();
	if (!local_scene) {
		return;
	}

	if (vp) {
		vp->viewport_textures.erase(this);
	}

	vp = nullptr;

	Node *vpn = local_scene->get_node(path);
	ERR_FAIL_COND_MSG(!vpn, "ViewportTexture: Path to node is invalid.");

	vp = Object::cast_to<Viewport>(vpn);

	ERR_FAIL_COND_MSG(!vp, "ViewportTexture: Path to node does not point to a viewport.");

	vp->viewport_textures.insert(this);

	VS::get_singleton()->texture_set_proxy(proxy, vp->texture_rid);

	vp->texture_flags = flags;
	VS::get_singleton()->texture_set_flags(vp->texture_rid, flags);
}

void ViewportTexture::set_viewport_path_in_scene(const NodePath &p_path) {
	if (path == p_path) {
		return;
	}

	path = p_path;

	if (get_local_scene()) {
		setup_local_to_scene();
	}
}

NodePath ViewportTexture::get_viewport_path_in_scene() const {
	return path;
}

int ViewportTexture::get_width() const {
	ERR_FAIL_COND_V_MSG(!vp, 0, "Viewport Texture must be set to use it.");
	return vp->size.width;
}
int ViewportTexture::get_height() const {
	ERR_FAIL_COND_V_MSG(!vp, 0, "Viewport Texture must be set to use it.");
	return vp->size.height;
}
Size2 ViewportTexture::get_size() const {
	ERR_FAIL_COND_V_MSG(!vp, Size2(), "Viewport Texture must be set to use it.");
	return vp->size;
}
RID ViewportTexture::get_rid() const {
	//ERR_FAIL_COND_V_MSG(!vp, RID(), "Viewport Texture must be set to use it.");
	return proxy;
}

bool ViewportTexture::has_alpha() const {
	return false;
}
Ref<Image> ViewportTexture::get_data() const {
	ERR_FAIL_COND_V_MSG(!vp, Ref<Image>(), "Viewport Texture must be set to use it.");
	return VS::get_singleton()->texture_get_data(vp->texture_rid);
}
void ViewportTexture::set_flags(uint32_t p_flags) {
	flags = p_flags;

	if (!vp) {
		return;
	}

	vp->texture_flags = flags;
	VS::get_singleton()->texture_set_flags(vp->texture_rid, flags);
}

uint32_t ViewportTexture::get_flags() const {
	return flags;
}

void ViewportTexture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_viewport_path_in_scene", "path"), &ViewportTexture::set_viewport_path_in_scene);
	ClassDB::bind_method(D_METHOD("get_viewport_path_in_scene"), &ViewportTexture::get_viewport_path_in_scene);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "viewport_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Viewport", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NODE_PATH_FROM_SCENE_ROOT), "set_viewport_path_in_scene", "get_viewport_path_in_scene");
}

ViewportTexture::ViewportTexture() {
	vp = nullptr;
	flags = 0;
	set_local_to_scene(true);
	proxy = RID_PRIME(VS::get_singleton()->texture_create());
}

ViewportTexture::~ViewportTexture() {
	if (vp) {
		vp->viewport_textures.erase(this);
	}

	VS::get_singleton()->free(proxy);
}

/////////////////////////////////////

// Aliases used to provide custom styles to tooltips in the default
// theme and editor theme.
// TooltipPanel is also used for custom tooltips, while TooltipLabel
// is only relevant for default tooltips.

class TooltipPanel : public PanelContainer {
	GDCLASS(TooltipPanel, PanelContainer);

public:
	TooltipPanel(){};
};

class TooltipLabel : public Label {
	GDCLASS(TooltipLabel, Label);

public:
	TooltipLabel(){};
};

/////////////////////////////////////

Viewport::GUI::GUI() {
	dragging = false;
	drag_successful = false;
	mouse_in_window = true;
	mouse_focus = nullptr;
	mouse_click_grabber = nullptr;
	mouse_focus_mask = 0;
	key_focus = nullptr;
	mouse_over = nullptr;

	tooltip_control = nullptr;
	tooltip_popup = nullptr;
	tooltip_label = nullptr;
	subwindow_visibility_dirty = false;
	subwindow_order_dirty = false;
}

/////////////////////////////////////

void Viewport::_update_stretch_transform() {
	if (size_override_stretch && size_override) {
		stretch_transform = Transform2D();
		Size2 scale = size / (size_override_size + size_override_margin * 2);
		stretch_transform.scale(scale);
		stretch_transform.elements[2] = size_override_margin * scale;

	} else {
		stretch_transform = Transform2D();
	}

	_update_global_transform();
}

void Viewport::update_worlds() {
	if (!is_inside_tree()) {
		return;
	}

	Rect2 abstracted_rect = Rect2(Vector2(), get_visible_rect().size);
	Rect2 xformed_rect = (global_canvas_transform * canvas_transform).affine_inverse().xform(abstracted_rect);
	find_world_2d()->_update_viewport(this, xformed_rect);
	find_world_2d()->_update();

	find_world()->_update(get_tree()->get_frame());
}

void Viewport::_collision_object_input_event(CollisionObject *p_object, Camera *p_camera, const Ref<InputEvent> &p_input_event, const Vector3 &p_pos, const Vector3 &p_normal, int p_shape) {
	Transform object_transform = p_object->get_global_transform();
	Transform camera_transform = p_camera->get_global_transform();
	ObjectID id = p_object->get_instance_id();

	//avoid sending the fake event unnecessarily if nothing really changed in the context
	if (object_transform == physics_last_object_transform && camera_transform == physics_last_camera_transform && physics_last_id == id) {
		Ref<InputEventMouseMotion> mm = p_input_event;
		if (mm.is_valid() && mm->get_device() == InputEvent::DEVICE_ID_INTERNAL) {
			return; //discarded
		}
	}
	p_object->_input_event(camera, p_input_event, p_pos, p_normal, p_shape);
	physics_last_object_transform = object_transform;
	physics_last_camera_transform = camera_transform;
	physics_last_id = id;
}

void Viewport::_own_world_changed() {
	ERR_FAIL_COND(world.is_null());
	ERR_FAIL_COND(own_world.is_null());

	if (is_inside_tree()) {
		_propagate_exit_world(this);
	}

	own_world = world->duplicate();

	if (is_inside_tree()) {
		_propagate_enter_world(this);
	}

	if (is_inside_tree()) {
		VisualServer::get_singleton()->viewport_set_scenario(viewport, find_world()->get_scenario());
	}

	_update_listener();
}

void Viewport::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (get_parent()) {
				parent = get_parent()->get_viewport();
				VisualServer::get_singleton()->viewport_set_parent_viewport(viewport, parent->get_viewport_rid());
			} else {
				parent = nullptr;
			}

			current_canvas = find_world_2d()->get_canvas();
			VisualServer::get_singleton()->viewport_set_scenario(viewport, find_world()->get_scenario());
			VisualServer::get_singleton()->viewport_attach_canvas(viewport, current_canvas);

			_update_listener();
			_update_listener_2d();

			find_world_2d()->_register_viewport(this, Rect2());

			add_to_group("_viewports");
			if (get_tree()->is_debugging_collisions_hint()) {
				//2D
				Physics2DServer::get_singleton()->space_set_debug_contacts(find_world_2d()->get_space(), get_tree()->get_collision_debug_contact_count());
				contact_2d_debug = RID_PRIME(VisualServer::get_singleton()->canvas_item_create());
				VisualServer::get_singleton()->canvas_item_set_parent(contact_2d_debug, find_world_2d()->get_canvas());
				//3D
				PhysicsServer::get_singleton()->space_set_debug_contacts(find_world()->get_space(), get_tree()->get_collision_debug_contact_count());
				contact_3d_debug_multimesh = RID_PRIME(VisualServer::get_singleton()->multimesh_create());
				VisualServer::get_singleton()->multimesh_allocate(contact_3d_debug_multimesh, get_tree()->get_collision_debug_contact_count(), VS::MULTIMESH_TRANSFORM_3D, VS::MULTIMESH_COLOR_8BIT);
				VisualServer::get_singleton()->multimesh_set_visible_instances(contact_3d_debug_multimesh, 0);
				VisualServer::get_singleton()->multimesh_set_mesh(contact_3d_debug_multimesh, get_tree()->get_debug_contact_mesh()->get_rid());
				contact_3d_debug_instance = RID_PRIME(VisualServer::get_singleton()->instance_create());
				VisualServer::get_singleton()->instance_set_base(contact_3d_debug_instance, contact_3d_debug_multimesh);
				VisualServer::get_singleton()->instance_set_scenario(contact_3d_debug_instance, find_world()->get_scenario());
				//VisualServer::get_singleton()->instance_geometry_set_flag(contact_3d_debug_instance, VS::INSTANCE_FLAG_VISIBLE_IN_ALL_ROOMS, true);
			}

			VS::get_singleton()->viewport_set_active(viewport, true);
		} break;
		case NOTIFICATION_READY: {
#ifndef _3D_DISABLED
			if (listeners.size() && !listener) {
				Listener *first = nullptr;
				for (Set<Listener *>::Element *E = listeners.front(); E; E = E->next()) {
					if (first == nullptr || first->is_greater_than(E->get())) {
						first = E->get();
					}
				}

				if (first) {
					first->make_current();
				}
			}

			if (cameras.size() && !camera) {
				//there are cameras but no current camera, pick first in tree and make it current
				Camera *first = nullptr;
				for (Set<Camera *>::Element *E = cameras.front(); E; E = E->next()) {
					if (first == nullptr || first->is_greater_than(E->get())) {
						first = E->get();
					}
				}

				if (first) {
					first->make_current();
				}
			}
#endif

			// Enable processing for tooltips, collision debugging, physics object picking, etc.
			set_physics_process_internal(true);

		} break;
		case NOTIFICATION_EXIT_TREE: {
			_gui_cancel_tooltip();
			if (world_2d.is_valid()) {
				world_2d->_remove_viewport(this);
			}

			VisualServer::get_singleton()->viewport_set_scenario(viewport, RID());
			//			SpatialSoundServer::get_singleton()->listener_set_space(internal_listener, RID());
			VisualServer::get_singleton()->viewport_remove_canvas(viewport, current_canvas);
			if (contact_2d_debug.is_valid()) {
				VisualServer::get_singleton()->free(contact_2d_debug);
				contact_2d_debug = RID();
			}

			if (contact_3d_debug_multimesh.is_valid()) {
				VisualServer::get_singleton()->free(contact_3d_debug_multimesh);
				VisualServer::get_singleton()->free(contact_3d_debug_instance);
				contact_3d_debug_instance = RID();
				contact_3d_debug_multimesh = RID();
			}

			remove_from_group("_viewports");

			VS::get_singleton()->viewport_set_active(viewport, false);
		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (!get_tree()) {
				return;
			}

			if (get_tree()->is_debugging_collisions_hint() && contact_2d_debug.is_valid()) {
				VisualServer::get_singleton()->canvas_item_clear(contact_2d_debug);
				VisualServer::get_singleton()->canvas_item_set_draw_index(contact_2d_debug, 0xFFFFF); //very high index

				Vector<Vector2> points = Physics2DServer::get_singleton()->space_get_contacts(find_world_2d()->get_space());
				int point_count = Physics2DServer::get_singleton()->space_get_contact_count(find_world_2d()->get_space());
				Color ccol = get_tree()->get_debug_collision_contact_color();

				for (int i = 0; i < point_count; i++) {
					VisualServer::get_singleton()->canvas_item_add_rect(contact_2d_debug, Rect2(points[i] - Vector2(2, 2), Vector2(5, 5)), ccol);
				}
			}

			if (get_tree()->is_debugging_collisions_hint() && contact_3d_debug_multimesh.is_valid()) {
				Vector<Vector3> points = PhysicsServer::get_singleton()->space_get_contacts(find_world()->get_space());
				int point_count = PhysicsServer::get_singleton()->space_get_contact_count(find_world()->get_space());

				VS::get_singleton()->multimesh_set_visible_instances(contact_3d_debug_multimesh, point_count);

				for (int i = 0; i < point_count; i++) {
					Transform point_transform;
					point_transform.origin = points[i];
					VS::get_singleton()->multimesh_instance_set_transform(contact_3d_debug_multimesh, i, point_transform);
				}
			}

			if (!GLOBAL_GET("physics/common/enable_pause_aware_picking")) {
				_process_picking(false);
			}
		} break;
		case NOTIFICATION_WM_MOUSE_ENTER: {
			gui.mouse_in_window = true;
		} break;
		case NOTIFICATION_WM_MOUSE_EXIT: {
			gui.mouse_in_window = false;
			_drop_physics_mouseover();
			_drop_mouse_over();
			// When the mouse exits the window, we want to end mouse_over, but
			// not mouse_focus, because, for example, we want to continue
			// dragging a scrollbar even if the mouse has left the window.
		} break;
		case NOTIFICATION_WM_FOCUS_OUT: {
			_drop_physics_mouseover();
			if (gui.mouse_focus) {
				_drop_mouse_focus();
			}
			// When the window focus changes, we want to end mouse_focus, but
			// not the mouse_over. Note: The OS will trigger a separate mouse
			// exit event if the change in focus results in the mouse exiting
			// the window.
		} break;
	}
}

void Viewport::_process_picking(bool p_ignore_paused) {
	if (!is_inside_tree()) {
		return;
	}
	if (!physics_object_picking) {
		return;
	}
	if (to_screen_rect != Rect2() && Input::get_singleton()->get_mouse_mode() == Input::MOUSE_MODE_CAPTURED) {
		return;
	}

	if (p_ignore_paused) {
		_drop_physics_mouseover(true);
	}

#ifndef _3D_DISABLED
	Vector2 last_pos(1e20, 1e20);
	CollisionObject *last_object = nullptr;
	ObjectID last_id = 0;
#endif
	PhysicsDirectSpaceState::RayResult result;
	Physics2DDirectSpaceState *ss2d = Physics2DServer::get_singleton()->space_get_direct_state(find_world_2d()->get_space());

	if (physics_has_last_mousepos) {
		// if no mouse event exists, create a motion one. This is necessary because objects or camera may have moved.
		// while this extra event is sent, it is checked if both camera and last object and last ID did not move. If nothing changed, the event is discarded to avoid flooding with unnecessary motion events every frame
		bool has_mouse_event = false;
		for (List<Ref<InputEvent>>::Element *E = physics_picking_events.front(); E; E = E->next()) {
			Ref<InputEventMouse> m = E->get();
			if (m.is_valid()) {
				has_mouse_event = true;
				break;
			}
		}

		if (!has_mouse_event) {
			Ref<InputEventMouseMotion> mm;
			mm.instance();
			mm->set_device(InputEvent::DEVICE_ID_INTERNAL);
			mm->set_global_position(physics_last_mousepos);
			mm->set_position(physics_last_mousepos);
			mm->set_alt(physics_last_mouse_state.alt);
			mm->set_shift(physics_last_mouse_state.shift);
			mm->set_control(physics_last_mouse_state.control);
			mm->set_metakey(physics_last_mouse_state.meta);
			mm->set_button_mask(physics_last_mouse_state.mouse_mask);
			physics_picking_events.push_back(mm);
		}
	}

	while (physics_picking_events.size()) {
		Ref<InputEvent> ev = physics_picking_events.front()->get();
		physics_picking_events.pop_front();

		Vector2 pos;
		bool is_mouse = false;

		Ref<InputEventMouseMotion> mm = ev;

		if (mm.is_valid()) {
			pos = mm->get_position();
			is_mouse = true;

			physics_has_last_mousepos = true;
			physics_last_mousepos = pos;
			physics_last_mouse_state.alt = mm->get_alt();
			physics_last_mouse_state.shift = mm->get_shift();
			physics_last_mouse_state.control = mm->get_control();
			physics_last_mouse_state.meta = mm->get_metakey();
			physics_last_mouse_state.mouse_mask = mm->get_button_mask();
		}

		Ref<InputEventMouseButton> mb = ev;

		if (mb.is_valid()) {
			pos = mb->get_position();
			is_mouse = true;

			physics_has_last_mousepos = true;
			physics_last_mousepos = pos;
			physics_last_mouse_state.alt = mb->get_alt();
			physics_last_mouse_state.shift = mb->get_shift();
			physics_last_mouse_state.control = mb->get_control();
			physics_last_mouse_state.meta = mb->get_metakey();

			if (mb->is_pressed()) {
				physics_last_mouse_state.mouse_mask |= (1 << (mb->get_button_index() - 1));
			} else {
				physics_last_mouse_state.mouse_mask &= ~(1 << (mb->get_button_index() - 1));

				// If touch mouse raised, assume we don't know last mouse pos until new events come
				if (mb->get_device() == InputEvent::DEVICE_ID_TOUCH_MOUSE) {
					physics_has_last_mousepos = false;
				}
			}
		}

		Ref<InputEventKey> k = ev;
		if (k.is_valid()) {
			//only for mask
			physics_last_mouse_state.alt = k->get_alt();
			physics_last_mouse_state.shift = k->get_shift();
			physics_last_mouse_state.control = k->get_control();
			physics_last_mouse_state.meta = k->get_metakey();
			continue;
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
			//send to 2D

			uint64_t frame = get_tree()->get_frame();

			Physics2DDirectSpaceState::ShapeResult res[64];
			for (Set<CanvasLayer *>::Element *E = canvas_layers.front(); E; E = E->next()) {
				Transform2D canvas_transform;
				ObjectID canvas_layer_id;
				if (E->get()) {
					// A descendant CanvasLayer
					canvas_transform = E->get()->get_final_transform();
					canvas_layer_id = E->get()->get_instance_id();
				} else {
					// This Viewport's builtin canvas
					canvas_transform = get_canvas_transform();
					canvas_layer_id = 0;
				}

				Vector2 point = canvas_transform.affine_inverse().xform(pos);

				int rc = ss2d->intersect_point_on_canvas(point, canvas_layer_id, res, 64, Set<RID>(), 0xFFFFFFFF, true, true, true);
				for (int i = 0; i < rc; i++) {
					if (res[i].collider_id && res[i].collider) {
						CollisionObject2D *co = Object::cast_to<CollisionObject2D>(res[i].collider);
						if (co && (!p_ignore_paused || co->can_process())) {
							bool send_event = true;
							if (is_mouse) {
								Map<ObjectID, uint64_t>::Element *F = physics_2d_mouseover.find(res[i].collider_id);

								if (!F) {
									physics_2d_mouseover.insert(res[i].collider_id, frame);
									co->_mouse_enter();
								} else {
									F->get() = frame;
									// It was already hovered, so don't send the event if it's faked
									if (mm.is_valid() && mm->get_device() == InputEvent::DEVICE_ID_INTERNAL) {
										send_event = false;
									}
								}
							}

							if (send_event) {
								co->_input_event(this, ev, res[i].shape);
							}
						}
					}
				}
			}

			if (is_mouse) {
				List<Map<ObjectID, uint64_t>::Element *> to_erase;

				for (Map<ObjectID, uint64_t>::Element *E = physics_2d_mouseover.front(); E; E = E->next()) {
					if (E->get() != frame) {
						Object *o = ObjectDB::get_instance(E->key());
						if (o) {
							CollisionObject2D *co = Object::cast_to<CollisionObject2D>(o);
							if (co) {
								co->_mouse_exit();
							}
						}
						to_erase.push_back(E);
					}
				}

				while (to_erase.size()) {
					physics_2d_mouseover.erase(to_erase.front()->get());
					to_erase.pop_front();
				}
			}
		}

#ifndef _3D_DISABLED
		bool captured = false;

		if (physics_object_capture != 0) {
			CollisionObject *co = Object::cast_to<CollisionObject>(ObjectDB::get_instance(physics_object_capture));
			if (co && camera) {
				_collision_object_input_event(co, camera, ev, Vector3(), Vector3(), 0);
				captured = true;
				if (mb.is_valid() && mb->get_button_index() == 1 && !mb->is_pressed()) {
					physics_object_capture = 0;
				}

			} else {
				physics_object_capture = 0;
			}
		}

		if (captured) {
			//none
		} else if (pos == last_pos) {
			if (last_id) {
				if (ObjectDB::get_instance(last_id) && last_object) {
					//good, exists
					_collision_object_input_event(last_object, camera, ev, result.position, result.normal, result.shape);
					if (last_object->get_capture_input_on_drag() && mb.is_valid() && mb->get_button_index() == 1 && mb->is_pressed()) {
						physics_object_capture = last_id;
					}
				}
			}
		} else {
			if (camera) {
				Vector3 from = camera->project_ray_origin(pos);
				Vector3 dir = camera->project_ray_normal(pos);
				float far = camera->far;

				PhysicsDirectSpaceState *space = PhysicsServer::get_singleton()->space_get_direct_state(find_world()->get_space());
				if (space) {
					bool col = space->intersect_ray(from, from + dir * far, result, Set<RID>(), 0xFFFFFFFF, true, true, true);
					ObjectID new_collider = 0;
					if (col) {
						CollisionObject *co = Object::cast_to<CollisionObject>(result.collider);
						if (co && (!p_ignore_paused || co->can_process())) {
							_collision_object_input_event(co, camera, ev, result.position, result.normal, result.shape);
							last_object = co;
							last_id = result.collider_id;
							new_collider = last_id;
							if (co->get_capture_input_on_drag() && mb.is_valid() && mb->get_button_index() == 1 && mb->is_pressed()) {
								physics_object_capture = last_id;
							}
						}
					}

					if (is_mouse && new_collider != physics_object_over) {
						if (physics_object_over) {
							CollisionObject *co = Object::cast_to<CollisionObject>(ObjectDB::get_instance(physics_object_over));
							if (co) {
								co->_mouse_exit();
							}
						}

						if (new_collider) {
							CollisionObject *co = Object::cast_to<CollisionObject>(ObjectDB::get_instance(new_collider));
							if (co) {
								co->_mouse_enter();
							}
						}

						physics_object_over = new_collider;
					}
				}

				last_pos = pos;
			}
		}
#endif
	}
}

RID Viewport::get_viewport_rid() const {
	return viewport;
}

void Viewport::set_use_arvr(bool p_use_arvr) {
	arvr = p_use_arvr;

	VS::get_singleton()->viewport_set_use_arvr(viewport, arvr);
}

bool Viewport::use_arvr() {
	return arvr;
}

void Viewport::update_canvas_items() {
	if (!is_inside_tree()) {
		return;
	}

	_update_canvas_items(this);
}

void Viewport::set_size(const Size2 &p_size) {
	if (size == p_size.floor()) {
		return;
	}
	size = p_size.floor();
	VS::get_singleton()->viewport_set_size(viewport, size.width, size.height);

	ViewportContainer *c = Object::cast_to<ViewportContainer>(get_parent());
	if (c) {
		c->minimum_size_changed();
	}

	_update_stretch_transform();
	update_configuration_warning();

	for (Set<ViewportTexture *>::Element *E = viewport_textures.front(); E; E = E->next()) {
		E->get()->emit_changed();
	}

	emit_signal("size_changed");
}

Rect2 Viewport::get_visible_rect() const {
	Rect2 r;

	if (size == Size2()) {
		r = Rect2(Point2(), OS::get_singleton()->get_window_size());
	} else {
		r = Rect2(Point2(), size);
	}

	if (size_override) {
		r.size = size_override_size;
	}

	return r;
}

Size2 Viewport::get_size() const {
	return size;
}

void Viewport::_update_listener() {
	/*
	if (is_inside_tree() && audio_listener && (camera || listener) && (!get_parent() || (Object::cast_to<Control>(get_parent()) && Object::cast_to<Control>(get_parent())->is_visible_in_tree())))  {
		SpatialSoundServer::get_singleton()->listener_set_space(internal_listener, find_world()->get_sound_space());
	} else {
		SpatialSoundServer::get_singleton()->listener_set_space(internal_listener, RID());
	}
*/
}

void Viewport::_update_listener_2d() {
	/*
	if (is_inside_tree() && audio_listener && (!get_parent() || (Object::cast_to<Control>(get_parent()) && Object::cast_to<Control>(get_parent())->is_visible_in_tree())))
		SpatialSound2DServer::get_singleton()->listener_set_space(internal_listener_2d, find_world_2d()->get_sound_space());
	else
		SpatialSound2DServer::get_singleton()->listener_set_space(internal_listener_2d, RID());
*/
}

void Viewport::set_as_audio_listener(bool p_enable) {
	if (p_enable == audio_listener) {
		return;
	}

	audio_listener = p_enable;
	_update_listener();
}

bool Viewport::is_audio_listener() const {
	return audio_listener;
}

void Viewport::set_as_audio_listener_2d(bool p_enable) {
	if (p_enable == audio_listener_2d) {
		return;
	}

	audio_listener_2d = p_enable;
	_update_listener_2d();
}

bool Viewport::is_audio_listener_2d() const {
	return audio_listener_2d;
}

Listener2D *Viewport::get_listener_2d() const {
	return listener_2d;
}

void Viewport::enable_canvas_transform_override(bool p_enable) {
	if (override_canvas_transform == p_enable) {
		return;
	}

	override_canvas_transform = p_enable;
	if (p_enable) {
		VisualServer::get_singleton()->viewport_set_canvas_transform(viewport, find_world_2d()->get_canvas(), canvas_transform_override);
	} else {
		VisualServer::get_singleton()->viewport_set_canvas_transform(viewport, find_world_2d()->get_canvas(), canvas_transform);
	}
}

bool Viewport::is_canvas_transform_override_enbled() const {
	return override_canvas_transform;
}

void Viewport::set_canvas_transform_override(const Transform2D &p_transform) {
	if (canvas_transform_override == p_transform) {
		return;
	}

	canvas_transform_override = p_transform;
	if (override_canvas_transform) {
		VisualServer::get_singleton()->viewport_set_canvas_transform(viewport, find_world_2d()->get_canvas(), canvas_transform_override);
	}
}

Transform2D Viewport::get_canvas_transform_override() const {
	return canvas_transform_override;
}

void Viewport::set_canvas_transform(const Transform2D &p_transform) {
	canvas_transform = p_transform;

	if (!override_canvas_transform) {
		VisualServer::get_singleton()->viewport_set_canvas_transform(viewport, find_world_2d()->get_canvas(), canvas_transform);
	}
}

Transform2D Viewport::get_canvas_transform() const {
	return canvas_transform;
}

void Viewport::_update_global_transform() {
	Transform2D sxform = stretch_transform * global_canvas_transform;

	VisualServer::get_singleton()->viewport_set_global_canvas_transform(viewport, sxform);
}

void Viewport::set_global_canvas_transform(const Transform2D &p_transform) {
	global_canvas_transform = p_transform;

	_update_global_transform();
}

Transform2D Viewport::get_global_canvas_transform() const {
	return global_canvas_transform;
}

void Viewport::_listener_transform_changed_notify() {
#ifndef _3D_DISABLED
//if (listener)
//		SpatialSoundServer::get_singleton()->listener_set_transform(internal_listener, listener->get_listener_transform());
#endif
}

void Viewport::_listener_set(Listener *p_listener) {
#ifndef _3D_DISABLED

	if (listener == p_listener) {
		return;
	}

	listener = p_listener;

	_update_listener();
	_listener_transform_changed_notify();
#endif
}

bool Viewport::_listener_add(Listener *p_listener) {
	listeners.insert(p_listener);
	return listeners.size() == 1;
}

void Viewport::_listener_remove(Listener *p_listener) {
	listeners.erase(p_listener);
	if (listener == p_listener) {
		listener = nullptr;
	}
}

#ifndef _3D_DISABLED
void Viewport::_listener_make_next_current(Listener *p_exclude) {
	if (listeners.size() > 0) {
		for (Set<Listener *>::Element *E = listeners.front(); E; E = E->next()) {
			if (p_exclude == E->get()) {
				continue;
			}
			if (!E->get()->is_inside_tree()) {
				continue;
			}
			if (listener != nullptr) {
				return;
			}

			E->get()->make_current();
		}
	} else {
		// Attempt to reset listener to the camera position
		if (camera != nullptr) {
			_update_listener();
			_camera_transform_changed_notify();
		}
	}
}
#endif

void Viewport::_camera_transform_changed_notify() {
#ifndef _3D_DISABLED
// If there is an active listener in the scene, it takes priority over the camera
//	if (camera && !listener)
//		SpatialSoundServer::get_singleton()->listener_set_transform(internal_listener, camera->get_camera_transform());
#endif
}

void Viewport::_camera_set(Camera *p_camera) {
#ifndef _3D_DISABLED

	if (camera == p_camera) {
		return;
	}

	if (camera) {
		camera->notification(Camera::NOTIFICATION_LOST_CURRENT);
	}
	camera = p_camera;
	if (!camera_override) {
		if (camera) {
			VisualServer::get_singleton()->viewport_attach_camera(viewport, camera->get_camera());
		} else {
			VisualServer::get_singleton()->viewport_attach_camera(viewport, RID());
		}
	}

	if (camera) {
		camera->notification(Camera::NOTIFICATION_BECAME_CURRENT);
	}

	_update_listener();
	_camera_transform_changed_notify();
#endif
}

bool Viewport::_camera_add(Camera *p_camera) {
	cameras.insert(p_camera);
	return cameras.size() == 1;
}

void Viewport::_camera_remove(Camera *p_camera) {
	cameras.erase(p_camera);
	if (camera == p_camera) {
		camera->notification(Camera::NOTIFICATION_LOST_CURRENT);
		camera = nullptr;
	}
}

#ifndef _3D_DISABLED
void Viewport::_camera_make_next_current(Camera *p_exclude) {
	for (Set<Camera *>::Element *E = cameras.front(); E; E = E->next()) {
		if (p_exclude == E->get()) {
			continue;
		}
		if (!E->get()->is_inside_tree()) {
			continue;
		}
		if (camera != nullptr) {
			return;
		}

		E->get()->make_current();
	}
}
#endif

void Viewport::_listener_2d_set(Listener2D *p_listener) {
	if (listener_2d == p_listener) {
		return;
	} else if (listener_2d) {
		listener_2d->clear_current();
	}
	listener_2d = p_listener;
}

void Viewport::_listener_2d_remove(Listener2D *p_listener) {
	if (listener_2d == p_listener) {
		listener_2d = nullptr;
	}
}

void Viewport::_canvas_layer_add(CanvasLayer *p_canvas_layer) {
	canvas_layers.insert(p_canvas_layer);
}

void Viewport::_canvas_layer_remove(CanvasLayer *p_canvas_layer) {
	canvas_layers.erase(p_canvas_layer);
}

void Viewport::set_transparent_background(bool p_enable) {
	transparent_bg = p_enable;
	VS::get_singleton()->viewport_set_transparent_background(viewport, p_enable);
}

bool Viewport::has_transparent_background() const {
	return transparent_bg;
}

void Viewport::set_world_2d(const Ref<World2D> &p_world_2d) {
	if (world_2d == p_world_2d) {
		return;
	}

	if (parent && parent->find_world_2d() == p_world_2d) {
		WARN_PRINT("Unable to use parent world as world_2d");
		return;
	}

	if (is_inside_tree()) {
		find_world_2d()->_remove_viewport(this);
		VisualServer::get_singleton()->viewport_remove_canvas(viewport, current_canvas);
	}

	if (p_world_2d.is_valid()) {
		world_2d = p_world_2d;
	} else {
		WARN_PRINT("Invalid world");
		world_2d = Ref<World2D>(memnew(World2D));
	}

	_update_listener_2d();

	if (is_inside_tree()) {
		current_canvas = find_world_2d()->get_canvas();
		VisualServer::get_singleton()->viewport_attach_canvas(viewport, current_canvas);
		find_world_2d()->_register_viewport(this, Rect2());
	}
}

Ref<World2D> Viewport::find_world_2d() const {
	if (world_2d.is_valid()) {
		return world_2d;
	} else if (parent) {
		return parent->find_world_2d();
	} else {
		return Ref<World2D>();
	}
}

void Viewport::_propagate_enter_world(Node *p_node) {
	if (p_node != this) {
		if (!p_node->is_inside_tree()) { //may not have entered scene yet
			return;
		}

		if (Object::cast_to<Spatial>(p_node) || Object::cast_to<WorldEnvironment>(p_node)) {
			p_node->notification(Spatial::NOTIFICATION_ENTER_WORLD);
		} else {
			Viewport *v = Object::cast_to<Viewport>(p_node);
			if (v) {
				if (v->world.is_valid() || v->own_world.is_valid()) {
					return;
				}
			}
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_propagate_enter_world(p_node->get_child(i));
	}
}

void Viewport::_propagate_viewport_notification(Node *p_node, int p_what) {
	p_node->notification(p_what);
	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *c = p_node->get_child(i);
		if (Object::cast_to<Viewport>(c)) {
			continue;
		}
		_propagate_viewport_notification(c, p_what);
	}
}

void Viewport::_propagate_exit_world(Node *p_node) {
	if (p_node != this) {
		if (!p_node->is_inside_tree()) { //may have exited scene already
			return;
		}

		if (Object::cast_to<Spatial>(p_node) || Object::cast_to<WorldEnvironment>(p_node)) {
			p_node->notification(Spatial::NOTIFICATION_EXIT_WORLD);
		} else {
			Viewport *v = Object::cast_to<Viewport>(p_node);
			if (v) {
				if (v->world.is_valid() || v->own_world.is_valid()) {
					return;
				}
			}
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_propagate_exit_world(p_node->get_child(i));
	}
}

void Viewport::set_world(const Ref<World> &p_world) {
	if (world == p_world) {
		return;
	}

	if (is_inside_tree()) {
		_propagate_exit_world(this);
	}

	if (own_world.is_valid() && world.is_valid()) {
		world->disconnect(CoreStringNames::get_singleton()->changed, this, "_own_world_changed");
	}

	world = p_world;

	if (own_world.is_valid()) {
		if (world.is_valid()) {
			own_world = world->duplicate();
			world->connect(CoreStringNames::get_singleton()->changed, this, "_own_world_changed");
		} else {
			own_world = Ref<World>(memnew(World));
		}
	}

	if (is_inside_tree()) {
		_propagate_enter_world(this);
	}

	if (is_inside_tree()) {
		VisualServer::get_singleton()->viewport_set_scenario(viewport, find_world()->get_scenario());
	}

	_update_listener();
}

Ref<World> Viewport::get_world() const {
	return world;
}

Ref<World2D> Viewport::get_world_2d() const {
	return world_2d;
}

Ref<World> Viewport::find_world() const {
	if (own_world.is_valid()) {
		return own_world;
	} else if (world.is_valid()) {
		return world;
	} else if (parent) {
		return parent->find_world();
	} else {
		return Ref<World>();
	}
}

Listener *Viewport::get_listener() const {
	return listener;
}

Camera *Viewport::get_camera() const {
	return camera;
}

void Viewport::enable_camera_override(bool p_enable) {
#ifndef _3D_DISABLED
	if (p_enable == camera_override) {
		return;
	}

	if (p_enable) {
		camera_override.rid = RID_PRIME(VisualServer::get_singleton()->camera_create());
	} else {
		VisualServer::get_singleton()->free(camera_override.rid);
		camera_override.rid = RID();
	}

	if (p_enable) {
		VisualServer::get_singleton()->viewport_attach_camera(viewport, camera_override.rid);
	} else if (camera) {
		VisualServer::get_singleton()->viewport_attach_camera(viewport, camera->get_camera());
	} else {
		VisualServer::get_singleton()->viewport_attach_camera(viewport, RID());
	}
#endif
}

bool Viewport::is_camera_override_enabled() const {
	return camera_override;
}

void Viewport::set_camera_override_transform(const Transform &p_transform) {
	if (camera_override) {
		camera_override.transform = p_transform;
		VisualServer::get_singleton()->camera_set_transform(camera_override.rid, p_transform);
	}
}

Transform Viewport::get_camera_override_transform() const {
	if (camera_override) {
		return camera_override.transform;
	}

	return Transform();
}

void Viewport::set_camera_override_perspective(float p_fovy_degrees, float p_z_near, float p_z_far) {
	if (camera_override) {
		if (camera_override.fov == p_fovy_degrees && camera_override.z_near == p_z_near &&
				camera_override.z_far == p_z_far && camera_override.projection == CameraOverrideData::PROJECTION_PERSPECTIVE) {
			return;
		}

		camera_override.fov = p_fovy_degrees;
		camera_override.z_near = p_z_near;
		camera_override.z_far = p_z_far;
		camera_override.projection = CameraOverrideData::PROJECTION_PERSPECTIVE;

		VisualServer::get_singleton()->camera_set_perspective(camera_override.rid, camera_override.fov, camera_override.z_near, camera_override.z_far);
	}
}

void Viewport::set_camera_override_orthogonal(float p_size, float p_z_near, float p_z_far) {
	if (camera_override) {
		if (camera_override.size == p_size && camera_override.z_near == p_z_near &&
				camera_override.z_far == p_z_far && camera_override.projection == CameraOverrideData::PROJECTION_ORTHOGONAL) {
			return;
		}

		camera_override.size = p_size;
		camera_override.z_near = p_z_near;
		camera_override.z_far = p_z_far;
		camera_override.projection = CameraOverrideData::PROJECTION_ORTHOGONAL;

		VisualServer::get_singleton()->camera_set_orthogonal(camera_override.rid, camera_override.size, camera_override.z_near, camera_override.z_far);
	}
}

Transform2D Viewport::get_final_transform() const {
	return stretch_transform * global_canvas_transform;
}

void Viewport::_update_canvas_items(Node *p_node) {
	if (p_node != this) {
		Viewport *vp = Object::cast_to<Viewport>(p_node);
		if (vp) {
			return;
		}

		CanvasItem *ci = Object::cast_to<CanvasItem>(p_node);
		if (ci) {
			ci->update();
		}
	}

	int cc = p_node->get_child_count();

	for (int i = 0; i < cc; i++) {
		_update_canvas_items(p_node->get_child(i));
	}
}

void Viewport::set_size_override(bool p_enable, const Size2 &p_size, const Vector2 &p_margin) {
	if (size_override == p_enable && p_size == size_override_size) {
		return;
	}

	size_override = p_enable;
	if (p_size.x >= 0 || p_size.y >= 0) {
		size_override_size = p_size;
	}
	size_override_margin = p_margin;

	_update_stretch_transform();
	emit_signal("size_changed");
}

Size2 Viewport::get_size_override() const {
	return size_override_size;
}
bool Viewport::is_size_override_enabled() const {
	return size_override;
}
void Viewport::set_size_override_stretch(bool p_enable) {
	if (p_enable == size_override_stretch) {
		return;
	}

	size_override_stretch = p_enable;

	_update_stretch_transform();
}

bool Viewport::is_size_override_stretch_enabled() const {
	return size_override_stretch;
}

void Viewport::set_update_mode(UpdateMode p_mode) {
	update_mode = p_mode;
	VS::get_singleton()->viewport_set_update_mode(viewport, VS::ViewportUpdateMode(p_mode));
}
Viewport::UpdateMode Viewport::get_update_mode() const {
	return update_mode;
}

Ref<ViewportTexture> Viewport::get_texture() const {
	return default_texture;
}

void Viewport::set_vflip(bool p_enable) {
	vflip = p_enable;
	VisualServer::get_singleton()->viewport_set_vflip(viewport, p_enable);
}

bool Viewport::get_vflip() const {
	return vflip;
}

void Viewport::set_clear_mode(ClearMode p_mode) {
	clear_mode = p_mode;
	VS::get_singleton()->viewport_set_clear_mode(viewport, VS::ViewportClearMode(p_mode));
}

Viewport::ClearMode Viewport::get_clear_mode() const {
	return clear_mode;
}

void Viewport::set_shadow_atlas_size(int p_size) {
	if (shadow_atlas_size == p_size) {
		return;
	}

	shadow_atlas_size = p_size;
	VS::get_singleton()->viewport_set_shadow_atlas_size(viewport, p_size);
}

int Viewport::get_shadow_atlas_size() const {
	return shadow_atlas_size;
}

void Viewport::set_shadow_atlas_quadrant_subdiv(int p_quadrant, ShadowAtlasQuadrantSubdiv p_subdiv) {
	ERR_FAIL_INDEX(p_quadrant, 4);
	ERR_FAIL_INDEX(p_subdiv, SHADOW_ATLAS_QUADRANT_SUBDIV_MAX);

	if (shadow_atlas_quadrant_subdiv[p_quadrant] == p_subdiv) {
		return;
	}

	shadow_atlas_quadrant_subdiv[p_quadrant] = p_subdiv;
	static const int subdiv[SHADOW_ATLAS_QUADRANT_SUBDIV_MAX] = { 0, 1, 4, 16, 64, 256, 1024 };

	VS::get_singleton()->viewport_set_shadow_atlas_quadrant_subdivision(viewport, p_quadrant, subdiv[p_subdiv]);
}
Viewport::ShadowAtlasQuadrantSubdiv Viewport::get_shadow_atlas_quadrant_subdiv(int p_quadrant) const {
	ERR_FAIL_INDEX_V(p_quadrant, 4, SHADOW_ATLAS_QUADRANT_SUBDIV_DISABLED);
	return shadow_atlas_quadrant_subdiv[p_quadrant];
}

Transform2D Viewport::_get_input_pre_xform() const {
	Transform2D pre_xf;

	if (to_screen_rect != Rect2()) {
		pre_xf.elements[2] = -to_screen_rect.position;
		pre_xf.scale(size / to_screen_rect.size);
	}

	return pre_xf;
}

Vector2 Viewport::_get_window_offset() const {
	if (get_parent() && get_parent()->has_method("get_global_position")) {
		return get_parent()->call("get_global_position");
	}
	return Vector2();
}

Ref<InputEvent> Viewport::_make_input_local(const Ref<InputEvent> &ev) {
	if (ev.is_null()) {
		return ev;
	}
	Vector2 vp_ofs = _get_window_offset();
	Transform2D ai = get_final_transform().affine_inverse() * _get_input_pre_xform();

	return ev->xformed_by(ai, -vp_ofs);
}

void Viewport::_vp_input_text(const String &p_text) {
	if (gui.key_focus) {
		gui.key_focus->call("set_text", p_text);
	}
}

void Viewport::_vp_input(const Ref<InputEvent> &p_ev) {
	ERR_FAIL_COND(p_ev.is_null());

	if (disable_input) {
		return;
	}

#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint() && get_tree()->get_edited_scene_root() && get_tree()->get_edited_scene_root()->is_a_parent_of(this)) {
		return;
	}
#endif

	if (to_screen_rect == Rect2()) {
		return; //if render target, can't get input events
	}

	//this one handles system input, p_ev are in system coordinates
	//they are converted to viewport coordinates

	Ref<InputEvent> ev = _make_input_local(p_ev);
	input(ev);
}

void Viewport::_vp_unhandled_input(const Ref<InputEvent> &p_ev) {
	ERR_FAIL_COND(p_ev.is_null());

	if (disable_input) {
		return;
	}
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint() && get_tree()->get_edited_scene_root() && get_tree()->get_edited_scene_root()->is_a_parent_of(this)) {
		return;
	}
#endif

	/*
	if (parent_control && !parent_control->is_visible_in_tree())
		return;
	*/

	if (to_screen_rect == Rect2()) {
		return; //if render target, can't get input events
	}

	//this one handles system input, p_ev are in system coordinates
	//they are converted to viewport coordinates

	Ref<InputEvent> ev = _make_input_local(p_ev);
	unhandled_input(ev);
}

Vector2 Viewport::get_mouse_position() const {
	return (get_final_transform().affine_inverse() * _get_input_pre_xform()).xform(Input::get_singleton()->get_mouse_position() - _get_window_offset());
}

void Viewport::warp_mouse(const Vector2 &p_pos) {
	Vector2 gpos = (get_final_transform().affine_inverse() * _get_input_pre_xform()).affine_inverse().xform(p_pos);
	Input::get_singleton()->warp_mouse_position(gpos);
}

void Viewport::_gui_prepare_subwindows() {
	if (gui.subwindow_visibility_dirty) {
		gui.subwindows.clear();
		for (List<Control *>::Element *E = gui.all_known_subwindows.front(); E; E = E->next()) {
			if (E->get()->is_visible_in_tree()) {
				gui.subwindows.push_back(E->get());
			}
		}

		gui.subwindow_visibility_dirty = false;
		gui.subwindow_order_dirty = true;
	}

	_gui_sort_subwindows();
}

void Viewport::_gui_sort_subwindows() {
	if (!gui.subwindow_order_dirty) {
		return;
	}

	gui.modal_stack.sort_custom<Control::CComparator>();
	gui.subwindows.sort_custom<Control::CComparator>();

	gui.subwindow_order_dirty = false;
}

void Viewport::_gui_sort_modal_stack() {
	gui.modal_stack.sort_custom<Control::CComparator>();
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
	if (gui.tooltip_timer.is_valid()) {
		gui.tooltip_timer->release_connections();
		gui.tooltip_timer = Ref<SceneTreeTimer>();
	}
	if (gui.tooltip_popup) {
		gui.tooltip_popup->queue_delete();
		gui.tooltip_popup = nullptr;
		gui.tooltip_label = nullptr;
	}
}

String Viewport::_gui_get_tooltip(Control *p_control, const Vector2 &p_pos, Control **r_tooltip_owner) {
	Vector2 pos = p_pos;
	String tooltip;

	while (p_control) {
		tooltip = p_control->get_tooltip(pos);

		if (r_tooltip_owner) {
			*r_tooltip_owner = p_control;
		}

		// If we found a tooltip, we stop here.
		if (!tooltip.empty()) {
			break;
		}

		// Otherwise, we check parent controls unless some conditions prevent it.

		if (p_control->data.mouse_filter == Control::MOUSE_FILTER_STOP) {
			break;
		}
		if (p_control->is_set_as_toplevel()) {
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
	String tooltip_text = tr(_gui_get_tooltip(
			gui.tooltip_control,
			gui.tooltip_control->get_global_transform().xform_inv(gui.tooltip_pos),
			&tooltip_owner));
	tooltip_text = tooltip_text.strip_edges();
	if (tooltip_text.empty()) {
		return; // Nothing to show.
	}

	// Remove previous popup if we change something.
	if (gui.tooltip_popup) {
		memdelete(gui.tooltip_popup);
		gui.tooltip_popup = nullptr;
		gui.tooltip_label = nullptr;
	}

	if (!tooltip_owner) {
		return;
	}

	// Controls can implement `make_custom_tooltip` to provide their own tooltip.
	// This should be a Control node which will be added as child to the tooltip owner.
	gui.tooltip_popup = tooltip_owner->make_custom_tooltip(tooltip_text);

	// If no custom tooltip is given, use a default implementation.
	if (!gui.tooltip_popup) {
		gui.tooltip_popup = memnew(TooltipPanel);
		gui.tooltip_label = memnew(TooltipLabel);
		gui.tooltip_popup->add_child(gui.tooltip_label);

		Ref<StyleBox> ttp = gui.tooltip_label->get_stylebox("panel", "TooltipPanel");

		gui.tooltip_label->set_anchor_and_margin(MARGIN_LEFT, Control::ANCHOR_BEGIN, ttp->get_margin(MARGIN_LEFT));
		gui.tooltip_label->set_anchor_and_margin(MARGIN_TOP, Control::ANCHOR_BEGIN, ttp->get_margin(MARGIN_TOP));
		gui.tooltip_label->set_anchor_and_margin(MARGIN_RIGHT, Control::ANCHOR_END, -ttp->get_margin(MARGIN_RIGHT));
		gui.tooltip_label->set_anchor_and_margin(MARGIN_BOTTOM, Control::ANCHOR_END, -ttp->get_margin(MARGIN_BOTTOM));
		gui.tooltip_label->set_text(tooltip_text);
	}

	tooltip_owner->add_child(gui.tooltip_popup);
	gui.tooltip_popup->force_parent_owned();
	gui.tooltip_popup->set_as_toplevel(true);
	if (gui.tooltip_control) { // Avoids crash when rapidly switching controls.
		gui.tooltip_popup->set_scale(gui.tooltip_control->get_global_transform().get_scale());
	}

	Point2 tooltip_offset = ProjectSettings::get_singleton()->get("display/mouse_cursor/tooltip_position_offset");
	Rect2 r(gui.tooltip_pos + tooltip_offset, gui.tooltip_popup->get_minimum_size());
	Rect2 vr = gui.tooltip_popup->get_viewport_rect();
	if (r.size.x * gui.tooltip_popup->get_scale().x + r.position.x > vr.size.x) {
		r.position.x = vr.size.x - r.size.x * gui.tooltip_popup->get_scale().x;
	} else if (r.position.x < 0) {
		r.position.x = 0;
	}

	if (r.size.y * gui.tooltip_popup->get_scale().y + r.position.y > vr.size.y) {
		r.position.y = vr.size.y - r.size.y * gui.tooltip_popup->get_scale().y;
	} else if (r.position.y < 0) {
		r.position.y = 0;
	}

	gui.tooltip_popup->set_global_position(r.position);
	gui.tooltip_popup->set_size(r.size);

	gui.tooltip_popup->raise();
	gui.tooltip_popup->show();
}

void Viewport::_gui_call_input(Control *p_control, const Ref<InputEvent> &p_input) {
	//_block();

	Ref<InputEvent> ev = p_input;

	//mouse wheel events can't be stopped
	Ref<InputEventMouseButton> mb = p_input;

	bool cant_stop_me_now = (mb.is_valid() &&
			(mb->get_button_index() == BUTTON_WHEEL_DOWN ||
					mb->get_button_index() == BUTTON_WHEEL_UP ||
					mb->get_button_index() == BUTTON_WHEEL_LEFT ||
					mb->get_button_index() == BUTTON_WHEEL_RIGHT));
	Ref<InputEventPanGesture> pn = p_input;
	cant_stop_me_now = pn.is_valid() || cant_stop_me_now;

	bool ismouse = ev.is_valid() || Object::cast_to<InputEventMouseMotion>(*p_input) != nullptr;

	CanvasItem *ci = p_control;
	while (ci) {
		Control *control = Object::cast_to<Control>(ci);
		if (control) {
			if (control->data.mouse_filter != Control::MOUSE_FILTER_IGNORE) {
				control->emit_signal(SceneStringNames::get_singleton()->gui_input, ev); //signal should be first, so it's possible to override an event (and then accept it)
			}
			if (gui.key_event_accepted) {
				break;
			}
			if (!control->is_inside_tree()) {
				break;
			}

			if (control->data.mouse_filter != Control::MOUSE_FILTER_IGNORE) {
				control->call_multilevel(SceneStringNames::get_singleton()->_gui_input, ev);
			}

			if (!control->is_inside_tree() || control->is_set_as_toplevel()) {
				break;
			}
			if (gui.key_event_accepted) {
				break;
			}
			if (!cant_stop_me_now && control->data.mouse_filter == Control::MOUSE_FILTER_STOP && ismouse) {
				break;
			}
		}

		if (ci->is_set_as_toplevel()) {
			break;
		}

		ev = ev->xformed_by(ci->get_transform()); //transform event upwards
		ci = ci->get_parent_item();
	}

	//_unblock();
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

			if (!control->is_inside_tree() || control->is_set_as_toplevel()) {
				break;
			}
			if (control->data.mouse_filter == Control::MOUSE_FILTER_STOP) {
				break;
			}
		}

		if (ci->is_set_as_toplevel()) {
			break;
		}

		ci = ci->get_parent_item();
	}

	//_unblock();
}
Control *Viewport::_gui_find_control(const Point2 &p_global) {
	_gui_prepare_subwindows();

	for (List<Control *>::Element *E = gui.subwindows.back(); E; E = E->prev()) {
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

		Control *ret = _gui_find_control_at_pos(sw, p_global, xform, gui.focus_inv_xform);
		if (ret) {
			return ret;
		}
	}

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

		Control *ret = _gui_find_control_at_pos(sw, p_global, xform, gui.focus_inv_xform);
		if (ret) {
			return ret;
		}
	}

	return nullptr;
}

Control *Viewport::_gui_find_control_at_pos(CanvasItem *p_node, const Point2 &p_global, const Transform2D &p_xform, Transform2D &r_inv_xform) {
	if (Object::cast_to<Viewport>(p_node)) {
		return nullptr;
	}

	//subwindows first!!

	if (!p_node->is_visible()) {
		//return _find_next_visible_control_at_pos(p_node,p_global,r_inv_xform);
		return nullptr; //canvas item hidden, discard
	}

	Transform2D matrix = p_xform * p_node->get_transform();
	// matrix.determinant() == 0.0f implies that node does not exist on scene
	if (matrix.determinant() == 0.0f) {
		return nullptr;
	}

	Control *c = Object::cast_to<Control>(p_node);

	if (!c || !c->clips_input() || c->has_point(matrix.affine_inverse().xform(p_global))) {
		for (int i = p_node->get_child_count() - 1; i >= 0; i--) {
			if (p_node == gui.tooltip_popup) {
				continue;
			}

			CanvasItem *ci = Object::cast_to<CanvasItem>(p_node->get_child(i));
			if (!ci || ci->is_set_as_toplevel()) {
				continue;
			}

			Control *ret = _gui_find_control_at_pos(ci, p_global, matrix, r_inv_xform);
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
	if (!drag_preview || (c != drag_preview && !drag_preview->is_a_parent_of(c))) {
		r_inv_xform = matrix;
		return c;
	}

	return nullptr;
}

bool Viewport::_gui_drop(Control *p_at_control, Point2 p_at_pos, bool p_just_check) {
	{ //attempt grab, try parent controls too
		CanvasItem *ci = p_at_control;
		while (ci) {
			Control *control = Object::cast_to<Control>(ci);
			if (control) {
				if (control->can_drop_data(p_at_pos, gui.drag_data)) {
					if (!p_just_check) {
						control->drop_data(p_at_pos, gui.drag_data);
					}

					return true;
				}

				if (control->data.mouse_filter == Control::MOUSE_FILTER_STOP) {
					break;
				}
			}

			p_at_pos = ci->get_transform().xform(p_at_pos);

			if (ci->is_set_as_toplevel()) {
				break;
			}

			ci = ci->get_parent_item();
		}
	}

	return false;
}

void Viewport::_gui_input_event(Ref<InputEvent> p_event) {
	ERR_FAIL_COND(p_event.is_null());

	//?
	/*
	if (!is_visible()) {
		return; //simple and plain
	}
	*/

	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid()) {
		gui.key_event_accepted = false;
		Point2 mpos = mb->get_position();
		if (mb->is_pressed()) {
			Size2 pos = mpos;
			if (gui.mouse_focus_mask) {
				//do not steal mouse focus and stuff while a focus mask exists
				gui.mouse_focus_mask |= 1 << (mb->get_button_index() - 1); //add the button to the mask
			} else {
				bool is_handled = false;

				_gui_sort_modal_stack();
				while (!gui.modal_stack.empty()) {
					Control *top = gui.modal_stack.back()->get();
					Vector2 pos2 = top->get_global_transform_with_canvas().affine_inverse().xform(mpos);
					if (!top->has_point(pos2)) {
						if (top->data.modal_exclusive || top->data.modal_frame == Engine::get_singleton()->get_frames_drawn()) {
							//cancel event, sorry, modal exclusive EATS UP ALL
							//alternative, you can't pop out a window the same frame it was made modal (fixes many issues)
							set_input_as_handled();

							return; // no one gets the event if exclusive NO ONE
						}

						if (mb->get_button_index() == BUTTON_WHEEL_UP || mb->get_button_index() == BUTTON_WHEEL_DOWN || mb->get_button_index() == BUTTON_WHEEL_LEFT || mb->get_button_index() == BUTTON_WHEEL_RIGHT) {
							//cancel scroll wheel events, only clicks should trigger focus changes.
							set_input_as_handled();
							return;
						}

						top->notification(Control::NOTIFICATION_MODAL_CLOSE);
						top->_modal_stack_remove();
						top->hide();

						if (!top->get_pass_on_modal_close_click()) {
							is_handled = true;
						}
					} else {
						break;
					}
				}

				if (is_handled) {
					set_input_as_handled();
					return;
				}

				//Matrix32 parent_xform;

				/*
				if (data.parent_canvas_item)
					parent_xform=data.parent_canvas_item->get_global_transform();
				*/

				gui.mouse_focus = _gui_find_control(pos);
				gui.last_mouse_focus = gui.mouse_focus;

				if (!gui.mouse_focus) {
					gui.mouse_focus_mask = 0;
					return;
				}

				gui.mouse_focus_mask = 1 << (mb->get_button_index() - 1);

				if (mb->get_button_index() == BUTTON_LEFT) {
					gui.drag_accum = Vector2();
					gui.drag_attempted = false;
				}
			}

			mb = mb->xformed_by(Transform2D()); // make a copy of the event

			mb->set_global_position(pos);

			pos = gui.focus_inv_xform.xform(pos);

			mb->set_position(pos);

#ifdef DEBUG_ENABLED
			if (ScriptDebugger::get_singleton() && gui.mouse_focus) {
				Array arr;
				arr.push_back(gui.mouse_focus->get_path());
				arr.push_back(gui.mouse_focus->get_class());
				ScriptDebugger::get_singleton()->send_message("click_ctrl", arr);
			}
#endif

			if (mb->get_button_index() == BUTTON_LEFT) { //assign focus
				CanvasItem *ci = gui.mouse_focus;
				while (ci) {
					Control *control = Object::cast_to<Control>(ci);
					if (control) {
						if (control->get_focus_mode() != Control::FOCUS_NONE) {
							if (control != gui.key_focus) {
								control->grab_focus();
							}
							break;
						}

						if (control->data.mouse_filter == Control::MOUSE_FILTER_STOP) {
							break;
						}
					}

					if (ci->is_set_as_toplevel()) {
						break;
					}

					ci = ci->get_parent_item();
				}
			}

			if (gui.mouse_focus && gui.mouse_focus->can_process()) {
				_gui_call_input(gui.mouse_focus, mb);
			}

			set_input_as_handled();

			if (gui.drag_data.get_type() != Variant::NIL && mb->get_button_index() == BUTTON_LEFT) {
				//alternate drop use (when using force_drag(), as proposed by #5342
				gui.drag_successful = false;
				if (gui.mouse_focus) {
					gui.drag_successful = _gui_drop(gui.mouse_focus, pos, false);
				}

				gui.drag_data = Variant();
				gui.dragging = false;

				Control *drag_preview = _gui_get_drag_preview();
				if (drag_preview) {
					memdelete(drag_preview);
					gui.drag_preview_id = 0;
				}
				_propagate_viewport_notification(this, NOTIFICATION_DRAG_END);
				//change mouse accordingly
			}

			_gui_cancel_tooltip();
		} else {
			if (gui.drag_data.get_type() != Variant::NIL && mb->get_button_index() == BUTTON_LEFT) {
				gui.drag_successful = false;
				if (gui.mouse_over) {
					Size2 pos = mpos;
					pos = gui.focus_inv_xform.xform(pos);

					gui.drag_successful = _gui_drop(gui.mouse_over, pos, false);
				}

				Control *drag_preview = _gui_get_drag_preview();
				if (drag_preview) {
					memdelete(drag_preview);
					gui.drag_preview_id = 0;
				}

				gui.drag_data = Variant();
				gui.dragging = false;
				_propagate_viewport_notification(this, NOTIFICATION_DRAG_END);
				//change mouse accordingly
			}

			gui.mouse_focus_mask &= ~(1 << (mb->get_button_index() - 1)); //remove from mask

			if (!gui.mouse_focus) {
				//release event is only sent if a mouse focus (previously pressed button) exists
				return;
			}

			Size2 pos = mpos;

			mb = mb->xformed_by(Transform2D()); //make a copy
			mb->set_global_position(pos);
			pos = gui.focus_inv_xform.xform(pos);
			mb->set_position(pos);

			Control *mouse_focus = gui.mouse_focus;

			//disable mouse focus if needed before calling input, this makes popups on mouse press event work better, as the release will never be received otherwise
			if (gui.mouse_focus_mask == 0) {
				gui.mouse_focus = nullptr;
			}

			if (mouse_focus && mouse_focus->can_process()) {
				_gui_call_input(mouse_focus, mb);
			}

			/*if (gui.drag_data.get_type()!=Variant::NIL && mb->get_button_index()==BUTTON_LEFT) {
				_propagate_viewport_notification(this,NOTIFICATION_DRAG_END);
				gui.drag_data=Variant(); //always clear
			}*/

			// In case the mouse was released after for example dragging a scrollbar,
			// check whether the current control is different from the stored one. If
			// it is different, rather than wait for it to be updated the next time the
			// mouse is moved, notify the control so that it can e.g. drop the highlight.
			// This code is duplicated from the mm.is_valid()-case further below.

			Control *over = nullptr;
			if (gui.mouse_focus) {
				over = gui.mouse_focus;
			} else {
				over = _gui_find_control(mpos);
			}

			if (gui.mouse_focus_mask == 0 && over != gui.mouse_over) {
				_drop_mouse_over();

				_gui_cancel_tooltip();

				if (over) {
					_gui_call_notification(over, Control::NOTIFICATION_MOUSE_ENTER);
				}
			}

			gui.mouse_over = over;

			set_input_as_handled();
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;

	if (mm.is_valid()) {
		gui.key_event_accepted = false;
		Point2 mpos = mm->get_position();

		gui.last_mouse_pos = mpos;

		// D&D
		if (!gui.drag_attempted && gui.mouse_focus && mm->get_button_mask() & BUTTON_MASK_LEFT) {
			gui.drag_accum += mm->get_relative();
			float len = gui.drag_accum.length();
			if (len > 10) {
				{ //attempt grab, try parent controls too
					CanvasItem *ci = gui.mouse_focus;
					while (ci) {
						Control *control = Object::cast_to<Control>(ci);
						if (control) {
							gui.dragging = true;
							gui.drag_data = control->get_drag_data(control->get_global_transform_with_canvas().affine_inverse().xform(mpos) - gui.drag_accum);
							if (gui.drag_data.get_type() != Variant::NIL) {
								gui.mouse_focus = nullptr;
								gui.mouse_focus_mask = 0;
								break;
							} else {
								Control *drag_preview = _gui_get_drag_preview();
								if (drag_preview) {
									ERR_PRINT("Don't set a drag preview and return null data. Preview was deleted and drag request ignored.");
									memdelete(drag_preview);
									gui.drag_preview_id = 0;
								}
								gui.dragging = false;
							}

							if (control->data.mouse_filter == Control::MOUSE_FILTER_STOP) {
								break;
							}
						}

						if (ci->is_set_as_toplevel()) {
							break;
						}

						ci = ci->get_parent_item();
					}
				}

				gui.drag_attempted = true;
				if (gui.drag_data.get_type() != Variant::NIL) {
					_propagate_viewport_notification(this, NOTIFICATION_DRAG_BEGIN);
				}
			}
		}

		Control *over = nullptr;
		if (gui.mouse_focus) {
			over = gui.mouse_focus;
		} else if (gui.mouse_in_window) {
			over = _gui_find_control(mpos);
		}

		if (gui.drag_data.get_type() == Variant::NIL && over && !gui.modal_stack.empty()) {
			Control *top = gui.modal_stack.back()->get();

			if (over != top && !top->is_a_parent_of(over)) {
				PopupMenu *popup_menu = Object::cast_to<PopupMenu>(top);
				MenuButton *popup_menu_parent = nullptr;
				MenuButton *menu_button = Object::cast_to<MenuButton>(over);

				if (popup_menu) {
					popup_menu_parent = Object::cast_to<MenuButton>(popup_menu->get_parent());
					if (!popup_menu_parent) {
						// Go through the parents to see if there's a MenuButton at the end.
						while (Object::cast_to<PopupMenu>(popup_menu->get_parent())) {
							popup_menu = Object::cast_to<PopupMenu>(popup_menu->get_parent());
						}
						popup_menu_parent = Object::cast_to<MenuButton>(popup_menu->get_parent());
					}
				}

				// If the mouse is over a menu button, this menu will open automatically
				// if there is already a pop-up menu open at the same hierarchical level.
				if (popup_menu_parent && menu_button && popup_menu_parent->is_switch_on_hover() &&
						!menu_button->is_disabled() && menu_button->is_switch_on_hover() &&
						(popup_menu_parent->get_parent()->is_a_parent_of(menu_button) ||
								menu_button->get_parent()->is_a_parent_of(popup_menu))) {
					popup_menu->notification(Control::NOTIFICATION_MODAL_CLOSE);
					popup_menu->_modal_stack_remove();
					popup_menu->hide();

					menu_button->pressed();
					// As the popup wasn't triggered by a mouse click, the item focus needs to be removed manually.
					menu_button->get_popup()->set_current_index(-1);
				} else {
					over = nullptr; //nothing can be found outside the modal stack
				}
			}
		}

		if (over != gui.mouse_over) {
			_drop_mouse_over();

			_gui_cancel_tooltip();

			if (over) {
				_gui_call_notification(over, Control::NOTIFICATION_MOUSE_ENTER);
				gui.mouse_over = over;
			}
		}

		Control *drag_preview = _gui_get_drag_preview();
		if (drag_preview) {
			drag_preview->set_position(mpos);
		}

		if (!over) {
			OS::get_singleton()->set_cursor_shape((OS::CursorShape)Input::get_singleton()->get_default_cursor_shape());
			return;
		}

		Transform2D localizer = over->get_global_transform_with_canvas().affine_inverse();
		Size2 pos = localizer.xform(mpos);
		Vector2 speed = localizer.basis_xform(mm->get_speed());
		Vector2 rel = localizer.basis_xform(mm->get_relative());

		mm = mm->xformed_by(Transform2D()); //make a copy

		mm->set_global_position(mpos);
		mm->set_speed(speed);
		mm->set_relative(rel);

		if (mm->get_button_mask() == 0) {
			//nothing pressed

			bool can_tooltip = true;

			if (!gui.modal_stack.empty()) {
				if (gui.modal_stack.back()->get() != over && !gui.modal_stack.back()->get()->is_a_parent_of(over)) {
					can_tooltip = false;
				}
			}

			bool is_tooltip_shown = false;

			if (gui.tooltip_popup) {
				if (can_tooltip && gui.tooltip_control) {
					String tooltip = _gui_get_tooltip(over, gui.tooltip_control->get_global_transform().xform_inv(mpos));

					if (tooltip.length() == 0) {
						_gui_cancel_tooltip();
					} else if (gui.tooltip_label) {
						if (tooltip == gui.tooltip_label->get_text()) {
							is_tooltip_shown = true;
						}
					}
				} else {
					_gui_cancel_tooltip();
				}
			}

			if (can_tooltip && !is_tooltip_shown && over->can_process()) {
				if (gui.tooltip_timer.is_valid()) {
					gui.tooltip_timer->release_connections();
					gui.tooltip_timer = Ref<SceneTreeTimer>();
				}
				gui.tooltip_control = over;
				gui.tooltip_pos = mpos;
				gui.tooltip_timer = get_tree()->create_timer(gui.tooltip_delay);
				gui.tooltip_timer->set_ignore_time_scale(true);
				gui.tooltip_timer->connect("timeout", this, "_gui_show_tooltip");
			}
		}

		mm->set_position(pos);

		Control::CursorShape cursor_shape = Control::CURSOR_ARROW;
		{
			Control *c = over;
			Vector2 cpos = pos;
			while (c) {
				cursor_shape = c->get_cursor_shape(cpos);
				cpos = c->get_transform().xform(cpos);
				if (cursor_shape != Control::CURSOR_ARROW) {
					break;
				}
				if (c->data.mouse_filter == Control::MOUSE_FILTER_STOP) {
					break;
				}
				if (c->is_set_as_toplevel()) {
					break;
				}
				c = c->get_parent_control();
			}
		}

		OS::get_singleton()->set_cursor_shape((OS::CursorShape)cursor_shape);

		if (over && over->can_process()) {
			_gui_call_input(over, mm);
		}

		set_input_as_handled();

		if (gui.drag_data.get_type() != Variant::NIL) {
			bool can_drop = _gui_drop(over, pos, true);

			if (!can_drop) {
				OS::get_singleton()->set_cursor_shape(OS::CURSOR_FORBIDDEN);
			} else {
				OS::get_singleton()->set_cursor_shape(OS::CURSOR_CAN_DROP);
			}
		}
	}

	Ref<InputEventScreenTouch> touch_event = p_event;
	if (touch_event.is_valid()) {
		Size2 pos = touch_event->get_position();
		const int touch_index = touch_event->get_index();
		if (touch_event->is_pressed()) {
			Control *over = _gui_find_control(pos);
			if (over) {
				gui.touch_focus[touch_index] = over->get_instance_id();
				if (!gui.modal_stack.empty()) {
					Control *top = gui.modal_stack.back()->get();
					if (over != top && !top->is_a_parent_of(over)) {
						return;
					}
				}
				if (over->can_process()) {
					touch_event = touch_event->xformed_by(Transform2D()); //make a copy
					if (over == gui.mouse_focus) {
						pos = gui.focus_inv_xform.xform(pos);
					} else {
						pos = over->get_global_transform_with_canvas().affine_inverse().xform(pos);
					}
					touch_event->set_position(pos);
					_gui_call_input(over, touch_event);
					set_input_as_handled();
				}
				return;
			}
		} else {
			ObjectID control_id = gui.touch_focus[touch_index];
			Control *over = Object::cast_to<Control>(ObjectDB::get_instance(control_id));
			if (over && over->can_process()) {
				touch_event = touch_event->xformed_by(Transform2D()); //make a copy
				if (over == gui.last_mouse_focus) {
					pos = gui.focus_inv_xform.xform(pos);
				} else {
					pos = over->get_global_transform_with_canvas().affine_inverse().xform(pos);
				}
				touch_event->set_position(pos);

				_gui_call_input(over, touch_event);
				set_input_as_handled();
			}
			gui.touch_focus.erase(touch_index);
			return;
		}
	}

	Ref<InputEventGesture> gesture_event = p_event;
	if (gesture_event.is_valid()) {
		gui.key_event_accepted = false;

		_gui_cancel_tooltip();

		Size2 pos = gesture_event->get_position();

		Control *over = _gui_find_control(pos);
		if (over) {
			if (over->can_process()) {
				gesture_event = gesture_event->xformed_by(Transform2D()); //make a copy
				if (over == gui.mouse_focus) {
					pos = gui.focus_inv_xform.xform(pos);
				} else {
					pos = over->get_global_transform_with_canvas().affine_inverse().xform(pos);
				}
				gesture_event->set_position(pos);
				_gui_call_input(over, gesture_event);
			}
			set_input_as_handled();
			return;
		}
	}

	Ref<InputEventScreenDrag> drag_event = p_event;
	if (drag_event.is_valid()) {
		const int drag_event_index = drag_event->get_index();
		ObjectID control_id = gui.touch_focus[drag_event_index];
		Control *over = Object::cast_to<Control>(ObjectDB::get_instance(control_id));
		if (!over) {
			over = _gui_find_control(drag_event->get_position());
		}
		if (over) {
			if (!gui.modal_stack.empty()) {
				Control *top = gui.modal_stack.back()->get();
				if (over != top && !top->is_a_parent_of(over)) {
					return;
				}
			}
			if (over->can_process()) {
				Transform2D localizer = over->get_global_transform_with_canvas().affine_inverse();
				Size2 pos = localizer.xform(drag_event->get_position());
				Vector2 speed = localizer.basis_xform(drag_event->get_speed());
				Vector2 rel = localizer.basis_xform(drag_event->get_relative());

				drag_event = drag_event->xformed_by(Transform2D()); //make a copy

				drag_event->set_speed(speed);
				drag_event->set_relative(rel);
				drag_event->set_position(pos);

				_gui_call_input(over, drag_event);
			}

			set_input_as_handled();
			return;
		}
	}

	if (mm.is_null() && mb.is_null() && p_event->is_action_type()) {
		if (gui.key_focus && !gui.key_focus->is_visible_in_tree()) {
			gui.key_focus->release_focus();
		}

		if (gui.key_focus) {
			gui.key_event_accepted = false;
			if (gui.key_focus->can_process()) {
				gui.key_focus->call_multilevel(SceneStringNames::get_singleton()->_gui_input, p_event);
				if (gui.key_focus) { //maybe lost it
					gui.key_focus->emit_signal(SceneStringNames::get_singleton()->gui_input, p_event);
				}
			}

			if (gui.key_event_accepted) {
				set_input_as_handled();
				return;
			}
		}

		if (p_event->is_pressed() && p_event->is_action("ui_cancel") && !gui.modal_stack.empty()) {
			_gui_sort_modal_stack();
			Control *top = gui.modal_stack.back()->get();
			if (!top->data.modal_exclusive) {
				top->notification(Control::NOTIFICATION_MODAL_CLOSE);
				top->_modal_stack_remove();
				top->hide();
				// Close modal, set input as handled
				set_input_as_handled();
				return;
			}
		}

		Control *from = gui.key_focus ? gui.key_focus : nullptr; //hmm

		//keyboard focus
		//if (from && p_event->is_pressed() && !p_event->get_alt() && !p_event->get_metakey() && !p_event->key->get_command()) {

		Ref<InputEventKey> k = p_event;
		//need to check for mods, otherwise any combination of alt/ctrl/shift+<up/down/left/right/etc> is handled here when it shouldn't be.
		bool mods = k.is_valid() && (k->get_control() || k->get_alt() || k->get_shift() || k->get_metakey());

		if (from && p_event->is_pressed()) {
			Control *next = nullptr;

			Ref<InputEventJoypadMotion> joypadmotion_event = p_event;
			if (joypadmotion_event.is_valid()) {
				Input *input = Input::get_singleton();

				if (p_event->is_action_pressed("ui_focus_next") && input->is_action_just_pressed("ui_focus_next")) {
					next = from->find_next_valid_focus();
				}

				if (p_event->is_action_pressed("ui_focus_prev") && input->is_action_just_pressed("ui_focus_prev")) {
					next = from->find_prev_valid_focus();
				}

				if (!mods && p_event->is_action_pressed("ui_up") && input->is_action_just_pressed("ui_up")) {
					next = from->_get_focus_neighbour(MARGIN_TOP);
				}

				if (!mods && p_event->is_action_pressed("ui_left") && input->is_action_just_pressed("ui_left")) {
					next = from->_get_focus_neighbour(MARGIN_LEFT);
				}

				if (!mods && p_event->is_action_pressed("ui_right") && input->is_action_just_pressed("ui_right")) {
					next = from->_get_focus_neighbour(MARGIN_RIGHT);
				}

				if (!mods && p_event->is_action_pressed("ui_down") && input->is_action_just_pressed("ui_down")) {
					next = from->_get_focus_neighbour(MARGIN_BOTTOM);
				}
			} else {
				if (p_event->is_action_pressed("ui_focus_next", true, true)) {
					next = from->find_next_valid_focus();
				}

				if (p_event->is_action_pressed("ui_focus_prev", true, true)) {
					next = from->find_prev_valid_focus();
				}

				if (!mods && p_event->is_action_pressed("ui_up", true)) {
					next = from->_get_focus_neighbour(MARGIN_TOP);
				}

				if (!mods && p_event->is_action_pressed("ui_left", true)) {
					next = from->_get_focus_neighbour(MARGIN_LEFT);
				}

				if (!mods && p_event->is_action_pressed("ui_right", true)) {
					next = from->_get_focus_neighbour(MARGIN_RIGHT);
				}

				if (!mods && p_event->is_action_pressed("ui_down", true)) {
					next = from->_get_focus_neighbour(MARGIN_BOTTOM);
				}
			}

			if (next) {
				next->grab_focus();
				set_input_as_handled();
			}
		}
	}
}

void Viewport::_gui_cleanup_internal_state(Ref<InputEvent> p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		if (!mb->is_pressed()) {
			gui.mouse_focus_mask &= ~(1 << (mb->get_button_index() - 1)); // Remove from mask.
		}
	}
}

List<Control *>::Element *Viewport::_gui_add_root_control(Control *p_control) {
	gui.roots_order_dirty = true;
	return gui.roots.push_back(p_control);
}

List<Control *>::Element *Viewport::_gui_add_subwindow_control(Control *p_control) {
	p_control->connect("visibility_changed", this, "_subwindow_visibility_changed");

	if (p_control->is_visible_in_tree()) {
		gui.subwindow_order_dirty = true;
		gui.subwindows.push_back(p_control);
	}

	return gui.all_known_subwindows.push_back(p_control);
}

void Viewport::_gui_set_subwindow_order_dirty() {
	gui.subwindow_order_dirty = true;
}

void Viewport::_gui_set_root_order_dirty() {
	gui.roots_order_dirty = true;
}

void Viewport::_gui_remove_modal_control(List<Control *>::Element *MI) {
	gui.modal_stack.erase(MI);
}

void Viewport::_gui_remove_from_modal_stack(List<Control *>::Element *MI, ObjectID p_prev_focus_owner) {
	//transfer the focus stack to the next

	List<Control *>::Element *next = MI->next();

	gui.modal_stack.erase(MI);

	if (p_prev_focus_owner) {
		// for previous window in stack, pass the focus so it feels more
		// natural

		if (!next) { //top of stack

			Object *pfo = ObjectDB::get_instance(p_prev_focus_owner);
			Control *pfoc = Object::cast_to<Control>(pfo);
			if (!pfoc) {
				return;
			}

			if (!pfoc->is_inside_tree() || !pfoc->is_visible_in_tree()) {
				return;
			}
			pfoc->grab_focus();
		} else {
			next->get()->_modal_set_prev_focus_owner(p_prev_focus_owner);
		}
	}
}

void Viewport::_gui_force_drag(Control *p_base, const Variant &p_data, Control *p_control) {
	ERR_FAIL_COND_MSG(p_data.get_type() == Variant::NIL, "Drag data must be a value.");

	gui.dragging = true;
	gui.drag_data = p_data;
	gui.mouse_focus = nullptr;

	if (p_control) {
		_gui_set_drag_preview(p_base, p_control);
	}
	_propagate_viewport_notification(this, NOTIFICATION_DRAG_BEGIN);
}

void Viewport::_gui_set_drag_preview(Control *p_base, Control *p_control) {
	ERR_FAIL_NULL(p_control);
	ERR_FAIL_COND(!Object::cast_to<Control>((Object *)p_control));
	ERR_FAIL_COND(p_control->is_inside_tree());
	ERR_FAIL_COND(p_control->get_parent() != nullptr);

	Control *drag_preview = _gui_get_drag_preview();
	if (drag_preview) {
		memdelete(drag_preview);
	}
	p_control->set_as_toplevel(true);
	p_control->set_position(gui.last_mouse_pos);
	p_base->get_root_parent_control()->add_child(p_control); //add as child of viewport
	p_control->raise();

	gui.drag_preview_id = p_control->get_instance_id();
}

Control *Viewport::_gui_get_drag_preview() {
	if (!gui.drag_preview_id) {
		return nullptr;
	} else {
		Control *drag_preview = Object::cast_to<Control>(ObjectDB::get_instance(gui.drag_preview_id));
		if (!drag_preview) {
			ERR_PRINT("Don't free the control set as drag preview.");
			gui.drag_preview_id = 0;
		}
		return drag_preview;
	}
}

void Viewport::_gui_remove_root_control(List<Control *>::Element *RI) {
	gui.roots.erase(RI);
}

void Viewport::_gui_remove_subwindow_control(List<Control *>::Element *SI) {
	ERR_FAIL_COND(!SI);

	Control *control = SI->get();

	control->disconnect("visibility_changed", this, "_subwindow_visibility_changed");

	List<Control *>::Element *E = gui.subwindows.find(control);
	if (E) {
		gui.subwindows.erase(E);
	}

	gui.all_known_subwindows.erase(SI);
}

void Viewport::_gui_unfocus_control(Control *p_control) {
	if (gui.key_focus == p_control) {
		gui.key_focus->release_focus();
	}
}

void Viewport::_gui_hid_control(Control *p_control) {
	if (gui.mouse_focus == p_control) {
		_drop_mouse_focus();
	}

	if (gui.key_focus == p_control) {
		_gui_remove_focus();
	}
	if (gui.mouse_over == p_control) {
		gui.mouse_over = nullptr;
	}
	if (gui.tooltip_control == p_control) {
		_gui_cancel_tooltip();
	}
}

void Viewport::_gui_remove_control(Control *p_control) {
	if (gui.mouse_focus == p_control) {
		gui.mouse_focus = nullptr;
		gui.mouse_focus_mask = 0;
	}
	if (gui.last_mouse_focus == p_control) {
		gui.last_mouse_focus = nullptr;
	}
	if (gui.key_focus == p_control) {
		gui.key_focus = nullptr;
	}
	if (gui.mouse_over == p_control) {
		gui.mouse_over = nullptr;
	}
	if (gui.tooltip_control == p_control) {
		gui.tooltip_control = nullptr;
	}
	if (gui.tooltip_popup == p_control) {
		_gui_cancel_tooltip();
	}
}

void Viewport::_gui_remove_focus() {
	if (gui.key_focus) {
		Node *f = gui.key_focus;
		gui.key_focus = nullptr;
		f->notification(Control::NOTIFICATION_FOCUS_EXIT, true);
	}
}

bool Viewport::_gui_is_modal_on_top(const Control *p_control) {
	return (gui.modal_stack.size() && gui.modal_stack.back()->get() == p_control);
}

bool Viewport::_gui_control_has_focus(const Control *p_control) {
	return gui.key_focus == p_control;
}

void Viewport::_gui_control_grab_focus(Control *p_control) {
	//no need for change
	if (gui.key_focus && gui.key_focus == p_control) {
		return;
	}
	get_tree()->call_group_flags(SceneTree::GROUP_CALL_REALTIME, "_viewports", "_gui_remove_focus");
	gui.key_focus = p_control;
	emit_signal("gui_focus_changed", p_control);
	p_control->notification(Control::NOTIFICATION_FOCUS_ENTER);
	p_control->update();
}

void Viewport::_gui_accept_event() {
	gui.key_event_accepted = true;
	if (is_inside_tree()) {
		set_input_as_handled();
	}
}

void Viewport::_drop_mouse_focus() {
	Control *c = gui.mouse_focus;
	int mask = gui.mouse_focus_mask;
	gui.mouse_focus = nullptr;
	gui.mouse_focus_mask = 0;

	for (int i = 0; i < 3; i++) {
		if (mask & (1 << i)) {
			Ref<InputEventMouseButton> mb;
			mb.instance();
			mb->set_position(c->get_local_mouse_position());
			mb->set_global_position(c->get_local_mouse_position());
			mb->set_button_index(i + 1);
			mb->set_pressed(false);
			c->call_multilevel(SceneStringNames::get_singleton()->_gui_input, mb);
		}
	}
}

void Viewport::_drop_mouse_over() {
	if (gui.mouse_over) {
		_gui_call_notification(gui.mouse_over, Control::NOTIFICATION_MOUSE_EXIT);
		gui.mouse_over = nullptr;
	}
}

void Viewport::_drop_physics_mouseover(bool p_paused_only) {
	physics_has_last_mousepos = false;

	List<Map<ObjectID, uint64_t>::Element *> to_erase;

	for (Map<ObjectID, uint64_t>::Element *E = physics_2d_mouseover.front(); E; E = E->next()) {
		Object *o = ObjectDB::get_instance(E->key());
		if (o) {
			CollisionObject2D *co = Object::cast_to<CollisionObject2D>(o);
			if (co) {
				if (!co->is_inside_tree()) {
					to_erase.push_back(E);
					continue;
				}
				if (p_paused_only && co->can_process()) {
					continue;
				}
				co->_mouse_exit();
				to_erase.push_back(E);
			}
		}
	}

	while (to_erase.size()) {
		physics_2d_mouseover.erase(to_erase.front()->get());
		to_erase.pop_front();
	}

#ifndef _3D_DISABLED
	if (physics_object_over) {
		CollisionObject *co = Object::cast_to<CollisionObject>(ObjectDB::get_instance(physics_object_over));
		if (co) {
			if (!co->is_inside_tree()) {
				physics_object_over = physics_object_capture = 0;
			} else if (!(p_paused_only && co->can_process())) {
				co->_mouse_exit();
				physics_object_over = physics_object_capture = 0;
			}
		}
	}
#endif
}

List<Control *>::Element *Viewport::_gui_show_modal(Control *p_control) {
	List<Control *>::Element *node = gui.modal_stack.push_back(p_control);
	if (gui.key_focus) {
		p_control->_modal_set_prev_focus_owner(gui.key_focus->get_instance_id());
	} else {
		p_control->_modal_set_prev_focus_owner(0);
	}

	if (gui.mouse_focus && !p_control->is_a_parent_of(gui.mouse_focus) && !gui.mouse_click_grabber) {
		_drop_mouse_focus();
	}

	return node;
}

Control *Viewport::_gui_get_focus_owner() {
	return gui.key_focus;
}

void Viewport::_gui_grab_click_focus(Control *p_control) {
	gui.mouse_click_grabber = p_control;
	call_deferred("_post_gui_grab_click_focus");
}

void Viewport::_post_gui_grab_click_focus() {
	Control *focus_grabber = gui.mouse_click_grabber;
	if (!focus_grabber) {
		// Redundant grab requests were made
		return;
	}
	gui.mouse_click_grabber = nullptr;

	if (gui.mouse_focus) {
		if (gui.mouse_focus == focus_grabber) {
			return;
		}

		int mask = gui.mouse_focus_mask;
		Point2 click = gui.mouse_focus->get_global_transform_with_canvas().affine_inverse().xform(gui.last_mouse_pos);

		for (int i = 0; i < 3; i++) {
			if (mask & (1 << i)) {
				Ref<InputEventMouseButton> mb;
				mb.instance();

				//send unclic

				mb->set_position(click);
				mb->set_button_index(i + 1);
				mb->set_pressed(false);
				gui.mouse_focus->call_multilevel(SceneStringNames::get_singleton()->_gui_input, mb);
			}
		}

		gui.mouse_focus = focus_grabber;
		gui.focus_inv_xform = gui.mouse_focus->get_global_transform_with_canvas().affine_inverse();
		click = gui.mouse_focus->get_global_transform_with_canvas().affine_inverse().xform(gui.last_mouse_pos);

		for (int i = 0; i < 3; i++) {
			if (mask & (1 << i)) {
				Ref<InputEventMouseButton> mb;
				mb.instance();

				//send clic

				mb->set_position(click);
				mb->set_button_index(i + 1);
				mb->set_pressed(true);
				gui.mouse_focus->call_deferred(SceneStringNames::get_singleton()->_gui_input, mb);
			}
		}
	}
}

///////////////////////////////

void Viewport::input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(!is_inside_tree());

	local_input_handled = false;

	if (!is_input_handled()) {
		get_tree()->_call_input_pause(input_group, "_input", p_event); //not a bug, must happen before GUI, order is _input -> gui input -> _unhandled input
	}

	if (!is_input_handled()) {
		_gui_input_event(p_event);
	} else {
		_gui_cleanup_internal_state(p_event);
	}
	//get_tree()->call_group(SceneTree::GROUP_CALL_REVERSE|SceneTree::GROUP_CALL_REALTIME|SceneTree::GROUP_CALL_MULIILEVEL,gui_input_group,"_gui_input",p_event); //special one for GUI, as controls use their own process check
}

void Viewport::unhandled_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(!is_inside_tree());

	get_tree()->_call_input_pause(unhandled_input_group, "_unhandled_input", p_event);
	//call_group(GROUP_CALL_REVERSE|GROUP_CALL_REALTIME|GROUP_CALL_MULIILEVEL,"unhandled_input","_unhandled_input",ev);
	if (!get_tree()->input_handled && Object::cast_to<InputEventKey>(*p_event) != nullptr) {
		get_tree()->_call_input_pause(unhandled_key_input_group, "_unhandled_key_input", p_event);
		//call_group(GROUP_CALL_REVERSE|GROUP_CALL_REALTIME|GROUP_CALL_MULIILEVEL,"unhandled_key_input","_unhandled_key_input",ev);
	}

	if (physics_object_picking && !get_tree()->input_handled) {
		if (Input::get_singleton()->get_mouse_mode() != Input::MOUSE_MODE_CAPTURED &&
				(Object::cast_to<InputEventMouseButton>(*p_event) ||
						Object::cast_to<InputEventMouseMotion>(*p_event) ||
						Object::cast_to<InputEventScreenDrag>(*p_event) ||
						Object::cast_to<InputEventScreenTouch>(*p_event) ||
						Object::cast_to<InputEventKey>(*p_event) //to remember state

						)) {
			physics_picking_events.push_back(p_event);
		}
	}
}

void Viewport::set_use_own_world(bool p_use_own_world) {
	if (p_use_own_world == own_world.is_valid()) {
		return;
	}

	if (is_inside_tree()) {
		_propagate_exit_world(this);
	}

	if (p_use_own_world) {
		if (world.is_valid()) {
			own_world = world->duplicate();
			world->connect(CoreStringNames::get_singleton()->changed, this, "_own_world_changed");
		} else {
			own_world = Ref<World>(memnew(World));
		}
	} else {
		own_world = Ref<World>();
		if (world.is_valid()) {
			world->disconnect(CoreStringNames::get_singleton()->changed, this, "_own_world_changed");
		}
	}

	if (is_inside_tree()) {
		_propagate_enter_world(this);
	}

	if (is_inside_tree()) {
		VisualServer::get_singleton()->viewport_set_scenario(viewport, find_world()->get_scenario());
	}

	_update_listener();
}

bool Viewport::is_using_own_world() const {
	return own_world.is_valid();
}

void Viewport::set_attach_to_screen_rect(const Rect2 &p_rect) {
	VS::get_singleton()->viewport_attach_to_screen(viewport, p_rect);
	to_screen_rect = p_rect;
}

Rect2 Viewport::get_attach_to_screen_rect() const {
	return to_screen_rect;
}

void Viewport::set_use_render_direct_to_screen(bool p_render_direct_to_screen) {
	if (p_render_direct_to_screen == render_direct_to_screen) {
		return;
	}

	render_direct_to_screen = p_render_direct_to_screen;
	VS::get_singleton()->viewport_set_render_direct_to_screen(viewport, p_render_direct_to_screen);
}

bool Viewport::is_using_render_direct_to_screen() const {
	return render_direct_to_screen;
}

void Viewport::set_physics_object_picking(bool p_enable) {
	physics_object_picking = p_enable;
	if (!physics_object_picking) {
		physics_picking_events.clear();
	}
}

bool Viewport::get_physics_object_picking() {
	return physics_object_picking;
}

Vector2 Viewport::get_camera_coords(const Vector2 &p_viewport_coords) const {
	Transform2D xf = get_final_transform();
	return xf.xform(p_viewport_coords);
}

Vector2 Viewport::get_camera_rect_size() const {
	return size;
}

bool Viewport::gui_has_modal_stack() const {
	return gui.modal_stack.size();
}

void Viewport::set_disable_input(bool p_disable) {
	if (p_disable == disable_input) {
		return;
	}
	if (p_disable && GLOBAL_GET("gui/common/drop_mouse_on_gui_input_disabled")) {
		_drop_mouse_focus();
		_drop_mouse_over();
		_gui_cancel_tooltip();
	}
	disable_input = p_disable;
}

bool Viewport::is_input_disabled() const {
	return disable_input;
}

void Viewport::set_disable_3d(bool p_disable) {
	disable_3d = p_disable;
	VS::get_singleton()->viewport_set_disable_3d(viewport, p_disable);
}

bool Viewport::is_3d_disabled() const {
	return disable_3d;
}

void Viewport::set_keep_3d_linear(bool p_keep_3d_linear) {
	keep_3d_linear = p_keep_3d_linear;
	VS::get_singleton()->viewport_set_keep_3d_linear(viewport, keep_3d_linear);
}

bool Viewport::get_keep_3d_linear() const {
	return keep_3d_linear;
}

Variant Viewport::gui_get_drag_data() const {
	return gui.drag_data;
}

Control *Viewport::get_modal_stack_top() const {
	return gui.modal_stack.size() ? gui.modal_stack.back()->get() : NULL;
}

String Viewport::get_configuration_warning() const {
	String warning = Node::get_configuration_warning();

	if (size.x <= 1 || size.y <= 1) {
		if (warning != String()) {
			warning += "\n\n";
		}
		warning += TTR("The Viewport size must be greater than or equal to 2 pixels on both dimensions to render anything.");
	}

	if (!VisualServer::get_singleton()->is_low_end() && hdr && (usage == USAGE_2D || usage == USAGE_2D_NO_SAMPLING)) {
		if (warning != String()) {
			warning += "\n\n";
		}
		warning += TTR("This Viewport has HDR enabled, but its Usage is set to 2D or 2D No-Sampling.\nHDR is only supported in Viewports that have their Usage set to 3D or 3D No-Effects.\nHDR will be disabled for this Viewport.");
	}

	return warning;
}

void Viewport::gui_reset_canvas_sort_index() {
	gui.canvas_sort_index = 0;
}
int Viewport::gui_get_canvas_sort_index() {
	return gui.canvas_sort_index++;
}

void Viewport::set_msaa(MSAA p_msaa) {
	ERR_FAIL_INDEX(p_msaa, 7);
	if (msaa == p_msaa) {
		return;
	}
	msaa = p_msaa;
	VS::get_singleton()->viewport_set_msaa(viewport, VS::ViewportMSAA(p_msaa));
}

Viewport::MSAA Viewport::get_msaa() const {
	return msaa;
}

void Viewport::set_use_fxaa(bool p_fxaa) {
	if (p_fxaa == use_fxaa) {
		return;
	}
	use_fxaa = p_fxaa;
	VS::get_singleton()->viewport_set_use_fxaa(viewport, use_fxaa);
}

bool Viewport::get_use_fxaa() const {
	return use_fxaa;
}

void Viewport::set_use_debanding(bool p_debanding) {
	if (p_debanding == use_debanding) {
		return;
	}
	use_debanding = p_debanding;
	VS::get_singleton()->viewport_set_use_debanding(viewport, use_debanding);
}

bool Viewport::get_use_debanding() const {
	return use_debanding;
}

void Viewport::set_sharpen_intensity(float p_intensity) {
	if (p_intensity == sharpen_intensity) {
		return;
	}
	sharpen_intensity = p_intensity;
	VS::get_singleton()->viewport_set_sharpen_intensity(viewport, sharpen_intensity);
}

float Viewport::get_sharpen_intensity() const {
	return sharpen_intensity;
}

void Viewport::set_hdr(bool p_hdr) {
	if (hdr == p_hdr) {
		return;
	}

	hdr = p_hdr;
	VS::get_singleton()->viewport_set_hdr(viewport, p_hdr);
	update_configuration_warning();
}

bool Viewport::get_hdr() const {
	return hdr;
}

void Viewport::set_use_32_bpc_depth(bool p_enable) {
	if (use_32_bpc_depth == p_enable) {
		return;
	}

	use_32_bpc_depth = p_enable;
	VS::get_singleton()->viewport_set_use_32_bpc_depth(viewport, p_enable);
}

bool Viewport::is_using_32_bpc_depth() const {
	return use_32_bpc_depth;
}

void Viewport::set_usage(Usage p_usage) {
	if (usage == p_usage) {
		return;
	}

	usage = p_usage;
	VS::get_singleton()->viewport_set_usage(viewport, VS::ViewportUsage(p_usage));
	update_configuration_warning();
}

Viewport::Usage Viewport::get_usage() const {
	return usage;
}

void Viewport::set_debug_draw(DebugDraw p_debug_draw) {
	debug_draw = p_debug_draw;
	VS::get_singleton()->viewport_set_debug_draw(viewport, VS::ViewportDebugDraw(p_debug_draw));
}

Viewport::DebugDraw Viewport::get_debug_draw() const {
	return debug_draw;
}

int Viewport::get_render_info(RenderInfo p_info) {
	return VS::get_singleton()->viewport_get_render_info(viewport, VS::ViewportRenderInfo(p_info));
}

void Viewport::set_snap_controls_to_pixels(bool p_enable) {
	snap_controls_to_pixels = p_enable;
}

bool Viewport::is_snap_controls_to_pixels_enabled() const {
	return snap_controls_to_pixels;
}

bool Viewport::gui_is_dragging() const {
	return gui.dragging;
}

bool Viewport::gui_is_drag_successful() const {
	return gui.drag_successful;
}

void Viewport::set_input_as_handled() {
	_drop_physics_mouseover();
	if (handle_input_locally) {
		local_input_handled = true;
	} else {
		ERR_FAIL_COND(!is_inside_tree());
		get_tree()->set_input_as_handled();
	}
}

bool Viewport::is_input_handled() const {
	if (handle_input_locally) {
		return local_input_handled;
	} else {
		ERR_FAIL_COND_V(!is_inside_tree(), false);
		return get_tree()->is_input_handled();
	}
}

void Viewport::set_handle_input_locally(bool p_enable) {
	handle_input_locally = p_enable;
}

bool Viewport::is_handling_input_locally() const {
	return handle_input_locally;
}

void Viewport::notify_canvas_parent_child_count_reduced(const Node &p_parent) {
	uint32_t id = p_parent.get_canvas_parent_id();

	ERR_FAIL_COND(id == UINT32_MAX);
	ERR_FAIL_UNSIGNED_INDEX(id, GUI::canvas_parents_dirty_order.size());

	GUI::CanvasParent &d = GUI::canvas_parents_dirty_order[id];

	// No pending children moved.
	if (!d.last_child_moved_plus_one) {
		return;
	}

	uint32_t num_children = p_parent.get_child_count();
	d.last_child_moved_plus_one = MIN(d.last_child_moved_plus_one, num_children);
}

void Viewport::notify_canvas_parent_children_moved(Node &r_parent, uint32_t p_first_child, uint32_t p_last_child_plus_one) {
	// First ensure the parent has a CanvasParent allocated.
	uint32_t id = r_parent.get_canvas_parent_id();

	if (id == UINT32_MAX) {
		id = GUI::canvas_parents_dirty_order.size();
		r_parent.set_canvas_parent_id(id);
		GUI::canvas_parents_dirty_order.resize(id + 1);

		GUI::CanvasParent &d = GUI::canvas_parents_dirty_order[id];
		d.id = r_parent.get_instance_id();
		d.first_child_moved = p_first_child;
		d.last_child_moved_plus_one = p_last_child_plus_one;
		return;
	}

	GUI::CanvasParent &d = GUI::canvas_parents_dirty_order[id];
	DEV_CHECK_ONCE(d.id == r_parent.get_instance_id());
	d.first_child_moved = MIN(d.first_child_moved, p_first_child);
	d.last_child_moved_plus_one = MAX(d.last_child_moved_plus_one, p_last_child_plus_one);
}

void Viewport::flush_canvas_parents_dirty_order() {
	bool canvas_layers_moved = false;

	for (uint32_t n = 0; n < GUI::canvas_parents_dirty_order.size(); n++) {
		GUI::CanvasParent &d = GUI::canvas_parents_dirty_order[n];

		Object *obj = ObjectDB::get_instance(d.id);
		if (!obj) {
			// May have been deleted.
			continue;
		}

		Node *node = static_cast<Node *>(obj);
		Control *parent_control = Object::cast_to<Control>(node);

		// Allow anything subscribing to this signal (skeletons etc)
		// to make any changes necessary.
		node->emit_signal("child_order_changed");

		// Reset the id stored in the node, as this data is cleared after every flush.
		node->set_canvas_parent_id(UINT32_MAX);

		// This should be very rare, as the last_child_moved_plus_one is usually kept up
		// to date when within the scene tree. But it may become out of date outside the scene tree.
		// This will cause no problems, just a very small possibility of more notifications being sent than
		// necessary in that very rare situation.
		if (d.last_child_moved_plus_one > (uint32_t)node->get_child_count()) {
			d.last_child_moved_plus_one = node->get_child_count();
		}

		bool subwindow_order_dirty = false;
		bool root_order_dirty = false;
		bool control_found = false;

		for (uint32_t c = d.first_child_moved; c < d.last_child_moved_plus_one; c++) {
			Node *child = node->get_child(c);

			CanvasItem *ci = Object::cast_to<CanvasItem>(child);
			if (ci) {
				ci->update_draw_order();

				Control *control = Object::cast_to<Control>(ci);
				if (control) {
					control->_query_order_update(subwindow_order_dirty, root_order_dirty);

					if (parent_control && !control_found) {
						control_found = true;

						// In case of regressions, this could be moved to AFTER
						// the children have been updated.
						parent_control->update();
					}
					control->update();
				}

				continue;
			}

			CanvasLayer *cl = Object::cast_to<CanvasLayer>(child);
			if (cl) {
				// If any canvas layers moved, we need to do an expensive update,
				// so we ensure doing this only once.
				if (!canvas_layers_moved) {
					canvas_layers_moved = true;
					cl->update_draw_order();
				}
			}
		}

		Viewport *viewport = node->get_viewport();
		if (viewport) {
			if (subwindow_order_dirty) {
				viewport->_gui_set_subwindow_order_dirty();
			}
			if (root_order_dirty) {
				viewport->_gui_set_root_order_dirty();
			}
		}

		node->notification(Node::NOTIFICATION_CHILD_ORDER_CHANGED);
	}

	GUI::canvas_parents_dirty_order.clear();
}

void Viewport::_validate_property(PropertyInfo &property) const {
	if (VisualServer::get_singleton()->is_low_end() && (property.name == "hdr" || property.name == "use_32_bpc_depth" || property.name == "debanding" || property.name == "sharpen_intensity" || property.name == "debug_draw")) {
		// Only available in GLES3.
		property.usage = PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL;
	}

	if (!VisualServer::get_singleton()->is_low_end() && (property.name == "render_direct_to_screen")) {
		// Only available in GLES2.
		property.usage = PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL;
	}
}

void Viewport::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_use_arvr", "use"), &Viewport::set_use_arvr);
	ClassDB::bind_method(D_METHOD("use_arvr"), &Viewport::use_arvr);

	ClassDB::bind_method(D_METHOD("set_size", "size"), &Viewport::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &Viewport::get_size);
	ClassDB::bind_method(D_METHOD("set_world_2d", "world_2d"), &Viewport::set_world_2d);
	ClassDB::bind_method(D_METHOD("get_world_2d"), &Viewport::get_world_2d);
	ClassDB::bind_method(D_METHOD("find_world_2d"), &Viewport::find_world_2d);
	ClassDB::bind_method(D_METHOD("set_world", "world"), &Viewport::set_world);
	ClassDB::bind_method(D_METHOD("get_world"), &Viewport::get_world);
	ClassDB::bind_method(D_METHOD("find_world"), &Viewport::find_world);

	ClassDB::bind_method(D_METHOD("set_canvas_transform", "xform"), &Viewport::set_canvas_transform);
	ClassDB::bind_method(D_METHOD("get_canvas_transform"), &Viewport::get_canvas_transform);

	ClassDB::bind_method(D_METHOD("set_global_canvas_transform", "xform"), &Viewport::set_global_canvas_transform);
	ClassDB::bind_method(D_METHOD("get_global_canvas_transform"), &Viewport::get_global_canvas_transform);
	ClassDB::bind_method(D_METHOD("get_final_transform"), &Viewport::get_final_transform);

	ClassDB::bind_method(D_METHOD("get_visible_rect"), &Viewport::get_visible_rect);
	ClassDB::bind_method(D_METHOD("set_transparent_background", "enable"), &Viewport::set_transparent_background);
	ClassDB::bind_method(D_METHOD("has_transparent_background"), &Viewport::has_transparent_background);

	ClassDB::bind_method(D_METHOD("_vp_input"), &Viewport::_vp_input);
	ClassDB::bind_method(D_METHOD("_vp_input_text", "text"), &Viewport::_vp_input_text);
	ClassDB::bind_method(D_METHOD("_vp_unhandled_input"), &Viewport::_vp_unhandled_input);

	ClassDB::bind_method(D_METHOD("set_size_override", "enable", "size", "margin"), &Viewport::set_size_override, DEFVAL(Size2(-1, -1)), DEFVAL(Size2(0, 0)));
	ClassDB::bind_method(D_METHOD("get_size_override"), &Viewport::get_size_override);
	ClassDB::bind_method(D_METHOD("is_size_override_enabled"), &Viewport::is_size_override_enabled);
	ClassDB::bind_method(D_METHOD("set_size_override_stretch", "enabled"), &Viewport::set_size_override_stretch);
	ClassDB::bind_method(D_METHOD("is_size_override_stretch_enabled"), &Viewport::is_size_override_stretch_enabled);

	ClassDB::bind_method(D_METHOD("set_vflip", "enable"), &Viewport::set_vflip);
	ClassDB::bind_method(D_METHOD("get_vflip"), &Viewport::get_vflip);

	ClassDB::bind_method(D_METHOD("set_clear_mode", "mode"), &Viewport::set_clear_mode);
	ClassDB::bind_method(D_METHOD("get_clear_mode"), &Viewport::get_clear_mode);

	ClassDB::bind_method(D_METHOD("set_update_mode", "mode"), &Viewport::set_update_mode);
	ClassDB::bind_method(D_METHOD("get_update_mode"), &Viewport::get_update_mode);

	ClassDB::bind_method(D_METHOD("set_msaa", "msaa"), &Viewport::set_msaa);
	ClassDB::bind_method(D_METHOD("get_msaa"), &Viewport::get_msaa);

	ClassDB::bind_method(D_METHOD("set_use_fxaa", "enable"), &Viewport::set_use_fxaa);
	ClassDB::bind_method(D_METHOD("get_use_fxaa"), &Viewport::get_use_fxaa);

	ClassDB::bind_method(D_METHOD("set_use_debanding", "enable"), &Viewport::set_use_debanding);
	ClassDB::bind_method(D_METHOD("get_use_debanding"), &Viewport::get_use_debanding);

	ClassDB::bind_method(D_METHOD("set_sharpen_intensity", "intensity"), &Viewport::set_sharpen_intensity);
	ClassDB::bind_method(D_METHOD("get_sharpen_intensity"), &Viewport::get_sharpen_intensity);

	ClassDB::bind_method(D_METHOD("set_hdr", "enable"), &Viewport::set_hdr);
	ClassDB::bind_method(D_METHOD("get_hdr"), &Viewport::get_hdr);

	ClassDB::bind_method(D_METHOD("set_use_32_bpc_depth", "enable"), &Viewport::set_use_32_bpc_depth);
	ClassDB::bind_method(D_METHOD("get_use_32_bpc_depth"), &Viewport::is_using_32_bpc_depth);

	ClassDB::bind_method(D_METHOD("set_usage", "usage"), &Viewport::set_usage);
	ClassDB::bind_method(D_METHOD("get_usage"), &Viewport::get_usage);

	ClassDB::bind_method(D_METHOD("set_debug_draw", "debug_draw"), &Viewport::set_debug_draw);
	ClassDB::bind_method(D_METHOD("get_debug_draw"), &Viewport::get_debug_draw);

	ClassDB::bind_method(D_METHOD("get_render_info", "info"), &Viewport::get_render_info);

	ClassDB::bind_method(D_METHOD("get_texture"), &Viewport::get_texture);

	ClassDB::bind_method(D_METHOD("set_physics_object_picking", "enable"), &Viewport::set_physics_object_picking);
	ClassDB::bind_method(D_METHOD("get_physics_object_picking"), &Viewport::get_physics_object_picking);

	ClassDB::bind_method(D_METHOD("get_viewport_rid"), &Viewport::get_viewport_rid);
	ClassDB::bind_method(D_METHOD("input", "local_event"), &Viewport::input);
	ClassDB::bind_method(D_METHOD("unhandled_input", "local_event"), &Viewport::unhandled_input);

	ClassDB::bind_method(D_METHOD("update_worlds"), &Viewport::update_worlds);

	ClassDB::bind_method(D_METHOD("set_use_own_world", "enable"), &Viewport::set_use_own_world);
	ClassDB::bind_method(D_METHOD("is_using_own_world"), &Viewport::is_using_own_world);

	ClassDB::bind_method(D_METHOD("get_camera"), &Viewport::get_camera);

	ClassDB::bind_method(D_METHOD("set_as_audio_listener", "enable"), &Viewport::set_as_audio_listener);
	ClassDB::bind_method(D_METHOD("is_audio_listener"), &Viewport::is_audio_listener);

	ClassDB::bind_method(D_METHOD("set_as_audio_listener_2d", "enable"), &Viewport::set_as_audio_listener_2d);
	ClassDB::bind_method(D_METHOD("is_audio_listener_2d"), &Viewport::is_audio_listener_2d);
	ClassDB::bind_method(D_METHOD("set_attach_to_screen_rect", "rect"), &Viewport::set_attach_to_screen_rect);
	ClassDB::bind_method(D_METHOD("set_use_render_direct_to_screen", "enable"), &Viewport::set_use_render_direct_to_screen);
	ClassDB::bind_method(D_METHOD("is_using_render_direct_to_screen"), &Viewport::is_using_render_direct_to_screen);

	ClassDB::bind_method(D_METHOD("get_mouse_position"), &Viewport::get_mouse_position);
	ClassDB::bind_method(D_METHOD("warp_mouse", "to_position"), &Viewport::warp_mouse);

	ClassDB::bind_method(D_METHOD("gui_has_modal_stack"), &Viewport::gui_has_modal_stack);
	ClassDB::bind_method(D_METHOD("gui_get_drag_data"), &Viewport::gui_get_drag_data);
	ClassDB::bind_method(D_METHOD("gui_is_dragging"), &Viewport::gui_is_dragging);
	ClassDB::bind_method(D_METHOD("gui_is_drag_successful"), &Viewport::gui_is_drag_successful);

	ClassDB::bind_method(D_METHOD("get_modal_stack_top"), &Viewport::get_modal_stack_top);

	ClassDB::bind_method(D_METHOD("set_disable_input", "disable"), &Viewport::set_disable_input);
	ClassDB::bind_method(D_METHOD("is_input_disabled"), &Viewport::is_input_disabled);

	ClassDB::bind_method(D_METHOD("set_disable_3d", "disable"), &Viewport::set_disable_3d);
	ClassDB::bind_method(D_METHOD("is_3d_disabled"), &Viewport::is_3d_disabled);

	ClassDB::bind_method(D_METHOD("set_keep_3d_linear", "keep_3d_linear"), &Viewport::set_keep_3d_linear);
	ClassDB::bind_method(D_METHOD("get_keep_3d_linear"), &Viewport::get_keep_3d_linear);

	ClassDB::bind_method(D_METHOD("_gui_show_tooltip"), &Viewport::_gui_show_tooltip);
	ClassDB::bind_method(D_METHOD("_gui_remove_focus"), &Viewport::_gui_remove_focus);
	ClassDB::bind_method(D_METHOD("_post_gui_grab_click_focus"), &Viewport::_post_gui_grab_click_focus);

	ClassDB::bind_method(D_METHOD("set_shadow_atlas_size", "size"), &Viewport::set_shadow_atlas_size);
	ClassDB::bind_method(D_METHOD("get_shadow_atlas_size"), &Viewport::get_shadow_atlas_size);

	ClassDB::bind_method(D_METHOD("set_snap_controls_to_pixels", "enabled"), &Viewport::set_snap_controls_to_pixels);
	ClassDB::bind_method(D_METHOD("is_snap_controls_to_pixels_enabled"), &Viewport::is_snap_controls_to_pixels_enabled);

	ClassDB::bind_method(D_METHOD("set_shadow_atlas_quadrant_subdiv", "quadrant", "subdiv"), &Viewport::set_shadow_atlas_quadrant_subdiv);
	ClassDB::bind_method(D_METHOD("get_shadow_atlas_quadrant_subdiv", "quadrant"), &Viewport::get_shadow_atlas_quadrant_subdiv);

	ClassDB::bind_method(D_METHOD("set_input_as_handled"), &Viewport::set_input_as_handled);
	ClassDB::bind_method(D_METHOD("is_input_handled"), &Viewport::is_input_handled);

	ClassDB::bind_method(D_METHOD("set_handle_input_locally", "enable"), &Viewport::set_handle_input_locally);
	ClassDB::bind_method(D_METHOD("is_handling_input_locally"), &Viewport::is_handling_input_locally);

	ClassDB::bind_method(D_METHOD("_subwindow_visibility_changed"), &Viewport::_subwindow_visibility_changed);

	ClassDB::bind_method(D_METHOD("_own_world_changed"), &Viewport::_own_world_changed);

	ClassDB::bind_method(D_METHOD("_process_picking", "ignore_paused"), &Viewport::_process_picking);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "arvr"), "set_use_arvr", "use_arvr");

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "size"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "size_override_stretch"), "set_size_override_stretch", "is_size_override_stretch_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "own_world"), "set_use_own_world", "is_using_own_world");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "world", PROPERTY_HINT_RESOURCE_TYPE, "World"), "set_world", "get_world");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "world_2d", PROPERTY_HINT_RESOURCE_TYPE, "World2D", 0), "set_world_2d", "get_world_2d");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "transparent_bg"), "set_transparent_background", "has_transparent_background");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "handle_input_locally"), "set_handle_input_locally", "is_handling_input_locally");
	ADD_GROUP("Rendering", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "msaa", PROPERTY_HINT_ENUM, "Disabled,2x,4x,8x,16x,AndroidVR 2x,AndroidVR 4x"), "set_msaa", "get_msaa");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "fxaa"), "set_use_fxaa", "get_use_fxaa");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debanding"), "set_use_debanding", "get_use_debanding");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "sharpen_intensity", PROPERTY_HINT_RANGE, "0,1"), "set_sharpen_intensity", "get_sharpen_intensity");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hdr"), "set_hdr", "get_hdr");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_32_bpc_depth"), "set_use_32_bpc_depth", "get_use_32_bpc_depth");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disable_3d"), "set_disable_3d", "is_3d_disabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "keep_3d_linear"), "set_keep_3d_linear", "get_keep_3d_linear");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "usage", PROPERTY_HINT_ENUM, "2D,2D Without Sampling,3D,3D Without Effects"), "set_usage", "get_usage");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "render_direct_to_screen"), "set_use_render_direct_to_screen", "is_using_render_direct_to_screen");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "debug_draw", PROPERTY_HINT_ENUM, "Disabled,Unshaded,Overdraw,Wireframe"), "set_debug_draw", "get_debug_draw");
	ADD_GROUP("Render Target", "render_target_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "render_target_v_flip"), "set_vflip", "get_vflip");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "render_target_clear_mode", PROPERTY_HINT_ENUM, "Always,Never,Next Frame"), "set_clear_mode", "get_clear_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "render_target_update_mode", PROPERTY_HINT_ENUM, "Disabled,Once,When Visible,Always"), "set_update_mode", "get_update_mode");
	ADD_GROUP("Audio Listener", "audio_listener_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "audio_listener_enable_2d"), "set_as_audio_listener_2d", "is_audio_listener_2d");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "audio_listener_enable_3d"), "set_as_audio_listener", "is_audio_listener");
	ADD_GROUP("Physics", "physics_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "physics_object_picking"), "set_physics_object_picking", "get_physics_object_picking");
	ADD_GROUP("GUI", "gui_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "gui_disable_input"), "set_disable_input", "is_input_disabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "gui_snap_controls_to_pixels"), "set_snap_controls_to_pixels", "is_snap_controls_to_pixels_enabled");
	ADD_GROUP("Shadow Atlas", "shadow_atlas_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "shadow_atlas_size", PROPERTY_HINT_RANGE, "0,16384,256"), "set_shadow_atlas_size", "get_shadow_atlas_size");
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "shadow_atlas_quad_0", PROPERTY_HINT_ENUM, "Disabled,1 Shadow,4 Shadows,16 Shadows,64 Shadows,256 Shadows,1024 Shadows"), "set_shadow_atlas_quadrant_subdiv", "get_shadow_atlas_quadrant_subdiv", 0);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "shadow_atlas_quad_1", PROPERTY_HINT_ENUM, "Disabled,1 Shadow,4 Shadows,16 Shadows,64 Shadows,256 Shadows,1024 Shadows"), "set_shadow_atlas_quadrant_subdiv", "get_shadow_atlas_quadrant_subdiv", 1);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "shadow_atlas_quad_2", PROPERTY_HINT_ENUM, "Disabled,1 Shadow,4 Shadows,16 Shadows,64 Shadows,256 Shadows,1024 Shadows"), "set_shadow_atlas_quadrant_subdiv", "get_shadow_atlas_quadrant_subdiv", 2);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "shadow_atlas_quad_3", PROPERTY_HINT_ENUM, "Disabled,1 Shadow,4 Shadows,16 Shadows,64 Shadows,256 Shadows,1024 Shadows"), "set_shadow_atlas_quadrant_subdiv", "get_shadow_atlas_quadrant_subdiv", 3);
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM2D, "canvas_transform", PROPERTY_HINT_NONE, "", 0), "set_canvas_transform", "get_canvas_transform");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM2D, "global_canvas_transform", PROPERTY_HINT_NONE, "", 0), "set_global_canvas_transform", "get_global_canvas_transform");

	ADD_SIGNAL(MethodInfo("size_changed"));
	ADD_SIGNAL(MethodInfo("gui_focus_changed", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Control")));

	BIND_ENUM_CONSTANT(UPDATE_DISABLED);
	BIND_ENUM_CONSTANT(UPDATE_ONCE);
	BIND_ENUM_CONSTANT(UPDATE_WHEN_VISIBLE);
	BIND_ENUM_CONSTANT(UPDATE_ALWAYS);

	BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_DISABLED);
	BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_1);
	BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_4);
	BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_16);
	BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_64);
	BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_256);
	BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_1024);
	BIND_ENUM_CONSTANT(SHADOW_ATLAS_QUADRANT_SUBDIV_MAX);

	BIND_ENUM_CONSTANT(RENDER_INFO_OBJECTS_IN_FRAME);
	BIND_ENUM_CONSTANT(RENDER_INFO_VERTICES_IN_FRAME);
	BIND_ENUM_CONSTANT(RENDER_INFO_MATERIAL_CHANGES_IN_FRAME);
	BIND_ENUM_CONSTANT(RENDER_INFO_SHADER_CHANGES_IN_FRAME);
	BIND_ENUM_CONSTANT(RENDER_INFO_SURFACE_CHANGES_IN_FRAME);
	BIND_ENUM_CONSTANT(RENDER_INFO_DRAW_CALLS_IN_FRAME);
	BIND_ENUM_CONSTANT(RENDER_INFO_2D_ITEMS_IN_FRAME);
	BIND_ENUM_CONSTANT(RENDER_INFO_2D_DRAW_CALLS_IN_FRAME);
	BIND_ENUM_CONSTANT(RENDER_INFO_MAX);

	BIND_ENUM_CONSTANT(DEBUG_DRAW_DISABLED);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_UNSHADED);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_OVERDRAW);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_WIREFRAME);

	BIND_ENUM_CONSTANT(MSAA_DISABLED);
	BIND_ENUM_CONSTANT(MSAA_2X);
	BIND_ENUM_CONSTANT(MSAA_4X);
	BIND_ENUM_CONSTANT(MSAA_8X);
	BIND_ENUM_CONSTANT(MSAA_16X);

	BIND_ENUM_CONSTANT(USAGE_2D);
	BIND_ENUM_CONSTANT(USAGE_2D_NO_SAMPLING);
	BIND_ENUM_CONSTANT(USAGE_3D);
	BIND_ENUM_CONSTANT(USAGE_3D_NO_EFFECTS);

	BIND_ENUM_CONSTANT(CLEAR_MODE_ALWAYS);
	BIND_ENUM_CONSTANT(CLEAR_MODE_NEVER);
	BIND_ENUM_CONSTANT(CLEAR_MODE_ONLY_NEXT_FRAME);
}

void Viewport::_subwindow_visibility_changed() {
	// unfortunately, we don't know the sender, i.e. which subwindow changed;
	// so we have to check them all.
	gui.subwindow_visibility_dirty = true;
}

Viewport::Viewport() {
	world_2d = Ref<World2D>(memnew(World2D));

	viewport = RID_PRIME(VisualServer::get_singleton()->viewport_create());
	texture_rid = VisualServer::get_singleton()->viewport_get_texture(viewport);
	texture_flags = 0;

	render_direct_to_screen = false;

	default_texture.instance();
	default_texture->vp = const_cast<Viewport *>(this);
	viewport_textures.insert(default_texture.ptr());
	VS::get_singleton()->texture_set_proxy(default_texture->proxy, texture_rid);

	//internal_listener = SpatialSoundServer::get_singleton()->listener_create();
	audio_listener = false;
	//internal_listener_2d = SpatialSound2DServer::get_singleton()->listener_create();
	audio_listener_2d = false;
	transparent_bg = false;
	parent = nullptr;
	listener = nullptr;
	camera = nullptr;
	override_canvas_transform = false;
	canvas_layers.insert(NULL); // This eases picking code (interpreted as the canvas of the Viewport)
	arvr = false;
	size_override = false;
	size_override_stretch = false;
	size_override_size = Size2(1, 1);
	gen_mipmaps = false;

	vflip = false;

	//clear=true;
	update_mode = UPDATE_WHEN_VISIBLE;

	physics_object_picking = false;
	physics_object_capture = 0;
	physics_object_over = 0;
	physics_has_last_mousepos = false;
	physics_last_mousepos = Vector2(Math_INF, Math_INF);

	shadow_atlas_size = 0;
	for (int i = 0; i < 4; i++) {
		shadow_atlas_quadrant_subdiv[i] = SHADOW_ATLAS_QUADRANT_SUBDIV_MAX;
	}
	set_shadow_atlas_quadrant_subdiv(0, SHADOW_ATLAS_QUADRANT_SUBDIV_4);
	set_shadow_atlas_quadrant_subdiv(1, SHADOW_ATLAS_QUADRANT_SUBDIV_4);
	set_shadow_atlas_quadrant_subdiv(2, SHADOW_ATLAS_QUADRANT_SUBDIV_16);
	set_shadow_atlas_quadrant_subdiv(3, SHADOW_ATLAS_QUADRANT_SUBDIV_64);

	String id = itos(get_instance_id());
	input_group = "_vp_input" + id;
	gui_input_group = "_vp_gui_input" + id;
	unhandled_input_group = "_vp_unhandled_input" + id;
	unhandled_key_input_group = "_vp_unhandled_key_input" + id;

	disable_input = false;
	disable_3d = false;
	keep_3d_linear = false;

	// Window tooltip.
	gui.tooltip_timer = -1;

	gui.tooltip_delay = GLOBAL_DEF("gui/timers/tooltip_delay_sec", 0.5);
	ProjectSettings::get_singleton()->set_custom_property_info("gui/timers/tooltip_delay_sec", PropertyInfo(Variant::REAL, "gui/timers/tooltip_delay_sec", PROPERTY_HINT_RANGE, "0,5,0.01,or_greater")); // No negative numbers

	gui.tooltip_control = nullptr;
	gui.tooltip_label = nullptr;
	gui.drag_preview_id = 0;
	gui.drag_attempted = false;
	gui.canvas_sort_index = 0;
	gui.roots_order_dirty = false;
	gui.mouse_focus = nullptr;
	gui.last_mouse_focus = nullptr;

	msaa = MSAA_DISABLED;
	use_fxaa = false;
	use_debanding = false;
	sharpen_intensity = 0.0;
	hdr = true;
	use_32_bpc_depth = false;

	usage = USAGE_3D;
	debug_draw = DEBUG_DRAW_DISABLED;
	clear_mode = CLEAR_MODE_ALWAYS;

	snap_controls_to_pixels = true;
	physics_last_mouse_state.alt = false;
	physics_last_mouse_state.control = false;
	physics_last_mouse_state.shift = false;
	physics_last_mouse_state.meta = false;
	physics_last_mouse_state.mouse_mask = 0;
	local_input_handled = false;
	handle_input_locally = true;
	physics_last_id = 0; //ensures first time there will be a check

	// Physics interpolation mode for viewports is a special case.
	// Typically viewports will be housed within Controls,
	// and Controls default to PHYSICS_INTERPOLATION_MODE_OFF.
	// Viewports can thus inherit physics interpolation OFF, which is unexpected.
	// Setting to ON allows each viewport to have a fresh interpolation state.
	set_physics_interpolation_mode(Node::PHYSICS_INTERPOLATION_MODE_ON);
}

Viewport::~Viewport() {
	//erase itself from viewport textures
	for (Set<ViewportTexture *>::Element *E = viewport_textures.front(); E; E = E->next()) {
		E->get()->vp = nullptr;
	}
	VisualServer::get_singleton()->free(viewport);
	//SpatialSoundServer::get_singleton()->free(internal_listener);
	//SpatialSound2DServer::get_singleton()->free(internal_listener_2d);
}

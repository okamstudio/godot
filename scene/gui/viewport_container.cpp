/**************************************************************************/
/*  viewport_container.cpp                                                */
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

#include "viewport_container.h"

#include "core/engine.h"
#include "scene/main/viewport.h"

Size2 ViewportContainer::get_minimum_size() const {
	if (stretch) {
		return Size2();
	}
	Size2 ms;
	for (int i = 0; i < get_child_count(); i++) {
		Viewport *c = Object::cast_to<Viewport>(get_child(i));
		if (!c) {
			continue;
		}

		Size2 minsize = c->get_size();
		if (c->is_size_override_enabled()) {
			minsize = c->get_size_override();
		}
		ms.width = MAX(ms.width, minsize.width);
		ms.height = MAX(ms.height, minsize.height);
	}

	return ms;
}

void ViewportContainer::set_stretch(bool p_enable) {
	stretch = p_enable;
	minimum_size_changed();
	queue_sort();
	update();
}

bool ViewportContainer::is_stretch_enabled() const {
	return stretch;
}

void ViewportContainer::set_stretch_shrink(int p_shrink) {
	ERR_FAIL_COND(p_shrink < 1);
	if (shrink == p_shrink) {
		return;
	}

	shrink = p_shrink;

	if (!stretch) {
		return;
	}

	for (int i = 0; i < get_child_count(); i++) {
		Viewport *c = Object::cast_to<Viewport>(get_child(i));
		if (!c) {
			continue;
		}

		set_child_viewport_size(c);
	}

	update();
}

void ViewportContainer::set_child_viewport_size(Viewport *child_viewport) {
	Size2 normal_size = get_size() / shrink;
	if (get_tree()->get_stretch_mode() == SceneTree::STRETCH_MODE_2D) {
		Viewport *parent_viewport = get_viewport();
		Size2 screen_stretch = parent_viewport->get_size() / parent_viewport->get_size_override();
		child_viewport->set_size(normal_size * screen_stretch);
		child_viewport->set_size_override_stretch(true);
		child_viewport->set_size_override(true, normal_size);
	} else {
		child_viewport->set_size(normal_size);
	}
}

int ViewportContainer::get_stretch_shrink() const {
	return shrink;
}

void ViewportContainer::_notification(int p_what) {
	if (p_what == NOTIFICATION_RESIZED || p_what == NOTIFICATION_READY) {
		if (!stretch) {
			return;
		}

		for (int i = 0; i < get_child_count(); i++) {
			Viewport *c = Object::cast_to<Viewport>(get_child(i));
			if (!c) {
				continue;
			}

			set_child_viewport_size(c);
		}
	}

	if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_VISIBILITY_CHANGED) {
		for (int i = 0; i < get_child_count(); i++) {
			Viewport *c = Object::cast_to<Viewport>(get_child(i));
			if (!c) {
				continue;
			}

			if (is_visible_in_tree()) {
				c->set_update_mode(Viewport::UPDATE_ALWAYS);
			} else {
				c->set_update_mode(Viewport::UPDATE_DISABLED);
			}

			c->set_handle_input_locally(false); //do not handle input locally here
		}
	}

	if (p_what == NOTIFICATION_DRAW) {
		for (int i = 0; i < get_child_count(); i++) {
			Viewport *c = Object::cast_to<Viewport>(get_child(i));
			if (!c) {
				continue;
			}

			if (stretch) {
				draw_texture_rect(c->get_texture(), Rect2(Vector2(), get_size() * Size2(1, -1)));
			} else {
				draw_texture_rect(c->get_texture(), Rect2(Vector2(), c->get_size() * Size2(1, -1)));
			}
		}
	}
}

void ViewportContainer::_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	Transform2D xform = get_global_transform_with_canvas();

	if (stretch) {
		Transform2D scale_xf;
		scale_xf.scale(Vector2(shrink, shrink));
		xform *= scale_xf;
	}

	Ref<InputEvent> ev = p_event->xformed_by(xform.affine_inverse());

	for (int i = 0; i < get_child_count(); i++) {
		Viewport *c = Object::cast_to<Viewport>(get_child(i));
		if (!c || c->is_input_disabled()) {
			continue;
		}

		c->input(ev);
	}
}

void ViewportContainer::_unhandled_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	Transform2D xform = get_global_transform_with_canvas();

	if (stretch) {
		Transform2D scale_xf;
		scale_xf.scale(Vector2(shrink, shrink));
		xform *= scale_xf;
	}

	Ref<InputEvent> ev = p_event->xformed_by(xform.affine_inverse());

	for (int i = 0; i < get_child_count(); i++) {
		Viewport *c = Object::cast_to<Viewport>(get_child(i));
		if (!c || c->is_input_disabled()) {
			continue;
		}

		c->unhandled_input(ev);
	}
}

void ViewportContainer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_unhandled_input", "event"), &ViewportContainer::_unhandled_input);
	ClassDB::bind_method(D_METHOD("_input", "event"), &ViewportContainer::_input);
	ClassDB::bind_method(D_METHOD("set_stretch", "enable"), &ViewportContainer::set_stretch);
	ClassDB::bind_method(D_METHOD("is_stretch_enabled"), &ViewportContainer::is_stretch_enabled);

	ClassDB::bind_method(D_METHOD("set_stretch_shrink", "amount"), &ViewportContainer::set_stretch_shrink);
	ClassDB::bind_method(D_METHOD("get_stretch_shrink"), &ViewportContainer::get_stretch_shrink);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "stretch"), "set_stretch", "is_stretch_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "stretch_shrink", PROPERTY_HINT_RANGE, "1,32,1,or_greater"), "set_stretch_shrink", "get_stretch_shrink");
}

ViewportContainer::ViewportContainer() {
	stretch = false;
	shrink = 1;
	set_process_input(true);
	set_process_unhandled_input(true);
}

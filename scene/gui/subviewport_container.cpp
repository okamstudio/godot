/**************************************************************************/
/*  subviewport_container.cpp                                             */
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

#include "subviewport_container.h"

#include "core/config/engine.h"
#include "scene/main/viewport.h"

Size2 SubViewportContainer::get_minimum_size() const {
	if (stretch) {
		return Size2();
	}
	Size2 ms;
	for (int i = 0; i < get_child_count(); i++) {
		SubViewport *c = Object::cast_to<SubViewport>(get_child(i));
		if (!c) {
			continue;
		}

		Size2 minsize = c->get_size();
		ms = ms.max(minsize);
	}

	return ms;
}

void SubViewportContainer::set_stretch(bool p_enable) {
	if (stretch == p_enable) {
		return;
	}

	stretch = p_enable;
	recalc_force_viewport_sizes();
	update_minimum_size();
	queue_sort();
	queue_redraw();
}

bool SubViewportContainer::is_stretch_enabled() const {
	return stretch;
}

void SubViewportContainer::set_stretch_shrink(int p_shrink) {
	ERR_FAIL_COND(p_shrink < 1);
	if (shrink == p_shrink) {
		return;
	}

	shrink = p_shrink;

	recalc_force_viewport_sizes();
	queue_redraw();
}

void SubViewportContainer::set_send_input_to_all_viewports(bool p_enable) {
	send_input_event_to_all_viewports = p_enable;
}

bool SubViewportContainer::is_send_input_to_all_viewports_enabled() {
	return send_input_event_to_all_viewports;
}

void SubViewportContainer::recalc_force_viewport_sizes() {
	if (!stretch) {
		return;
	}

	// If stretch is enabled, make sure that all child SubViwewports have the correct size.
	for (int i = 0; i < get_child_count(); i++) {
		SubViewport *c = Object::cast_to<SubViewport>(get_child(i));
		if (!c) {
			continue;
		}

		c->set_size_force(get_size() / shrink);
	}
}

int SubViewportContainer::get_stretch_shrink() const {
	return shrink;
}

Vector<int> SubViewportContainer::get_allowed_size_flags_horizontal() const {
	return Vector<int>();
}

Vector<int> SubViewportContainer::get_allowed_size_flags_vertical() const {
	return Vector<int>();
}

void SubViewportContainer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_RESIZED: {
			recalc_force_viewport_sizes();
		} break;

		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_VISIBILITY_CHANGED: {
			for (int i = 0; i < get_child_count(); i++) {
				SubViewport *c = Object::cast_to<SubViewport>(get_child(i));
				if (!c) {
					continue;
				}

				if (is_visible_in_tree()) {
					c->set_update_mode(SubViewport::UPDATE_ALWAYS);
				} else {
					c->set_update_mode(SubViewport::UPDATE_DISABLED);
				}
			}
		} break;

		case NOTIFICATION_DRAW: {
			for (int i = 0; i < get_child_count(); i++) {
				SubViewport *c = Object::cast_to<SubViewport>(get_child(i));
				if (!c) {
					continue;
				}

				if (stretch) {
					draw_texture_rect(c->get_texture(), Rect2(Vector2(), get_size()));
				} else {
					draw_texture_rect(c->get_texture(), Rect2(Vector2(), c->get_size()));
				}
			}
		} break;

		case NOTIFICATION_FOCUS_ENTER: {
			// If focused, send InputEvent to the SubViewport before the Gui-Input stage.
			set_process_input(true);
			set_process_unhandled_input(false);
		} break;

		case NOTIFICATION_FOCUS_EXIT: {
			// A different Control has focus and should receive Gui-Input before the InputEvent is sent to the SubViewport.
			set_process_input(false);
			set_process_unhandled_input(true);
		} break;
	}
}

void SubViewportContainer::_notify_viewports(int p_notification) {
	for (int i = 0; i < get_child_count(); i++) {
		SubViewport *c = Object::cast_to<SubViewport>(get_child(i));
		if (!c) {
			continue;
		}
		c->notification(p_notification);
	}
}

void SubViewportContainer::input(const Ref<InputEvent> &p_event) {
	_propagate_nonpositional_event(p_event);
}

void SubViewportContainer::unhandled_input(const Ref<InputEvent> &p_event) {
	_propagate_nonpositional_event(p_event);
}

void SubViewportContainer::_propagate_nonpositional_event(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	if (_is_propagated_in_gui_input(p_event)) {
		return;
	}

	bool send;
	if (GDVIRTUAL_CALL(_propagate_input_event, p_event, send)) {
		if (!send) {
			return;
		}
	}

	_send_event_to_viewports(p_event);
}

void SubViewportContainer::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	if (!_is_propagated_in_gui_input(p_event)) {
		return;
	}

	bool send;
	if (GDVIRTUAL_CALL(_propagate_input_event, p_event, send)) {
		if (!send) {
			return;
		}
	}

	if (stretch && shrink > 1) {
		Transform2D xform;
		xform.scale(Vector2(1, 1) / shrink);
		_send_event_to_viewports(p_event->xformed_by(xform));
	} else {
		_send_event_to_viewports(p_event);
	}
}

void SubViewportContainer::_send_event_to_viewports(const Ref<InputEvent> &p_event) {
	bool is_handled = false;
	// The input order is consistent with the display order, so that the topmost SubViewport receives the input events first.
	for (int i = get_child_count() - 1; i >= 0; i--) {
		SubViewport *c = Object::cast_to<SubViewport>(get_child(i));
		if (!c || c->is_input_disabled()) {
			continue;
		}

		c->push_input(p_event);
		if (c->is_input_handled()) {
			is_handled = true;
			if (!send_input_event_to_all_viewports) {
				break;
			}
		}
	}
	if (is_handled) {
		get_viewport()->set_input_as_handled();
	}
}

bool SubViewportContainer::_is_propagated_in_gui_input(const Ref<InputEvent> &p_event) {
	// Propagation of events with a position property happen in gui_input
	// Propagation of other events happen in input
	if (Object::cast_to<InputEventMouse>(*p_event) || Object::cast_to<InputEventScreenDrag>(*p_event) || Object::cast_to<InputEventScreenTouch>(*p_event) || Object::cast_to<InputEventGesture>(*p_event)) {
		return true;
	}
	return false;
}

void SubViewportContainer::set_mouse_target(bool p_enable) {
	mouse_target = p_enable;
}

bool SubViewportContainer::is_mouse_target_enabled() {
	return mouse_target;
}

void SubViewportContainer::add_child_notify(Node *p_child) {
	if (Object::cast_to<SubViewport>(p_child)) {
		queue_redraw();
	}
}

void SubViewportContainer::remove_child_notify(Node *p_child) {
	if (Object::cast_to<SubViewport>(p_child)) {
		queue_redraw();
	}
}

PackedStringArray SubViewportContainer::get_configuration_warnings() const {
	PackedStringArray warnings = Container::get_configuration_warnings();

	bool has_viewport = false;
	for (int i = 0; i < get_child_count(); i++) {
		if (Object::cast_to<SubViewport>(get_child(i))) {
			has_viewport = true;
			break;
		}
	}
	if (!has_viewport) {
		warnings.push_back(RTR("This node doesn't have a SubViewport as child, so it can't display its intended content.\nConsider adding a SubViewport as a child to provide something displayable."));
	}

	if (get_default_cursor_shape() != Control::CURSOR_ARROW) {
		warnings.push_back(RTR("The default mouse cursor shape of SubViewportContainer has no effect.\nConsider leaving it at its initial value `CURSOR_ARROW`."));
	}

	return warnings;
}

void SubViewportContainer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_stretch", "enable"), &SubViewportContainer::set_stretch);
	ClassDB::bind_method(D_METHOD("is_stretch_enabled"), &SubViewportContainer::is_stretch_enabled);

	ClassDB::bind_method(D_METHOD("set_stretch_shrink", "amount"), &SubViewportContainer::set_stretch_shrink);
	ClassDB::bind_method(D_METHOD("get_stretch_shrink"), &SubViewportContainer::get_stretch_shrink);

	ClassDB::bind_method(D_METHOD("set_mouse_target", "amount"), &SubViewportContainer::set_mouse_target);
	ClassDB::bind_method(D_METHOD("is_mouse_target_enabled"), &SubViewportContainer::is_mouse_target_enabled);

	ClassDB::bind_method(D_METHOD("set_send_input_to_all_viewports", "enable"), &SubViewportContainer::set_send_input_to_all_viewports);
	ClassDB::bind_method(D_METHOD("is_send_input_to_all_viewports_enabled"), &SubViewportContainer::is_send_input_to_all_viewports_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "stretch"), "set_stretch", "is_stretch_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "stretch_shrink", PROPERTY_HINT_RANGE, "1,32,1,or_greater"), "set_stretch_shrink", "get_stretch_shrink");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "mouse_target"), "set_mouse_target", "is_mouse_target_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "send_input_to_all_viewports"), "set_send_input_to_all_viewports", "is_send_input_to_all_viewports_enabled");

	GDVIRTUAL_BIND(_propagate_input_event, "event");
}

SubViewportContainer::SubViewportContainer() {
	set_process_unhandled_input(true);
	set_focus_mode(FOCUS_CLICK);
}

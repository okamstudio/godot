/**************************************************************************/
/*  visible_on_screen_notifier_2d.cpp                                     */
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

#include "visible_on_screen_notifier_2d.h"

#ifdef DEBUG_ENABLED
Dictionary VisibleOnScreenNotifier2D::_edit_get_state() const {
	Dictionary state = Node2D::_edit_get_state();
	state["rect"] = rect;
	return state;
}

void VisibleOnScreenNotifier2D::_edit_set_state(const Dictionary &p_state) {
	ERR_FAIL_COND(p_state.is_empty() || !p_state.has("rect"));
	set_rect(p_state["rect"]);
	Node2D::_edit_set_state(p_state);
}

void VisibleOnScreenNotifier2D::_edit_set_rect(const Rect2 &p_edit_rect) {
	set_rect(p_edit_rect);
}

Rect2 VisibleOnScreenNotifier2D::_edit_get_rect() const {
	return rect;
}

bool VisibleOnScreenNotifier2D::_edit_use_rect() const {
	return show_rect;
}
#endif // DEBUG_ENABLED

void VisibleOnScreenNotifier2D::_visibility_enter() {
	if (!is_inside_tree() || Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	on_screen = true;
	emit_signal(SceneStringName(screen_entered));
	_screen_enter();
}
void VisibleOnScreenNotifier2D::_visibility_exit() {
	if (!is_inside_tree() || Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	on_screen = false;
	emit_signal(SceneStringName(screen_exited));
	_screen_exit();
}

void VisibleOnScreenNotifier2D::set_rect(const Rect2 &p_rect) {
	rect = p_rect;
	if (is_inside_tree()) {
		RS::get_singleton()->canvas_item_set_visibility_notifier(get_canvas_item(), true, rect, callable_mp(this, &VisibleOnScreenNotifier2D::_visibility_enter), callable_mp(this, &VisibleOnScreenNotifier2D::_visibility_exit));
	}
	queue_redraw();
}

Rect2 VisibleOnScreenNotifier2D::get_rect() const {
	return rect;
}

void VisibleOnScreenNotifier2D::set_show_rect(bool p_show_rect) {
	if (show_rect == p_show_rect) {
		return;
	}
	show_rect = p_show_rect;
	queue_redraw();
}

bool VisibleOnScreenNotifier2D::is_showing_rect() const {
	return show_rect;
}

void VisibleOnScreenNotifier2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			on_screen = false;
			RS::get_singleton()->canvas_item_set_visibility_notifier(get_canvas_item(), true, rect, callable_mp(this, &VisibleOnScreenNotifier2D::_visibility_enter), callable_mp(this, &VisibleOnScreenNotifier2D::_visibility_exit));
		} break;

		case NOTIFICATION_DRAW: {
			if (show_rect && Engine::get_singleton()->is_editor_hint()) {
				draw_rect(rect, Color(1, 0.5, 1, 0.2));
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			on_screen = false;
			RS::get_singleton()->canvas_item_set_visibility_notifier(get_canvas_item(), false, Rect2(), Callable(), Callable());
		} break;
	}
}

bool VisibleOnScreenNotifier2D::is_on_screen() const {
	return on_screen;
}

void VisibleOnScreenNotifier2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_rect", "rect"), &VisibleOnScreenNotifier2D::set_rect);
	ClassDB::bind_method(D_METHOD("get_rect"), &VisibleOnScreenNotifier2D::get_rect);
	ClassDB::bind_method(D_METHOD("set_show_rect", "show_rect"), &VisibleOnScreenNotifier2D::set_show_rect);
	ClassDB::bind_method(D_METHOD("is_showing_rect"), &VisibleOnScreenNotifier2D::is_showing_rect);
	ClassDB::bind_method(D_METHOD("is_on_screen"), &VisibleOnScreenNotifier2D::is_on_screen);

	ADD_PROPERTY(PropertyInfo(Variant::RECT2, "rect", PROPERTY_HINT_NONE, "suffix:px"), "set_rect", "get_rect");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_rect"), "set_show_rect", "is_showing_rect");

	ADD_SIGNAL(MethodInfo("screen_entered"));
	ADD_SIGNAL(MethodInfo("screen_exited"));
}

VisibleOnScreenNotifier2D::VisibleOnScreenNotifier2D() {
	rect = Rect2(-10, -10, 20, 20);
	set_hide_clip_children(true);
}

//////////////////////////////////////

void VisibleOnScreenEnabler2D::_screen_enter() {
	_update_enable_mode(true);
}

void VisibleOnScreenEnabler2D::_screen_exit() {
	_update_enable_mode(false);
}

void VisibleOnScreenEnabler2D::set_enable_mode(EnableMode p_mode) {
	enable_mode = p_mode;
	if (is_inside_tree()) {
		_update_enable_mode(is_on_screen());
	}
}
VisibleOnScreenEnabler2D::EnableMode VisibleOnScreenEnabler2D::get_enable_mode() {
	return enable_mode;
}

void VisibleOnScreenEnabler2D::set_enable_node_path(NodePath p_path) {
	if (enable_node_path == p_path) {
		return;
	}
	enable_node_path = p_path;
	if (enable_node_path.is_empty()) {
		node_id = ObjectID();
		return;
	}
	if (is_inside_tree() && !Engine::get_singleton()->is_editor_hint()) {
		node_id = ObjectID();
		Node *node = get_node(enable_node_path);
		if (node) {
			node_id = node->get_instance_id();
			_update_enable_mode(is_on_screen());
		}
	}
}
NodePath VisibleOnScreenEnabler2D::get_enable_node_path() {
	return enable_node_path;
}

void VisibleOnScreenEnabler2D::_update_enable_mode(bool p_enable) {
	Node *node = static_cast<Node *>(ObjectDB::get_instance(node_id));
	if (node) {
		if (p_enable) {
			switch (enable_mode) {
				case ENABLE_MODE_INHERIT: {
					node->set_process_mode(PROCESS_MODE_INHERIT);
				} break;
				case ENABLE_MODE_ALWAYS: {
					node->set_process_mode(PROCESS_MODE_ALWAYS);
				} break;
				case ENABLE_MODE_WHEN_PAUSED: {
					node->set_process_mode(PROCESS_MODE_WHEN_PAUSED);
				} break;
			}
		} else {
			node->set_process_mode(PROCESS_MODE_DISABLED);
		}
	}
}
void VisibleOnScreenEnabler2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (Engine::get_singleton()->is_editor_hint()) {
				return;
			}
			node_id = ObjectID();
			if (enable_node_path.is_empty()) {
				return;
			}

			Node *node = get_node(enable_node_path);
			if (node) {
				node_id = node->get_instance_id();
				node->set_process_mode(PROCESS_MODE_DISABLED);
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			node_id = ObjectID();
		} break;
	}
}

void VisibleOnScreenEnabler2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enable_mode", "mode"), &VisibleOnScreenEnabler2D::set_enable_mode);
	ClassDB::bind_method(D_METHOD("get_enable_mode"), &VisibleOnScreenEnabler2D::get_enable_mode);

	ClassDB::bind_method(D_METHOD("set_enable_node_path", "path"), &VisibleOnScreenEnabler2D::set_enable_node_path);
	ClassDB::bind_method(D_METHOD("get_enable_node_path"), &VisibleOnScreenEnabler2D::get_enable_node_path);

	ADD_GROUP("Enabling", "enable_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "enable_mode", PROPERTY_HINT_ENUM, "Inherit,Always,When Paused"), "set_enable_mode", "get_enable_mode");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "enable_node_path"), "set_enable_node_path", "get_enable_node_path");

	BIND_ENUM_CONSTANT(ENABLE_MODE_INHERIT);
	BIND_ENUM_CONSTANT(ENABLE_MODE_ALWAYS);
	BIND_ENUM_CONSTANT(ENABLE_MODE_WHEN_PAUSED);
}

VisibleOnScreenEnabler2D::VisibleOnScreenEnabler2D() {
}

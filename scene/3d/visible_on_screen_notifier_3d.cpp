/**************************************************************************/
/*  visible_on_screen_notifier_3d.cpp                                     */
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

#include "visible_on_screen_notifier_3d.h"

void VisibleOnScreenNotifier3D::_visibility_enter() {
	if (!is_inside_tree() || Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	on_screen = true;
	emit_signal(SceneStringName(screen_entered));
	_screen_enter();
}
void VisibleOnScreenNotifier3D::_visibility_exit() {
	if (!is_inside_tree() || Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	on_screen = false;
	emit_signal(SceneStringName(screen_exited));
	_screen_exit();
}

void VisibleOnScreenNotifier3D::set_aabb(const AABB &p_aabb) {
	if (aabb == p_aabb) {
		return;
	}
	aabb = p_aabb;

	RS::get_singleton()->visibility_notifier_set_aabb(get_base(), aabb);

	update_gizmos();
}

AABB VisibleOnScreenNotifier3D::get_aabb() const {
	return aabb;
}

bool VisibleOnScreenNotifier3D::is_on_screen() const {
	return on_screen;
}

void VisibleOnScreenNotifier3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_EXIT_TREE: {
			on_screen = false;
		} break;
	}
}

void VisibleOnScreenNotifier3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_aabb", "rect"), &VisibleOnScreenNotifier3D::set_aabb);
	ClassDB::bind_method(D_METHOD("is_on_screen"), &VisibleOnScreenNotifier3D::is_on_screen);

	ADD_PROPERTY(PropertyInfo(Variant::AABB, "aabb", PROPERTY_HINT_NONE, "suffix:m"), "set_aabb", "get_aabb");

	ADD_SIGNAL(MethodInfo("screen_entered"));
	ADD_SIGNAL(MethodInfo("screen_exited"));
}

VisibleOnScreenNotifier3D::VisibleOnScreenNotifier3D() {
	RID notifier = RS::get_singleton()->visibility_notifier_create();
	RS::get_singleton()->visibility_notifier_set_aabb(notifier, aabb);
	RS::get_singleton()->visibility_notifier_set_callbacks(notifier, callable_mp(this, &VisibleOnScreenNotifier3D::_visibility_enter), callable_mp(this, &VisibleOnScreenNotifier3D::_visibility_exit));
	set_base(notifier);
}

VisibleOnScreenNotifier3D::~VisibleOnScreenNotifier3D() {
	RID base_old = get_base();
	set_base(RID());
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RS::get_singleton()->free(base_old);
}

//////////////////////////////////////

void VisibleOnScreenEnabler3D::_screen_enter() {
	_update_enable_mode(true);
}

void VisibleOnScreenEnabler3D::_screen_exit() {
	_update_enable_mode(false);
}

void VisibleOnScreenEnabler3D::set_enable_mode(EnableMode p_mode) {
	enable_mode = p_mode;
	if (is_inside_tree()) {
		_update_enable_mode(is_on_screen());
	}
}
VisibleOnScreenEnabler3D::EnableMode VisibleOnScreenEnabler3D::get_enable_mode() {
	return enable_mode;
}

void VisibleOnScreenEnabler3D::set_enable_node_path(NodePath p_path) {
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
NodePath VisibleOnScreenEnabler3D::get_enable_node_path() {
	return enable_node_path;
}

void VisibleOnScreenEnabler3D::_update_enable_mode(bool p_enable) {
	Node *node = node_id.get_object<Node>();
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
void VisibleOnScreenEnabler3D::_notification(int p_what) {
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

void VisibleOnScreenEnabler3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enable_mode", "mode"), &VisibleOnScreenEnabler3D::set_enable_mode);
	ClassDB::bind_method(D_METHOD("get_enable_mode"), &VisibleOnScreenEnabler3D::get_enable_mode);

	ClassDB::bind_method(D_METHOD("set_enable_node_path", "path"), &VisibleOnScreenEnabler3D::set_enable_node_path);
	ClassDB::bind_method(D_METHOD("get_enable_node_path"), &VisibleOnScreenEnabler3D::get_enable_node_path);

	ADD_GROUP("Enabling", "enable_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "enable_mode", PROPERTY_HINT_ENUM, "Inherit,Always,When Paused"), "set_enable_mode", "get_enable_mode");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "enable_node_path"), "set_enable_node_path", "get_enable_node_path");

	BIND_ENUM_CONSTANT(ENABLE_MODE_INHERIT);
	BIND_ENUM_CONSTANT(ENABLE_MODE_ALWAYS);
	BIND_ENUM_CONSTANT(ENABLE_MODE_WHEN_PAUSED);
}

VisibleOnScreenEnabler3D::VisibleOnScreenEnabler3D() {
}

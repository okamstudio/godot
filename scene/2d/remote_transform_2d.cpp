/**************************************************************************/
/*  remote_transform_2d.cpp                                               */
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

#include "remote_transform_2d.h"

void RemoteTransform2D::_update_cache() {
	cache = ObjectID();
	if (has_node(remote_node)) {
		Node *node = get_node(remote_node);
		if (!node || this == node || node->is_ancestor_of(this) || is_ancestor_of(node)) {
			return;
		}

		cache = node->get_instance_id();
	}
}

void RemoteTransform2D::_update_notify_settings() {
	set_notify_transform(use_parent_transform || use_global_coordinates);
	set_notify_local_transform(!(use_parent_transform || use_global_coordinates));
}

void RemoteTransform2D::_update_remote() {
	if (!is_inside_tree()) {
		return;
	}

	if (cache.is_null()) {
		return;
	}

	Node2D *n = Object::cast_to<Node2D>(ObjectDB::get_instance(cache));
	if (!n) {
		return;
	}

	if (!n->is_inside_tree()) {
		return;
	}

	if (!(update_remote_position || update_remote_rotation || update_remote_scale)) {
		return; // The transform data of the RemoteTransform2D is not used at all.
	}

	//todo make faster
	if (use_global_coordinates) {
		Transform2D our_trans;
		if (use_parent_transform) {
			Node2D *parent = Object::cast_to<Node2D>(get_parent());
			ERR_FAIL_NULL_MSG(parent, "RemoteTransform2D must have a valid Node2D parent if use_parent_transform is true");
			our_trans = parent->get_global_transform();
		} else {
			our_trans = get_global_transform();
		}

		if (update_remote_position && update_remote_rotation && update_remote_scale) {
			n->set_global_transform(our_trans);
			return;
		}

		Transform2D n_trans = n->get_global_transform();

		// There are more steps in the operation of set_rotation, so avoid calling it.
		Transform2D trans = update_remote_rotation ? our_trans : n_trans;

		if (update_remote_rotation ^ update_remote_position) {
			trans.set_origin(update_remote_position ? our_trans.get_origin() : n_trans.get_origin());
		}
		if (update_remote_rotation ^ update_remote_scale) {
			trans.set_scale(update_remote_scale ? our_trans.get_scale() : n_trans.get_scale());
		}

		n->set_global_transform(trans);
	} else {
		Transform2D our_trans;
		if (use_parent_transform) {
			Node2D *parent = Object::cast_to<Node2D>(get_parent());
			ERR_FAIL_NULL_MSG(parent, "RemoteTransform2D must have a valid Node2D parent if use_parent_transform is true");
			our_trans = parent->get_transform();
		} else {
			our_trans = get_transform();
		}

		if (update_remote_position && update_remote_rotation && update_remote_scale) {
			n->set_transform(our_trans);
			return;
		}

		Transform2D n_trans = n->get_transform();

		// There are more steps in the operation of set_rotation, so avoid calling it.
		Transform2D trans = update_remote_rotation ? our_trans : n_trans;

		if (update_remote_rotation ^ update_remote_position) {
			trans.set_origin(update_remote_position ? our_trans.get_origin() : n_trans.get_origin());
		}
		if (update_remote_rotation ^ update_remote_scale) {
			trans.set_scale(update_remote_scale ? our_trans.get_scale() : n_trans.get_scale());
		}

		n->set_transform(trans);
	}
}

void RemoteTransform2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_update_cache();
		} break;

		case NOTIFICATION_RESET_PHYSICS_INTERPOLATION: {
			if (cache.is_valid()) {
				_update_remote();
				Node2D *n = Object::cast_to<Node2D>(ObjectDB::get_instance(cache));
				if (n) {
					n->reset_physics_interpolation();
				}
			}
		} break;

		case NOTIFICATION_LOCAL_TRANSFORM_CHANGED:
		case NOTIFICATION_TRANSFORM_CHANGED: {
			if (!is_inside_tree()) {
				break;
			}

			if (cache.is_valid()) {
				_update_remote();
			}

			//HACK! See https://github.com/godotengine/godot/issues/67640#issuecomment-1284370297
			Point2 _ = get_global_position();
		} break;
	}
}

void RemoteTransform2D::set_remote_node(const NodePath &p_remote_node) {
	if (remote_node == p_remote_node) {
		return;
	}

	remote_node = p_remote_node;
	if (is_inside_tree()) {
		_update_cache();
		_update_remote();
	}

	update_configuration_warnings();
}

NodePath RemoteTransform2D::get_remote_node() const {
	return remote_node;
}

void RemoteTransform2D::set_use_global_coordinates(const bool p_enable) {
	if (use_global_coordinates == p_enable) {
		return;
	}

	use_global_coordinates = p_enable;
	_update_notify_settings();
	_update_remote();
}

bool RemoteTransform2D::get_use_global_coordinates() const {
	return use_global_coordinates;
}

void RemoteTransform2D::set_use_parent_transform(const bool p_enable) {
	if (use_parent_transform == p_enable) {
		return;
	}

	use_parent_transform = p_enable;
	_update_notify_settings();
	_update_remote();
}

bool RemoteTransform2D::get_use_parent_transform() const {
	return use_parent_transform;
}

void RemoteTransform2D::set_update_position(const bool p_update) {
	if (update_remote_position == p_update) {
		return;
	}
	update_remote_position = p_update;
	_update_remote();
}

bool RemoteTransform2D::get_update_position() const {
	return update_remote_position;
}

void RemoteTransform2D::set_update_rotation(const bool p_update) {
	if (update_remote_rotation == p_update) {
		return;
	}
	update_remote_rotation = p_update;
	_update_remote();
}

bool RemoteTransform2D::get_update_rotation() const {
	return update_remote_rotation;
}

void RemoteTransform2D::set_update_scale(const bool p_update) {
	if (update_remote_scale == p_update) {
		return;
	}
	update_remote_scale = p_update;
	_update_remote();
}

bool RemoteTransform2D::get_update_scale() const {
	return update_remote_scale;
}

void RemoteTransform2D::force_update_cache() {
	_update_cache();
}

PackedStringArray RemoteTransform2D::get_configuration_warnings() const {
	PackedStringArray warnings = Node2D::get_configuration_warnings();

	if (!has_node(remote_node) || !Object::cast_to<Node2D>(get_node(remote_node))) {
		warnings.push_back(RTR("Path property must point to a valid Node2D node to work."));
	}

	return warnings;
}

void RemoteTransform2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_remote_node", "path"), &RemoteTransform2D::set_remote_node);
	ClassDB::bind_method(D_METHOD("get_remote_node"), &RemoteTransform2D::get_remote_node);
	ClassDB::bind_method(D_METHOD("force_update_cache"), &RemoteTransform2D::force_update_cache);

	ClassDB::bind_method(D_METHOD("set_use_global_coordinates", "use_global_coordinates"), &RemoteTransform2D::set_use_global_coordinates);
	ClassDB::bind_method(D_METHOD("get_use_global_coordinates"), &RemoteTransform2D::get_use_global_coordinates);
	ClassDB::bind_method(D_METHOD("set_use_parent_transform", "use_parent_transform"), &RemoteTransform2D::set_use_parent_transform);
	ClassDB::bind_method(D_METHOD("get_use_parent_transform"), &RemoteTransform2D::get_use_parent_transform);

	ClassDB::bind_method(D_METHOD("set_update_position", "update_remote_position"), &RemoteTransform2D::set_update_position);
	ClassDB::bind_method(D_METHOD("get_update_position"), &RemoteTransform2D::get_update_position);
	ClassDB::bind_method(D_METHOD("set_update_rotation", "update_remote_rotation"), &RemoteTransform2D::set_update_rotation);
	ClassDB::bind_method(D_METHOD("get_update_rotation"), &RemoteTransform2D::get_update_rotation);
	ClassDB::bind_method(D_METHOD("set_update_scale", "update_remote_scale"), &RemoteTransform2D::set_update_scale);
	ClassDB::bind_method(D_METHOD("get_update_scale"), &RemoteTransform2D::get_update_scale);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "remote_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Node2D"), "set_remote_node", "get_remote_node");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_global_coordinates"), "set_use_global_coordinates", "get_use_global_coordinates");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_parent_transform"), "set_use_parent_transform", "get_use_parent_transform");

	ADD_GROUP("Update", "update_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "update_position"), "set_update_position", "get_update_position");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "update_rotation"), "set_update_rotation", "get_update_rotation");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "update_scale"), "set_update_scale", "get_update_scale");
}

RemoteTransform2D::RemoteTransform2D() {
	_update_notify_settings();
	set_hide_clip_children(true);
}

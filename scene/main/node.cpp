/**************************************************************************/
/*  node.cpp                                                              */
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

#include "node.h"

#include "core/core_string_names.h"
#include "core/io/resource_loader.h"
#include "core/message_queue.h"
#include "core/print_string.h"
#include "instance_placeholder.h"
#include "scene/animation/scene_tree_tween.h"
#include "scene/resources/packed_scene.h"
#include "scene/scene_string_names.h"
#include "viewport.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_settings.h"
#endif

VARIANT_ENUM_CAST(Node::PauseMode);
VARIANT_ENUM_CAST(Node::PhysicsInterpolationMode);

int Node::orphan_node_count = 0;

void Node::_notification(int p_notification) {
	switch (p_notification) {
		case NOTIFICATION_PROCESS: {
			if (get_script_instance()) {
				Variant time = get_process_delta_time();
				const Variant *ptr[1] = { &time };
				get_script_instance()->call_multilevel(SceneStringNames::get_singleton()->_process, ptr, 1);
			}
		} break;
		case NOTIFICATION_PHYSICS_PROCESS: {
			if (get_script_instance()) {
				Variant time = get_physics_process_delta_time();
				const Variant *ptr[1] = { &time };
				get_script_instance()->call_multilevel(SceneStringNames::get_singleton()->_physics_process, ptr, 1);
			}

		} break;
		case NOTIFICATION_ENTER_TREE: {
			ERR_FAIL_COND(!get_viewport());
			ERR_FAIL_COND(!get_tree());

			if (data.pause_mode == PAUSE_MODE_INHERIT) {
				if (data.parent) {
					data.pause_owner = data.parent->data.pause_owner;
				} else {
					data.pause_owner = nullptr;
				}
			} else {
				data.pause_owner = this;
			}

			if (data.physics_interpolation_mode == PHYSICS_INTERPOLATION_MODE_INHERIT) {
				bool interpolate = true; // Root node default is for interpolation to be on
				if (data.parent) {
					interpolate = data.parent->is_physics_interpolated();
				}
				_propagate_physics_interpolated(interpolate);
			}

			if (data.input) {
				add_to_group("_vp_input" + itos(get_viewport()->get_instance_id()));
			}
			if (data.unhandled_input) {
				add_to_group("_vp_unhandled_input" + itos(get_viewport()->get_instance_id()));
			}
			if (data.unhandled_key_input) {
				add_to_group("_vp_unhandled_key_input" + itos(get_viewport()->get_instance_id()));
			}

			get_tree()->node_count++;
			orphan_node_count--;

			// Allow physics interpolated nodes to automatically reset when added to the tree
			// (this is to save the user doing this manually each time).
			if (get_tree()->is_physics_interpolation_enabled()) {
				_set_physics_interpolation_reset_requested(true);
			}

		} break;
		case NOTIFICATION_EXIT_TREE: {
			ERR_FAIL_COND(!get_viewport());
			ERR_FAIL_COND(!get_tree());

			get_tree()->node_count--;
			orphan_node_count++;

			if (data.input) {
				remove_from_group("_vp_input" + itos(get_viewport()->get_instance_id()));
			}
			if (data.unhandled_input) {
				remove_from_group("_vp_unhandled_input" + itos(get_viewport()->get_instance_id()));
			}
			if (data.unhandled_key_input) {
				remove_from_group("_vp_unhandled_key_input" + itos(get_viewport()->get_instance_id()));
			}

			data.pause_owner = nullptr;
			if (data.path_cache) {
				memdelete(data.path_cache);
				data.path_cache = nullptr;
			}
		} break;
		case NOTIFICATION_PAUSED: {
			if (is_physics_interpolated_and_enabled() && is_inside_tree()) {
				reset_physics_interpolation();
			}
		} break;
		case NOTIFICATION_PATH_CHANGED: {
			if (data.path_cache) {
				memdelete(data.path_cache);
				data.path_cache = nullptr;
			}
		} break;
		case NOTIFICATION_READY: {
			if (get_script_instance()) {
				if (get_script_instance()->has_method(SceneStringNames::get_singleton()->_input)) {
					set_process_input(true);
				}

				if (get_script_instance()->has_method(SceneStringNames::get_singleton()->_unhandled_input)) {
					set_process_unhandled_input(true);
				}

				if (get_script_instance()->has_method(SceneStringNames::get_singleton()->_unhandled_key_input)) {
					set_process_unhandled_key_input(true);
				}

				if (get_script_instance()->has_method(SceneStringNames::get_singleton()->_process)) {
					set_process(true);
				}

				if (get_script_instance()->has_method(SceneStringNames::get_singleton()->_physics_process)) {
					set_physics_process(true);
				}

				get_script_instance()->call_multilevel_reversed(SceneStringNames::get_singleton()->_ready, nullptr, 0);
			}

		} break;
		case NOTIFICATION_POSTINITIALIZE: {
			data.in_constructor = false;
		} break;
		case NOTIFICATION_PREDELETE: {
			if (data.parent) {
				data.parent->remove_child(this);
			}

			// kill children as cleanly as possible
			while (data.children.size()) {
				Node *child = data.children[data.children.size() - 1]; //begin from the end because its faster and more consistent with creation
				memdelete(child);
			}
		} break;
	}
}

void Node::_propagate_ready() {
	data.ready_notified = true;
	data.blocked++;
	for (int i = 0; i < data.children.size(); i++) {
		data.children[i]->_propagate_ready();
	}
	data.blocked--;

	notification(NOTIFICATION_POST_ENTER_TREE);

	if (data.ready_first) {
		data.ready_first = false;
		notification(NOTIFICATION_READY);
		emit_signal(SceneStringNames::get_singleton()->ready);
	}
}

void Node::_propagate_physics_interpolated(bool p_interpolated) {
	switch (data.physics_interpolation_mode) {
		case PHYSICS_INTERPOLATION_MODE_INHERIT:
			// keep the parent p_interpolated
			break;
		case PHYSICS_INTERPOLATION_MODE_OFF: {
			p_interpolated = false;
		} break;
		case PHYSICS_INTERPOLATION_MODE_ON: {
			p_interpolated = true;
		} break;
	}

	// no change? no need to propagate further
	if (data.physics_interpolated == p_interpolated) {
		return;
	}

	data.physics_interpolated = p_interpolated;

	// allow a call to the VisualServer etc in derived classes
	_physics_interpolated_changed();

	data.blocked++;
	for (int i = 0; i < data.children.size(); i++) {
		data.children[i]->_propagate_physics_interpolated(p_interpolated);
	}
	data.blocked--;
}

void Node::_propagate_physics_interpolation_reset_requested(bool p_requested) {
	if (is_physics_interpolated()) {
		data.physics_interpolation_reset_requested = p_requested;
	}

	data.blocked++;
	for (int i = 0; i < data.children.size(); i++) {
		data.children[i]->_propagate_physics_interpolation_reset_requested(p_requested);
	}
	data.blocked--;
}

void Node::_propagate_enter_tree() {
	// this needs to happen to all children before any enter_tree

	if (data.parent) {
		data.tree = data.parent->data.tree;
		data.depth = data.parent->data.depth + 1;
	} else {
		data.depth = 1;
	}

	data.viewport = Object::cast_to<Viewport>(this);
	if (!data.viewport && data.parent) {
		data.viewport = data.parent->data.viewport;
	}

	data.inside_tree = true;

	for (Map<StringName, GroupData>::Element *E = data.grouped.front(); E; E = E->next()) {
		E->get().group = data.tree->add_to_group(E->key(), this);
	}

	notification(NOTIFICATION_ENTER_TREE);

	if (get_script_instance()) {
		get_script_instance()->call_multilevel_reversed(SceneStringNames::get_singleton()->_enter_tree, nullptr, 0);
	}

	emit_signal(SceneStringNames::get_singleton()->tree_entered);

	data.tree->node_added(this);

	if (data.parent) {
		Variant c = this;
		const Variant *cptr = &c;
		data.parent->emit_signal(SceneStringNames::get_singleton()->child_entered_tree, &cptr, 1);
	}

	data.blocked++;
	//block while adding children

	for (int i = 0; i < data.children.size(); i++) {
		if (!data.children[i]->is_inside_tree()) { // could have been added in enter_tree
			data.children[i]->_propagate_enter_tree();
		}
	}

	data.blocked--;

#ifdef DEBUG_ENABLED

	if (ScriptDebugger::get_singleton() && data.filename != String()) {
		//used for live edit
		data.tree->live_scene_edit_cache[data.filename].insert(this);
	}
#endif
	// enter groups
}

void Node::_propagate_after_exit_branch(bool p_exiting_tree) {
	// Clear owner if it was not part of the pruned branch
	if (data.owner) {
		bool found = false;
		Node *parent = data.parent;

		while (parent) {
			if (parent == data.owner) {
				found = true;
				break;
			}

			parent = parent->data.parent;
		}

		if (!found) {
			if (data.unique_name_in_owner) {
				_release_unique_name_in_owner();
			}
			data.owner->data.owned.erase(data.OW);
			data.owner = nullptr;
		}
	}

	data.blocked++;
	for (int i = 0; i < data.children.size(); i++) {
		data.children[i]->_propagate_after_exit_branch(p_exiting_tree);
	}
	data.blocked--;

	if (p_exiting_tree) {
		emit_signal(SceneStringNames::get_singleton()->tree_exited);
	}
}

void Node::_propagate_exit_tree() {
	//block while removing children

#ifdef DEBUG_ENABLED

	if (ScriptDebugger::get_singleton() && data.filename != String()) {
		//used for live edit
		Map<String, Set<Node *>>::Element *E = data.tree->live_scene_edit_cache.find(data.filename);
		if (E) {
			E->get().erase(this);
			if (E->get().size() == 0) {
				data.tree->live_scene_edit_cache.erase(E);
			}
		}

		Map<Node *, Map<ObjectID, Node *>>::Element *F = data.tree->live_edit_remove_list.find(this);
		if (F) {
			for (Map<ObjectID, Node *>::Element *G = F->get().front(); G; G = G->next()) {
				memdelete(G->get());
			}
			data.tree->live_edit_remove_list.erase(F);
		}
	}
#endif
	data.blocked++;

	for (int i = data.children.size() - 1; i >= 0; i--) {
		data.children[i]->_propagate_exit_tree();
	}

	data.blocked--;

	if (get_script_instance()) {
		get_script_instance()->call_multilevel(SceneStringNames::get_singleton()->_exit_tree, nullptr, 0);
	}
	emit_signal(SceneStringNames::get_singleton()->tree_exiting);

	notification(NOTIFICATION_EXIT_TREE, true);
	if (data.tree) {
		data.tree->node_removed(this);
	}

	if (data.parent) {
		Variant c = this;
		const Variant *cptr = &c;
		data.parent->emit_signal(SceneStringNames::get_singleton()->child_exiting_tree, &cptr, 1);
	}

	// exit groups

	for (Map<StringName, GroupData>::Element *E = data.grouped.front(); E; E = E->next()) {
		data.tree->remove_from_group(E->key(), this);
		E->get().group = nullptr;
	}

	data.viewport = nullptr;

	if (data.tree) {
		data.tree->tree_changed();
	}

	data.inside_tree = false;
	data.ready_notified = false;
	data.tree = nullptr;
	data.depth = -1;
}

void Node::move_child(Node *p_child, int p_pos) {
	ERR_FAIL_NULL(p_child);
	ERR_FAIL_INDEX_MSG(p_pos, data.children.size() + 1, vformat("Invalid new child position: %d.", p_pos));
	ERR_FAIL_COND_MSG(p_child->data.parent != this, "Child is not a child of this node.");
	ERR_FAIL_COND_MSG(data.blocked > 0, "Parent node is busy setting up children, move_child() failed. Consider using call_deferred(\"move_child\") instead (or \"popup\" if this is from a popup).");

	// Specifying one place beyond the end
	// means the same as moving to the last position
	if (p_pos == data.children.size()) {
		p_pos--;
	}

	if (p_child->data.pos == p_pos) {
		return; //do nothing
	}

	int motion_from = MIN(p_pos, p_child->data.pos);
	int motion_to = MAX(p_pos, p_child->data.pos);

	data.children.remove(p_child->data.pos);
	data.children.insert(p_pos, p_child);

	if (data.tree) {
		data.tree->tree_changed();
	}

	data.blocked++;
	//new pos first
	for (int i = motion_from; i <= motion_to; i++) {
		data.children[i]->data.pos = i;
	}
	// notification second
	move_child_notify(p_child);
	Viewport::notify_canvas_parent_children_moved(*this, motion_from, motion_to + 1);

	p_child->_propagate_groups_dirty();

	data.blocked--;
}

void Node::raise() {
	if (!data.parent) {
		return;
	}

	data.parent->move_child(this, data.parent->data.children.size() - 1);
}

void Node::_propagate_groups_dirty() {
	for (const Map<StringName, GroupData>::Element *E = data.grouped.front(); E; E = E->next()) {
		if (E->get().group) {
			E->get().group->changed = true;
		}
	}

	for (int i = 0; i < data.children.size(); i++) {
		data.children[i]->_propagate_groups_dirty();
	}
}

void Node::add_child_notify(Node *p_child) {
	// to be used when not wanted
}

void Node::remove_child_notify(Node *p_child) {
	// to be used when not wanted
}

void Node::move_child_notify(Node *p_child) {
	// to be used when not wanted
}

void Node::owner_changed_notify() {
}

void Node::_physics_interpolated_changed() {}

void Node::set_physics_process(bool p_process) {
	if (data.physics_process == p_process) {
		return;
	}

	data.physics_process = p_process;

	if (data.physics_process) {
		add_to_group("physics_process", false);
	} else {
		remove_from_group("physics_process");
	}

	_change_notify("physics_process");
}

bool Node::is_physics_processing() const {
	return data.physics_process;
}

void Node::set_physics_process_internal(bool p_process_internal) {
	if (data.physics_process_internal == p_process_internal) {
		return;
	}

	data.physics_process_internal = p_process_internal;

	if (data.physics_process_internal) {
		add_to_group("physics_process_internal", false);
	} else {
		remove_from_group("physics_process_internal");
	}

	_change_notify("physics_process_internal");
}

bool Node::is_physics_processing_internal() const {
	return data.physics_process_internal;
}

void Node::set_pause_mode(PauseMode p_mode) {
	if (data.pause_mode == p_mode) {
		return;
	}

	bool prev_inherits = data.pause_mode == PAUSE_MODE_INHERIT;
	bool prev_can_process = is_inside_tree() && can_process();
	data.pause_mode = p_mode;
	if (!is_inside_tree()) {
		return; //pointless
	}
	if ((data.pause_mode == PAUSE_MODE_INHERIT) != prev_inherits) {
		Node *owner = nullptr;

		if (data.pause_mode == PAUSE_MODE_INHERIT) {
			if (data.parent) {
				owner = data.parent->data.pause_owner;
			}
		} else {
			owner = this;
		}

		_propagate_pause_owner(owner);
	}
	if (prev_can_process != can_process()) {
		_propagate_pause_change_notification(can_process() ? NOTIFICATION_UNPAUSED : NOTIFICATION_PAUSED);
	}
}

Node::PauseMode Node::get_pause_mode() const {
	return data.pause_mode;
}

void Node::_propagate_pause_owner(Node *p_owner) {
	if (this != p_owner && data.pause_mode != PAUSE_MODE_INHERIT) {
		return;
	}
	data.pause_owner = p_owner;
	for (int i = 0; i < data.children.size(); i++) {
		data.children[i]->_propagate_pause_owner(p_owner);
	}
}

void Node::_propagate_pause_change_notification(int p_notification) {
	notification(p_notification);

	for (int i = 0; i < data.children.size(); i++) {
		if (data.children[i]->data.pause_mode == PAUSE_MODE_INHERIT) {
			data.children[i]->_propagate_pause_change_notification(p_notification);
		}
	}
}

void Node::set_network_master(int p_peer_id, bool p_recursive) {
	data.network_master = p_peer_id;

	if (p_recursive) {
		for (int i = 0; i < data.children.size(); i++) {
			data.children[i]->set_network_master(p_peer_id, true);
		}
	}
}

int Node::get_network_master() const {
	return data.network_master;
}

bool Node::is_network_master() const {
	ERR_FAIL_COND_V(!is_inside_tree(), false);

	return get_multiplayer()->get_network_unique_id() == data.network_master;
}

/***** RPC CONFIG ********/

void Node::rpc_config(const StringName &p_method, MultiplayerAPI::RPCMode p_mode) {
	if (p_mode == MultiplayerAPI::RPC_MODE_DISABLED) {
		data.rpc_methods.erase(p_method);
	} else {
		data.rpc_methods[p_method] = p_mode;
	};
}

void Node::rset_config(const StringName &p_property, MultiplayerAPI::RPCMode p_mode) {
	if (p_mode == MultiplayerAPI::RPC_MODE_DISABLED) {
		data.rpc_properties.erase(p_property);
	} else {
		data.rpc_properties[p_property] = p_mode;
	};
}

/***** RPC FUNCTIONS ********/

void Node::rpc(const StringName &p_method, VARIANT_ARG_DECLARE) {
	VARIANT_ARGPTRS;

	int argc = 0;
	for (int i = 0; i < VARIANT_ARG_MAX; i++) {
		if (argptr[i]->get_type() == Variant::NIL) {
			break;
		}
		argc++;
	}

	rpcp(0, false, p_method, argptr, argc);
}

void Node::rpc_id(int p_peer_id, const StringName &p_method, VARIANT_ARG_DECLARE) {
	VARIANT_ARGPTRS;

	int argc = 0;
	for (int i = 0; i < VARIANT_ARG_MAX; i++) {
		if (argptr[i]->get_type() == Variant::NIL) {
			break;
		}
		argc++;
	}

	rpcp(p_peer_id, false, p_method, argptr, argc);
}

void Node::rpc_unreliable(const StringName &p_method, VARIANT_ARG_DECLARE) {
	VARIANT_ARGPTRS;

	int argc = 0;
	for (int i = 0; i < VARIANT_ARG_MAX; i++) {
		if (argptr[i]->get_type() == Variant::NIL) {
			break;
		}
		argc++;
	}

	rpcp(0, true, p_method, argptr, argc);
}

void Node::rpc_unreliable_id(int p_peer_id, const StringName &p_method, VARIANT_ARG_DECLARE) {
	VARIANT_ARGPTRS;

	int argc = 0;
	for (int i = 0; i < VARIANT_ARG_MAX; i++) {
		if (argptr[i]->get_type() == Variant::NIL) {
			break;
		}
		argc++;
	}

	rpcp(p_peer_id, true, p_method, argptr, argc);
}

Variant Node::_rpc_bind(const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
	if (p_argcount < 1) {
		r_error.error = Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.argument = 1;
		return Variant();
	}

	if (p_args[0]->get_type() != Variant::STRING) {
		r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 0;
		r_error.expected = Variant::STRING;
		return Variant();
	}

	StringName method = *p_args[0];

	rpcp(0, false, method, &p_args[1], p_argcount - 1);

	r_error.error = Variant::CallError::CALL_OK;
	return Variant();
}

Variant Node::_rpc_id_bind(const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
	if (p_argcount < 2) {
		r_error.error = Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.argument = 2;
		return Variant();
	}

	if (p_args[0]->get_type() != Variant::INT) {
		r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 0;
		r_error.expected = Variant::INT;
		return Variant();
	}

	if (p_args[1]->get_type() != Variant::STRING) {
		r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 1;
		r_error.expected = Variant::STRING;
		return Variant();
	}

	int peer_id = *p_args[0];
	StringName method = *p_args[1];

	rpcp(peer_id, false, method, &p_args[2], p_argcount - 2);

	r_error.error = Variant::CallError::CALL_OK;
	return Variant();
}

Variant Node::_rpc_unreliable_bind(const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
	if (p_argcount < 1) {
		r_error.error = Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.argument = 1;
		return Variant();
	}

	if (p_args[0]->get_type() != Variant::STRING) {
		r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 0;
		r_error.expected = Variant::STRING;
		return Variant();
	}

	StringName method = *p_args[0];

	rpcp(0, true, method, &p_args[1], p_argcount - 1);

	r_error.error = Variant::CallError::CALL_OK;
	return Variant();
}

Variant Node::_rpc_unreliable_id_bind(const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
	if (p_argcount < 2) {
		r_error.error = Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.argument = 2;
		return Variant();
	}

	if (p_args[0]->get_type() != Variant::INT) {
		r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 0;
		r_error.expected = Variant::INT;
		return Variant();
	}

	if (p_args[1]->get_type() != Variant::STRING) {
		r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 1;
		r_error.expected = Variant::STRING;
		return Variant();
	}

	int peer_id = *p_args[0];
	StringName method = *p_args[1];

	rpcp(peer_id, true, method, &p_args[2], p_argcount - 2);

	r_error.error = Variant::CallError::CALL_OK;
	return Variant();
}

void Node::rpcp(int p_peer_id, bool p_unreliable, const StringName &p_method, const Variant **p_arg, int p_argcount) {
	ERR_FAIL_COND(!is_inside_tree());
	get_multiplayer()->rpcp(this, p_peer_id, p_unreliable, p_method, p_arg, p_argcount);
}

void Node::rsetp(int p_peer_id, bool p_unreliable, const StringName &p_property, const Variant &p_value) {
	ERR_FAIL_COND(!is_inside_tree());
	get_multiplayer()->rsetp(this, p_peer_id, p_unreliable, p_property, p_value);
}

/******** RSET *********/
void Node::rset(const StringName &p_property, const Variant &p_value) {
	rsetp(0, false, p_property, p_value);
}

void Node::rset_id(int p_peer_id, const StringName &p_property, const Variant &p_value) {
	rsetp(p_peer_id, false, p_property, p_value);
}

void Node::rset_unreliable(const StringName &p_property, const Variant &p_value) {
	rsetp(0, true, p_property, p_value);
}

void Node::rset_unreliable_id(int p_peer_id, const StringName &p_property, const Variant &p_value) {
	rsetp(p_peer_id, true, p_property, p_value);
}

//////////// end of rpc
Ref<MultiplayerAPI> Node::get_multiplayer() const {
	if (multiplayer.is_valid()) {
		return multiplayer;
	}
	if (!is_inside_tree()) {
		return Ref<MultiplayerAPI>();
	}
	return get_tree()->get_multiplayer();
}

Ref<MultiplayerAPI> Node::get_custom_multiplayer() const {
	return multiplayer;
}

void Node::set_custom_multiplayer(Ref<MultiplayerAPI> p_multiplayer) {
	multiplayer = p_multiplayer;
}

const Map<StringName, MultiplayerAPI::RPCMode>::Element *Node::get_node_rpc_mode(const StringName &p_method) {
	return data.rpc_methods.find(p_method);
}

const Map<StringName, MultiplayerAPI::RPCMode>::Element *Node::get_node_rset_mode(const StringName &p_property) {
	return data.rpc_properties.find(p_property);
}

bool Node::can_process_notification(int p_what) const {
	switch (p_what) {
		case NOTIFICATION_PHYSICS_PROCESS:
			return data.physics_process;
		case NOTIFICATION_PROCESS:
			return data.idle_process;
		case NOTIFICATION_INTERNAL_PROCESS:
			return data.idle_process_internal;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS:
			return data.physics_process_internal;
	}

	return true;
}

bool Node::can_process() const {
	ERR_FAIL_COND_V(!is_inside_tree(), false);

	if (get_tree()->is_paused()) {
		if (data.pause_mode == PAUSE_MODE_STOP) {
			return false;
		}
		if (data.pause_mode == PAUSE_MODE_PROCESS) {
			return true;
		}
		if (data.pause_mode == PAUSE_MODE_INHERIT) {
			if (!data.pause_owner) {
				return false; //clearly no pause owner by default
			}

			if (data.pause_owner->data.pause_mode == PAUSE_MODE_PROCESS) {
				return true;
			}

			if (data.pause_owner->data.pause_mode == PAUSE_MODE_STOP) {
				return false;
			}
		}
	}

	return true;
}

void Node::set_physics_interpolation_mode(PhysicsInterpolationMode p_mode) {
	if (data.physics_interpolation_mode == p_mode) {
		return;
	}

	data.physics_interpolation_mode = p_mode;

	bool interpolate = true; // default for root node

	switch (p_mode) {
		case PHYSICS_INTERPOLATION_MODE_INHERIT: {
			if (is_inside_tree() && data.parent) {
				interpolate = data.parent->is_physics_interpolated();
			}
		} break;
		case PHYSICS_INTERPOLATION_MODE_OFF: {
			interpolate = false;
		} break;
		case PHYSICS_INTERPOLATION_MODE_ON: {
			interpolate = true;
		} break;
	}

	// if swapping from interpolated to non-interpolated, use this as
	// an extra means to cause a reset
	if (is_physics_interpolated() && !interpolate && is_inside_tree()) {
		propagate_notification(NOTIFICATION_RESET_PHYSICS_INTERPOLATION);
	}

	_propagate_physics_interpolated(interpolate);
}

void Node::reset_physics_interpolation() {
	if (is_inside_tree()) {
		propagate_notification(NOTIFICATION_RESET_PHYSICS_INTERPOLATION);

		// If `reset_physics_interpolation()` is called explicitly by the user
		// (e.g. from scripts) then we prevent deferred auto-resets taking place.
		// The user is trusted to call reset in the right order, and auto-reset
		// will interfere with their control of prev / curr, so should be turned off.
		_propagate_physics_interpolation_reset_requested(false);
	}
}

float Node::get_physics_process_delta_time() const {
	if (data.tree) {
		return data.tree->get_physics_process_time();
	} else {
		return 0;
	}
}

float Node::get_process_delta_time() const {
	if (data.tree) {
		return data.tree->get_idle_process_time();
	} else {
		return 0;
	}
}

void Node::set_process(bool p_idle_process) {
	if (data.idle_process == p_idle_process) {
		return;
	}

	data.idle_process = p_idle_process;

	if (data.idle_process) {
		add_to_group("idle_process", false);
	} else {
		remove_from_group("idle_process");
	}

	_change_notify("idle_process");
}

bool Node::is_processing() const {
	return data.idle_process;
}

void Node::set_process_internal(bool p_idle_process_internal) {
	if (data.idle_process_internal == p_idle_process_internal) {
		return;
	}

	data.idle_process_internal = p_idle_process_internal;

	if (data.idle_process_internal) {
		add_to_group("idle_process_internal", false);
	} else {
		remove_from_group("idle_process_internal");
	}

	_change_notify("idle_process_internal");
}

bool Node::is_processing_internal() const {
	return data.idle_process_internal;
}

void Node::set_process_priority(int p_priority) {
	data.process_priority = p_priority;

	// Make sure we are in SceneTree.
	if (data.tree == nullptr) {
		return;
	}

	if (is_processing()) {
		data.tree->make_group_changed("idle_process");
	}

	if (is_processing_internal()) {
		data.tree->make_group_changed("idle_process_internal");
	}

	if (is_physics_processing()) {
		data.tree->make_group_changed("physics_process");
	}

	if (is_physics_processing_internal()) {
		data.tree->make_group_changed("physics_process_internal");
	}
}

int Node::get_process_priority() const {
	return data.process_priority;
}

void Node::set_process_input(bool p_enable) {
	if (p_enable == data.input) {
		return;
	}

	data.input = p_enable;
	if (!is_inside_tree()) {
		return;
	}

	if (p_enable) {
		add_to_group("_vp_input" + itos(get_viewport()->get_instance_id()));
	} else {
		remove_from_group("_vp_input" + itos(get_viewport()->get_instance_id()));
	}
}

bool Node::is_processing_input() const {
	return data.input;
}

void Node::set_process_unhandled_input(bool p_enable) {
	if (p_enable == data.unhandled_input) {
		return;
	}
	data.unhandled_input = p_enable;
	if (!is_inside_tree()) {
		return;
	}

	if (p_enable) {
		add_to_group("_vp_unhandled_input" + itos(get_viewport()->get_instance_id()));
	} else {
		remove_from_group("_vp_unhandled_input" + itos(get_viewport()->get_instance_id()));
	}
}

bool Node::is_processing_unhandled_input() const {
	return data.unhandled_input;
}

void Node::set_process_unhandled_key_input(bool p_enable) {
	if (p_enable == data.unhandled_key_input) {
		return;
	}
	data.unhandled_key_input = p_enable;
	if (!is_inside_tree()) {
		return;
	}

	if (p_enable) {
		add_to_group("_vp_unhandled_key_input" + itos(get_viewport()->get_instance_id()));
	} else {
		remove_from_group("_vp_unhandled_key_input" + itos(get_viewport()->get_instance_id()));
	}
}

bool Node::is_processing_unhandled_key_input() const {
	return data.unhandled_key_input;
}

void Node::_set_physics_interpolated_client_side(bool p_enable) {
	data.physics_interpolated_client_side = p_enable;
}

void Node::_set_physics_interpolation_reset_requested(bool p_enable) {
	data.physics_interpolation_reset_requested = p_enable;
}

void Node::_set_use_identity_transform(bool p_enable) {
	data.use_identity_transform = p_enable;
}

StringName Node::get_name() const {
	return data.name;
}

void Node::_set_name_nocheck(const StringName &p_name) {
	data.name = p_name;
#ifdef DEV_ENABLED
	_name_changed_notify();
#endif
}

#ifdef DEV_ENABLED
void Node::_name_changed_notify() {
}
#endif

void Node::set_name(const String &p_name) {
	String name = p_name.validate_node_name();

	ERR_FAIL_COND(name == "");

	if (data.unique_name_in_owner && data.owner) {
		_release_unique_name_in_owner();
	}
	data.name = name;

	if (data.parent) {
		data.parent->_validate_child_name(this);
	}

	if (data.unique_name_in_owner && data.owner) {
		_acquire_unique_name_in_owner();
	}

	propagate_notification(NOTIFICATION_PATH_CHANGED);

	if (is_inside_tree()) {
		emit_signal("renamed");
		get_tree()->node_renamed(this);
		get_tree()->tree_changed();
	}

#ifdef DEV_ENABLED
	_name_changed_notify();
#endif
}

static bool node_hrcr = false;
static SafeRefCount node_hrcr_count;

void Node::init_node_hrcr() {
	node_hrcr_count.init(1);
}

void Node::set_human_readable_collision_renaming(bool p_enabled) {
	node_hrcr = p_enabled;
}

#ifdef TOOLS_ENABLED
String Node::validate_child_name(Node *p_child) {
	StringName name = p_child->data.name;
	_generate_serial_child_name(p_child, name);
	return name;
}
#endif

void Node::_validate_child_name(Node *p_child, bool p_force_human_readable) {
	/* Make sure the name is unique */

	if (node_hrcr || p_force_human_readable) {
		//this approach to autoset node names is human readable but very slow
		//it's turned on while running in the editor

		StringName name = p_child->data.name;
		_generate_serial_child_name(p_child, name);
		p_child->data.name = name;

	} else {
		//this approach to autoset node names is fast but not as readable
		//it's the default and reserves the '@' character for unique names.

		bool unique = true;

		if (p_child->data.name == StringName()) {
			//new unique name must be assigned
			unique = false;
		} else {
			//check if exists
			Node **children = data.children.ptrw();
			int cc = data.children.size();

			for (int i = 0; i < cc; i++) {
				if (children[i] == p_child) {
					continue;
				}
				if (children[i]->data.name == p_child->data.name) {
					unique = false;
					break;
				}
			}
		}

		if (!unique) {
			ERR_FAIL_COND(!node_hrcr_count.ref());
			String name = "@" + String(p_child->get_name()) + "@" + itos(node_hrcr_count.get());
			p_child->data.name = name;
		}
	}
}

// Return s + 1 as if it were an integer
String increase_numeric_string(const String &s) {
	String res = s;
	bool carry = res.length() > 0;

	for (int i = res.length() - 1; i >= 0; i--) {
		if (!carry) {
			break;
		}
		CharType n = s[i];
		if (n == '9') { // keep carry as true: 9 + 1
			res[i] = '0';
		} else {
			res[i] = s[i] + 1;
			carry = false;
		}
	}

	if (carry) {
		res = "1" + res;
	}

	return res;
}

void Node::_generate_serial_child_name(const Node *p_child, StringName &name) const {
	if (name == StringName()) {
		//no name and a new nade is needed, create one.

		name = p_child->get_class();
		// Adjust casing according to project setting. The current type name is expected to be in PascalCase.
		switch (ProjectSettings::get_singleton()->get("node/name_casing").operator int()) {
			case NAME_CASING_PASCAL_CASE:
				break;
			case NAME_CASING_CAMEL_CASE: {
				String n = name;
				n[0] = n.to_lower()[0];
				name = n;
			} break;
			case NAME_CASING_SNAKE_CASE:
				name = String(name).camelcase_to_underscore(true);
				break;
		}
	}

	//quickly test if proposed name exists
	int cc = data.children.size(); //children count
	const Node *const *children_ptr = data.children.ptr();

	{
		bool exists = false;

		for (int i = 0; i < cc; i++) {
			if (children_ptr[i] == p_child) { //exclude self in renaming if its already a child
				continue;
			}
			if (children_ptr[i]->data.name == name) {
				exists = true;
			}
		}

		if (!exists) {
			return; //if it does not exist, it does not need validation
		}
	}

	// Extract trailing number
	String name_string = name;
	String nums;
	for (int i = name_string.length() - 1; i >= 0; i--) {
		CharType n = name_string[i];
		if (n >= '0' && n <= '9') {
			nums = String::chr(name_string[i]) + nums;
		} else {
			break;
		}
	}

	String nnsep = _get_name_num_separator();
	int name_last_index = name_string.length() - nnsep.length() - nums.length();

	// Assign the base name + separator to name if we have numbers preceded by a separator
	if (nums.length() > 0 && name_string.substr(name_last_index, nnsep.length()) == nnsep) {
		name_string = name_string.substr(0, name_last_index + nnsep.length());
	} else {
		nums = "";
	}

	for (;;) {
		StringName attempt = name_string + nums;
		bool exists = false;

		for (int i = 0; i < cc; i++) {
			if (children_ptr[i] == p_child) {
				continue;
			}
			if (children_ptr[i]->data.name == attempt) {
				exists = true;
			}
		}

		if (!exists) {
			name = attempt;
			return;
		} else {
			if (nums.length() == 0) {
				// Name was undecorated so skip to 2 for a more natural result
				nums = "2";
				name_string += nnsep; // Add separator because nums.length() > 0 was false
			} else {
				nums = increase_numeric_string(nums);
			}
		}
	}
}

void Node::_add_child_nocheck(Node *p_child, const StringName &p_name) {
	//add a child node quickly, without name validation

	p_child->_set_name_nocheck(p_name);
	p_child->data.pos = data.children.size();
	data.children.push_back(p_child);
	p_child->data.parent = this;
	p_child->notification(NOTIFICATION_PARENTED);

	if (data.tree) {
		p_child->_set_tree(data.tree);
	}

	/* Notify */
	//recognize children created in this node constructor
	p_child->data.parent_owned = data.in_constructor;
	add_child_notify(p_child);
}

void Node::add_child(Node *p_child, bool p_force_readable_name) {
	ERR_FAIL_NULL(p_child);
	ERR_FAIL_COND_MSG(p_child == this, vformat("Can't add child '%s' to itself.", p_child->get_name())); // adding to itself!
	ERR_FAIL_COND_MSG(p_child->data.parent, vformat("Can't add child '%s' to '%s', already has a parent '%s'.", p_child->get_name(), get_name(), p_child->data.parent->get_name())); //Fail if node has a parent
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND_MSG(p_child->is_a_parent_of(this), vformat("Can't add child '%s' to '%s' as it would result in a cyclic dependency since '%s' is already a parent of '%s'.", p_child->get_name(), get_name(), p_child->get_name(), get_name()));
#endif
	ERR_FAIL_COND_MSG(data.blocked > 0, "Parent node is busy setting up children, add_node() failed. Consider using call_deferred(\"add_child\", child) instead.");

	/* Validate name */
	_validate_child_name(p_child, p_force_readable_name);

	_add_child_nocheck(p_child, p_child->data.name);
}

void Node::add_child_below_node(Node *p_node, Node *p_child, bool p_force_readable_name) {
	ERR_FAIL_NULL(p_node);
	ERR_FAIL_NULL(p_child);

	add_child(p_child, p_force_readable_name);

	if (p_node->data.parent == this) {
		move_child(p_child, p_node->get_position_in_parent() + 1);
	} else {
		WARN_PRINT("Cannot move under node " + p_node->get_name() + " as " + p_child->get_name() + " does not share a parent.");
	}
}

void Node::remove_child(Node *p_child) {
	ERR_FAIL_NULL(p_child);
	ERR_FAIL_COND_MSG(data.blocked > 0, "Parent node is busy setting up children, remove_node() failed. Consider using call_deferred(\"remove_child\", child) instead.");

	int child_count = data.children.size();
	Node **children = data.children.ptrw();
	int idx = -1;

	if (p_child->data.pos >= 0 && p_child->data.pos < child_count) {
		if (children[p_child->data.pos] == p_child) {
			idx = p_child->data.pos;
		}
	}

	if (idx == -1) { //maybe removed while unparenting or something and index was not updated, so just in case the above fails, try this.
		for (int i = 0; i < child_count; i++) {
			if (children[i] == p_child) {
				idx = i;
				break;
			}
		}
	}

	ERR_FAIL_COND_MSG(idx == -1, vformat("Cannot remove child node '%s' as it is not a child of this node.", p_child->get_name()));
	//ERR_FAIL_COND( p_child->data.blocked > 0 );

	//if (data.scene) { does not matter

	p_child->_set_tree(nullptr);
	//}

	remove_child_notify(p_child);
	p_child->notification(NOTIFICATION_UNPARENTED);

	// Note that calling _set_tree() could have caused
	// another node to be deleted from the same parent,
	// the childlist could be different, and thus
	// idx by this point is not guaranteed to be correct.
	// We therefore recalculate idx here, for safety, if it has changed.

	// Child count could have changed.
	child_count = data.children.size();

	if ((p_child->data.pos != idx) || (idx >= child_count) || (data.children[idx] != p_child)) {
		idx = -1;

		// Reget the childen, the list may have changed...
		children = data.children.ptrw();

		// If the new p_child->data.pos is correct...
		if (p_child->data.pos >= 0 && p_child->data.pos < child_count) {
			if (children[p_child->data.pos] == p_child) {
				idx = p_child->data.pos;
			}
		}

		// If data.pos is incorrect, it still may be one of the children...
		if (idx == -1) {
			for (int i = 0; i < child_count; i++) {
				if (children[i] == p_child) {
					idx = i;
					break;
				}
			}
		}

		// This should not happen unless something has gone horribly wrong,
		// but just in case prevent a crash.
		// By this point, the child will have been partially removed,
		// so normal behaviour cannot be guaranteed
		// if this ERR FAIL occurs.
		ERR_FAIL_COND_MSG(idx == -1, vformat("Cannot remove child node '%s' as it is no longer a child of this node.", p_child->get_name()));
	}

	data.children.remove(idx);

	//update pointer and size
	child_count = data.children.size();
	children = data.children.ptrw();

	for (int i = idx; i < child_count; i++) {
		children[i]->data.pos = i;
	}

	Viewport::notify_canvas_parent_children_moved(*this, idx, child_count);
	Viewport::notify_canvas_parent_child_count_reduced(*this);

	p_child->data.parent = nullptr;
	p_child->data.pos = -1;

	p_child->_propagate_after_exit_branch(data.inside_tree);
}

int Node::get_child_count() const {
	return data.children.size();
}
Node *Node::get_child(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, data.children.size(), nullptr);

	return data.children[p_index];
}

Node *Node::_get_child_by_name(const StringName &p_name) const {
	int cc = data.children.size();
	Node *const *cd = data.children.ptr();

	for (int i = 0; i < cc; i++) {
		if (cd[i]->data.name == p_name) {
			return cd[i];
		}
	}

	return nullptr;
}

Node *Node::get_node_or_null(const NodePath &p_path) const {
	if (p_path.is_empty()) {
		return nullptr;
	}

	ERR_FAIL_COND_V_MSG(!data.inside_tree && p_path.is_absolute(), nullptr, "Can't use get_node() with absolute paths from outside the active scene tree.");

	Node *current = nullptr;
	Node *root = nullptr;

	if (!p_path.is_absolute()) {
		current = const_cast<Node *>(this); //start from this
	} else {
		root = const_cast<Node *>(this);
		while (root->data.parent) {
			root = root->data.parent; //start from root
		}
	}

	for (int i = 0; i < p_path.get_name_count(); i++) {
		StringName name = p_path.get_name(i);
		Node *next = nullptr;

		if (name == SceneStringNames::get_singleton()->dot) { // .

			next = current;

		} else if (name == SceneStringNames::get_singleton()->doubledot) { // ..

			if (current == nullptr || !current->data.parent) {
				return nullptr;
			}

			next = current->data.parent;
		} else if (current == nullptr) {
			if (name == root->get_name()) {
				next = root;
			}

		} else if (name.is_node_unique_name()) {
			Node **unique = current->data.owned_unique_nodes.getptr(name);
			if (!unique && current->data.owner) {
				unique = current->data.owner->data.owned_unique_nodes.getptr(name);
			}
			if (!unique) {
				return nullptr;
			}
			next = *unique;
		} else {
			next = nullptr;

			for (int j = 0; j < current->data.children.size(); j++) {
				Node *child = current->data.children[j];

				if (child->data.name == name) {
					next = child;
					break;
				}
			}
			if (next == nullptr) {
				return nullptr;
			};
		}
		current = next;
	}

	return current;
}

Node *Node::get_node(const NodePath &p_path) const {
	Node *node = get_node_or_null(p_path);
	if (unlikely(!node)) {
		// Try to get a clear description of this node in the error message.
		String desc;
		if (is_inside_tree()) {
			desc = get_path();
		} else {
			desc = get_name();
			if (desc.empty()) {
				desc = get_class();
			}
		}

		if (p_path.is_absolute()) {
			ERR_FAIL_V_MSG(nullptr,
					vformat("(Node not found: \"%s\" (absolute path attempted from \"%s\").)", p_path, desc));
		} else {
			ERR_FAIL_V_MSG(nullptr,
					vformat("(Node not found: \"%s\" (relative to \"%s\").)", p_path, desc));
		}
	}

	return node;
}

bool Node::has_node(const NodePath &p_path) const {
	return get_node_or_null(p_path) != nullptr;
}

Node *Node::find_node(const String &p_mask, bool p_recursive, bool p_owned) const {
	Node *const *cptr = data.children.ptr();
	int ccount = data.children.size();
	for (int i = 0; i < ccount; i++) {
		if (p_owned && !cptr[i]->data.owner) {
			continue;
		}
		if (cptr[i]->data.name.operator String().match(p_mask)) {
			return cptr[i];
		}

		if (!p_recursive) {
			continue;
		}

		Node *ret = cptr[i]->find_node(p_mask, true, p_owned);
		if (ret) {
			return ret;
		}
	}
	return nullptr;
}

Node *Node::get_parent() const {
	return data.parent;
}

Node *Node::find_parent(const String &p_mask) const {
	Node *p = data.parent;
	while (p) {
		if (p->data.name.operator String().match(p_mask)) {
			return p;
		}
		p = p->data.parent;
	}

	return nullptr;
}

bool Node::is_a_parent_of(const Node *p_node) const {
	ERR_FAIL_NULL_V(p_node, false);
	Node *p = p_node->data.parent;
	while (p) {
		if (p == this) {
			return true;
		}
		p = p->data.parent;
	}

	return false;
}

bool Node::is_greater_than(const Node *p_node) const {
	ERR_FAIL_NULL_V(p_node, false);
	ERR_FAIL_COND_V(!data.inside_tree, false);
	ERR_FAIL_COND_V(!p_node->data.inside_tree, false);

	ERR_FAIL_COND_V(data.depth < 0, false);
	ERR_FAIL_COND_V(p_node->data.depth < 0, false);
#ifdef NO_ALLOCA

	Vector<int> this_stack;
	Vector<int> that_stack;
	this_stack.resize(data.depth);
	that_stack.resize(p_node->data.depth);

#else

	int *this_stack = (int *)alloca(sizeof(int) * data.depth);
	int *that_stack = (int *)alloca(sizeof(int) * p_node->data.depth);

#endif

	const Node *n = this;

	int idx = data.depth - 1;
	while (n) {
		ERR_FAIL_INDEX_V(idx, data.depth, false);
		this_stack[idx--] = n->data.pos;
		n = n->data.parent;
	}
	ERR_FAIL_COND_V(idx != -1, false);
	n = p_node;
	idx = p_node->data.depth - 1;
	while (n) {
		ERR_FAIL_INDEX_V(idx, p_node->data.depth, false);
		that_stack[idx--] = n->data.pos;

		n = n->data.parent;
	}
	ERR_FAIL_COND_V(idx != -1, false);
	idx = 0;

	bool res;
	while (true) {
		// using -2 since out-of-tree or nonroot nodes have -1
		int this_idx = (idx >= data.depth) ? -2 : this_stack[idx];
		int that_idx = (idx >= p_node->data.depth) ? -2 : that_stack[idx];

		if (this_idx > that_idx) {
			res = true;
			break;
		} else if (this_idx < that_idx) {
			res = false;
			break;
		} else if (this_idx == -2) {
			res = false; // equal
			break;
		}
		idx++;
	}

	return res;
}

void Node::get_owned_by(Node *p_by, List<Node *> *p_owned) {
	if (data.owner == p_by) {
		p_owned->push_back(this);
	}

	for (int i = 0; i < get_child_count(); i++) {
		get_child(i)->get_owned_by(p_by, p_owned);
	}
}

void Node::_set_owner_nocheck(Node *p_owner) {
	if (data.owner == p_owner) {
		return;
	}

	ERR_FAIL_COND(data.owner);
	data.owner = p_owner;
	data.owner->data.owned.push_back(this);
	data.OW = data.owner->data.owned.back();

	owner_changed_notify();
}

void Node::_release_unique_name_in_owner() {
	ERR_FAIL_NULL(data.owner); // Sanity check.
	StringName key = StringName(UNIQUE_NODE_PREFIX + data.name.operator String());
	Node **which = data.owner->data.owned_unique_nodes.getptr(key);
	if (which == nullptr || *which != this) {
		return; // Ignore.
	}
	data.owner->data.owned_unique_nodes.erase(key);
}

void Node::_acquire_unique_name_in_owner() {
	ERR_FAIL_NULL(data.owner); // Sanity check.
	StringName key = StringName(UNIQUE_NODE_PREFIX + data.name.operator String());
	Node **which = data.owner->data.owned_unique_nodes.getptr(key);
	if (which != nullptr && *which != this) {
		WARN_PRINT(vformat(RTR("Setting node name '%s' to be unique within scene for '%s', but it's already claimed by '%s'. This node is no longer set unique."), get_name(), is_inside_tree() ? get_path() : data.owner->get_path_to(this), is_inside_tree() ? (*which)->get_path() : data.owner->get_path_to(*which)));
		data.unique_name_in_owner = false;
		return;
	}
	data.owner->data.owned_unique_nodes[key] = this;
}

void Node::set_unique_name_in_owner(bool p_enabled) {
	if (data.unique_name_in_owner == p_enabled) {
		return;
	}

	if (data.unique_name_in_owner && data.owner != nullptr) {
		_release_unique_name_in_owner();
	}
	data.unique_name_in_owner = p_enabled;

	if (data.unique_name_in_owner && data.owner != nullptr) {
		_acquire_unique_name_in_owner();
	}

	update_configuration_warning();
}

bool Node::is_unique_name_in_owner() const {
	return data.unique_name_in_owner;
}

void Node::set_owner(Node *p_owner) {
	if (data.owner) {
		if (data.unique_name_in_owner) {
			_release_unique_name_in_owner();
		}
		data.owner->data.owned.erase(data.OW);
		data.OW = nullptr;
		data.owner = nullptr;
	}

	ERR_FAIL_COND(p_owner == this);

	if (!p_owner) {
		return;
	}

	Node *check = this->get_parent();
	bool owner_valid = false;

	while (check) {
		if (check == p_owner) {
			owner_valid = true;
			break;
		}

		check = check->data.parent;
	}

	ERR_FAIL_COND(!owner_valid);

	_set_owner_nocheck(p_owner);

	if (data.unique_name_in_owner) {
		_acquire_unique_name_in_owner();
	}
}
Node *Node::get_owner() const {
	return data.owner;
}

Node *Node::find_common_parent_with(const Node *p_node) const {
	if (this == p_node) {
		return const_cast<Node *>(p_node);
	}

	Set<const Node *> visited;

	const Node *n = this;

	while (n) {
		visited.insert(n);
		n = n->data.parent;
	}

	const Node *common_parent = p_node;

	while (common_parent) {
		if (visited.has(common_parent)) {
			break;
		}
		common_parent = common_parent->data.parent;
	}

	if (!common_parent) {
		return nullptr;
	}

	return const_cast<Node *>(common_parent);
}

NodePath Node::get_path_to(const Node *p_node) const {
	ERR_FAIL_NULL_V(p_node, NodePath());

	if (this == p_node) {
		return NodePath(".");
	}

	Set<const Node *> visited;

	const Node *n = this;

	while (n) {
		visited.insert(n);
		n = n->data.parent;
	}

	const Node *common_parent = p_node;

	while (common_parent) {
		if (visited.has(common_parent)) {
			break;
		}
		common_parent = common_parent->data.parent;
	}

	ERR_FAIL_COND_V(!common_parent, NodePath()); //nodes not in the same tree

	visited.clear();

	Vector<StringName> path;

	n = p_node;

	while (n != common_parent) {
		path.push_back(n->get_name());
		n = n->data.parent;
	}

	n = this;
	StringName up = String("..");

	while (n != common_parent) {
		path.push_back(up);
		n = n->data.parent;
	}

	path.invert();

	return NodePath(path, false);
}

NodePath Node::get_path() const {
	ERR_FAIL_COND_V_MSG(!is_inside_tree(), NodePath(), "Cannot get path of node as it is not in a scene tree.");

	if (data.path_cache) {
		return *data.path_cache;
	}

	const Node *n = this;

	Vector<StringName> path;

	while (n) {
		path.push_back(n->get_name());
		n = n->data.parent;
	}

	path.invert();

	data.path_cache = memnew(NodePath(path, true));

	return *data.path_cache;
}

bool Node::is_in_group(const StringName &p_identifier) const {
	return data.grouped.has(p_identifier);
}

void Node::add_to_group(const StringName &p_identifier, bool p_persistent) {
	ERR_FAIL_COND(!p_identifier.operator String().length());

	if (data.grouped.has(p_identifier)) {
		return;
	}

	GroupData gd;

	if (data.tree) {
		gd.group = data.tree->add_to_group(p_identifier, this);
	} else {
		gd.group = nullptr;
	}

	gd.persistent = p_persistent;

	data.grouped[p_identifier] = gd;
}

void Node::remove_from_group(const StringName &p_identifier) {
	ERR_FAIL_COND(!data.grouped.has(p_identifier));

	Map<StringName, GroupData>::Element *E = data.grouped.find(p_identifier);

	ERR_FAIL_COND(!E);

	if (data.tree) {
		data.tree->remove_from_group(E->key(), this);
	}

	data.grouped.erase(E);
}

Array Node::_get_groups() const {
	Array groups;
	List<GroupInfo> gi;
	get_groups(&gi);
	for (List<GroupInfo>::Element *E = gi.front(); E; E = E->next()) {
		groups.push_back(E->get().name);
	}

	return groups;
}

void Node::get_groups(List<GroupInfo> *p_groups) const {
	for (const Map<StringName, GroupData>::Element *E = data.grouped.front(); E; E = E->next()) {
		GroupInfo gi;
		gi.name = E->key();
		gi.persistent = E->get().persistent;
		p_groups->push_back(gi);
	}
}

int Node::get_persistent_group_count() const {
	int count = 0;

	for (const Map<StringName, GroupData>::Element *E = data.grouped.front(); E; E = E->next()) {
		if (E->get().persistent) {
			count += 1;
		}
	}

	return count;
}
void Node::_print_tree_pretty(const String &prefix, const bool last) {
	String new_prefix = last ? String::utf8(" ┖╴") : String::utf8(" ┠╴");
	print_line(prefix + new_prefix + String(get_name()));
	for (int i = 0; i < data.children.size(); i++) {
		new_prefix = last ? String::utf8("   ") : String::utf8(" ┃ ");
		data.children[i]->_print_tree_pretty(prefix + new_prefix, i == data.children.size() - 1);
	}
}

void Node::print_tree_pretty() {
	_print_tree_pretty("", true);
}

void Node::print_tree() {
	_print_tree(this);
}

void Node::_print_tree(const Node *p_node) {
	print_line(String(p_node->get_path_to(this)));
	for (int i = 0; i < data.children.size(); i++) {
		data.children[i]->_print_tree(p_node);
	}
}

void Node::_propagate_reverse_notification(int p_notification) {
	data.blocked++;
	for (int i = data.children.size() - 1; i >= 0; i--) {
		data.children[i]->_propagate_reverse_notification(p_notification);
	}

	notification(p_notification, true);
	data.blocked--;
}

void Node::_propagate_deferred_notification(int p_notification, bool p_reverse) {
	ERR_FAIL_COND(!is_inside_tree());

	data.blocked++;

	if (!p_reverse) {
		MessageQueue::get_singleton()->push_notification(this, p_notification);
	}

	for (int i = 0; i < data.children.size(); i++) {
		data.children[i]->_propagate_deferred_notification(p_notification, p_reverse);
	}

	if (p_reverse) {
		MessageQueue::get_singleton()->push_notification(this, p_notification);
	}

	data.blocked--;
}

void Node::propagate_notification(int p_notification) {
	data.blocked++;
	notification(p_notification);

	for (int i = 0; i < data.children.size(); i++) {
		data.children[i]->propagate_notification(p_notification);
	}
	data.blocked--;
}

void Node::propagate_call(const StringName &p_method, const Array &p_args, const bool p_parent_first) {
	data.blocked++;

	if (p_parent_first && has_method(p_method)) {
		callv(p_method, p_args);
	}

	for (int i = 0; i < data.children.size(); i++) {
		data.children[i]->propagate_call(p_method, p_args, p_parent_first);
	}

	if (!p_parent_first && has_method(p_method)) {
		callv(p_method, p_args);
	}

	data.blocked--;
}

void Node::_propagate_replace_owner(Node *p_owner, Node *p_by_owner) {
	if (get_owner() == p_owner) {
		set_owner(p_by_owner);
	}

	data.blocked++;
	for (int i = 0; i < data.children.size(); i++) {
		data.children[i]->_propagate_replace_owner(p_owner, p_by_owner);
	}
	data.blocked--;
}

int Node::get_index() const {
	return data.pos;
}

Ref<SceneTreeTween> Node::create_tween() {
	ERR_FAIL_COND_V_MSG(!data.tree, nullptr, "Can't create SceneTreeTween when not inside scene tree.");
	Ref<SceneTreeTween> tween = get_tree()->create_tween();
	tween->bind_node(this);
	return tween;
}

void Node::remove_and_skip() {
	ERR_FAIL_COND(!data.parent);

	Node *new_owner = get_owner();

	List<Node *> children;

	while (true) {
		bool clear = true;
		for (int i = 0; i < data.children.size(); i++) {
			Node *c_node = data.children[i];
			if (!c_node->get_owner()) {
				continue;
			}

			remove_child(c_node);
			c_node->_propagate_replace_owner(this, nullptr);
			children.push_back(c_node);
			clear = false;
			break;
		}

		if (clear) {
			break;
		}
	}

	while (!children.empty()) {
		Node *c_node = children.front()->get();
		data.parent->add_child(c_node);
		c_node->_propagate_replace_owner(nullptr, new_owner);
		children.pop_front();
	}

	data.parent->remove_child(this);
}

void Node::set_filename(const String &p_filename) {
	data.filename = p_filename;
}
String Node::get_filename() const {
	return data.filename;
}

void Node::set_editor_description(const String &p_editor_description) {
	set_meta("_editor_description_", p_editor_description);
}
String Node::get_editor_description() const {
	if (has_meta("_editor_description_")) {
		return get_meta("_editor_description_");
	} else {
		return "";
	}
}

void Node::set_editable_instance(Node *p_node, bool p_editable) {
	ERR_FAIL_NULL(p_node);
	ERR_FAIL_COND(!is_a_parent_of(p_node));
	if (!p_editable) {
		p_node->data.editable_instance = false;
		// Avoid this flag being needlessly saved;
		// also give more visual feedback if editable children is re-enabled
		set_display_folded(false);
	} else {
		p_node->data.editable_instance = true;
	}
}

bool Node::is_editable_instance(const Node *p_node) const {
	if (!p_node) {
		return false; //easier, null is never editable :)
	}
	ERR_FAIL_COND_V(!is_a_parent_of(p_node), false);
	return p_node->data.editable_instance;
}

Node *Node::get_deepest_editable_node(Node *p_start_node) const {
	ERR_FAIL_NULL_V(p_start_node, nullptr);
	ERR_FAIL_COND_V(!is_a_parent_of(p_start_node), p_start_node);

	Node const *iterated_item = p_start_node;
	Node *node = p_start_node;

	while (iterated_item->get_owner() && iterated_item->get_owner() != this) {
		if (!is_editable_instance(iterated_item->get_owner())) {
			node = iterated_item->get_owner();
		}

		iterated_item = iterated_item->get_owner();
	}

	return node;
}

#ifdef TOOLS_ENABLED
void Node::set_property_pinned(const StringName &p_property, bool p_pinned) {
	bool current_pinned = false;
	bool has_pinned = has_meta("_edit_pinned_properties_");
	Array pinned;
	String psa = get_property_store_alias(p_property);
	if (has_pinned) {
		pinned = get_meta("_edit_pinned_properties_");
		current_pinned = pinned.has(psa);
	}

	if (current_pinned != p_pinned) {
		if (p_pinned) {
			pinned.append(psa);
			if (!has_pinned) {
				set_meta("_edit_pinned_properties_", pinned);
			}
		} else {
			pinned.erase(psa);
			if (pinned.empty()) {
				remove_meta("_edit_pinned_properties_");
			}
		}
	}
}

bool Node::is_property_pinned(const StringName &p_property) const {
	if (!has_meta("_edit_pinned_properties_")) {
		return false;
	}
	Array pinned = get_meta("_edit_pinned_properties_");
	String psa = get_property_store_alias(p_property);
	return pinned.has(psa);
}

StringName Node::get_property_store_alias(const StringName &p_property) const {
	return p_property;
}
#endif

void Node::get_storable_properties(Set<StringName> &r_storable_properties) const {
	List<PropertyInfo> pi;
	get_property_list(&pi);
	for (List<PropertyInfo>::Element *E = pi.front(); E; E = E->next()) {
		if ((E->get().usage & PROPERTY_USAGE_STORAGE)) {
			r_storable_properties.insert(E->get().name);
		}
	}
}

String Node::to_string() {
	// This code doesn't print the script's name, it calls to_string() if you override it in a Node's script,
	// which you only do if you specifically want to customize how the node should be represented by print().
	if (get_script_instance()) {
		bool valid;
		String ret = get_script_instance()->to_string(&valid);
		if (valid) {
			return ret;
		}
	}

	return (get_name() ? String(get_name()) + ":" : "") + Object::to_string();
}

void Node::set_scene_instance_state(const Ref<SceneState> &p_state) {
	data.instance_state = p_state;
}

Ref<SceneState> Node::get_scene_instance_state() const {
	return data.instance_state;
}

void Node::set_scene_inherited_state(const Ref<SceneState> &p_state) {
	data.inherited_state = p_state;
}

Ref<SceneState> Node::get_scene_inherited_state() const {
	return data.inherited_state;
}

void Node::set_scene_instance_load_placeholder(bool p_enable) {
	data.use_placeholder = p_enable;
}

bool Node::get_scene_instance_load_placeholder() const {
	return data.use_placeholder;
}

int Node::get_position_in_parent() const {
	return data.pos;
}

Node *Node::_duplicate(int p_flags, Map<const Node *, Node *> *r_duplimap) const {
	Node *node = nullptr;

	bool instanced = false;

	// No need to load a packed scene more than once if features
	// several times in the branch being duplicated.
	struct LoadedPackedScene {
		Ref<PackedScene> scene;
		String filename;
	};
	LocalVector<LoadedPackedScene> loaded_scenes;

	if (Object::cast_to<InstancePlaceholder>(this)) {
		const InstancePlaceholder *ip = Object::cast_to<const InstancePlaceholder>(this);
		InstancePlaceholder *nip = memnew(InstancePlaceholder);
		nip->set_instance_path(ip->get_instance_path());
		node = nip;

	} else if ((p_flags & DUPLICATE_USE_INSTANCING) && get_filename() != String()) {
		// already loaded?
		int found = -1;
		for (unsigned int n = 0; n < loaded_scenes.size(); n++) {
			if (loaded_scenes[n].filename == get_filename()) {
				found = n;
				break;
			}
		}
		Ref<PackedScene> res = Ref<PackedScene>();
		if (found != -1) {
			res = loaded_scenes[found].scene;
		} else {
			LoadedPackedScene ps;
			ps.filename = get_filename();
			ps.scene = ResourceLoader::load(get_filename());
			res = ps.scene;
			loaded_scenes.push_back(ps);
		}

		ERR_FAIL_COND_V(res.is_null(), nullptr);
		PackedScene::GenEditState ges = PackedScene::GEN_EDIT_STATE_DISABLED;
#ifdef TOOLS_ENABLED
		if (p_flags & DUPLICATE_FROM_EDITOR) {
			ges = PackedScene::GEN_EDIT_STATE_INSTANCE;
		}
#endif
		node = res->instance(ges);
		ERR_FAIL_COND_V(!node, nullptr);
		node->set_scene_instance_load_placeholder(get_scene_instance_load_placeholder());

		instanced = true;

	} else {
		Object *obj = ClassDB::instance(get_class());
		ERR_FAIL_COND_V(!obj, nullptr);
		node = Object::cast_to<Node>(obj);
		if (!node) {
			memdelete(obj);
		}
		ERR_FAIL_COND_V(!node, nullptr);
	}

	if (get_filename() != "") { //an instance
		node->set_filename(get_filename());
		node->data.editable_instance = data.editable_instance;
	}

	StringName script_property_name = CoreStringNames::get_singleton()->_script;

	List<const Node *> hidden_roots;
	List<const Node *> node_tree;
	node_tree.push_front(this);

	if (instanced) {
		// Since nodes in the instanced hierarchy won't be duplicated explicitly, we need to make an inventory
		// of all the nodes in the tree of the instanced scene in order to transfer the values of the properties

		Vector<const Node *> instance_roots;
		instance_roots.push_back(this);

		for (List<const Node *>::Element *N = node_tree.front(); N; N = N->next()) {
			for (int i = 0; i < N->get()->get_child_count(); ++i) {
				Node *descendant = N->get()->get_child(i);
				// Skip nodes not really belonging to the instanced hierarchy; they'll be processed normally later
				// but remember non-instanced nodes that are hidden below instanced ones
				if (instance_roots.find(descendant->get_owner()) == -1) {
					if (descendant->get_parent() && descendant->get_parent() != this && descendant->data.owner != descendant->get_parent()) {
						hidden_roots.push_back(descendant);
					}
					continue;
				}

				node_tree.push_back(descendant);

				if (descendant->get_filename() != "" && instance_roots.find(descendant->get_owner()) != -1) {
					instance_roots.push_back(descendant);
				}
			}
		}
	}

	for (List<const Node *>::Element *N = node_tree.front(); N; N = N->next()) {
		Node *current_node = node->get_node(get_path_to(N->get()));
		ERR_CONTINUE(!current_node);

		if (p_flags & DUPLICATE_SCRIPTS) {
			bool is_valid = false;
			Variant script = N->get()->get(script_property_name, &is_valid);
			if (is_valid) {
				current_node->set(script_property_name, script);
			}
		}

		List<PropertyInfo> plist;
		N->get()->get_property_list(&plist);

		for (List<PropertyInfo>::Element *E = plist.front(); E; E = E->next()) {
			if (!(E->get().usage & PROPERTY_USAGE_STORAGE)) {
				continue;
			}
			String name = E->get().name;
			if (name == script_property_name) {
				continue;
			}

			Variant value = N->get()->get(name).duplicate(true);

			if (E->get().usage & PROPERTY_USAGE_DO_NOT_SHARE_ON_DUPLICATE) {
				Resource *res = Object::cast_to<Resource>(value);
				if (res) { // Duplicate only if it's a resource
					current_node->set(name, res->duplicate());
				}

			} else {
				current_node->set(name, value);
			}
		}
	}

	if (get_name() != String()) {
		node->set_name(get_name());
	}

#ifdef TOOLS_ENABLED
	if ((p_flags & DUPLICATE_FROM_EDITOR) && r_duplimap) {
		r_duplimap->insert(this, node);
	}
#endif

	if (p_flags & DUPLICATE_GROUPS) {
		List<GroupInfo> gi;
		get_groups(&gi);
		for (List<GroupInfo>::Element *E = gi.front(); E; E = E->next()) {
#ifdef TOOLS_ENABLED
			if ((p_flags & DUPLICATE_FROM_EDITOR) && !E->get().persistent) {
				continue;
			}
#endif

			node->add_to_group(E->get().name, E->get().persistent);
		}
	}

	for (int i = 0; i < get_child_count(); i++) {
		if (get_child(i)->data.parent_owned) {
			continue;
		}
		if (instanced && get_child(i)->data.owner == this) {
			continue; //part of instance
		}

		Node *dup = get_child(i)->_duplicate(p_flags, r_duplimap);
		if (!dup) {
			memdelete(node);
			return nullptr;
		}

		node->add_child(dup);
		if (i < node->get_child_count() - 1) {
			node->move_child(dup, i);
		}
	}

	for (List<const Node *>::Element *E = hidden_roots.front(); E; E = E->next()) {
		Node *parent = node->get_node(get_path_to(E->get()->data.parent));
		if (!parent) {
			memdelete(node);
			return nullptr;
		}

		Node *dup = E->get()->_duplicate(p_flags, r_duplimap);
		if (!dup) {
			memdelete(node);
			return nullptr;
		}

		parent->add_child(dup);
		int pos = E->get()->get_position_in_parent();

		if (pos < parent->get_child_count() - 1) {
			parent->move_child(dup, pos);
		}
	}

	return node;
}

Node *Node::duplicate(int p_flags) const {
	Node *dupe = _duplicate(p_flags);

	if (dupe && (p_flags & DUPLICATE_SIGNALS)) {
		_duplicate_signals(this, dupe);
	}

	return dupe;
}

#ifdef TOOLS_ENABLED
Node *Node::duplicate_from_editor(Map<const Node *, Node *> &r_duplimap) const {
	return duplicate_from_editor(r_duplimap, Map<RES, RES>());
}

Node *Node::duplicate_from_editor(Map<const Node *, Node *> &r_duplimap, const Map<RES, RES> &p_resource_remap) const {
	Node *dupe = _duplicate(DUPLICATE_SIGNALS | DUPLICATE_GROUPS | DUPLICATE_SCRIPTS | DUPLICATE_USE_INSTANCING | DUPLICATE_FROM_EDITOR, &r_duplimap);

	// This is used by SceneTreeDock's paste functionality. When pasting to foreign scene, resources are duplicated.
	if (!p_resource_remap.empty()) {
		remap_node_resources(dupe, p_resource_remap);
	}

	// Duplication of signals must happen after all the node descendants have been copied,
	// because re-targeting of connections from some descendant to another is not possible
	// if the emitter node comes later in tree order than the receiver
	_duplicate_signals(this, dupe);

	return dupe;
}

void Node::remap_node_resources(Node *p_node, const Map<RES, RES> &p_resource_remap) const {
	List<PropertyInfo> props;
	p_node->get_property_list(&props);

	for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
		if (!(E->get().usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}

		Variant v = p_node->get(E->get().name);
		if (v.is_ref()) {
			RES res = v;
			if (res.is_valid()) {
				if (p_resource_remap.has(res)) {
					p_node->set(E->get().name, p_resource_remap[res]);
					remap_nested_resources(res, p_resource_remap);
				}
			}
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		remap_node_resources(p_node->get_child(i), p_resource_remap);
	}
}

void Node::remap_nested_resources(RES p_resource, const Map<RES, RES> &p_resource_remap) const {
	List<PropertyInfo> props;
	p_resource->get_property_list(&props);

	for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
		if (!(E->get().usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}

		Variant v = p_resource->get(E->get().name);
		if (v.is_ref()) {
			RES res = v;
			if (res.is_valid()) {
				if (p_resource_remap.has(res)) {
					p_resource->set(E->get().name, p_resource_remap[res]);
					remap_nested_resources(res, p_resource_remap);
				}
			}
		}
	}
}
#endif

void Node::_duplicate_and_reown(Node *p_new_parent, const Map<Node *, Node *> &p_reown_map) const {
	if (get_owner() != get_parent()->get_owner()) {
		return;
	}

	Node *node = nullptr;

	if (get_filename() != "") {
		Ref<PackedScene> res = ResourceLoader::load(get_filename());
		ERR_FAIL_COND_MSG(res.is_null(), "Cannot load scene: " + get_filename());
		node = res->instance();
		ERR_FAIL_COND(!node);
	} else {
		Object *obj = ClassDB::instance(get_class());
		ERR_FAIL_COND_MSG(!obj, "Node: Could not duplicate: " + String(get_class()) + ".");
		node = Object::cast_to<Node>(obj);
		if (!node) {
			memdelete(obj);
			ERR_FAIL_MSG("Node: Could not duplicate: " + String(get_class()) + ".");
		}
	}

	List<PropertyInfo> plist;

	get_property_list(&plist);

	for (List<PropertyInfo>::Element *E = plist.front(); E; E = E->next()) {
		if (!(E->get().usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}
		String name = E->get().name;

		Variant value = get(name).duplicate(true);

		node->set(name, value);
	}

	List<GroupInfo> groups;
	get_groups(&groups);

	for (List<GroupInfo>::Element *E = groups.front(); E; E = E->next()) {
		node->add_to_group(E->get().name, E->get().persistent);
	}

	node->set_name(get_name());
	p_new_parent->add_child(node);

	Node *owner = get_owner();

	if (p_reown_map.has(owner)) {
		owner = p_reown_map[owner];
	}

	if (owner) {
		NodePath p = get_path_to(owner);
		if (owner != this) {
			Node *new_owner = node->get_node(p);
			if (new_owner) {
				node->set_owner(new_owner);
			}
		}
	}

	for (int i = 0; i < get_child_count(); i++) {
		get_child(i)->_duplicate_and_reown(node, p_reown_map);
	}
}

// Duplication of signals must happen after all the node descendants have been copied,
// because re-targeting of connections from some descendant to another is not possible
// if the emitter node comes later in tree order than the receiver
void Node::_duplicate_signals(const Node *p_original, Node *p_copy) const {
	if ((this != p_original) && !(p_original->is_a_parent_of(this))) {
		return;
	}

	List<const Node *> process_list;
	process_list.push_back(this);
	while (!process_list.empty()) {
		const Node *n = process_list.front()->get();
		process_list.pop_front();

		List<Connection> conns;
		n->get_all_signal_connections(&conns);

		for (List<Connection>::Element *E = conns.front(); E; E = E->next()) {
			if (E->get().flags & CONNECT_PERSIST) {
				//user connected
				NodePath p = p_original->get_path_to(n);
				Node *copy = p_copy->get_node(p);

				Node *target = Object::cast_to<Node>(E->get().target);
				if (!target) {
					continue;
				}
				NodePath ptarget = p_original->get_path_to(target);

				Node *copytarget = target;

				// Attempt to find a path to the duplicate target, if it seems it's not part
				// of the duplicated and not yet parented hierarchy then at least try to connect
				// to the same target as the original

				if (p_copy->has_node(ptarget)) {
					copytarget = p_copy->get_node(ptarget);
				}

				if (copy && copytarget && !copy->is_connected(E->get().signal, copytarget, E->get().method)) {
					copy->connect(E->get().signal, copytarget, E->get().method, E->get().binds, E->get().flags);
				}
			}
		}

		for (int i = 0; i < n->get_child_count(); i++) {
			process_list.push_back(n->get_child(i));
		}
	}
}

Node *Node::duplicate_and_reown(const Map<Node *, Node *> &p_reown_map) const {
	ERR_FAIL_COND_V(get_filename() != "", nullptr);

	Object *obj = ClassDB::instance(get_class());
	ERR_FAIL_COND_V_MSG(!obj, nullptr, "Node: Could not duplicate: " + String(get_class()) + ".");

	Node *node = Object::cast_to<Node>(obj);
	if (!node) {
		memdelete(obj);
		ERR_FAIL_V_MSG(nullptr, "Node: Could not duplicate: " + String(get_class()) + ".");
	}
	node->set_name(get_name());

	List<PropertyInfo> plist;

	get_property_list(&plist);

	for (List<PropertyInfo>::Element *E = plist.front(); E; E = E->next()) {
		if (!(E->get().usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}
		String name = E->get().name;
		node->set(name, get(name));
	}

	List<GroupInfo> groups;
	get_groups(&groups);

	for (List<GroupInfo>::Element *E = groups.front(); E; E = E->next()) {
		node->add_to_group(E->get().name, E->get().persistent);
	}

	for (int i = 0; i < get_child_count(); i++) {
		get_child(i)->_duplicate_and_reown(node, p_reown_map);
	}

	// Duplication of signals must happen after all the node descendants have been copied,
	// because re-targeting of connections from some descendant to another is not possible
	// if the emitter node comes later in tree order than the receiver
	_duplicate_signals(this, node);
	return node;
}

static void find_owned_by(Node *p_by, Node *p_node, List<Node *> *p_owned) {
	if (p_node->get_owner() == p_by) {
		p_owned->push_back(p_node);
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		find_owned_by(p_by, p_node->get_child(i), p_owned);
	}
}

struct _NodeReplaceByPair {
	String name;
	Variant value;
};

void Node::replace_by(Node *p_node, bool p_keep_data) {
	ERR_FAIL_NULL(p_node);
	ERR_FAIL_COND(p_node->data.parent);

	List<Node *> owned = data.owned;
	List<Node *> owned_by_owner;
	Node *owner = (data.owner == this) ? p_node : data.owner;

	List<_NodeReplaceByPair> replace_data;

	if (p_keep_data) {
		List<PropertyInfo> plist;
		get_property_list(&plist);

		for (List<PropertyInfo>::Element *E = plist.front(); E; E = E->next()) {
			_NodeReplaceByPair rd;
			if (!(E->get().usage & PROPERTY_USAGE_STORAGE)) {
				continue;
			}
			rd.name = E->get().name;
			rd.value = get(rd.name);
		}

		List<GroupInfo> groups;
		get_groups(&groups);

		for (List<GroupInfo>::Element *E = groups.front(); E; E = E->next()) {
			p_node->add_to_group(E->get().name, E->get().persistent);
		}
	}

	_replace_connections_target(p_node);

	if (data.owner) {
		for (int i = 0; i < get_child_count(); i++) {
			find_owned_by(data.owner, get_child(i), &owned_by_owner);
		}
	}

	Node *parent = data.parent;
	int pos_in_parent = data.pos;

	if (data.parent) {
		parent->remove_child(this);
		parent->add_child(p_node);
		parent->move_child(p_node, pos_in_parent);
	}

	while (get_child_count()) {
		Node *child = get_child(0);
		remove_child(child);
		if (!child->is_owned_by_parent()) {
			// add the custom children to the p_node
			p_node->add_child(child);
		}
	}

	p_node->set_owner(owner);
	for (int i = 0; i < owned.size(); i++) {
		owned[i]->set_owner(p_node);
	}

	for (int i = 0; i < owned_by_owner.size(); i++) {
		owned_by_owner[i]->set_owner(owner);
	}

	p_node->set_filename(get_filename());

	for (List<_NodeReplaceByPair>::Element *E = replace_data.front(); E; E = E->next()) {
		p_node->set(E->get().name, E->get().value);
	}
}

void Node::_replace_connections_target(Node *p_new_target) {
	List<Connection> cl;
	get_signals_connected_to_this(&cl);

	for (List<Connection>::Element *E = cl.front(); E; E = E->next()) {
		Connection &c = E->get();

		if (c.flags & CONNECT_PERSIST) {
			c.source->disconnect(c.signal, this, c.method);
			bool valid = p_new_target->has_method(c.method) || Ref<Script>(p_new_target->get_script()).is_null() || Ref<Script>(p_new_target->get_script())->has_method(c.method);
			ERR_CONTINUE_MSG(!valid, "Attempt to connect signal '" + c.source->get_class() + "." + c.signal + "' to nonexistent method '" + c.target->get_class() + "." + c.method + "'.");
			c.source->connect(c.signal, p_new_target, c.method, c.binds, c.flags);
		}
	}
}

Vector<Variant> Node::make_binds(VARIANT_ARG_DECLARE) {
	Vector<Variant> ret;

	if (p_arg1.get_type() == Variant::NIL) {
		return ret;
	} else {
		ret.push_back(p_arg1);
	}

	if (p_arg2.get_type() == Variant::NIL) {
		return ret;
	} else {
		ret.push_back(p_arg2);
	}

	if (p_arg3.get_type() == Variant::NIL) {
		return ret;
	} else {
		ret.push_back(p_arg3);
	}

	if (p_arg4.get_type() == Variant::NIL) {
		return ret;
	} else {
		ret.push_back(p_arg4);
	}

	if (p_arg5.get_type() == Variant::NIL) {
		return ret;
	} else {
		ret.push_back(p_arg5);
	}

	return ret;
}

bool Node::has_node_and_resource(const NodePath &p_path) const {
	if (!has_node(p_path)) {
		return false;
	}
	RES res;
	Vector<StringName> leftover_path;
	Node *node = get_node_and_resource(p_path, res, leftover_path, false);

	return node;
}

Array Node::_get_node_and_resource(const NodePath &p_path) {
	RES res;
	Vector<StringName> leftover_path;
	Node *node = get_node_and_resource(p_path, res, leftover_path, false);
	Array result;

	if (node) {
		result.push_back(node);
	} else {
		result.push_back(Variant());
	}

	if (res.is_valid()) {
		result.push_back(res);
	} else {
		result.push_back(Variant());
	}

	result.push_back(NodePath(Vector<StringName>(), leftover_path, false));

	return result;
}

Node *Node::get_node_and_resource(const NodePath &p_path, RES &r_res, Vector<StringName> &r_leftover_subpath, bool p_last_is_property) const {
	Node *node = get_node(p_path);
	r_res = RES();
	r_leftover_subpath = Vector<StringName>();
	if (!node) {
		return nullptr;
	}

	if (p_path.get_subname_count()) {
		int j = 0;
		// If not p_last_is_property, we shouldn't consider the last one as part of the resource
		for (; j < p_path.get_subname_count() - (int)p_last_is_property; j++) {
			Variant new_res_v = j == 0 ? node->get(p_path.get_subname(j)) : r_res->get(p_path.get_subname(j));

			if (new_res_v.get_type() == Variant::NIL) { // Found nothing on that path
				return nullptr;
			}

			RES new_res = new_res_v;

			if (new_res.is_null()) { // No longer a resource, assume property
				break;
			}

			r_res = new_res;
		}
		for (; j < p_path.get_subname_count(); j++) {
			// Put the rest of the subpath in the leftover path
			r_leftover_subpath.push_back(p_path.get_subname(j));
		}
	}

	return node;
}

void Node::_set_tree(SceneTree *p_tree) {
	SceneTree *tree_changed_a = nullptr;
	SceneTree *tree_changed_b = nullptr;

	//ERR_FAIL_COND(p_scene && data.parent && !data.parent->data.scene); //nobug if both are null

	if (data.tree) {
		_propagate_exit_tree();

		tree_changed_a = data.tree;
	}

	data.tree = p_tree;

	if (data.tree) {
		_propagate_enter_tree();
		if (!data.parent || data.parent->data.ready_notified) { // No parent (root) or parent ready
			_propagate_ready(); //reverse_notification(NOTIFICATION_READY);
		}

		tree_changed_b = data.tree;
	}

	if (tree_changed_a) {
		tree_changed_a->tree_changed();
	}
	if (tree_changed_b) {
		tree_changed_b->tree_changed();
	}
}

#ifdef DEBUG_ENABLED
static void _Node_debug_sn(Object *p_obj) {
	Node *n = Object::cast_to<Node>(p_obj);
	if (!n) {
		return;
	}

	if (n->is_inside_tree()) {
		return;
	}

	Node *p = n;
	while (p->get_parent()) {
		p = p->get_parent();
	}

	String path;
	if (p == n) {
		path = n->get_name();
	} else {
		path = String(p->get_name()) + "/" + p->get_path_to(n);
	}

	String script_file_string;
	if (!n->get_script().is_null()) {
		Ref<Script> script = n->get_script();
		if (script.is_valid()) {
			script_file_string = ", Script: " + script->get_path();
		}
	}

	print_line(itos(p_obj->get_instance_id()) + " - Stray Node: " + path + " (Type: " + n->get_class() + script_file_string + ")");
}
#endif // DEBUG_ENABLED

void Node::_print_stray_nodes() {
	print_stray_nodes();
}

void Node::print_stray_nodes() {
#ifdef DEBUG_ENABLED
	ObjectDB::debug_objects(_Node_debug_sn);
#endif
}

void Node::queue_delete() {
	// There are users which instantiate multiple scene trees for their games.
	// Use the node's own tree to handle its deletion when relevant.
	if (is_inside_tree()) {
		get_tree()->queue_delete(this);
	} else {
		SceneTree *tree = SceneTree::get_singleton();
		ERR_FAIL_NULL_MSG(tree, "Can't queue free a node when no SceneTree is available.");
		tree->queue_delete(this);
	}
}

Array Node::_get_children() const {
	Array arr;
	int cc = get_child_count();
	arr.resize(cc);
	for (int i = 0; i < cc; i++) {
		arr[i] = get_child(i);
	}

	return arr;
}

void Node::set_import_path(const NodePath &p_import_path) {
#ifdef TOOLS_ENABLED
	data.import_path = p_import_path;
#endif
}

NodePath Node::get_import_path() const {
#ifdef TOOLS_ENABLED
	return data.import_path;
#else
	return NodePath();
#endif
}

static void _add_nodes_to_options(const Node *p_base, const Node *p_node, List<String> *r_options) {
#ifdef TOOLS_ENABLED
	const String quote_style = EDITOR_GET("text_editor/completion/use_single_quotes") ? "'" : "\"";
#else
	const String quote_style = "\"";
#endif

	if (p_node != p_base && !p_node->get_owner()) {
		return;
	}
	String n = p_base->get_path_to(p_node);
	r_options->push_back(quote_style + n + quote_style);
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_add_nodes_to_options(p_base, p_node->get_child(i), r_options);
	}
}

void Node::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {
	String pf = p_function;
	if ((pf == "has_node" || pf == "get_node") && p_idx == 0) {
		_add_nodes_to_options(this, this, r_options);
	}
	Object::get_argument_options(p_function, p_idx, r_options);
}

void Node::clear_internal_tree_resource_paths() {
	clear_internal_resource_paths();
	for (int i = 0; i < data.children.size(); i++) {
		data.children[i]->clear_internal_tree_resource_paths();
	}
}

String Node::get_configuration_warning() const {
	if (get_script_instance() && get_script_instance()->get_script().is_valid() &&
			get_script_instance()->get_script()->is_tool() && get_script_instance()->has_method("_get_configuration_warning")) {
		return get_script_instance()->call("_get_configuration_warning");
	}
	return String();
}

void Node::update_configuration_warning() {
#ifdef TOOLS_ENABLED
	if (!is_inside_tree()) {
		return;
	}
	if (get_tree()->get_edited_scene_root() && (get_tree()->get_edited_scene_root() == this || get_tree()->get_edited_scene_root()->is_a_parent_of(this))) {
		get_tree()->emit_signal(SceneStringNames::get_singleton()->node_configuration_warning_changed, this);
	}
#endif
}

bool Node::is_owned_by_parent() const {
	return data.parent_owned;
}

void Node::set_display_folded(bool p_folded) {
	data.display_folded = p_folded;
}

bool Node::is_displayed_folded() const {
	return data.display_folded;
}

bool Node::is_node_ready() const {
	return !data.ready_first;
}

void Node::request_ready() {
	data.ready_first = true;
}

void Node::_bind_methods() {
	GLOBAL_DEF("node/name_num_separator", 0);
	ProjectSettings::get_singleton()->set_custom_property_info("node/name_num_separator", PropertyInfo(Variant::INT, "node/name_num_separator", PROPERTY_HINT_ENUM, "None,Space,Underscore,Dash"));
	GLOBAL_DEF("node/name_casing", NAME_CASING_PASCAL_CASE);
	ProjectSettings::get_singleton()->set_custom_property_info("node/name_casing", PropertyInfo(Variant::INT, "node/name_casing", PROPERTY_HINT_ENUM, "PascalCase,camelCase,snake_case"));

	ClassDB::bind_method(D_METHOD("add_child_below_node", "node", "child_node", "force_readable_name"), &Node::add_child_below_node, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("set_name", "name"), &Node::set_name);
	ClassDB::bind_method(D_METHOD("get_name"), &Node::get_name);
	ClassDB::bind_method(D_METHOD("add_child", "node", "force_readable_name"), &Node::add_child, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("remove_child", "node"), &Node::remove_child);
	ClassDB::bind_method(D_METHOD("get_child_count"), &Node::get_child_count);
	ClassDB::bind_method(D_METHOD("get_children"), &Node::_get_children);
	ClassDB::bind_method(D_METHOD("get_child", "idx"), &Node::get_child);
	ClassDB::bind_method(D_METHOD("has_node", "path"), &Node::has_node);
	ClassDB::bind_method(D_METHOD("get_node", "path"), &Node::get_node);
	ClassDB::bind_method(D_METHOD("get_node_or_null", "path"), &Node::get_node_or_null);
	ClassDB::bind_method(D_METHOD("get_parent"), &Node::get_parent);
	ClassDB::bind_method(D_METHOD("find_node", "mask", "recursive", "owned"), &Node::find_node, DEFVAL(true), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("find_parent", "mask"), &Node::find_parent);
	ClassDB::bind_method(D_METHOD("has_node_and_resource", "path"), &Node::has_node_and_resource);
	ClassDB::bind_method(D_METHOD("get_node_and_resource", "path"), &Node::_get_node_and_resource);

	ClassDB::bind_method(D_METHOD("is_inside_tree"), &Node::is_inside_tree);
	ClassDB::bind_method(D_METHOD("is_a_parent_of", "node"), &Node::is_a_parent_of);
	ClassDB::bind_method(D_METHOD("is_greater_than", "node"), &Node::is_greater_than);
	ClassDB::bind_method(D_METHOD("get_path"), &Node::get_path);
	ClassDB::bind_method(D_METHOD("get_path_to", "node"), &Node::get_path_to);
	ClassDB::bind_method(D_METHOD("add_to_group", "group", "persistent"), &Node::add_to_group, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("remove_from_group", "group"), &Node::remove_from_group);
	ClassDB::bind_method(D_METHOD("is_in_group", "group"), &Node::is_in_group);
	ClassDB::bind_method(D_METHOD("move_child", "child_node", "to_position"), &Node::move_child);
	ClassDB::bind_method(D_METHOD("get_groups"), &Node::_get_groups);
	ClassDB::bind_method(D_METHOD("raise"), &Node::raise);
	ClassDB::bind_method(D_METHOD("set_owner", "owner"), &Node::set_owner);
	ClassDB::bind_method(D_METHOD("get_owner"), &Node::get_owner);
	ClassDB::bind_method(D_METHOD("remove_and_skip"), &Node::remove_and_skip);
	ClassDB::bind_method(D_METHOD("get_index"), &Node::get_index);
	ClassDB::bind_method(D_METHOD("print_tree"), &Node::print_tree);
	ClassDB::bind_method(D_METHOD("print_tree_pretty"), &Node::print_tree_pretty);
	ClassDB::bind_method(D_METHOD("set_filename", "filename"), &Node::set_filename);
	ClassDB::bind_method(D_METHOD("get_filename"), &Node::get_filename);
	ClassDB::bind_method(D_METHOD("propagate_notification", "what"), &Node::propagate_notification);
	ClassDB::bind_method(D_METHOD("propagate_call", "method", "args", "parent_first"), &Node::propagate_call, DEFVAL(Array()), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("set_physics_process", "enable"), &Node::set_physics_process);
	ClassDB::bind_method(D_METHOD("get_physics_process_delta_time"), &Node::get_physics_process_delta_time);
	ClassDB::bind_method(D_METHOD("is_physics_processing"), &Node::is_physics_processing);
	ClassDB::bind_method(D_METHOD("get_process_delta_time"), &Node::get_process_delta_time);
	ClassDB::bind_method(D_METHOD("set_process", "enable"), &Node::set_process);
	ClassDB::bind_method(D_METHOD("set_process_priority", "priority"), &Node::set_process_priority);
	ClassDB::bind_method(D_METHOD("get_process_priority"), &Node::get_process_priority);
	ClassDB::bind_method(D_METHOD("is_processing"), &Node::is_processing);
	ClassDB::bind_method(D_METHOD("set_process_input", "enable"), &Node::set_process_input);
	ClassDB::bind_method(D_METHOD("is_processing_input"), &Node::is_processing_input);
	ClassDB::bind_method(D_METHOD("set_process_unhandled_input", "enable"), &Node::set_process_unhandled_input);
	ClassDB::bind_method(D_METHOD("is_processing_unhandled_input"), &Node::is_processing_unhandled_input);
	ClassDB::bind_method(D_METHOD("set_process_unhandled_key_input", "enable"), &Node::set_process_unhandled_key_input);
	ClassDB::bind_method(D_METHOD("is_processing_unhandled_key_input"), &Node::is_processing_unhandled_key_input);
	ClassDB::bind_method(D_METHOD("set_pause_mode", "mode"), &Node::set_pause_mode);
	ClassDB::bind_method(D_METHOD("get_pause_mode"), &Node::get_pause_mode);
	ClassDB::bind_method(D_METHOD("can_process"), &Node::can_process);
	ClassDB::bind_method(D_METHOD("print_stray_nodes"), &Node::_print_stray_nodes);
	ClassDB::bind_method(D_METHOD("get_position_in_parent"), &Node::get_position_in_parent);

	ClassDB::bind_method(D_METHOD("set_display_folded", "fold"), &Node::set_display_folded);
	ClassDB::bind_method(D_METHOD("is_displayed_folded"), &Node::is_displayed_folded);

	ClassDB::bind_method(D_METHOD("set_process_internal", "enable"), &Node::set_process_internal);
	ClassDB::bind_method(D_METHOD("is_processing_internal"), &Node::is_processing_internal);

	ClassDB::bind_method(D_METHOD("set_physics_process_internal", "enable"), &Node::set_physics_process_internal);
	ClassDB::bind_method(D_METHOD("is_physics_processing_internal"), &Node::is_physics_processing_internal);

	ClassDB::bind_method(D_METHOD("set_physics_interpolation_mode", "mode"), &Node::set_physics_interpolation_mode);
	ClassDB::bind_method(D_METHOD("get_physics_interpolation_mode"), &Node::get_physics_interpolation_mode);
	ClassDB::bind_method(D_METHOD("is_physics_interpolated"), &Node::is_physics_interpolated);
	ClassDB::bind_method(D_METHOD("is_physics_interpolated_and_enabled"), &Node::is_physics_interpolated_and_enabled);
	ClassDB::bind_method(D_METHOD("reset_physics_interpolation"), &Node::reset_physics_interpolation);

	ClassDB::bind_method(D_METHOD("get_tree"), &Node::get_tree);
	ClassDB::bind_method(D_METHOD("create_tween"), &Node::create_tween);

	ClassDB::bind_method(D_METHOD("duplicate", "flags"), &Node::duplicate, DEFVAL(DUPLICATE_USE_INSTANCING | DUPLICATE_SIGNALS | DUPLICATE_GROUPS | DUPLICATE_SCRIPTS));
	ClassDB::bind_method(D_METHOD("replace_by", "node", "keep_data"), &Node::replace_by, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("set_scene_instance_load_placeholder", "load_placeholder"), &Node::set_scene_instance_load_placeholder);
	ClassDB::bind_method(D_METHOD("get_scene_instance_load_placeholder"), &Node::get_scene_instance_load_placeholder);
	ClassDB::bind_method(D_METHOD("set_editable_instance", "node", "is_editable"), &Node::set_editable_instance);
	ClassDB::bind_method(D_METHOD("is_editable_instance", "node"), &Node::is_editable_instance);

	ClassDB::bind_method(D_METHOD("get_viewport"), &Node::get_viewport);

	ClassDB::bind_method(D_METHOD("queue_free"), &Node::queue_delete);

	ClassDB::bind_method(D_METHOD("request_ready"), &Node::request_ready);
	ClassDB::bind_method(D_METHOD("is_node_ready"), &Node::is_node_ready);

	ClassDB::bind_method(D_METHOD("set_network_master", "id", "recursive"), &Node::set_network_master, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_network_master"), &Node::get_network_master);

	ClassDB::bind_method(D_METHOD("is_network_master"), &Node::is_network_master);

	ClassDB::bind_method(D_METHOD("get_multiplayer"), &Node::get_multiplayer);
	ClassDB::bind_method(D_METHOD("get_custom_multiplayer"), &Node::get_custom_multiplayer);
	ClassDB::bind_method(D_METHOD("set_custom_multiplayer", "api"), &Node::set_custom_multiplayer);
	ClassDB::bind_method(D_METHOD("rpc_config", "method", "mode"), &Node::rpc_config);
	ClassDB::bind_method(D_METHOD("rset_config", "property", "mode"), &Node::rset_config);

	ClassDB::bind_method(D_METHOD("_set_editor_description", "editor_description"), &Node::set_editor_description);
	ClassDB::bind_method(D_METHOD("_get_editor_description"), &Node::get_editor_description);
	ADD_GROUP("Editor Description", "editor_");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "editor_description", PROPERTY_HINT_MULTILINE_TEXT, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_editor_description", "_get_editor_description");

	ClassDB::bind_method(D_METHOD("_set_import_path", "import_path"), &Node::set_import_path);
	ClassDB::bind_method(D_METHOD("_get_import_path"), &Node::get_import_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "_import_path", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "_set_import_path", "_get_import_path");

#ifdef TOOLS_ENABLED
	ClassDB::bind_method(D_METHOD("_set_property_pinned", "property", "pinned"), &Node::set_property_pinned);
#endif
	ClassDB::bind_method(D_METHOD("set_unique_name_in_owner", "enable"), &Node::set_unique_name_in_owner);
	ClassDB::bind_method(D_METHOD("is_unique_name_in_owner"), &Node::is_unique_name_in_owner);

	{
		MethodInfo mi;

		mi.arguments.push_back(PropertyInfo(Variant::STRING, "method"));

		mi.name = "rpc";
		ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "rpc", &Node::_rpc_bind, mi);
		mi.name = "rpc_unreliable";
		ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "rpc_unreliable", &Node::_rpc_unreliable_bind, mi);

		mi.arguments.push_front(PropertyInfo(Variant::INT, "peer_id"));

		mi.name = "rpc_id";
		ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "rpc_id", &Node::_rpc_id_bind, mi);
		mi.name = "rpc_unreliable_id";
		ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "rpc_unreliable_id", &Node::_rpc_unreliable_id_bind, mi);
	}

	ClassDB::bind_method(D_METHOD("rset", "property", "value"), &Node::rset);
	ClassDB::bind_method(D_METHOD("rset_id", "peer_id", "property", "value"), &Node::rset_id);
	ClassDB::bind_method(D_METHOD("rset_unreliable", "property", "value"), &Node::rset_unreliable);
	ClassDB::bind_method(D_METHOD("rset_unreliable_id", "peer_id", "property", "value"), &Node::rset_unreliable_id);

	ClassDB::bind_method(D_METHOD("update_configuration_warning"), &Node::update_configuration_warning);

	BIND_CONSTANT(NOTIFICATION_ENTER_TREE);
	BIND_CONSTANT(NOTIFICATION_EXIT_TREE);
	BIND_CONSTANT(NOTIFICATION_MOVED_IN_PARENT);
	BIND_CONSTANT(NOTIFICATION_READY);
	BIND_CONSTANT(NOTIFICATION_PAUSED);
	BIND_CONSTANT(NOTIFICATION_UNPAUSED);
	BIND_CONSTANT(NOTIFICATION_PHYSICS_PROCESS);
	BIND_CONSTANT(NOTIFICATION_PROCESS);
	BIND_CONSTANT(NOTIFICATION_PARENTED);
	BIND_CONSTANT(NOTIFICATION_UNPARENTED);
	BIND_CONSTANT(NOTIFICATION_INSTANCED);
	BIND_CONSTANT(NOTIFICATION_DRAG_BEGIN);
	BIND_CONSTANT(NOTIFICATION_DRAG_END);
	BIND_CONSTANT(NOTIFICATION_PATH_CHANGED);
	BIND_CONSTANT(NOTIFICATION_CHILD_ORDER_CHANGED);
	BIND_CONSTANT(NOTIFICATION_INTERNAL_PROCESS);
	BIND_CONSTANT(NOTIFICATION_INTERNAL_PHYSICS_PROCESS);
	BIND_CONSTANT(NOTIFICATION_POST_ENTER_TREE);
	BIND_CONSTANT(NOTIFICATION_RESET_PHYSICS_INTERPOLATION);

	BIND_CONSTANT(NOTIFICATION_WM_MOUSE_ENTER);
	BIND_CONSTANT(NOTIFICATION_WM_MOUSE_EXIT);
	BIND_CONSTANT(NOTIFICATION_WM_FOCUS_IN);
	BIND_CONSTANT(NOTIFICATION_WM_FOCUS_OUT);
	BIND_CONSTANT(NOTIFICATION_WM_QUIT_REQUEST);
	BIND_CONSTANT(NOTIFICATION_WM_GO_BACK_REQUEST);
	BIND_CONSTANT(NOTIFICATION_WM_UNFOCUS_REQUEST);
	BIND_CONSTANT(NOTIFICATION_OS_MEMORY_WARNING);
	BIND_CONSTANT(NOTIFICATION_TRANSLATION_CHANGED);
	BIND_CONSTANT(NOTIFICATION_WM_ABOUT);
	BIND_CONSTANT(NOTIFICATION_CRASH);
	BIND_CONSTANT(NOTIFICATION_OS_IME_UPDATE);
	BIND_CONSTANT(NOTIFICATION_APP_RESUMED);
	BIND_CONSTANT(NOTIFICATION_APP_PAUSED);

	BIND_ENUM_CONSTANT(PAUSE_MODE_INHERIT);
	BIND_ENUM_CONSTANT(PAUSE_MODE_STOP);
	BIND_ENUM_CONSTANT(PAUSE_MODE_PROCESS);

	BIND_ENUM_CONSTANT(PHYSICS_INTERPOLATION_MODE_INHERIT);
	BIND_ENUM_CONSTANT(PHYSICS_INTERPOLATION_MODE_OFF);
	BIND_ENUM_CONSTANT(PHYSICS_INTERPOLATION_MODE_ON);

	BIND_ENUM_CONSTANT(DUPLICATE_SIGNALS);
	BIND_ENUM_CONSTANT(DUPLICATE_GROUPS);
	BIND_ENUM_CONSTANT(DUPLICATE_SCRIPTS);
	BIND_ENUM_CONSTANT(DUPLICATE_USE_INSTANCING);

	ADD_SIGNAL(MethodInfo("ready"));
	ADD_SIGNAL(MethodInfo("renamed"));
	ADD_SIGNAL(MethodInfo("tree_entered"));
	ADD_SIGNAL(MethodInfo("tree_exiting"));
	ADD_SIGNAL(MethodInfo("tree_exited"));
	ADD_SIGNAL(MethodInfo("child_entered_tree", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "Node")));
	ADD_SIGNAL(MethodInfo("child_exiting_tree", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "Node")));
	ADD_SIGNAL(MethodInfo("child_order_changed"));

	ADD_PROPERTY(PropertyInfo(Variant::INT, "pause_mode", PROPERTY_HINT_ENUM, "Inherit,Stop,Process"), "set_pause_mode", "get_pause_mode");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "physics_interpolation_mode", PROPERTY_HINT_ENUM, "Inherit,Off,On"), "set_physics_interpolation_mode", "get_physics_interpolation_mode");

#ifdef ENABLE_DEPRECATED
	//no longer exists, but remains for compatibility (keep previous scenes folded
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "editor/display_folded", PROPERTY_HINT_NONE, "", 0), "set_display_folded", "is_displayed_folded");
#endif

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name", PROPERTY_HINT_NONE, "", 0), "set_name", "get_name");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "unique_name_in_owner", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR), "set_unique_name_in_owner", "is_unique_name_in_owner");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "filename", PROPERTY_HINT_NONE, "", 0), "set_filename", "get_filename");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "owner", PROPERTY_HINT_RESOURCE_TYPE, "Node", 0), "set_owner", "get_owner");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "multiplayer", PROPERTY_HINT_RESOURCE_TYPE, "MultiplayerAPI", 0), "", "get_multiplayer");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "custom_multiplayer", PROPERTY_HINT_RESOURCE_TYPE, "MultiplayerAPI", 0), "set_custom_multiplayer", "get_custom_multiplayer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "process_priority"), "set_process_priority", "get_process_priority");

	BIND_VMETHOD(MethodInfo("_process", PropertyInfo(Variant::REAL, "delta")));
	BIND_VMETHOD(MethodInfo("_physics_process", PropertyInfo(Variant::REAL, "delta")));
	BIND_VMETHOD(MethodInfo("_enter_tree"));
	BIND_VMETHOD(MethodInfo("_exit_tree"));
	BIND_VMETHOD(MethodInfo("_ready"));
	BIND_VMETHOD(MethodInfo("_input", PropertyInfo(Variant::OBJECT, "event", PROPERTY_HINT_RESOURCE_TYPE, "InputEvent")));
	BIND_VMETHOD(MethodInfo("_unhandled_input", PropertyInfo(Variant::OBJECT, "event", PROPERTY_HINT_RESOURCE_TYPE, "InputEvent")));
	BIND_VMETHOD(MethodInfo("_unhandled_key_input", PropertyInfo(Variant::OBJECT, "event", PROPERTY_HINT_RESOURCE_TYPE, "InputEventKey")));
	BIND_VMETHOD(MethodInfo(Variant::STRING, "_get_configuration_warning"));
}

String Node::_get_name_num_separator() {
	switch (ProjectSettings::get_singleton()->get("node/name_num_separator").operator int()) {
		case 0:
			return "";
		case 1:
			return " ";
		case 2:
			return "_";
		case 3:
			return "-";
	}
	return " ";
}

Node::Node() {
	data.pos = -1;
	data.depth = -1;
	data.blocked = 0;
	data.parent = nullptr;
	data.tree = nullptr;
	data.physics_process = false;
	data.idle_process = false;
	data.process_priority = 0;
	data.physics_process_internal = false;
	data.idle_process_internal = false;
	data.inside_tree = false;
	data.ready_notified = false;
	data.physics_interpolated = true;
	data.physics_interpolation_reset_requested = false;
	data.physics_interpolated_client_side = false;
	data.use_identity_transform = false;

	data.owner = nullptr;
	data.OW = nullptr;
	data.input = false;
	data.unhandled_input = false;
	data.unhandled_key_input = false;
	data.pause_mode = PAUSE_MODE_INHERIT;
	data.physics_interpolation_mode = PHYSICS_INTERPOLATION_MODE_INHERIT;
	data.pause_owner = nullptr;
	data.network_master = 1; //server by default
	data.path_cache = nullptr;
	data.parent_owned = false;
	data.in_constructor = true;
	data.viewport = nullptr;
	data.use_placeholder = false;
	data.display_folded = false;
	data.ready_first = true;
	data.editable_instance = false;

	orphan_node_count++;
}

Node::~Node() {
	data.grouped.clear();
	data.owned.clear();
	data.children.clear();

	ERR_FAIL_COND(data.parent);
	ERR_FAIL_COND(data.children.size());

	orphan_node_count--;
}

////////////////////////////////

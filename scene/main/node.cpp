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

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "core/object/message_queue.h"
#include "core/object/script_language.h"
#include "core/string/print_string.h"
#include "instance_placeholder.h"
#include "scene/animation/tween.h"
#include "scene/debugger/scene_debugger.h"
#include "scene/main/multiplayer_api.h"
#include "scene/main/window.h"
#include "scene/resources/packed_scene.h"
#include "viewport.h"

#include <stdint.h>

int Node::orphan_node_count = 0;

thread_local Node *Node::current_process_thread_group = nullptr;

void Node::_notification(int p_notification) {
	switch (p_notification) {
		case NOTIFICATION_PROCESS: {
			GDVIRTUAL_CALL(_process, get_process_delta_time());
		} break;

		case NOTIFICATION_PHYSICS_PROCESS: {
			GDVIRTUAL_CALL(_physics_process, get_physics_process_delta_time());
		} break;

		case NOTIFICATION_ENTER_TREE: {
			ERR_FAIL_NULL(get_viewport());
			ERR_FAIL_NULL(get_tree());

			// Update process mode.
			if (data.process_mode == PROCESS_MODE_INHERIT) {
				if (data.parent) {
					data.process_owner = data.parent->data.process_owner;
				} else {
					ERR_PRINT("The root node can't be set to Inherit process mode, reverting to Pausable instead.");
					data.process_mode = PROCESS_MODE_PAUSABLE;
					data.process_owner = this;
				}
			} else {
				data.process_owner = this;
			}

			{ // Update threaded process mode.
				if (data.process_thread_group == PROCESS_THREAD_GROUP_INHERIT) {
					if (data.parent) {
						data.process_thread_group_owner = data.parent->data.process_thread_group_owner;
					}

					if (data.process_thread_group_owner) {
						data.process_group = data.process_thread_group_owner->data.process_group;
					} else {
						data.process_group = &data.tree->default_process_group;
					}
				} else {
					data.process_thread_group_owner = this;
					_add_process_group();
				}

				if (_is_any_processing()) {
					_add_to_process_thread_group();
				}
			}

			if (data.physics_interpolation_mode == PHYSICS_INTERPOLATION_MODE_INHERIT) {
				bool interpolate = true; // Root node default is for interpolation to be on.
				if (data.parent) {
					interpolate = data.parent->is_physics_interpolated();
				}
				_propagate_physics_interpolated(interpolate);
			}

			// Update auto translate mode.
			if (data.auto_translate_mode == AUTO_TRANSLATE_MODE_INHERIT && !data.parent) {
				ERR_PRINT("The root node can't be set to Inherit auto translate mode, reverting to Always instead.");
				data.auto_translate_mode = AUTO_TRANSLATE_MODE_ALWAYS;
			}
			data.is_auto_translate_dirty = true;
			data.is_translation_domain_dirty = true;

#ifdef TOOLS_ENABLED
			// Don't translate UI elements when they're being edited.
			if (is_part_of_edited_scene()) {
				set_message_translation(false);
			}
#endif

			if (data.input) {
				add_to_group("_vp_input" + itos(get_viewport()->get_instance_id()));
			}
			if (data.shortcut_input) {
				add_to_group("_vp_shortcut_input" + itos(get_viewport()->get_instance_id()));
			}
			if (data.unhandled_input) {
				add_to_group("_vp_unhandled_input" + itos(get_viewport()->get_instance_id()));
			}
			if (data.unhandled_key_input) {
				add_to_group("_vp_unhandled_key_input" + itos(get_viewport()->get_instance_id()));
			}

			get_tree()->nodes_in_tree_count++;
			orphan_node_count--;

			// Allow physics interpolated nodes to automatically reset when added to the tree
			// (this is to save the user from doing this manually each time).
			if (get_tree()->is_physics_interpolation_enabled()) {
				_set_physics_interpolation_reset_requested(true);
			}
		} break;

		case NOTIFICATION_POST_ENTER_TREE: {
			if (data.auto_translate_mode != AUTO_TRANSLATE_MODE_DISABLED) {
				notification(NOTIFICATION_TRANSLATION_CHANGED);
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			ERR_FAIL_NULL(get_viewport());
			ERR_FAIL_NULL(get_tree());

			get_tree()->nodes_in_tree_count--;
			orphan_node_count++;

			if (data.input) {
				remove_from_group("_vp_input" + itos(get_viewport()->get_instance_id()));
			}
			if (data.shortcut_input) {
				remove_from_group("_vp_shortcut_input" + itos(get_viewport()->get_instance_id()));
			}
			if (data.unhandled_input) {
				remove_from_group("_vp_unhandled_input" + itos(get_viewport()->get_instance_id()));
			}
			if (data.unhandled_key_input) {
				remove_from_group("_vp_unhandled_key_input" + itos(get_viewport()->get_instance_id()));
			}

			// Remove from processing first.
			if (_is_any_processing()) {
				_remove_from_process_thread_group();
			}
			// Remove the process group.
			if (data.process_thread_group_owner == this) {
				_remove_process_group();
			}
			data.process_thread_group_owner = nullptr;
			data.process_owner = nullptr;

			if (data.path_cache) {
				memdelete(data.path_cache);
				data.path_cache = nullptr;
			}
		} break;

		case NOTIFICATION_SUSPENDED:
		case NOTIFICATION_PAUSED: {
			if (is_physics_interpolated_and_enabled() && is_inside_tree()) {
				reset_physics_interpolation();
			}
		} break;

		case NOTIFICATION_PATH_RENAMED: {
			if (data.path_cache) {
				memdelete(data.path_cache);
				data.path_cache = nullptr;
			}
		} break;

		case NOTIFICATION_READY: {
			if (GDVIRTUAL_IS_OVERRIDDEN(_input)) {
				set_process_input(true);
			}

			if (GDVIRTUAL_IS_OVERRIDDEN(_shortcut_input)) {
				set_process_shortcut_input(true);
			}

			if (GDVIRTUAL_IS_OVERRIDDEN(_unhandled_input)) {
				set_process_unhandled_input(true);
			}

			if (GDVIRTUAL_IS_OVERRIDDEN(_unhandled_key_input)) {
				set_process_unhandled_key_input(true);
			}

			if (GDVIRTUAL_IS_OVERRIDDEN(_process)) {
				set_process(true);
			}
			if (GDVIRTUAL_IS_OVERRIDDEN(_physics_process)) {
				set_physics_process(true);
			}

			GDVIRTUAL_CALL(_ready);
		} break;

		case NOTIFICATION_POSTINITIALIZE: {
			data.in_constructor = false;
		} break;

		case NOTIFICATION_PREDELETE: {
			if (data.inside_tree && !Thread::is_main_thread()) {
				cancel_free();
				ERR_PRINT("Attempted to free a node that is currently added to the SceneTree from a thread. This is not permitted, use queue_free() instead. Node has not been freed.");
				return;
			}

			if (data.owner) {
				_clean_up_owner();
			}

			while (!data.owned.is_empty()) {
				Node *n = data.owned.back()->get();
				n->_clean_up_owner(); // This will change data.owned. So it's impossible to loop over the list in the usual manner.
			}

			if (data.parent) {
				data.parent->remove_child(this);
			}

			// kill children as cleanly as possible
			while (data.children.size()) {
				Node *child = data.children.last()->value; // begin from the end because its faster and more consistent with creation
				memdelete(child);
			}
		} break;

		case NOTIFICATION_TRANSLATION_CHANGED: {
			if (data.inside_tree) {
				data.is_auto_translate_dirty = true;
			}
		} break;
	}
}

void Node::_propagate_ready() {
	data.ready_notified = true;
	data.blocked++;
	for (KeyValue<StringName, Node *> &K : data.children) {
		K.value->_propagate_ready();
	}

	data.blocked--;

	notification(NOTIFICATION_POST_ENTER_TREE);

	if (data.ready_first) {
		data.ready_first = false;
		notification(NOTIFICATION_READY);
		emit_signal(SceneStringName(ready));
	}
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

	for (KeyValue<StringName, GroupData> &E : data.grouped) {
		E.value.group = data.tree->add_to_group(E.key, this);
	}

	notification(NOTIFICATION_ENTER_TREE);

	GDVIRTUAL_CALL(_enter_tree);

	emit_signal(SceneStringName(tree_entered));

	data.tree->node_added(this);

	if (data.parent) {
		Variant c = this;
		const Variant *cptr = &c;
		data.parent->emit_signalp(SNAME("child_entered_tree"), &cptr, 1);
	}

	data.blocked++;
	//block while adding children

	for (KeyValue<StringName, Node *> &K : data.children) {
		if (!K.value->is_inside_tree()) { // could have been added in enter_tree
			K.value->_propagate_enter_tree();
		}
	}

	data.blocked--;

#ifdef DEBUG_ENABLED
	SceneDebugger::add_to_cache(data.scene_file_path, this);
#endif
	// enter groups
}

void Node::_propagate_after_exit_tree() {
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
			_clean_up_owner();
		}
	}

	data.blocked++;

	for (HashMap<StringName, Node *>::Iterator I = data.children.last(); I; --I) {
		I->value->_propagate_after_exit_tree();
	}

	data.blocked--;

	emit_signal(SceneStringName(tree_exited));
}

void Node::_propagate_exit_tree() {
	//block while removing children

#ifdef DEBUG_ENABLED
	if (!data.scene_file_path.is_empty()) {
		// Only remove if file path is set (optimization).
		SceneDebugger::remove_from_cache(data.scene_file_path, this);
	}
#endif
	data.blocked++;

	for (HashMap<StringName, Node *>::Iterator I = data.children.last(); I; --I) {
		I->value->_propagate_exit_tree();
	}

	data.blocked--;

	GDVIRTUAL_CALL(_exit_tree);

	emit_signal(SceneStringName(tree_exiting));

	notification(NOTIFICATION_EXIT_TREE, true);
	if (data.tree) {
		data.tree->node_removed(this);
	}

	if (data.parent) {
		Variant c = this;
		const Variant *cptr = &c;
		data.parent->emit_signalp(SNAME("child_exiting_tree"), &cptr, 1);
	}

	// exit groups
	for (KeyValue<StringName, GroupData> &E : data.grouped) {
		data.tree->remove_from_group(E.key, this);
		E.value.group = nullptr;
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

void Node::_propagate_physics_interpolated(bool p_interpolated) {
	switch (data.physics_interpolation_mode) {
		case PHYSICS_INTERPOLATION_MODE_INHERIT:
			// Keep the parent p_interpolated.
			break;
		case PHYSICS_INTERPOLATION_MODE_OFF: {
			p_interpolated = false;
		} break;
		case PHYSICS_INTERPOLATION_MODE_ON: {
			p_interpolated = true;
		} break;
	}

	// No change? No need to propagate further.
	if (data.physics_interpolated == p_interpolated) {
		return;
	}

	data.physics_interpolated = p_interpolated;

	// Allow a call to the RenderingServer etc. in derived classes.
	_physics_interpolated_changed();

	data.blocked++;
	for (KeyValue<StringName, Node *> &K : data.children) {
		K.value->_propagate_physics_interpolated(p_interpolated);
	}
	data.blocked--;
}

void Node::_propagate_physics_interpolation_reset_requested(bool p_requested) {
	if (is_physics_interpolated()) {
		data.physics_interpolation_reset_requested = p_requested;
	}

	data.blocked++;
	for (KeyValue<StringName, Node *> &K : data.children) {
		K.value->_propagate_physics_interpolation_reset_requested(p_requested);
	}
	data.blocked--;
}

void Node::move_child(Node *p_child, int p_index) {
	ERR_FAIL_COND_MSG(data.inside_tree && !Thread::is_main_thread(), "Moving child node positions inside the SceneTree is only allowed from the main thread. Use call_deferred(\"move_child\",child,index).");
	ERR_FAIL_NULL(p_child);
	ERR_FAIL_COND_MSG(p_child->data.parent != this, "Child is not a child of this node.");

	_update_children_cache();
	// We need to check whether node is internal and move it only in the relevant node range.
	if (p_child->data.internal_mode == INTERNAL_MODE_FRONT) {
		if (p_index < 0) {
			p_index += data.internal_children_front_count_cache;
		}
		ERR_FAIL_INDEX_MSG(p_index, data.internal_children_front_count_cache, vformat("Invalid new child index: %d. Child is internal.", p_index));
		_move_child(p_child, p_index);
	} else if (p_child->data.internal_mode == INTERNAL_MODE_BACK) {
		if (p_index < 0) {
			p_index += data.internal_children_back_count_cache;
		}
		ERR_FAIL_INDEX_MSG(p_index, data.internal_children_back_count_cache, vformat("Invalid new child index: %d. Child is internal.", p_index));
		_move_child(p_child, (int)data.children_cache.size() - data.internal_children_back_count_cache + p_index);
	} else {
		if (p_index < 0) {
			p_index += get_child_count(false);
		}
		ERR_FAIL_INDEX_MSG(p_index, (int)data.children_cache.size() + 1 - data.internal_children_front_count_cache - data.internal_children_back_count_cache, vformat("Invalid new child index: %d.", p_index));
		_move_child(p_child, p_index + data.internal_children_front_count_cache);
	}
}

void Node::_move_child(Node *p_child, int p_index, bool p_ignore_end) {
	ERR_FAIL_COND_MSG(data.blocked > 0, "Parent node is busy setting up children, `move_child()` failed. Consider using `move_child.call_deferred(child, index)` instead (or `popup.call_deferred()` if this is from a popup).");

	// Specifying one place beyond the end
	// means the same as moving to the last index
	if (!p_ignore_end) { // p_ignore_end is a little hack to make back internal children work properly.
		if (p_child->data.internal_mode == INTERNAL_MODE_FRONT) {
			if (p_index == data.internal_children_front_count_cache) {
				p_index--;
			}
		} else if (p_child->data.internal_mode == INTERNAL_MODE_BACK) {
			if (p_index == (int)data.children_cache.size()) {
				p_index--;
			}
		} else {
			if (p_index == (int)data.children_cache.size() - data.internal_children_back_count_cache) {
				p_index--;
			}
		}
	}

	int child_index = p_child->get_index();

	if (child_index == p_index) {
		return; //do nothing
	}

	int motion_from = MIN(p_index, child_index);
	int motion_to = MAX(p_index, child_index);

	data.children_cache.remove_at(child_index);
	data.children_cache.insert(p_index, p_child);

	if (data.tree) {
		data.tree->tree_changed();
	}

	data.blocked++;
	//new pos first
	for (int i = motion_from; i <= motion_to; i++) {
		if (data.children_cache[i]->data.internal_mode == INTERNAL_MODE_DISABLED) {
			data.children_cache[i]->data.index = i - data.internal_children_front_count_cache;
		} else if (data.children_cache[i]->data.internal_mode == INTERNAL_MODE_BACK) {
			data.children_cache[i]->data.index = i - data.internal_children_front_count_cache - data.external_children_count_cache;
		} else {
			data.children_cache[i]->data.index = i;
		}
	}
	// notification second
	move_child_notify(p_child);
	notification(NOTIFICATION_CHILD_ORDER_CHANGED);
	emit_signal(SNAME("child_order_changed"));
	p_child->_propagate_groups_dirty();

	data.blocked--;
}

void Node::_propagate_groups_dirty() {
	for (const KeyValue<StringName, GroupData> &E : data.grouped) {
		if (E.value.group) {
			E.value.group->changed = true;
		}
	}

	for (KeyValue<StringName, Node *> &K : data.children) {
		K.value->_propagate_groups_dirty();
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
	ERR_THREAD_GUARD
	if (data.physics_process == p_process) {
		return;
	}

	if (!is_inside_tree()) {
		data.physics_process = p_process;
		return;
	}

	if (_is_any_processing()) {
		_remove_from_process_thread_group();
	}

	data.physics_process = p_process;

	if (_is_any_processing()) {
		_add_to_process_thread_group();
	}
}

bool Node::is_physics_processing() const {
	return data.physics_process;
}

void Node::set_physics_process_internal(bool p_process_internal) {
	ERR_THREAD_GUARD
	if (data.physics_process_internal == p_process_internal) {
		return;
	}

	if (!is_inside_tree()) {
		data.physics_process_internal = p_process_internal;
		return;
	}

	if (_is_any_processing()) {
		_remove_from_process_thread_group();
	}

	data.physics_process_internal = p_process_internal;

	if (_is_any_processing()) {
		_add_to_process_thread_group();
	}
}

bool Node::is_physics_processing_internal() const {
	return data.physics_process_internal;
}

void Node::set_process_mode(ProcessMode p_mode) {
	ERR_THREAD_GUARD
	if (data.process_mode == p_mode) {
		return;
	}

	if (!is_inside_tree()) {
		data.process_mode = p_mode;
		return;
	}

	bool prev_can_process = can_process();
	bool prev_enabled = _is_enabled();

	if (p_mode == PROCESS_MODE_INHERIT) {
		if (data.parent) {
			data.process_owner = data.parent->data.process_owner;
		} else {
			ERR_FAIL_MSG("The root node can't be set to Inherit process mode.");
		}
	} else {
		data.process_owner = this;
	}

	data.process_mode = p_mode;

	bool next_can_process = can_process();
	bool next_enabled = _is_enabled();

	int pause_notification = 0;

	if (prev_can_process && !next_can_process) {
		pause_notification = NOTIFICATION_PAUSED;
	} else if (!prev_can_process && next_can_process) {
		pause_notification = NOTIFICATION_UNPAUSED;
	}

	int enabled_notification = 0;

	if (prev_enabled && !next_enabled) {
		enabled_notification = NOTIFICATION_DISABLED;
	} else if (!prev_enabled && next_enabled) {
		enabled_notification = NOTIFICATION_ENABLED;
	}

	_propagate_process_owner(data.process_owner, pause_notification, enabled_notification);

#ifdef TOOLS_ENABLED
	// This is required for the editor to update the visibility of disabled nodes
	// It's very expensive during runtime to change, so editor-only
	if (Engine::get_singleton()->is_editor_hint()) {
		get_tree()->emit_signal(SNAME("tree_process_mode_changed"));
	}

	_emit_editor_state_changed();
#endif
}

void Node::_propagate_pause_notification(bool p_enable) {
	bool prev_can_process = _can_process(!p_enable);
	bool next_can_process = _can_process(p_enable);

	if (prev_can_process && !next_can_process) {
		notification(NOTIFICATION_PAUSED);
	} else if (!prev_can_process && next_can_process) {
		notification(NOTIFICATION_UNPAUSED);
	}

	data.blocked++;
	for (KeyValue<StringName, Node *> &K : data.children) {
		K.value->_propagate_pause_notification(p_enable);
	}
	data.blocked--;
}

void Node::_propagate_suspend_notification(bool p_enable) {
	notification(p_enable ? NOTIFICATION_SUSPENDED : NOTIFICATION_UNSUSPENDED);

	data.blocked++;
	for (KeyValue<StringName, Node *> &KV : data.children) {
		KV.value->_propagate_suspend_notification(p_enable);
	}
	data.blocked--;
}

Node::ProcessMode Node::get_process_mode() const {
	return data.process_mode;
}

void Node::_propagate_process_owner(Node *p_owner, int p_pause_notification, int p_enabled_notification) {
	data.process_owner = p_owner;

	if (p_pause_notification != 0) {
		notification(p_pause_notification);
	}

	if (p_enabled_notification != 0) {
		notification(p_enabled_notification);
	}

	data.blocked++;
	for (KeyValue<StringName, Node *> &K : data.children) {
		Node *c = K.value;
		if (c->data.process_mode == PROCESS_MODE_INHERIT) {
			c->_propagate_process_owner(p_owner, p_pause_notification, p_enabled_notification);
		}
	}
	data.blocked--;
}

void Node::set_multiplayer_authority(int p_peer_id, bool p_recursive) {
	ERR_THREAD_GUARD
	data.multiplayer_authority = p_peer_id;

	if (p_recursive) {
		for (KeyValue<StringName, Node *> &K : data.children) {
			K.value->set_multiplayer_authority(p_peer_id, true);
		}
	}
}

int Node::get_multiplayer_authority() const {
	return data.multiplayer_authority;
}

bool Node::is_multiplayer_authority() const {
	ERR_FAIL_COND_V(!is_inside_tree(), false);

	Ref<MultiplayerAPI> api = get_multiplayer();
	return api.is_valid() && (api->get_unique_id() == data.multiplayer_authority);
}

/***** RPC CONFIG ********/

void Node::rpc_config(const StringName &p_method, const Variant &p_config) {
	ERR_THREAD_GUARD
	if (data.rpc_config.get_type() != Variant::DICTIONARY) {
		data.rpc_config = Dictionary();
	}
	Dictionary node_config = data.rpc_config;
	if (p_config.get_type() == Variant::NIL) {
		node_config.erase(p_method);
	} else {
		ERR_FAIL_COND(p_config.get_type() != Variant::DICTIONARY);
		node_config[p_method] = p_config;
	}
}

Variant Node::get_rpc_config() const {
	return data.rpc_config;
}

/***** RPC FUNCTIONS ********/

Error Node::_rpc_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	if (p_argcount < 1) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = 1;
		return ERR_INVALID_PARAMETER;
	}

	if (!p_args[0]->is_string()) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 0;
		r_error.expected = Variant::STRING_NAME;
		return ERR_INVALID_PARAMETER;
	}

	StringName method = (*p_args[0]).operator StringName();

	Error err = rpcp(0, method, &p_args[1], p_argcount - 1);
	r_error.error = Callable::CallError::CALL_OK;
	return err;
}

Error Node::_rpc_id_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	if (p_argcount < 2) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = 2;
		return ERR_INVALID_PARAMETER;
	}

	if (p_args[0]->get_type() != Variant::INT) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 0;
		r_error.expected = Variant::INT;
		return ERR_INVALID_PARAMETER;
	}

	if (!p_args[1]->is_string()) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 1;
		r_error.expected = Variant::STRING_NAME;
		return ERR_INVALID_PARAMETER;
	}

	int peer_id = *p_args[0];
	StringName method = (*p_args[1]).operator StringName();

	Error err = rpcp(peer_id, method, &p_args[2], p_argcount - 2);
	r_error.error = Callable::CallError::CALL_OK;
	return err;
}

Error Node::rpcp(int p_peer_id, const StringName &p_method, const Variant **p_arg, int p_argcount) {
	ERR_FAIL_COND_V(!is_inside_tree(), ERR_UNCONFIGURED);

	Ref<MultiplayerAPI> api = get_multiplayer();
	if (api.is_null()) {
		return ERR_UNCONFIGURED;
	}
	return api->rpcp(this, p_peer_id, p_method, p_arg, p_argcount);
}

Ref<MultiplayerAPI> Node::get_multiplayer() const {
	if (!is_inside_tree()) {
		return Ref<MultiplayerAPI>();
	}
	return get_tree()->get_multiplayer(get_path());
}

//////////// end of rpc

bool Node::can_process_notification(int p_what) const {
	switch (p_what) {
		case NOTIFICATION_PHYSICS_PROCESS:
			return data.physics_process;
		case NOTIFICATION_PROCESS:
			return data.process;
		case NOTIFICATION_INTERNAL_PROCESS:
			return data.process_internal;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS:
			return data.physics_process_internal;
	}

	return true;
}

bool Node::can_process() const {
	ERR_FAIL_COND_V(!is_inside_tree(), false);
	return !get_tree()->is_suspended() && _can_process(get_tree()->is_paused());
}

bool Node::_can_process(bool p_paused) const {
	ProcessMode process_mode;

	if (data.process_mode == PROCESS_MODE_INHERIT) {
		if (!data.process_owner) {
			process_mode = PROCESS_MODE_PAUSABLE;
		} else {
			process_mode = data.process_owner->data.process_mode;
		}
	} else {
		process_mode = data.process_mode;
	}

	// The owner can't be set to inherit, must be a bug.
	ERR_FAIL_COND_V(process_mode == PROCESS_MODE_INHERIT, false);

	if (process_mode == PROCESS_MODE_DISABLED) {
		return false;
	} else if (process_mode == PROCESS_MODE_ALWAYS) {
		return true;
	}

	if (p_paused) {
		return process_mode == PROCESS_MODE_WHEN_PAUSED;
	} else {
		return process_mode == PROCESS_MODE_PAUSABLE;
	}
}

void Node::set_physics_interpolation_mode(PhysicsInterpolationMode p_mode) {
	ERR_THREAD_GUARD
	if (data.physics_interpolation_mode == p_mode) {
		return;
	}

	data.physics_interpolation_mode = p_mode;

	bool interpolate = true; // Default for root node.

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

	// If swapping from interpolated to non-interpolated, use this as an extra means to cause a reset.
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

bool Node::_is_enabled() const {
	ProcessMode process_mode;

	if (data.process_mode == PROCESS_MODE_INHERIT) {
		if (!data.process_owner) {
			process_mode = PROCESS_MODE_PAUSABLE;
		} else {
			process_mode = data.process_owner->data.process_mode;
		}
	} else {
		process_mode = data.process_mode;
	}

	return (process_mode != PROCESS_MODE_DISABLED);
}

bool Node::is_enabled() const {
	ERR_FAIL_COND_V(!is_inside_tree(), false);
	return _is_enabled();
}

double Node::get_physics_process_delta_time() const {
	if (data.tree) {
		return data.tree->get_physics_process_time();
	} else {
		return 0;
	}
}

double Node::get_process_delta_time() const {
	if (data.tree) {
		return data.tree->get_process_time();
	} else {
		return 0;
	}
}

void Node::set_process(bool p_process) {
	ERR_THREAD_GUARD
	if (data.process == p_process) {
		return;
	}

	if (!is_inside_tree()) {
		data.process = p_process;
		return;
	}

	if (_is_any_processing()) {
		_remove_from_process_thread_group();
	}

	data.process = p_process;

	if (_is_any_processing()) {
		_add_to_process_thread_group();
	}
}

bool Node::is_processing() const {
	return data.process;
}

void Node::set_process_internal(bool p_process_internal) {
	ERR_THREAD_GUARD
	if (data.process_internal == p_process_internal) {
		return;
	}

	if (!is_inside_tree()) {
		data.process_internal = p_process_internal;
		return;
	}

	if (_is_any_processing()) {
		_remove_from_process_thread_group();
	}

	data.process_internal = p_process_internal;

	if (_is_any_processing()) {
		_add_to_process_thread_group();
	}
}

void Node::_add_process_group() {
	get_tree()->_add_process_group(this);
}

void Node::_remove_process_group() {
	get_tree()->_remove_process_group(this);
}

void Node::_remove_from_process_thread_group() {
	get_tree()->_remove_node_from_process_group(this, data.process_thread_group_owner);
}

void Node::_add_to_process_thread_group() {
	get_tree()->_add_node_to_process_group(this, data.process_thread_group_owner);
}

void Node::_remove_tree_from_process_thread_group() {
	if (!is_inside_tree()) {
		return; // May not be initialized yet.
	}

	for (KeyValue<StringName, Node *> &K : data.children) {
		if (K.value->data.process_thread_group != PROCESS_THREAD_GROUP_INHERIT) {
			continue;
		}

		K.value->_remove_tree_from_process_thread_group();
	}

	if (_is_any_processing()) {
		_remove_from_process_thread_group();
	}
}

void Node::_add_tree_to_process_thread_group(Node *p_owner) {
	if (_is_any_processing()) {
		_add_to_process_thread_group();
	}

	data.process_thread_group_owner = p_owner;
	if (p_owner != nullptr) {
		data.process_group = p_owner->data.process_group;
	} else {
		data.process_group = &data.tree->default_process_group;
	}

	for (KeyValue<StringName, Node *> &K : data.children) {
		if (K.value->data.process_thread_group != PROCESS_THREAD_GROUP_INHERIT) {
			continue;
		}

		K.value->_add_to_process_thread_group();
	}
}
bool Node::is_processing_internal() const {
	return data.process_internal;
}

void Node::set_process_thread_group_order(int p_order) {
	ERR_THREAD_GUARD
	if (data.process_thread_group_order == p_order) {
		return;
	}

	data.process_thread_group_order = p_order;

	// Not yet in the tree (or not a group owner, in whose case this is pointless but harmless); trivial update.
	if (!is_inside_tree() || data.process_thread_group_owner != this) {
		return;
	}

	get_tree()->process_groups_dirty = true;
}

int Node::get_process_thread_group_order() const {
	return data.process_thread_group_order;
}

void Node::set_process_priority(int p_priority) {
	ERR_THREAD_GUARD
	if (data.process_priority == p_priority) {
		return;
	}
	if (!is_inside_tree()) {
		// Not yet in the tree; trivial update.
		data.process_priority = p_priority;
		return;
	}

	if (_is_any_processing()) {
		_remove_from_process_thread_group();
	}

	data.process_priority = p_priority;

	if (_is_any_processing()) {
		_add_to_process_thread_group();
	}
}

int Node::get_process_priority() const {
	return data.process_priority;
}

void Node::set_physics_process_priority(int p_priority) {
	ERR_THREAD_GUARD
	if (data.physics_process_priority == p_priority) {
		return;
	}
	if (!is_inside_tree()) {
		// Not yet in the tree; trivial update.
		data.physics_process_priority = p_priority;
		return;
	}

	if (_is_any_processing()) {
		_remove_from_process_thread_group();
	}

	data.physics_process_priority = p_priority;

	if (_is_any_processing()) {
		_add_to_process_thread_group();
	}
}

int Node::get_physics_process_priority() const {
	return data.physics_process_priority;
}

void Node::set_process_thread_group(ProcessThreadGroup p_mode) {
	ERR_FAIL_COND_MSG(data.inside_tree && !Thread::is_main_thread(), "Changing the process thread group can only be done from the main thread. Use call_deferred(\"set_process_thread_group\",mode).");
	if (data.process_thread_group == p_mode) {
		return;
	}

	if (!is_inside_tree()) {
		// Not yet in the tree; trivial update.
		data.process_thread_group = p_mode;
		return;
	}

	_remove_tree_from_process_thread_group();
	if (data.process_thread_group != PROCESS_THREAD_GROUP_INHERIT) {
		_remove_process_group();
	}

	data.process_thread_group = p_mode;

	if (p_mode == PROCESS_THREAD_GROUP_INHERIT) {
		if (data.parent) {
			data.process_thread_group_owner = data.parent->data.process_thread_group_owner;
		} else {
			data.process_thread_group_owner = nullptr;
		}
	} else {
		data.process_thread_group_owner = this;
		_add_process_group();
	}

	_add_tree_to_process_thread_group(data.process_thread_group_owner);

	notify_property_list_changed();
}

Node::ProcessThreadGroup Node::get_process_thread_group() const {
	return data.process_thread_group;
}

void Node::set_process_thread_messages(BitField<ProcessThreadMessages> p_flags) {
	ERR_THREAD_GUARD
	if (data.process_thread_messages == p_flags) {
		return;
	}

	data.process_thread_messages = p_flags;
}

BitField<Node::ProcessThreadMessages> Node::get_process_thread_messages() const {
	return data.process_thread_messages;
}

void Node::set_process_input(bool p_enable) {
	ERR_THREAD_GUARD
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

void Node::set_process_shortcut_input(bool p_enable) {
	ERR_THREAD_GUARD
	if (p_enable == data.shortcut_input) {
		return;
	}
	data.shortcut_input = p_enable;
	if (!is_inside_tree()) {
		return;
	}

	if (p_enable) {
		add_to_group("_vp_shortcut_input" + itos(get_viewport()->get_instance_id()));
	} else {
		remove_from_group("_vp_shortcut_input" + itos(get_viewport()->get_instance_id()));
	}
}

bool Node::is_processing_shortcut_input() const {
	return data.shortcut_input;
}

void Node::set_process_unhandled_input(bool p_enable) {
	ERR_THREAD_GUARD
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
	ERR_THREAD_GUARD
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

void Node::set_auto_translate_mode(AutoTranslateMode p_mode) {
	ERR_THREAD_GUARD
	if (data.auto_translate_mode == p_mode) {
		return;
	}

	if (p_mode == AUTO_TRANSLATE_MODE_INHERIT && data.inside_tree && !data.parent) {
		ERR_FAIL_MSG("The root node can't be set to Inherit auto translate mode.");
	}

	data.auto_translate_mode = p_mode;
	data.is_auto_translating = p_mode != AUTO_TRANSLATE_MODE_DISABLED;
	data.is_auto_translate_dirty = true;

	propagate_notification(NOTIFICATION_TRANSLATION_CHANGED);
}

Node::AutoTranslateMode Node::get_auto_translate_mode() const {
	return data.auto_translate_mode;
}

bool Node::can_auto_translate() const {
	ERR_READ_THREAD_GUARD_V(false);
	if (!data.is_auto_translate_dirty || data.auto_translate_mode != AUTO_TRANSLATE_MODE_INHERIT) {
		return data.is_auto_translating;
	}

	data.is_auto_translate_dirty = false;

	Node *parent = data.parent;
	while (parent) {
		if (parent->data.auto_translate_mode == AUTO_TRANSLATE_MODE_INHERIT) {
			parent = parent->data.parent;
			continue;
		}

		data.is_auto_translating = parent->data.auto_translate_mode == AUTO_TRANSLATE_MODE_ALWAYS;
		break;
	}

	return data.is_auto_translating;
}

StringName Node::get_translation_domain() const {
	ERR_READ_THREAD_GUARD_V(StringName());

	if (data.is_translation_domain_inherited && data.is_translation_domain_dirty) {
		const_cast<Node *>(this)->_translation_domain = data.parent ? data.parent->get_translation_domain() : StringName();
		data.is_translation_domain_dirty = false;
	}
	return _translation_domain;
}

void Node::set_translation_domain(const StringName &p_domain) {
	ERR_THREAD_GUARD

	if (!data.is_translation_domain_inherited && _translation_domain == p_domain) {
		return;
	}

	_translation_domain = p_domain;
	data.is_translation_domain_inherited = false;
	data.is_translation_domain_dirty = false;
	_propagate_translation_domain_dirty();
}

void Node::set_translation_domain_inherited() {
	ERR_THREAD_GUARD

	if (data.is_translation_domain_inherited) {
		return;
	}
	data.is_translation_domain_inherited = true;
	data.is_translation_domain_dirty = true;
	_propagate_translation_domain_dirty();
}

void Node::_propagate_translation_domain_dirty() {
	for (KeyValue<StringName, Node *> &K : data.children) {
		Node *child = K.value;
		if (child->data.is_translation_domain_inherited) {
			child->data.is_translation_domain_dirty = true;
			child->_propagate_translation_domain_dirty();
		}
	}
	notification(NOTIFICATION_TRANSLATION_CHANGED);
}

StringName Node::get_name() const {
	return data.name;
}

void Node::_set_name_nocheck(const StringName &p_name) {
	data.name = p_name;
}

void Node::set_name(const String &p_name) {
	ERR_FAIL_COND_MSG(data.inside_tree && !Thread::is_main_thread(), "Changing the name to nodes inside the SceneTree is only allowed from the main thread. Use `set_name.call_deferred(new_name)`.");
	String name = p_name.validate_node_name();

	ERR_FAIL_COND(name.is_empty());

	if (data.unique_name_in_owner && data.owner) {
		_release_unique_name_in_owner();
	}
	String old_name = data.name;
	data.name = name;

	if (data.parent) {
		data.parent->_validate_child_name(this, true);
		bool success = data.parent->data.children.replace_key(old_name, data.name);
		ERR_FAIL_COND_MSG(!success, "Renaming child in hashtable failed, this is a bug.");
	}

	if (data.unique_name_in_owner && data.owner) {
		_acquire_unique_name_in_owner();
	}

	propagate_notification(NOTIFICATION_PATH_RENAMED);

	if (is_inside_tree()) {
		emit_signal(SNAME("renamed"));
		get_tree()->node_renamed(this);
		get_tree()->tree_changed();
	}
}

// Returns a clear description of this node depending on what is available. Useful for error messages.
String Node::get_description() const {
	String description;
	if (is_inside_tree()) {
		description = get_path();
	} else {
		description = get_name();
		if (description.is_empty()) {
			description = get_class();
		}
	}
	return description;
}

static SafeRefCount node_hrcr_count;

void Node::init_node_hrcr() {
	node_hrcr_count.init(1);
}

#ifdef TOOLS_ENABLED
String Node::validate_child_name(Node *p_child) {
	StringName name = p_child->data.name;
	_generate_serial_child_name(p_child, name);
	return name;
}

String Node::prevalidate_child_name(Node *p_child, StringName p_name) {
	_generate_serial_child_name(p_child, p_name);
	return p_name;
}
#endif

String Node::adjust_name_casing(const String &p_name) {
	switch (GLOBAL_GET("editor/naming/node_name_casing").operator int()) {
		case NAME_CASING_PASCAL_CASE:
			return p_name.to_pascal_case();
		case NAME_CASING_CAMEL_CASE:
			return p_name.to_camel_case();
		case NAME_CASING_SNAKE_CASE:
			return p_name.to_snake_case();
	}
	return p_name;
}

void Node::_validate_child_name(Node *p_child, bool p_force_human_readable) {
	/* Make sure the name is unique */

	if (p_force_human_readable) {
		//this approach to autoset node names is human readable but very slow

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
			const Node *const *existing = data.children.getptr(p_child->data.name);
			unique = !existing || *existing == p_child;
		}

		if (!unique) {
			ERR_FAIL_COND(!node_hrcr_count.ref());
			// Optimized version of the code below:
			// String name = "@" + String(p_child->get_name()) + "@" + itos(node_hrcr_count.get());
			uint32_t c = node_hrcr_count.get();
			String cn = p_child->get_class_name().operator String();
			const char32_t *cn_ptr = cn.ptr();
			uint32_t cn_length = cn.length();
			uint32_t c_chars = String::num_characters(c);
			uint32_t len = 2 + cn_length + c_chars;
			char32_t *str = (char32_t *)alloca(sizeof(char32_t) * (len + 1));
			uint32_t idx = 0;
			str[idx++] = '@';
			for (uint32_t i = 0; i < cn_length; i++) {
				str[idx++] = cn_ptr[i];
			}
			str[idx++] = '@';
			idx += c_chars;
			ERR_FAIL_COND(idx != len);
			str[idx] = 0;
			while (c) {
				str[--idx] = '0' + (c % 10);
				c /= 10;
			}
			p_child->data.name = String(str);
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
		char32_t n = s[i];
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
		// No name and a new name is needed, create one.

		name = p_child->get_class();
	}

	const Node *const *existing = data.children.getptr(name);
	if (!existing || *existing == p_child) { // Unused, or is current node.
		return;
	}

	// Extract trailing number
	String name_string = name;
	String nums;
	for (int i = name_string.length() - 1; i >= 0; i--) {
		char32_t n = name_string[i];
		if (is_digit(n)) {
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

		existing = data.children.getptr(attempt);
		bool exists = existing != nullptr && *existing != p_child;

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

Node::InternalMode Node::get_internal_mode() const {
	return data.internal_mode;
}

void Node::_add_child_nocheck(Node *p_child, const StringName &p_name, InternalMode p_internal_mode) {
	//add a child node quickly, without name validation

	p_child->data.name = p_name;
	data.children.insert(p_name, p_child);

	p_child->data.internal_mode = p_internal_mode;
	switch (p_internal_mode) {
		case INTERNAL_MODE_FRONT: {
			p_child->data.index = data.internal_children_front_count_cache++;
		} break;
		case INTERNAL_MODE_BACK: {
			p_child->data.index = data.internal_children_back_count_cache++;
		} break;
		case INTERNAL_MODE_DISABLED: {
			p_child->data.index = data.external_children_count_cache++;
		} break;
	}

	p_child->data.parent = this;

	if (!data.children_cache_dirty && p_internal_mode == INTERNAL_MODE_DISABLED && data.internal_children_back_count_cache == 0) {
		// Special case, also add to the cached children array since its cheap.
		data.children_cache.push_back(p_child);
	} else {
		data.children_cache_dirty = true;
	}

	p_child->notification(NOTIFICATION_PARENTED);

	if (data.tree) {
		p_child->_set_tree(data.tree);
	}

	/* Notify */
	//recognize children created in this node constructor
	p_child->data.parent_owned = data.in_constructor;
	add_child_notify(p_child);
	notification(NOTIFICATION_CHILD_ORDER_CHANGED);
	emit_signal(SNAME("child_order_changed"));
}

void Node::add_child(Node *p_child, bool p_force_readable_name, InternalMode p_internal) {
	ERR_FAIL_COND_MSG(data.inside_tree && !Thread::is_main_thread(), "Adding children to a node inside the SceneTree is only allowed from the main thread. Use call_deferred(\"add_child\",node).");

	ERR_THREAD_GUARD
	ERR_FAIL_NULL(p_child);
	ERR_FAIL_COND_MSG(p_child == this, vformat("Can't add child '%s' to itself.", p_child->get_name())); // adding to itself!
	ERR_FAIL_COND_MSG(p_child->data.parent, vformat("Can't add child '%s' to '%s', already has a parent '%s'.", p_child->get_name(), get_name(), p_child->data.parent->get_name())); //Fail if node has a parent
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND_MSG(p_child->is_ancestor_of(this), vformat("Can't add child '%s' to '%s' as it would result in a cyclic dependency since '%s' is already a parent of '%s'.", p_child->get_name(), get_name(), p_child->get_name(), get_name()));
#endif
	ERR_FAIL_COND_MSG(data.blocked > 0, "Parent node is busy setting up children, `add_child()` failed. Consider using `add_child.call_deferred(child)` instead.");

	_validate_child_name(p_child, p_force_readable_name);

#ifdef DEBUG_ENABLED
	if (p_child->data.owner && !p_child->data.owner->is_ancestor_of(p_child)) {
		// Owner of p_child should be ancestor of p_child.
		WARN_PRINT(vformat("Adding '%s' as child to '%s' will make owner '%s' inconsistent. Consider unsetting the owner beforehand.", p_child->get_name(), get_name(), p_child->data.owner->get_name()));
	}
#endif // DEBUG_ENABLED

	_add_child_nocheck(p_child, p_child->data.name, p_internal);
}

void Node::add_sibling(Node *p_sibling, bool p_force_readable_name) {
	ERR_FAIL_COND_MSG(data.inside_tree && !Thread::is_main_thread(), "Adding a sibling to a node inside the SceneTree is only allowed from the main thread. Use call_deferred(\"add_sibling\",node).");
	ERR_FAIL_NULL(p_sibling);
	ERR_FAIL_COND_MSG(p_sibling == this, vformat("Can't add sibling '%s' to itself.", p_sibling->get_name())); // adding to itself!
	ERR_FAIL_NULL(data.parent);
	ERR_FAIL_COND_MSG(data.parent->data.blocked > 0, "Parent node is busy setting up children, `add_sibling()` failed. Consider using `add_sibling.call_deferred(sibling)` instead.");

	data.parent->add_child(p_sibling, p_force_readable_name, data.internal_mode);
	data.parent->_update_children_cache();
	data.parent->_move_child(p_sibling, get_index() + 1);
}

void Node::remove_child(Node *p_child) {
	ERR_FAIL_COND_MSG(data.inside_tree && !Thread::is_main_thread(), "Removing children from a node inside the SceneTree is only allowed from the main thread. Use call_deferred(\"remove_child\",node).");
	ERR_FAIL_NULL(p_child);
	ERR_FAIL_COND_MSG(data.blocked > 0, "Parent node is busy adding/removing children, `remove_child()` can't be called at this time. Consider using `remove_child.call_deferred(child)` instead.");
	ERR_FAIL_COND(p_child->data.parent != this);

	/**
	 *  Do not change the data.internal_children*cache counters here.
	 *  Because if nodes are re-added, the indices can remain
	 *  greater-than-everything indices and children added remain
	 *  properly ordered.
	 *
	 *  All children indices and counters will be updated next time the
	 *  cache is re-generated.
	 */

	data.blocked++;
	p_child->_set_tree(nullptr);
	//}

	remove_child_notify(p_child);
	p_child->notification(NOTIFICATION_UNPARENTED);

	data.blocked--;

	data.children_cache_dirty = true;
	bool success = data.children.erase(p_child->data.name);
	ERR_FAIL_COND_MSG(!success, "Children name does not match parent name in hashtable, this is a bug.");

	p_child->data.parent = nullptr;
	p_child->data.index = -1;

	notification(NOTIFICATION_CHILD_ORDER_CHANGED);
	emit_signal(SNAME("child_order_changed"));

	if (data.inside_tree) {
		p_child->_propagate_after_exit_tree();
	}
}

void Node::_update_children_cache_impl() const {
	// Assign children
	data.children_cache.resize(data.children.size());
	int idx = 0;
	for (const KeyValue<StringName, Node *> &K : data.children) {
		data.children_cache[idx] = K.value;
		idx++;
	}
	// Sort them
	data.children_cache.sort_custom<ComparatorByIndex>();
	// Update indices
	data.external_children_count_cache = 0;
	data.internal_children_back_count_cache = 0;
	data.internal_children_front_count_cache = 0;

	for (uint32_t i = 0; i < data.children_cache.size(); i++) {
		switch (data.children_cache[i]->data.internal_mode) {
			case INTERNAL_MODE_DISABLED: {
				data.children_cache[i]->data.index = data.external_children_count_cache++;
			} break;
			case INTERNAL_MODE_FRONT: {
				data.children_cache[i]->data.index = data.internal_children_front_count_cache++;
			} break;
			case INTERNAL_MODE_BACK: {
				data.children_cache[i]->data.index = data.internal_children_back_count_cache++;
			} break;
		}
	}
	data.children_cache_dirty = false;
}

int Node::get_child_count(bool p_include_internal) const {
	ERR_THREAD_GUARD_V(0);
	_update_children_cache();

	if (p_include_internal) {
		return data.children_cache.size();
	} else {
		return data.children_cache.size() - data.internal_children_front_count_cache - data.internal_children_back_count_cache;
	}
}

Node *Node::get_child(int p_index, bool p_include_internal) const {
	ERR_THREAD_GUARD_V(nullptr);
	_update_children_cache();

	if (p_include_internal) {
		if (p_index < 0) {
			p_index += data.children_cache.size();
		}
		ERR_FAIL_INDEX_V(p_index, (int)data.children_cache.size(), nullptr);
		return data.children_cache[p_index];
	} else {
		if (p_index < 0) {
			p_index += (int)data.children_cache.size() - data.internal_children_front_count_cache - data.internal_children_back_count_cache;
		}
		ERR_FAIL_INDEX_V(p_index, (int)data.children_cache.size() - data.internal_children_front_count_cache - data.internal_children_back_count_cache, nullptr);
		p_index += data.internal_children_front_count_cache;
		return data.children_cache[p_index];
	}
}

TypedArray<Node> Node::get_children(bool p_include_internal) const {
	ERR_THREAD_GUARD_V(TypedArray<Node>());
	TypedArray<Node> arr;
	int cc = get_child_count(p_include_internal);
	arr.resize(cc);
	for (int i = 0; i < cc; i++) {
		arr[i] = get_child(i, p_include_internal);
	}

	return arr;
}

Node *Node::_get_child_by_name(const StringName &p_name) const {
	const Node *const *node = data.children.getptr(p_name);
	if (node) {
		return const_cast<Node *>(*node);
	} else {
		return nullptr;
	}
}

Node *Node::get_node_or_null(const NodePath &p_path) const {
	ERR_THREAD_GUARD_V(nullptr);
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

		if (name == SNAME(".")) {
			next = current;

		} else if (name == SNAME("..")) {
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
			const Node *const *node = current->data.children.getptr(name);
			if (node) {
				next = const_cast<Node *>(*node);
			} else {
				return nullptr;
			}
		}
		current = next;
	}

	return current;
}

Node *Node::get_node(const NodePath &p_path) const {
	Node *node = get_node_or_null(p_path);

	if (unlikely(!node)) {
		const String desc = get_description();
		if (p_path.is_absolute()) {
			ERR_FAIL_V_MSG(nullptr,
					vformat(R"(Node not found: "%s" (absolute path attempted from "%s").)", p_path, desc));
		} else {
			ERR_FAIL_V_MSG(nullptr,
					vformat(R"(Node not found: "%s" (relative to "%s").)", p_path, desc));
		}
	}

	return node;
}

bool Node::has_node(const NodePath &p_path) const {
	return get_node_or_null(p_path) != nullptr;
}

// Finds the first child node (in tree order) whose name matches the given pattern.
// Can be recursive or not, and limited to owned nodes.
Node *Node::find_child(const String &p_pattern, bool p_recursive, bool p_owned) const {
	ERR_THREAD_GUARD_V(nullptr);
	ERR_FAIL_COND_V(p_pattern.is_empty(), nullptr);
	_update_children_cache();
	Node *const *cptr = data.children_cache.ptr();
	int ccount = data.children_cache.size();
	for (int i = 0; i < ccount; i++) {
		if (p_owned && !cptr[i]->data.owner) {
			continue;
		}
		if (cptr[i]->data.name.operator String().match(p_pattern)) {
			return cptr[i];
		}

		if (!p_recursive) {
			continue;
		}

		Node *ret = cptr[i]->find_child(p_pattern, true, p_owned);
		if (ret) {
			return ret;
		}
	}
	return nullptr;
}

// Finds child nodes based on their name using pattern matching, or class name,
// or both (either pattern or type can be left empty).
// Can be recursive or not, and limited to owned nodes.
TypedArray<Node> Node::find_children(const String &p_pattern, const String &p_type, bool p_recursive, bool p_owned) const {
	ERR_THREAD_GUARD_V(TypedArray<Node>());
	TypedArray<Node> ret;
	ERR_FAIL_COND_V(p_pattern.is_empty() && p_type.is_empty(), ret);
	_update_children_cache();
	Node *const *cptr = data.children_cache.ptr();
	int ccount = data.children_cache.size();
	for (int i = 0; i < ccount; i++) {
		if (p_owned && !cptr[i]->data.owner) {
			continue;
		}

		if (p_pattern.is_empty() || cptr[i]->data.name.operator String().match(p_pattern)) {
			if (p_type.is_empty() || cptr[i]->is_class(p_type)) {
				ret.append(cptr[i]);
			} else if (cptr[i]->get_script_instance()) {
				Ref<Script> scr = cptr[i]->get_script_instance()->get_script();
				while (scr.is_valid()) {
					if ((ScriptServer::is_global_class(p_type) && ScriptServer::get_global_class_path(p_type) == scr->get_path()) || p_type == scr->get_path()) {
						ret.append(cptr[i]);
						break;
					}

					scr = scr->get_base_script();
				}
			}
		}

		if (p_recursive) {
			ret.append_array(cptr[i]->find_children(p_pattern, p_type, true, p_owned));
		}
	}

	return ret;
}

void Node::reparent(Node *p_parent, bool p_keep_global_transform) {
	ERR_THREAD_GUARD
	ERR_FAIL_NULL(p_parent);
	ERR_FAIL_NULL_MSG(data.parent, "Node needs a parent to be reparented.");

	if (p_parent == data.parent) {
		return;
	}

	bool preserve_owner = data.owner && (data.owner == p_parent || data.owner->is_ancestor_of(p_parent));
	Node *owner_temp = data.owner;
	LocalVector<Node *> common_parents;

	// If the new parent is related to the owner, find all children of the reparented node who have the same owner so that we can reassign them.
	if (preserve_owner) {
		LocalVector<Node *> to_visit;

		to_visit.push_back(this);
		common_parents.push_back(this);

		while (to_visit.size() > 0) {
			Node *check = to_visit[to_visit.size() - 1];
			to_visit.resize(to_visit.size() - 1);

			for (int i = 0; i < check->get_child_count(false); i++) {
				Node *child = check->get_child(i, false);
				to_visit.push_back(child);
				if (child->data.owner == owner_temp) {
					common_parents.push_back(child);
				}
			}
		}
	}

	data.parent->remove_child(this);
	p_parent->add_child(this);

	// Reassign the old owner to those found nodes.
	if (preserve_owner) {
		for (Node *E : common_parents) {
			E->set_owner(owner_temp);
		}
	}
}

Node *Node::get_parent() const {
	return data.parent;
}

Node *Node::find_parent(const String &p_pattern) const {
	ERR_THREAD_GUARD_V(nullptr);
	Node *p = data.parent;
	while (p) {
		if (p->data.name.operator String().match(p_pattern)) {
			return p;
		}
		p = p->data.parent;
	}

	return nullptr;
}

Window *Node::get_window() const {
	ERR_THREAD_GUARD_V(nullptr);
	Viewport *vp = get_viewport();
	if (vp) {
		return vp->get_base_window();
	}
	return nullptr;
}

Window *Node::get_last_exclusive_window() const {
	Window *w = get_window();
	while (w && w->get_exclusive_child()) {
		w = w->get_exclusive_child();
	}

	return w;
}

bool Node::is_ancestor_of(const Node *p_node) const {
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

	_update_children_cache();

	int *this_stack = (int *)alloca(sizeof(int) * data.depth);
	int *that_stack = (int *)alloca(sizeof(int) * p_node->data.depth);

	const Node *n = this;

	int idx = data.depth - 1;
	while (n) {
		ERR_FAIL_INDEX_V(idx, data.depth, false);
		this_stack[idx--] = n->get_index();
		n = n->data.parent;
	}

	ERR_FAIL_COND_V(idx != -1, false);
	n = p_node;
	idx = p_node->data.depth - 1;
	while (n) {
		ERR_FAIL_INDEX_V(idx, p_node->data.depth, false);
		that_stack[idx--] = n->get_index();

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

	for (KeyValue<StringName, Node *> &K : data.children) {
		K.value->get_owned_by(p_by, p_owned);
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
	ERR_FAIL_NULL(data.owner); // Safety check.
	StringName key = StringName(UNIQUE_NODE_PREFIX + data.name.operator String());
	Node **which = data.owner->data.owned_unique_nodes.getptr(key);
	if (which == nullptr || *which != this) {
		return; // Ignore.
	}
	data.owner->data.owned_unique_nodes.erase(key);
}

void Node::_acquire_unique_name_in_owner() {
	ERR_FAIL_NULL(data.owner); // Safety check.
	StringName key = StringName(UNIQUE_NODE_PREFIX + data.name.operator String());
	Node **which = data.owner->data.owned_unique_nodes.getptr(key);
	if (which != nullptr && *which != this) {
		String which_path = is_inside_tree() ? (*which)->get_path() : data.owner->get_path_to(*which);
		WARN_PRINT(vformat("Setting node name '%s' to be unique within scene for '%s', but it's already claimed by '%s'.\n'%s' is no longer set as having a unique name.",
				get_name(), is_inside_tree() ? get_path() : data.owner->get_path_to(this), which_path, which_path));
		data.unique_name_in_owner = false;
		return;
	}
	data.owner->data.owned_unique_nodes[key] = this;
}

void Node::set_unique_name_in_owner(bool p_enabled) {
	ERR_MAIN_THREAD_GUARD
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

	update_configuration_warnings();
	_emit_editor_state_changed();
}

bool Node::is_unique_name_in_owner() const {
	return data.unique_name_in_owner;
}

void Node::set_owner(Node *p_owner) {
	ERR_MAIN_THREAD_GUARD
	if (data.owner) {
		_clean_up_owner();
	}

	ERR_FAIL_COND(p_owner == this);

	if (!p_owner) {
		return;
	}

	Node *check = get_parent();
	bool owner_valid = false;

	while (check) {
		if (check == p_owner) {
			owner_valid = true;
			break;
		}

		check = check->data.parent;
	}

	ERR_FAIL_COND_MSG(!owner_valid, "Invalid owner. Owner must be an ancestor in the tree.");

	_set_owner_nocheck(p_owner);

	if (data.unique_name_in_owner) {
		_acquire_unique_name_in_owner();
	}

	_emit_editor_state_changed();
}

Node *Node::get_owner() const {
	return data.owner;
}

void Node::_clean_up_owner() {
	ERR_FAIL_NULL(data.owner); // Safety check.

	if (data.unique_name_in_owner) {
		_release_unique_name_in_owner();
	}
	data.owner->data.owned.erase(data.OW);
	data.owner = nullptr;
	data.OW = nullptr;
}

Node *Node::find_common_parent_with(const Node *p_node) const {
	if (this == p_node) {
		return const_cast<Node *>(p_node);
	}

	HashSet<const Node *> visited;

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

NodePath Node::get_path_to(const Node *p_node, bool p_use_unique_path) const {
	ERR_FAIL_NULL_V(p_node, NodePath());

	if (this == p_node) {
		return NodePath(".");
	}

	HashSet<const Node *> visited;

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

	ERR_FAIL_NULL_V(common_parent, NodePath()); //nodes not in the same tree

	visited.clear();

	Vector<StringName> path;
	StringName up = String("..");

	if (p_use_unique_path) {
		n = p_node;

		bool is_detected = false;
		while (n != common_parent) {
			if (n->is_unique_name_in_owner() && n->get_owner() == get_owner()) {
				path.push_back(UNIQUE_NODE_PREFIX + String(n->get_name()));
				is_detected = true;
				break;
			}
			path.push_back(n->get_name());
			n = n->data.parent;
		}

		if (!is_detected) {
			n = this;

			String detected_name;
			int up_count = 0;
			while (n != common_parent) {
				if (n->is_unique_name_in_owner() && n->get_owner() == get_owner()) {
					detected_name = n->get_name();
					up_count = 0;
				}
				up_count++;
				n = n->data.parent;
			}

			for (int i = 0; i < up_count; i++) {
				path.push_back(up);
			}

			if (!detected_name.is_empty()) {
				path.push_back(UNIQUE_NODE_PREFIX + detected_name);
			}
		}
	} else {
		n = p_node;

		while (n != common_parent) {
			path.push_back(n->get_name());
			n = n->data.parent;
		}

		n = this;

		while (n != common_parent) {
			path.push_back(up);
			n = n->data.parent;
		}
	}

	path.reverse();

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

	path.reverse();

	data.path_cache = memnew(NodePath(path, true));

	return *data.path_cache;
}

bool Node::is_in_group(const StringName &p_identifier) const {
	ERR_THREAD_GUARD_V(false);
	return data.grouped.has(p_identifier);
}

void Node::add_to_group(const StringName &p_identifier, bool p_persistent) {
	ERR_THREAD_GUARD
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
	if (p_persistent) {
		_emit_editor_state_changed();
	}
}

void Node::remove_from_group(const StringName &p_identifier) {
	ERR_THREAD_GUARD
	HashMap<StringName, GroupData>::Iterator E = data.grouped.find(p_identifier);

	if (!E) {
		return;
	}

#ifdef TOOLS_ENABLED
	bool persistent = E->value.persistent;
#endif

	if (data.tree) {
		data.tree->remove_from_group(E->key, this);
	}

	data.grouped.remove(E);

#ifdef TOOLS_ENABLED
	if (persistent) {
		_emit_editor_state_changed();
	}
#endif
}

TypedArray<StringName> Node::_get_groups() const {
	TypedArray<StringName> groups;
	List<GroupInfo> gi;
	get_groups(&gi);
	for (const GroupInfo &E : gi) {
		groups.push_back(E.name);
	}

	return groups;
}

void Node::get_groups(List<GroupInfo> *p_groups) const {
	ERR_THREAD_GUARD
	for (const KeyValue<StringName, GroupData> &E : data.grouped) {
		GroupInfo gi;
		gi.name = E.key;
		gi.persistent = E.value.persistent;
		p_groups->push_back(gi);
	}
}

int Node::get_persistent_group_count() const {
	ERR_THREAD_GUARD_V(0);
	int count = 0;

	for (const KeyValue<StringName, GroupData> &E : data.grouped) {
		if (E.value.persistent) {
			count += 1;
		}
	}

	return count;
}

void Node::print_tree_pretty() {
	print_line(_get_tree_string_pretty("", true));
}

void Node::print_tree() {
	print_line(_get_tree_string(this));
}

String Node::_get_tree_string_pretty(const String &p_prefix, bool p_last) {
	String new_prefix = p_last ? String::utf8(" ┖╴") : String::utf8(" ┠╴");
	_update_children_cache();
	String return_tree = p_prefix + new_prefix + String(get_name()) + "\n";
	for (uint32_t i = 0; i < data.children_cache.size(); i++) {
		new_prefix = p_last ? String::utf8("   ") : String::utf8(" ┃ ");
		return_tree += data.children_cache[i]->_get_tree_string_pretty(p_prefix + new_prefix, i == data.children_cache.size() - 1);
	}
	return return_tree;
}

String Node::get_tree_string_pretty() {
	return _get_tree_string_pretty("", true);
}

String Node::_get_tree_string(const Node *p_node) {
	_update_children_cache();
	String return_tree = String(p_node->get_path_to(this)) + "\n";
	for (uint32_t i = 0; i < data.children_cache.size(); i++) {
		return_tree += data.children_cache[i]->_get_tree_string(p_node);
	}
	return return_tree;
}

String Node::get_tree_string() {
	return _get_tree_string(this);
}

void Node::_propagate_reverse_notification(int p_notification) {
	data.blocked++;

	for (HashMap<StringName, Node *>::Iterator I = data.children.last(); I; --I) {
		I->value->_propagate_reverse_notification(p_notification);
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

	for (KeyValue<StringName, Node *> &K : data.children) {
		K.value->_propagate_deferred_notification(p_notification, p_reverse);
	}

	if (p_reverse) {
		MessageQueue::get_singleton()->push_notification(this, p_notification);
	}

	data.blocked--;
}

void Node::propagate_notification(int p_notification) {
	ERR_THREAD_GUARD
	data.blocked++;
	notification(p_notification);

	for (KeyValue<StringName, Node *> &K : data.children) {
		K.value->propagate_notification(p_notification);
	}
	data.blocked--;
}

void Node::propagate_call(const StringName &p_method, const Array &p_args, const bool p_parent_first) {
	ERR_THREAD_GUARD
	data.blocked++;

	if (p_parent_first && has_method(p_method)) {
		callv(p_method, p_args);
	}

	for (KeyValue<StringName, Node *> &K : data.children) {
		K.value->propagate_call(p_method, p_args, p_parent_first);
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
	for (KeyValue<StringName, Node *> &K : data.children) {
		K.value->_propagate_replace_owner(p_owner, p_by_owner);
	}
	data.blocked--;
}

Ref<Tween> Node::create_tween() {
	ERR_THREAD_GUARD_V(Ref<Tween>());

	SceneTree *tree = data.tree;
	if (!tree) {
		tree = SceneTree::get_singleton();
	}
	ERR_FAIL_NULL_V_MSG(tree, Ref<Tween>(), "No available SceneTree to create the Tween.");

	Ref<Tween> tween = tree->create_tween();
	tween->bind_node(this);
	return tween;
}

void Node::set_scene_file_path(const String &p_scene_file_path) {
	ERR_THREAD_GUARD
	data.scene_file_path = p_scene_file_path;
	_emit_editor_state_changed();
}

String Node::get_scene_file_path() const {
	return data.scene_file_path;
}

void Node::set_editor_description(const String &p_editor_description) {
	ERR_THREAD_GUARD
	if (data.editor_description == p_editor_description) {
		return;
	}

	data.editor_description = p_editor_description;
	emit_signal(SNAME("editor_description_changed"), this);
}

String Node::get_editor_description() const {
	return data.editor_description;
}

void Node::set_editable_instance(Node *p_node, bool p_editable) {
	ERR_THREAD_GUARD
	ERR_FAIL_NULL(p_node);
	ERR_FAIL_COND(!is_ancestor_of(p_node));
	if (!p_editable) {
		p_node->data.editable_instance = false;
		// Avoid this flag being needlessly saved;
		// also give more visual feedback if editable children are re-enabled
		set_display_folded(false);
	} else {
		p_node->data.editable_instance = true;
	}

	p_node->_emit_editor_state_changed();
}

bool Node::is_editable_instance(const Node *p_node) const {
	if (!p_node) {
		return false; // Easier, null is never editable. :)
	}
	ERR_FAIL_COND_V(!is_ancestor_of(p_node), false);
	return p_node->data.editable_instance;
}

Node *Node::get_deepest_editable_node(Node *p_start_node) const {
	ERR_THREAD_GUARD_V(nullptr);
	ERR_FAIL_NULL_V(p_start_node, nullptr);
	ERR_FAIL_COND_V(!is_ancestor_of(p_start_node), p_start_node);

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
void Node::set_property_pinned(const String &p_property, bool p_pinned) {
	ERR_THREAD_GUARD
	bool current_pinned = false;
	Array pinned = get_meta("_edit_pinned_properties_", Array());
	StringName psa = get_property_store_alias(p_property);
	current_pinned = pinned.has(psa);

	if (current_pinned != p_pinned) {
		if (p_pinned) {
			pinned.append(psa);
		} else {
			pinned.erase(psa);
		}
	}

	if (pinned.is_empty()) {
		remove_meta("_edit_pinned_properties_");
	} else {
		set_meta("_edit_pinned_properties_", pinned);
	}
}

bool Node::is_property_pinned(const StringName &p_property) const {
	Array pinned = get_meta("_edit_pinned_properties_", Array());
	StringName psa = get_property_store_alias(p_property);
	return pinned.has(psa);
}

StringName Node::get_property_store_alias(const StringName &p_property) const {
	return p_property;
}

bool Node::is_part_of_edited_scene() const {
	return Engine::get_singleton()->is_editor_hint() && is_inside_tree() && get_tree()->get_edited_scene_root() &&
			get_tree()->get_edited_scene_root()->get_parent()->is_ancestor_of(this);
}
#endif

void Node::get_storable_properties(HashSet<StringName> &r_storable_properties) const {
	ERR_THREAD_GUARD
	List<PropertyInfo> pi;
	get_property_list(&pi);
	for (List<PropertyInfo>::Element *E = pi.front(); E; E = E->next()) {
		if ((E->get().usage & PROPERTY_USAGE_STORAGE)) {
			r_storable_properties.insert(E->get().name);
		}
	}
}

String Node::to_string() {
	// Keep this method in sync with `Object::to_string`.
	ERR_THREAD_GUARD_V(String());
	if (get_script_instance()) {
		bool valid;
		String ret = get_script_instance()->to_string(&valid);
		if (valid) {
			return ret;
		}
	}
	if (_get_extension() && _get_extension()->to_string) {
		String ret;
		GDExtensionBool is_valid;
		_get_extension()->to_string(_get_extension_instance(), &is_valid, &ret);
		return ret;
	}
	return (get_name() ? String(get_name()) + ":" : "") + Object::to_string();
}

void Node::set_scene_instance_state(const Ref<SceneState> &p_state) {
	ERR_THREAD_GUARD
	data.instance_state = p_state;
}

Ref<SceneState> Node::get_scene_instance_state() const {
	return data.instance_state;
}

void Node::set_scene_inherited_state(const Ref<SceneState> &p_state) {
	ERR_THREAD_GUARD
	data.inherited_state = p_state;
	_emit_editor_state_changed();
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

Node *Node::_duplicate(int p_flags, HashMap<const Node *, Node *> *r_duplimap) const {
	ERR_THREAD_GUARD_V(nullptr);
	Node *node = nullptr;

	bool instantiated = false;

	if (Object::cast_to<InstancePlaceholder>(this)) {
		const InstancePlaceholder *ip = Object::cast_to<const InstancePlaceholder>(this);
		InstancePlaceholder *nip = memnew(InstancePlaceholder);
		nip->set_instance_path(ip->get_instance_path());
		node = nip;

	} else if ((p_flags & DUPLICATE_USE_INSTANTIATION) && !get_scene_file_path().is_empty()) {
		Ref<PackedScene> res = ResourceLoader::load(get_scene_file_path());
		ERR_FAIL_COND_V(res.is_null(), nullptr);
		PackedScene::GenEditState edit_state = PackedScene::GEN_EDIT_STATE_DISABLED;
#ifdef TOOLS_ENABLED
		if (p_flags & DUPLICATE_FROM_EDITOR) {
			edit_state = PackedScene::GEN_EDIT_STATE_INSTANCE;
		}
#endif
		node = res->instantiate(edit_state);
		ERR_FAIL_NULL_V(node, nullptr);
		node->set_scene_instance_load_placeholder(get_scene_instance_load_placeholder());

		instantiated = true;

	} else {
		Object *obj = ClassDB::instantiate(get_class());
		ERR_FAIL_NULL_V(obj, nullptr);
		node = Object::cast_to<Node>(obj);
		if (!node) {
			memdelete(obj);
		}
		ERR_FAIL_NULL_V(node, nullptr);
	}

	if (!get_scene_file_path().is_empty()) { //an instance
		node->set_scene_file_path(get_scene_file_path());
		node->data.editable_instance = data.editable_instance;
	}

	List<const Node *> hidden_roots;
	List<const Node *> node_tree;
	node_tree.push_front(this);

	if (instantiated) {
		// Since nodes in the instantiated hierarchy won't be duplicated explicitly, we need to make an inventory
		// of all the nodes in the tree of the instantiated scene in order to transfer the values of the properties

		Vector<const Node *> instance_roots;
		instance_roots.push_back(this);

		for (List<const Node *>::Element *N = node_tree.front(); N; N = N->next()) {
			for (int i = 0; i < N->get()->get_child_count(); ++i) {
				Node *descendant = N->get()->get_child(i);

				if (!descendant->get_owner()) {
					continue; // Internal nodes or nodes added by scripts.
				}

				// Skip nodes not really belonging to the instantiated hierarchy; they'll be processed normally later
				// but remember non-instantiated nodes that are hidden below instantiated ones
				if (!instance_roots.has(descendant->get_owner())) {
					if (descendant->get_parent() && descendant->get_parent() != this && descendant->data.owner != descendant->get_parent()) {
						hidden_roots.push_back(descendant);
					}
					continue;
				}

				node_tree.push_back(descendant);

				if (!descendant->get_scene_file_path().is_empty() && instance_roots.has(descendant->get_owner())) {
					instance_roots.push_back(descendant);
				}
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
		for (const GroupInfo &E : gi) {
#ifdef TOOLS_ENABLED
			if ((p_flags & DUPLICATE_FROM_EDITOR) && !E.persistent) {
				continue;
			}
#endif

			node->add_to_group(E.name, E.persistent);
		}
	}

	for (int i = 0; i < get_child_count(); i++) {
		if (get_child(i)->data.parent_owned) {
			continue;
		}
		if (instantiated && get_child(i)->data.owner == this) {
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

	for (const Node *&E : hidden_roots) {
		Node *parent = node->get_node(get_path_to(E->data.parent));
		if (!parent) {
			memdelete(node);
			return nullptr;
		}

		Node *dup = E->_duplicate(p_flags, r_duplimap);
		if (!dup) {
			memdelete(node);
			return nullptr;
		}

		parent->add_child(dup);
		int pos = E->get_index();

		if (pos < parent->get_child_count() - 1) {
			parent->move_child(dup, pos);
		}
	}
	return node;
}

Node *Node::duplicate(int p_flags) const {
	ERR_THREAD_GUARD_V(nullptr);
	Node *dupe = _duplicate(p_flags);

	ERR_FAIL_NULL_V_MSG(dupe, nullptr, "Failed to duplicate node.");

	_duplicate_properties(this, this, dupe, p_flags);

	if (p_flags & DUPLICATE_SIGNALS) {
		_duplicate_signals(this, dupe);
	}

	return dupe;
}

#ifdef TOOLS_ENABLED
Node *Node::duplicate_from_editor(HashMap<const Node *, Node *> &r_duplimap) const {
	return duplicate_from_editor(r_duplimap, HashMap<Ref<Resource>, Ref<Resource>>());
}

Node *Node::duplicate_from_editor(HashMap<const Node *, Node *> &r_duplimap, const HashMap<Ref<Resource>, Ref<Resource>> &p_resource_remap) const {
	int flags = DUPLICATE_SIGNALS | DUPLICATE_GROUPS | DUPLICATE_SCRIPTS | DUPLICATE_USE_INSTANTIATION | DUPLICATE_FROM_EDITOR;
	Node *dupe = _duplicate(flags, &r_duplimap);

	ERR_FAIL_NULL_V_MSG(dupe, nullptr, "Failed to duplicate node.");

	_duplicate_properties(this, this, dupe, flags);

	// This is used by SceneTreeDock's paste functionality. When pasting to foreign scene, resources are duplicated.
	if (!p_resource_remap.is_empty()) {
		remap_node_resources(dupe, p_resource_remap);
	}

	// Duplication of signals must happen after all the node descendants have been copied,
	// because re-targeting of connections from some descendant to another is not possible
	// if the emitter node comes later in tree order than the receiver
	_duplicate_signals(this, dupe);

	return dupe;
}

void Node::remap_node_resources(Node *p_node, const HashMap<Ref<Resource>, Ref<Resource>> &p_resource_remap) const {
	List<PropertyInfo> props;
	p_node->get_property_list(&props);

	for (const PropertyInfo &E : props) {
		if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}

		Variant v = p_node->get(E.name);
		if (v.is_ref_counted()) {
			Ref<Resource> res = v;
			if (res.is_valid()) {
				if (p_resource_remap.has(res)) {
					p_node->set(E.name, p_resource_remap[res]);
					remap_nested_resources(res, p_resource_remap);
				}
			}
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		remap_node_resources(p_node->get_child(i), p_resource_remap);
	}
}

void Node::remap_nested_resources(Ref<Resource> p_resource, const HashMap<Ref<Resource>, Ref<Resource>> &p_resource_remap) const {
	List<PropertyInfo> props;
	p_resource->get_property_list(&props);

	for (const PropertyInfo &E : props) {
		if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}

		Variant v = p_resource->get(E.name);
		if (v.is_ref_counted()) {
			Ref<Resource> res = v;
			if (res.is_valid()) {
				if (p_resource_remap.has(res)) {
					p_resource->set(E.name, p_resource_remap[res]);
					remap_nested_resources(res, p_resource_remap);
				}
			}
		}
	}
}

void Node::_emit_editor_state_changed() {
	// This is required for the SceneTreeEditor to properly keep track of when an update is needed.
	// This signal might be expensive and not needed for anything outside of the editor.
	if (Engine::get_singleton()->is_editor_hint()) {
		emit_signal(SNAME("editor_state_changed"));
	}
}
#endif

// Duplicate node's properties.
// This has to be called after nodes have been duplicated since there might be properties
// of type Node that can be updated properly only if duplicated node tree is complete.
void Node::_duplicate_properties(const Node *p_root, const Node *p_original, Node *p_copy, int p_flags) const {
	List<PropertyInfo> props;
	p_original->get_property_list(&props);
	const StringName &script_property_name = CoreStringName(script);
	if (p_flags & DUPLICATE_SCRIPTS) {
		bool is_valid = false;
		Variant scr = p_original->get(script_property_name, &is_valid);
		if (is_valid) {
			p_copy->set(script_property_name, scr);
		}
	}
	for (const PropertyInfo &E : props) {
		if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}
		const StringName name = E.name;

		if (name == script_property_name) {
			continue;
		}

		Variant value = p_original->get(name).duplicate(true);

		if (E.usage & PROPERTY_USAGE_ALWAYS_DUPLICATE) {
			Resource *res = Object::cast_to<Resource>(value);
			if (res) { // Duplicate only if it's a resource
				p_copy->set(name, res->duplicate());
			}
		} else {
			if (value.get_type() == Variant::OBJECT) {
				Node *property_node = Object::cast_to<Node>(value);
				Variant out_value = value;
				if (property_node && (p_root == property_node || p_root->is_ancestor_of(property_node))) {
					out_value = p_copy->get_node_or_null(p_original->get_path_to(property_node));
				}
				p_copy->set(name, out_value);
			} else if (value.get_type() == Variant::ARRAY) {
				Array arr = value;
				if (arr.get_typed_builtin() == Variant::OBJECT) {
					for (int i = 0; i < arr.size(); i++) {
						Node *property_node = Object::cast_to<Node>(arr[i]);
						if (property_node && (p_root == property_node || p_root->is_ancestor_of(property_node))) {
							arr[i] = p_copy->get_node_or_null(p_original->get_path_to(property_node));
						}
					}
				}
				p_copy->set(name, arr);
			} else {
				p_copy->set(name, value);
			}
		}
	}

	for (int i = 0; i < p_original->get_child_count(); i++) {
		Node *copy_child = p_copy->get_child(i);
		ERR_FAIL_NULL_MSG(copy_child, "Child node disappeared while duplicating.");
		_duplicate_properties(p_root, p_original->get_child(i), copy_child, p_flags);
	}
}

// Duplication of signals must happen after all the node descendants have been copied,
// because re-targeting of connections from some descendant to another is not possible
// if the emitter node comes later in tree order than the receiver
void Node::_duplicate_signals(const Node *p_original, Node *p_copy) const {
	if ((this != p_original) && !(p_original->is_ancestor_of(this))) {
		return;
	}

	List<const Node *> process_list;
	process_list.push_back(this);
	while (!process_list.is_empty()) {
		const Node *n = process_list.front()->get();
		process_list.pop_front();

		List<Connection> conns;
		n->get_all_signal_connections(&conns);

		for (const Connection &E : conns) {
			if (E.flags & CONNECT_PERSIST) {
				//user connected
				NodePath p = p_original->get_path_to(n);
				Node *copy = p_copy->get_node(p);

				Node *target = Object::cast_to<Node>(E.callable.get_object());
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

				if (copy && copytarget && E.callable.get_method() != StringName()) {
					Callable copy_callable = Callable(copytarget, E.callable.get_method());
					if (!copy->is_connected(E.signal.get_name(), copy_callable)) {
						int unbound_arg_count = E.callable.get_unbound_arguments_count();
						if (unbound_arg_count > 0) {
							copy_callable = copy_callable.unbind(unbound_arg_count);
						}
						if (E.callable.get_bound_arguments_count() > 0) {
							copy_callable = copy_callable.bindv(E.callable.get_bound_arguments());
						}
						copy->connect(E.signal.get_name(), copy_callable, E.flags);
					}
				}
			}
		}

		for (int i = 0; i < n->get_child_count(); i++) {
			process_list.push_back(n->get_child(i));
		}
	}
}

static void find_owned_by(Node *p_by, Node *p_node, List<Node *> *p_owned) {
	if (p_node->get_owner() == p_by) {
		p_owned->push_back(p_node);
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		find_owned_by(p_by, p_node->get_child(i), p_owned);
	}
}

void Node::replace_by(Node *p_node, bool p_keep_groups) {
	ERR_THREAD_GUARD
	ERR_FAIL_NULL(p_node);
	ERR_FAIL_COND(p_node->data.parent);

	List<Node *> owned = data.owned;
	List<Node *> owned_by_owner;
	Node *owner = (data.owner == this) ? p_node : data.owner;

	if (p_keep_groups) {
		List<GroupInfo> groups;
		get_groups(&groups);

		for (const GroupInfo &E : groups) {
			p_node->add_to_group(E.name, E.persistent);
		}
	}

	_replace_connections_target(p_node);

	if (data.owner) {
		for (int i = 0; i < get_child_count(); i++) {
			find_owned_by(data.owner, get_child(i), &owned_by_owner);
		}

		_clean_up_owner();
	}

	Node *parent = data.parent;
	int index_in_parent = get_index(false);

	if (data.parent) {
		parent->remove_child(this);
		parent->add_child(p_node);
		parent->move_child(p_node, index_in_parent);
	}

	emit_signal(SNAME("replacing_by"), p_node);

	while (get_child_count()) {
		Node *child = get_child(0);
		remove_child(child);
		if (!child->is_owned_by_parent()) {
			// add the custom children to the p_node
			Node *child_owner = child->get_owner() == this ? p_node : child->get_owner();
			child->set_owner(nullptr);
			p_node->add_child(child);
			child->set_owner(child_owner);
		}
	}

	p_node->set_owner(owner);
	for (Node *E : owned) {
		if (E->data.owner != p_node) {
			E->set_owner(p_node);
		}
	}

	for (Node *E : owned_by_owner) {
		if (E->data.owner != owner) {
			E->set_owner(owner);
		}
	}

	p_node->set_scene_file_path(get_scene_file_path());
}

void Node::_replace_connections_target(Node *p_new_target) {
	List<Connection> cl;
	get_signals_connected_to_this(&cl);

	for (const Connection &c : cl) {
		if (c.flags & CONNECT_PERSIST) {
			c.signal.get_object()->disconnect(c.signal.get_name(), Callable(this, c.callable.get_method()));
			bool valid = p_new_target->has_method(c.callable.get_method()) || Ref<Script>(p_new_target->get_script()).is_null() || Ref<Script>(p_new_target->get_script())->has_method(c.callable.get_method());
			ERR_CONTINUE_MSG(!valid, vformat("Attempt to connect signal '%s.%s' to nonexistent method '%s.%s'.", c.signal.get_object()->get_class(), c.signal.get_name(), c.callable.get_object()->get_class(), c.callable.get_method()));
			c.signal.get_object()->connect(c.signal.get_name(), Callable(p_new_target, c.callable.get_method()), c.flags);
		}
	}
}

bool Node::has_node_and_resource(const NodePath &p_path) const {
	ERR_THREAD_GUARD_V(false);
	if (!has_node(p_path)) {
		return false;
	}
	Ref<Resource> res;
	Vector<StringName> leftover_path;
	Node *node = get_node_and_resource(p_path, res, leftover_path, false);

	return node;
}

Array Node::_get_node_and_resource(const NodePath &p_path) {
	Ref<Resource> res;
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

Node *Node::get_node_and_resource(const NodePath &p_path, Ref<Resource> &r_res, Vector<StringName> &r_leftover_subpath, bool p_last_is_property) const {
	ERR_THREAD_GUARD_V(nullptr);
	r_res = Ref<Resource>();
	r_leftover_subpath = Vector<StringName>();
	Node *node = get_node_or_null(p_path);
	if (!node) {
		return nullptr;
	}

	if (p_path.get_subname_count()) {
		int j = 0;
		// If not p_last_is_property, we shouldn't consider the last one as part of the resource
		for (; j < p_path.get_subname_count() - (int)p_last_is_property; j++) {
			bool is_valid = false;
			Variant new_res_v = j == 0 ? node->get(p_path.get_subname(j), &is_valid) : r_res->get(p_path.get_subname(j), &is_valid);

			if (!is_valid) { // Found nothing on that path
				return nullptr;
			}

			Ref<Resource> new_res = new_res_v;

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
static HashMap<ObjectID, List<String>> _print_orphan_nodes_map;

static void _print_orphan_nodes_routine(Object *p_obj) {
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

	List<String> info_strings;
	info_strings.push_back(path);
	info_strings.push_back(n->get_class());

	_print_orphan_nodes_map[p_obj->get_instance_id()] = info_strings;
}
#endif // DEBUG_ENABLED

void Node::print_orphan_nodes() {
#ifdef DEBUG_ENABLED
	// Make sure it's empty.
	_print_orphan_nodes_map.clear();

	// Collect and print information about orphan nodes.
	ObjectDB::debug_objects(_print_orphan_nodes_routine);

	for (const KeyValue<ObjectID, List<String>> &E : _print_orphan_nodes_map) {
		print_line(itos(E.key) + " - Stray Node: " + E.value.get(0) + " (Type: " + E.value.get(1) + ")");
	}

	// Flush it after use.
	_print_orphan_nodes_map.clear();
#endif
}

void Node::queue_free() {
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

#ifdef TOOLS_ENABLED
static void _add_nodes_to_options(const Node *p_base, const Node *p_node, List<String> *r_options) {
	if (p_node != p_base && !p_node->get_owner()) {
		return;
	}
	if (p_node->is_unique_name_in_owner() && p_node->get_owner() == p_base) {
		String n = "%" + p_node->get_name();
		r_options->push_back(n.quote());
	}
	String n = p_base->get_path_to(p_node);
	r_options->push_back(n.quote());
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_add_nodes_to_options(p_base, p_node->get_child(i), r_options);
	}
}

void Node::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {
	const String pf = p_function;
	if (p_idx == 0 && (pf == "has_node" || pf == "get_node" || pf == "get_node_or_null")) {
		_add_nodes_to_options(this, this, r_options);
	} else if (p_idx == 0 && (pf == "add_to_group" || pf == "remove_from_group" || pf == "is_in_group")) {
		HashMap<StringName, String> global_groups = ProjectSettings::get_singleton()->get_global_groups_list();
		for (const KeyValue<StringName, String> &E : global_groups) {
			r_options->push_back(E.key.operator String().quote());
		}
	}
	Object::get_argument_options(p_function, p_idx, r_options);
}
#endif

void Node::clear_internal_tree_resource_paths() {
	clear_internal_resource_paths();
	for (KeyValue<StringName, Node *> &K : data.children) {
		K.value->clear_internal_tree_resource_paths();
	}
}

PackedStringArray Node::get_configuration_warnings() const {
	ERR_THREAD_GUARD_V(PackedStringArray());
	PackedStringArray ret;

	Vector<String> warnings;
	if (GDVIRTUAL_CALL(_get_configuration_warnings, warnings)) {
		ret.append_array(warnings);
	}

	return ret;
}

void Node::update_configuration_warnings() {
	ERR_THREAD_GUARD
#ifdef TOOLS_ENABLED
	if (!is_inside_tree()) {
		return;
	}
	if (get_tree()->get_edited_scene_root() && (get_tree()->get_edited_scene_root() == this || get_tree()->get_edited_scene_root()->is_ancestor_of(this))) {
		get_tree()->emit_signal(SceneStringName(node_configuration_warning_changed), this);
	}
#endif
}

bool Node::is_owned_by_parent() const {
	return data.parent_owned;
}

void Node::set_display_folded(bool p_folded) {
	ERR_THREAD_GUARD
	data.display_folded = p_folded;
}

bool Node::is_displayed_folded() const {
	return data.display_folded;
}

bool Node::is_ready() const {
	return !data.ready_first;
}

void Node::request_ready() {
	ERR_THREAD_GUARD
	data.ready_first = true;
}

void Node::_call_input(const Ref<InputEvent> &p_event) {
	if (p_event->get_device() != InputEvent::DEVICE_ID_INTERNAL) {
		GDVIRTUAL_CALL(_input, p_event);
	}
	if (!is_inside_tree() || !get_viewport() || get_viewport()->is_input_handled()) {
		return;
	}
	input(p_event);
}

void Node::_call_shortcut_input(const Ref<InputEvent> &p_event) {
	if (p_event->get_device() != InputEvent::DEVICE_ID_INTERNAL) {
		GDVIRTUAL_CALL(_shortcut_input, p_event);
	}
	if (!is_inside_tree() || !get_viewport() || get_viewport()->is_input_handled()) {
		return;
	}
	shortcut_input(p_event);
}

void Node::_call_unhandled_input(const Ref<InputEvent> &p_event) {
	if (p_event->get_device() != InputEvent::DEVICE_ID_INTERNAL) {
		GDVIRTUAL_CALL(_unhandled_input, p_event);
	}
	if (!is_inside_tree() || !get_viewport() || get_viewport()->is_input_handled()) {
		return;
	}
	unhandled_input(p_event);
}

void Node::_call_unhandled_key_input(const Ref<InputEvent> &p_event) {
	if (p_event->get_device() != InputEvent::DEVICE_ID_INTERNAL) {
		GDVIRTUAL_CALL(_unhandled_key_input, p_event);
	}
	if (!is_inside_tree() || !get_viewport() || get_viewport()->is_input_handled()) {
		return;
	}
	unhandled_key_input(p_event);
}

void Node::_validate_property(PropertyInfo &p_property) const {
	if ((p_property.name == "process_thread_group_order" || p_property.name == "process_thread_messages") && data.process_thread_group == PROCESS_THREAD_GROUP_INHERIT) {
		p_property.usage = 0;
	}
}

void Node::input(const Ref<InputEvent> &p_event) {
}

void Node::shortcut_input(const Ref<InputEvent> &p_key_event) {
}

void Node::unhandled_input(const Ref<InputEvent> &p_event) {
}

void Node::unhandled_key_input(const Ref<InputEvent> &p_key_event) {
}

Variant Node::_call_deferred_thread_group_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	if (p_argcount < 1) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = 1;
		return Variant();
	}

	if (!p_args[0]->is_string()) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 0;
		r_error.expected = Variant::STRING_NAME;
		return Variant();
	}

	r_error.error = Callable::CallError::CALL_OK;

	StringName method = *p_args[0];

	call_deferred_thread_groupp(method, &p_args[1], p_argcount - 1, true);

	return Variant();
}

Variant Node::_call_thread_safe_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	if (p_argcount < 1) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = 1;
		return Variant();
	}

	if (!p_args[0]->is_string()) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 0;
		r_error.expected = Variant::STRING_NAME;
		return Variant();
	}

	r_error.error = Callable::CallError::CALL_OK;

	StringName method = *p_args[0];

	call_thread_safep(method, &p_args[1], p_argcount - 1, true);

	return Variant();
}

void Node::call_deferred_thread_groupp(const StringName &p_method, const Variant **p_args, int p_argcount, bool p_show_error) {
	ERR_FAIL_COND(!is_inside_tree());
	SceneTree::ProcessGroup *pg = (SceneTree::ProcessGroup *)data.process_group;
	pg->call_queue.push_callp(this, p_method, p_args, p_argcount, p_show_error);
}
void Node::set_deferred_thread_group(const StringName &p_property, const Variant &p_value) {
	ERR_FAIL_COND(!is_inside_tree());
	SceneTree::ProcessGroup *pg = (SceneTree::ProcessGroup *)data.process_group;
	pg->call_queue.push_set(this, p_property, p_value);
}
void Node::notify_deferred_thread_group(int p_notification) {
	ERR_FAIL_COND(!is_inside_tree());
	SceneTree::ProcessGroup *pg = (SceneTree::ProcessGroup *)data.process_group;
	pg->call_queue.push_notification(this, p_notification);
}

void Node::call_thread_safep(const StringName &p_method, const Variant **p_args, int p_argcount, bool p_show_error) {
	if (is_accessible_from_caller_thread()) {
		Callable::CallError ce;
		callp(p_method, p_args, p_argcount, ce);
		if (p_show_error && ce.error != Callable::CallError::CALL_OK) {
			ERR_FAIL_MSG("Error calling method from 'call_threadp': " + Variant::get_call_error_text(this, p_method, p_args, p_argcount, ce) + ".");
		}
	} else {
		call_deferred_thread_groupp(p_method, p_args, p_argcount, p_show_error);
	}
}
void Node::set_thread_safe(const StringName &p_property, const Variant &p_value) {
	if (is_accessible_from_caller_thread()) {
		set(p_property, p_value);
	} else {
		set_deferred_thread_group(p_property, p_value);
	}
}
void Node::notify_thread_safe(int p_notification) {
	if (is_accessible_from_caller_thread()) {
		notification(p_notification);
	} else {
		notify_deferred_thread_group(p_notification);
	}
}

void Node::_bind_methods() {
	GLOBAL_DEF(PropertyInfo(Variant::INT, "editor/naming/node_name_num_separator", PROPERTY_HINT_ENUM, "None,Space,Underscore,Dash"), 0);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "editor/naming/node_name_casing", PROPERTY_HINT_ENUM, "PascalCase,camelCase,snake_case"), NAME_CASING_PASCAL_CASE);

	ClassDB::bind_static_method("Node", D_METHOD("print_orphan_nodes"), &Node::print_orphan_nodes);
	ClassDB::bind_method(D_METHOD("add_sibling", "sibling", "force_readable_name"), &Node::add_sibling, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("set_name", "name"), &Node::set_name);
	ClassDB::bind_method(D_METHOD("get_name"), &Node::get_name);
	ClassDB::bind_method(D_METHOD("add_child", "node", "force_readable_name", "internal"), &Node::add_child, DEFVAL(false), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("remove_child", "node"), &Node::remove_child);
	ClassDB::bind_method(D_METHOD("reparent", "new_parent", "keep_global_transform"), &Node::reparent, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_child_count", "include_internal"), &Node::get_child_count, DEFVAL(false)); // Note that the default value bound for include_internal is false, while the method is declared with true. This is because internal nodes are irrelevant for GDSCript.
	ClassDB::bind_method(D_METHOD("get_children", "include_internal"), &Node::get_children, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_child", "idx", "include_internal"), &Node::get_child, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("has_node", "path"), &Node::has_node);
	ClassDB::bind_method(D_METHOD("get_node", "path"), &Node::get_node);
	ClassDB::bind_method(D_METHOD("get_node_or_null", "path"), &Node::get_node_or_null);
	ClassDB::bind_method(D_METHOD("get_parent"), &Node::get_parent);
	ClassDB::bind_method(D_METHOD("find_child", "pattern", "recursive", "owned"), &Node::find_child, DEFVAL(true), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("find_children", "pattern", "type", "recursive", "owned"), &Node::find_children, DEFVAL(""), DEFVAL(true), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("find_parent", "pattern"), &Node::find_parent);
	ClassDB::bind_method(D_METHOD("has_node_and_resource", "path"), &Node::has_node_and_resource);
	ClassDB::bind_method(D_METHOD("get_node_and_resource", "path"), &Node::_get_node_and_resource);

	ClassDB::bind_method(D_METHOD("is_inside_tree"), &Node::is_inside_tree);
	ClassDB::bind_method(D_METHOD("is_part_of_edited_scene"), &Node::is_part_of_edited_scene);
	ClassDB::bind_method(D_METHOD("is_ancestor_of", "node"), &Node::is_ancestor_of);
	ClassDB::bind_method(D_METHOD("is_greater_than", "node"), &Node::is_greater_than);
	ClassDB::bind_method(D_METHOD("get_path"), &Node::get_path);
	ClassDB::bind_method(D_METHOD("get_path_to", "node", "use_unique_path"), &Node::get_path_to, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("add_to_group", "group", "persistent"), &Node::add_to_group, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("remove_from_group", "group"), &Node::remove_from_group);
	ClassDB::bind_method(D_METHOD("is_in_group", "group"), &Node::is_in_group);
	ClassDB::bind_method(D_METHOD("move_child", "child_node", "to_index"), &Node::move_child);
	ClassDB::bind_method(D_METHOD("get_groups"), &Node::_get_groups);
	ClassDB::bind_method(D_METHOD("set_owner", "owner"), &Node::set_owner);
	ClassDB::bind_method(D_METHOD("get_owner"), &Node::get_owner);
	ClassDB::bind_method(D_METHOD("get_index", "include_internal"), &Node::get_index, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("print_tree"), &Node::print_tree);
	ClassDB::bind_method(D_METHOD("print_tree_pretty"), &Node::print_tree_pretty);
	ClassDB::bind_method(D_METHOD("get_tree_string"), &Node::get_tree_string);
	ClassDB::bind_method(D_METHOD("get_tree_string_pretty"), &Node::get_tree_string_pretty);
	ClassDB::bind_method(D_METHOD("set_scene_file_path", "scene_file_path"), &Node::set_scene_file_path);
	ClassDB::bind_method(D_METHOD("get_scene_file_path"), &Node::get_scene_file_path);
	ClassDB::bind_method(D_METHOD("propagate_notification", "what"), &Node::propagate_notification);
	ClassDB::bind_method(D_METHOD("propagate_call", "method", "args", "parent_first"), &Node::propagate_call, DEFVAL(Array()), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("set_physics_process", "enable"), &Node::set_physics_process);
	ClassDB::bind_method(D_METHOD("get_physics_process_delta_time"), &Node::get_physics_process_delta_time);
	ClassDB::bind_method(D_METHOD("is_physics_processing"), &Node::is_physics_processing);
	ClassDB::bind_method(D_METHOD("get_process_delta_time"), &Node::get_process_delta_time);
	ClassDB::bind_method(D_METHOD("set_process", "enable"), &Node::set_process);
	ClassDB::bind_method(D_METHOD("set_process_priority", "priority"), &Node::set_process_priority);
	ClassDB::bind_method(D_METHOD("get_process_priority"), &Node::get_process_priority);
	ClassDB::bind_method(D_METHOD("set_physics_process_priority", "priority"), &Node::set_physics_process_priority);
	ClassDB::bind_method(D_METHOD("get_physics_process_priority"), &Node::get_physics_process_priority);
	ClassDB::bind_method(D_METHOD("is_processing"), &Node::is_processing);
	ClassDB::bind_method(D_METHOD("set_process_input", "enable"), &Node::set_process_input);
	ClassDB::bind_method(D_METHOD("is_processing_input"), &Node::is_processing_input);
	ClassDB::bind_method(D_METHOD("set_process_shortcut_input", "enable"), &Node::set_process_shortcut_input);
	ClassDB::bind_method(D_METHOD("is_processing_shortcut_input"), &Node::is_processing_shortcut_input);
	ClassDB::bind_method(D_METHOD("set_process_unhandled_input", "enable"), &Node::set_process_unhandled_input);
	ClassDB::bind_method(D_METHOD("is_processing_unhandled_input"), &Node::is_processing_unhandled_input);
	ClassDB::bind_method(D_METHOD("set_process_unhandled_key_input", "enable"), &Node::set_process_unhandled_key_input);
	ClassDB::bind_method(D_METHOD("is_processing_unhandled_key_input"), &Node::is_processing_unhandled_key_input);
	ClassDB::bind_method(D_METHOD("set_process_mode", "mode"), &Node::set_process_mode);
	ClassDB::bind_method(D_METHOD("get_process_mode"), &Node::get_process_mode);
	ClassDB::bind_method(D_METHOD("can_process"), &Node::can_process);

	ClassDB::bind_method(D_METHOD("set_process_thread_group", "mode"), &Node::set_process_thread_group);
	ClassDB::bind_method(D_METHOD("get_process_thread_group"), &Node::get_process_thread_group);

	ClassDB::bind_method(D_METHOD("set_process_thread_messages", "flags"), &Node::set_process_thread_messages);
	ClassDB::bind_method(D_METHOD("get_process_thread_messages"), &Node::get_process_thread_messages);

	ClassDB::bind_method(D_METHOD("set_process_thread_group_order", "order"), &Node::set_process_thread_group_order);
	ClassDB::bind_method(D_METHOD("get_process_thread_group_order"), &Node::get_process_thread_group_order);

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

	ClassDB::bind_method(D_METHOD("set_auto_translate_mode", "mode"), &Node::set_auto_translate_mode);
	ClassDB::bind_method(D_METHOD("get_auto_translate_mode"), &Node::get_auto_translate_mode);
	ClassDB::bind_method(D_METHOD("set_translation_domain_inherited"), &Node::set_translation_domain_inherited);

	ClassDB::bind_method(D_METHOD("get_window"), &Node::get_window);
	ClassDB::bind_method(D_METHOD("get_last_exclusive_window"), &Node::get_last_exclusive_window);
	ClassDB::bind_method(D_METHOD("get_tree"), &Node::get_tree);
	ClassDB::bind_method(D_METHOD("create_tween"), &Node::create_tween);

	ClassDB::bind_method(D_METHOD("duplicate", "flags"), &Node::duplicate, DEFVAL(DUPLICATE_USE_INSTANTIATION | DUPLICATE_SIGNALS | DUPLICATE_GROUPS | DUPLICATE_SCRIPTS));
	ClassDB::bind_method(D_METHOD("replace_by", "node", "keep_groups"), &Node::replace_by, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("set_scene_instance_load_placeholder", "load_placeholder"), &Node::set_scene_instance_load_placeholder);
	ClassDB::bind_method(D_METHOD("get_scene_instance_load_placeholder"), &Node::get_scene_instance_load_placeholder);
	ClassDB::bind_method(D_METHOD("set_editable_instance", "node", "is_editable"), &Node::set_editable_instance);
	ClassDB::bind_method(D_METHOD("is_editable_instance", "node"), &Node::is_editable_instance);

	ClassDB::bind_method(D_METHOD("get_viewport"), &Node::get_viewport);

	ClassDB::bind_method(D_METHOD("queue_free"), &Node::queue_free);

	ClassDB::bind_method(D_METHOD("request_ready"), &Node::request_ready);
	ClassDB::bind_method(D_METHOD("is_node_ready"), &Node::is_ready);

	ClassDB::bind_method(D_METHOD("set_multiplayer_authority", "id", "recursive"), &Node::set_multiplayer_authority, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_multiplayer_authority"), &Node::get_multiplayer_authority);

	ClassDB::bind_method(D_METHOD("is_multiplayer_authority"), &Node::is_multiplayer_authority);

	ClassDB::bind_method(D_METHOD("get_multiplayer"), &Node::get_multiplayer);
	ClassDB::bind_method(D_METHOD("rpc_config", "method", "config"), &Node::rpc_config);
	ClassDB::bind_method(D_METHOD("get_rpc_config"), &Node::get_rpc_config);

	ClassDB::bind_method(D_METHOD("set_editor_description", "editor_description"), &Node::set_editor_description);
	ClassDB::bind_method(D_METHOD("get_editor_description"), &Node::get_editor_description);

	ClassDB::bind_method(D_METHOD("_set_import_path", "import_path"), &Node::set_import_path);
	ClassDB::bind_method(D_METHOD("_get_import_path"), &Node::get_import_path);

	ClassDB::bind_method(D_METHOD("set_unique_name_in_owner", "enable"), &Node::set_unique_name_in_owner);
	ClassDB::bind_method(D_METHOD("is_unique_name_in_owner"), &Node::is_unique_name_in_owner);

	ClassDB::bind_method(D_METHOD("atr", "message", "context"), &Node::atr, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("atr_n", "message", "plural_message", "n", "context"), &Node::atr_n, DEFVAL(""));

#ifdef TOOLS_ENABLED
	ClassDB::bind_method(D_METHOD("_set_property_pinned", "property", "pinned"), &Node::set_property_pinned);
#endif

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "_import_path", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_import_path", "_get_import_path");

	{
		MethodInfo mi;

		mi.arguments.push_back(PropertyInfo(Variant::STRING_NAME, "method"));

		mi.name = "rpc";
		ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "rpc", &Node::_rpc_bind, mi);

		mi.arguments.push_front(PropertyInfo(Variant::INT, "peer_id"));

		mi.name = "rpc_id";
		ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "rpc_id", &Node::_rpc_id_bind, mi);
	}

	ClassDB::bind_method(D_METHOD("update_configuration_warnings"), &Node::update_configuration_warnings);

	{
		MethodInfo mi;
		mi.name = "call_deferred_thread_group";
		mi.arguments.push_back(PropertyInfo(Variant::STRING_NAME, "method"));

		ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "call_deferred_thread_group", &Node::_call_deferred_thread_group_bind, mi, varray(), false);
	}
	ClassDB::bind_method(D_METHOD("set_deferred_thread_group", "property", "value"), &Node::set_deferred_thread_group);
	ClassDB::bind_method(D_METHOD("notify_deferred_thread_group", "what"), &Node::notify_deferred_thread_group);

	{
		MethodInfo mi;
		mi.name = "call_thread_safe";
		mi.arguments.push_back(PropertyInfo(Variant::STRING_NAME, "method"));

		ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "call_thread_safe", &Node::_call_thread_safe_bind, mi, varray(), false);
	}
	ClassDB::bind_method(D_METHOD("set_thread_safe", "property", "value"), &Node::set_thread_safe);
	ClassDB::bind_method(D_METHOD("notify_thread_safe", "what"), &Node::notify_thread_safe);

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
	BIND_CONSTANT(NOTIFICATION_SCENE_INSTANTIATED);
	BIND_CONSTANT(NOTIFICATION_DRAG_BEGIN);
	BIND_CONSTANT(NOTIFICATION_DRAG_END);
	BIND_CONSTANT(NOTIFICATION_PATH_RENAMED);
	BIND_CONSTANT(NOTIFICATION_CHILD_ORDER_CHANGED);
	BIND_CONSTANT(NOTIFICATION_INTERNAL_PROCESS);
	BIND_CONSTANT(NOTIFICATION_INTERNAL_PHYSICS_PROCESS);
	BIND_CONSTANT(NOTIFICATION_POST_ENTER_TREE);
	BIND_CONSTANT(NOTIFICATION_DISABLED);
	BIND_CONSTANT(NOTIFICATION_ENABLED);
	BIND_CONSTANT(NOTIFICATION_RESET_PHYSICS_INTERPOLATION);

	BIND_CONSTANT(NOTIFICATION_EDITOR_PRE_SAVE);
	BIND_CONSTANT(NOTIFICATION_EDITOR_POST_SAVE);

	BIND_CONSTANT(NOTIFICATION_WM_MOUSE_ENTER);
	BIND_CONSTANT(NOTIFICATION_WM_MOUSE_EXIT);
	BIND_CONSTANT(NOTIFICATION_WM_WINDOW_FOCUS_IN);
	BIND_CONSTANT(NOTIFICATION_WM_WINDOW_FOCUS_OUT);
	BIND_CONSTANT(NOTIFICATION_WM_CLOSE_REQUEST);
	BIND_CONSTANT(NOTIFICATION_WM_GO_BACK_REQUEST);
	BIND_CONSTANT(NOTIFICATION_WM_SIZE_CHANGED);
	BIND_CONSTANT(NOTIFICATION_WM_DPI_CHANGE);
	BIND_CONSTANT(NOTIFICATION_VP_MOUSE_ENTER);
	BIND_CONSTANT(NOTIFICATION_VP_MOUSE_EXIT);
	BIND_CONSTANT(NOTIFICATION_OS_MEMORY_WARNING);
	BIND_CONSTANT(NOTIFICATION_TRANSLATION_CHANGED);
	BIND_CONSTANT(NOTIFICATION_WM_ABOUT);
	BIND_CONSTANT(NOTIFICATION_CRASH);
	BIND_CONSTANT(NOTIFICATION_OS_IME_UPDATE);
	BIND_CONSTANT(NOTIFICATION_APPLICATION_RESUMED);
	BIND_CONSTANT(NOTIFICATION_APPLICATION_PAUSED);
	BIND_CONSTANT(NOTIFICATION_APPLICATION_FOCUS_IN);
	BIND_CONSTANT(NOTIFICATION_APPLICATION_FOCUS_OUT);
	BIND_CONSTANT(NOTIFICATION_TEXT_SERVER_CHANGED);

	BIND_ENUM_CONSTANT(PROCESS_MODE_INHERIT);
	BIND_ENUM_CONSTANT(PROCESS_MODE_PAUSABLE);
	BIND_ENUM_CONSTANT(PROCESS_MODE_WHEN_PAUSED);
	BIND_ENUM_CONSTANT(PROCESS_MODE_ALWAYS);
	BIND_ENUM_CONSTANT(PROCESS_MODE_DISABLED);

	BIND_ENUM_CONSTANT(PROCESS_THREAD_GROUP_INHERIT);
	BIND_ENUM_CONSTANT(PROCESS_THREAD_GROUP_MAIN_THREAD);
	BIND_ENUM_CONSTANT(PROCESS_THREAD_GROUP_SUB_THREAD);

	BIND_BITFIELD_FLAG(FLAG_PROCESS_THREAD_MESSAGES);
	BIND_BITFIELD_FLAG(FLAG_PROCESS_THREAD_MESSAGES_PHYSICS);
	BIND_BITFIELD_FLAG(FLAG_PROCESS_THREAD_MESSAGES_ALL);

	BIND_ENUM_CONSTANT(PHYSICS_INTERPOLATION_MODE_INHERIT);
	BIND_ENUM_CONSTANT(PHYSICS_INTERPOLATION_MODE_ON);
	BIND_ENUM_CONSTANT(PHYSICS_INTERPOLATION_MODE_OFF);

	BIND_ENUM_CONSTANT(DUPLICATE_SIGNALS);
	BIND_ENUM_CONSTANT(DUPLICATE_GROUPS);
	BIND_ENUM_CONSTANT(DUPLICATE_SCRIPTS);
	BIND_ENUM_CONSTANT(DUPLICATE_USE_INSTANTIATION);

	BIND_ENUM_CONSTANT(INTERNAL_MODE_DISABLED);
	BIND_ENUM_CONSTANT(INTERNAL_MODE_FRONT);
	BIND_ENUM_CONSTANT(INTERNAL_MODE_BACK);

	BIND_ENUM_CONSTANT(AUTO_TRANSLATE_MODE_INHERIT);
	BIND_ENUM_CONSTANT(AUTO_TRANSLATE_MODE_ALWAYS);
	BIND_ENUM_CONSTANT(AUTO_TRANSLATE_MODE_DISABLED);

	ADD_SIGNAL(MethodInfo("ready"));
	ADD_SIGNAL(MethodInfo("renamed"));
	ADD_SIGNAL(MethodInfo("tree_entered"));
	ADD_SIGNAL(MethodInfo("tree_exiting"));
	ADD_SIGNAL(MethodInfo("tree_exited"));
	ADD_SIGNAL(MethodInfo("child_entered_tree", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "Node")));
	ADD_SIGNAL(MethodInfo("child_exiting_tree", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "Node")));

	ADD_SIGNAL(MethodInfo("child_order_changed"));
	ADD_SIGNAL(MethodInfo("replacing_by", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "Node")));
	ADD_SIGNAL(MethodInfo("editor_description_changed", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "Node")));
	ADD_SIGNAL(MethodInfo("editor_state_changed"));

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_name", "get_name");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "unique_name_in_owner", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_unique_name_in_owner", "is_unique_name_in_owner");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "scene_file_path", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_scene_file_path", "get_scene_file_path");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "owner", PROPERTY_HINT_RESOURCE_TYPE, "Node", PROPERTY_USAGE_NONE), "set_owner", "get_owner");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "multiplayer", PROPERTY_HINT_RESOURCE_TYPE, "MultiplayerAPI", PROPERTY_USAGE_NONE), "", "get_multiplayer");

	ADD_GROUP("Process", "process_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "process_mode", PROPERTY_HINT_ENUM, "Inherit,Pausable,When Paused,Always,Disabled"), "set_process_mode", "get_process_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "process_priority"), "set_process_priority", "get_process_priority");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "process_physics_priority"), "set_physics_process_priority", "get_physics_process_priority");

	ADD_SUBGROUP("Thread Group", "process_thread");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "process_thread_group", PROPERTY_HINT_ENUM, "Inherit,Main Thread,Sub Thread"), "set_process_thread_group", "get_process_thread_group");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "process_thread_group_order"), "set_process_thread_group_order", "get_process_thread_group_order");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "process_thread_messages", PROPERTY_HINT_FLAGS, "Process,Physics Process"), "set_process_thread_messages", "get_process_thread_messages");

	ADD_GROUP("Physics Interpolation", "physics_interpolation_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "physics_interpolation_mode", PROPERTY_HINT_ENUM, "Inherit,On,Off"), "set_physics_interpolation_mode", "get_physics_interpolation_mode");

	ADD_GROUP("Auto Translate", "auto_translate_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "auto_translate_mode", PROPERTY_HINT_ENUM, "Inherit,Always,Disabled"), "set_auto_translate_mode", "get_auto_translate_mode");

	ADD_GROUP("Editor Description", "editor_");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "editor_description", PROPERTY_HINT_MULTILINE_TEXT), "set_editor_description", "get_editor_description");

	GDVIRTUAL_BIND(_process, "delta");
	GDVIRTUAL_BIND(_physics_process, "delta");
	GDVIRTUAL_BIND(_enter_tree);
	GDVIRTUAL_BIND(_exit_tree);
	GDVIRTUAL_BIND(_ready);
	GDVIRTUAL_BIND(_get_configuration_warnings);
	GDVIRTUAL_BIND(_input, "event");
	GDVIRTUAL_BIND(_shortcut_input, "event");
	GDVIRTUAL_BIND(_unhandled_input, "event");
	GDVIRTUAL_BIND(_unhandled_key_input, "event");
}

String Node::_get_name_num_separator() {
	switch (GLOBAL_GET("editor/naming/node_name_num_separator").operator int()) {
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
	orphan_node_count++;

	// Default member initializer for bitfield is a C++20 extension, so:

	data.process_mode = PROCESS_MODE_INHERIT;
	data.physics_interpolation_mode = PHYSICS_INTERPOLATION_MODE_INHERIT;

	data.physics_process = false;
	data.process = false;

	data.physics_process_internal = false;
	data.process_internal = false;

	data.input = false;
	data.shortcut_input = false;
	data.unhandled_input = false;
	data.unhandled_key_input = false;

	data.physics_interpolated = true;
	data.physics_interpolation_reset_requested = false;
	data.physics_interpolated_client_side = false;
	data.use_identity_transform = false;

	data.parent_owned = false;
	data.in_constructor = true;
	data.use_placeholder = false;

	data.display_folded = false;
	data.editable_instance = false;

	data.inside_tree = false;
	data.ready_notified = false; // This is a small hack, so if a node is added during _ready() to the tree, it correctly gets the _ready() notification.
	data.ready_first = true;
}

Node::~Node() {
	data.grouped.clear();
	data.owned.clear();
	data.children.clear();
	data.children_cache.clear();

	ERR_FAIL_COND(data.parent);
	ERR_FAIL_COND(data.children_cache.size());

	orphan_node_count--;
}

////////////////////////////////
// Multithreaded locked version of Object functions.

#ifdef DEBUG_ENABLED

void Node::set_script(const Variant &p_script) {
	ERR_THREAD_GUARD;
	Object::set_script(p_script);
}

Variant Node::get_script() const {
	ERR_THREAD_GUARD_V(Variant());
	return Object::get_script();
}

bool Node::has_meta(const StringName &p_name) const {
	ERR_THREAD_GUARD_V(false);
	return Object::has_meta(p_name);
}

void Node::set_meta(const StringName &p_name, const Variant &p_value) {
	ERR_THREAD_GUARD;
	Object::set_meta(p_name, p_value);
	_emit_editor_state_changed();
}

void Node::remove_meta(const StringName &p_name) {
	ERR_THREAD_GUARD;
	Object::remove_meta(p_name);
	_emit_editor_state_changed();
}

Variant Node::get_meta(const StringName &p_name, const Variant &p_default) const {
	ERR_THREAD_GUARD_V(Variant());
	return Object::get_meta(p_name, p_default);
}

void Node::get_meta_list(List<StringName> *p_list) const {
	ERR_THREAD_GUARD;
	Object::get_meta_list(p_list);
}

Error Node::emit_signalp(const StringName &p_name, const Variant **p_args, int p_argcount) {
	ERR_THREAD_GUARD_V(ERR_INVALID_PARAMETER);
	return Object::emit_signalp(p_name, p_args, p_argcount);
}

bool Node::has_signal(const StringName &p_name) const {
	ERR_THREAD_GUARD_V(false);
	return Object::has_signal(p_name);
}

void Node::get_signal_list(List<MethodInfo> *p_signals) const {
	ERR_THREAD_GUARD;
	Object::get_signal_list(p_signals);
}

void Node::get_signal_connection_list(const StringName &p_signal, List<Connection> *p_connections) const {
	ERR_THREAD_GUARD;
	Object::get_signal_connection_list(p_signal, p_connections);
}

void Node::get_all_signal_connections(List<Connection> *p_connections) const {
	ERR_THREAD_GUARD;
	Object::get_all_signal_connections(p_connections);
}

int Node::get_persistent_signal_connection_count() const {
	ERR_THREAD_GUARD_V(0);
	return Object::get_persistent_signal_connection_count();
}

void Node::get_signals_connected_to_this(List<Connection> *p_connections) const {
	ERR_THREAD_GUARD;
	Object::get_signals_connected_to_this(p_connections);
}

Error Node::connect(const StringName &p_signal, const Callable &p_callable, uint32_t p_flags) {
	ERR_THREAD_GUARD_V(ERR_INVALID_PARAMETER);

	Error retval = Object::connect(p_signal, p_callable, p_flags);
#ifdef TOOLS_ENABLED
	if (p_flags & CONNECT_PERSIST) {
		_emit_editor_state_changed();
	}
#endif

	return retval;
}

void Node::disconnect(const StringName &p_signal, const Callable &p_callable) {
	ERR_THREAD_GUARD;

#ifdef TOOLS_ENABLED
	// Already under thread guard, don't check again.
	int old_connection_count = Object::get_persistent_signal_connection_count();
#endif

	Object::disconnect(p_signal, p_callable);

#ifdef TOOLS_ENABLED
	int new_connection_count = Object::get_persistent_signal_connection_count();
	if (old_connection_count != new_connection_count) {
		_emit_editor_state_changed();
	}
#endif
}

bool Node::is_connected(const StringName &p_signal, const Callable &p_callable) const {
	ERR_THREAD_GUARD_V(false);
	return Object::is_connected(p_signal, p_callable);
}

bool Node::has_connections(const StringName &p_signal) const {
	ERR_THREAD_GUARD_V(false);
	return Object::has_connections(p_signal);
}

#endif

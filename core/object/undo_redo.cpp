/**************************************************************************/
/*  undo_redo.cpp                                                         */
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

#include "undo_redo.h"

#include "core/io/resource.h"
#include "core/os/os.h"
#include "core/templates/local_vector.h"

void UndoRedo::Operation::delete_reference() {
	if (type != Operation::TYPE_REFERENCE) {
		return;
	}
	if (ref.is_valid()) {
		ref.unref();
	} else {
		Object *obj = ObjectDB::get_instance(object);
		if (obj) {
			memdelete(obj);
		}
	}
}

void UndoRedo::discard_redo() {
	if (current_action == actions.size() - 1) {
		return;
	}

	for (int i = current_action + 1; i < actions.size(); i++) {
		for (Operation &E : actions.write[i].do_ops) {
			E.delete_reference();
		}
		//ERASE do data
	}

	actions.resize(current_action + 1);
}

void UndoRedo::cancel_action() {
	ERR_FAIL_COND(action_level <= 0);
	action_level--;

	Action &head_action = actions.write[actions.size() - 1];

	for (Operation &E : head_action.do_ops) {
		E.delete_reference();
	}

	for (Operation &E : head_action.undo_ops) {
		E.delete_reference();
	}

	actions.resize(actions.size() - 1);
}

bool UndoRedo::_redo(bool p_execute) {
	ERR_FAIL_COND_V(action_level > 0, false);

	if ((current_action + 1) >= actions.size()) {
		return false; //nothing to redo
	}

	current_action++;

	List<Operation>::Element *start_doops_element = actions.write[current_action].do_ops.front();
	while (merge_total > 0 && start_doops_element) {
		start_doops_element = start_doops_element->next();
		merge_total--;
	}

	_process_operation_list(start_doops_element, p_execute);
	version++;
	emit_signal(SNAME("version_changed"));

	return true;
}

void UndoRedo::create_action(const String &p_name, MergeMode p_mode, bool p_backward_undo_ops) {
	uint64_t ticks = OS::get_singleton()->get_ticks_msec();

	if (action_level == 0) {
		discard_redo();

		// Check if the merge operation is valid
		if (p_mode != MERGE_DISABLE && actions.size() && actions[actions.size() - 1].name == p_name && actions[actions.size() - 1].backward_undo_ops == p_backward_undo_ops && actions[actions.size() - 1].last_tick + 800 > ticks) {
			current_action = actions.size() - 2;

			if (p_mode == MERGE_ENDS) {
				// Clear all do ops from last action if they are not forced kept
				LocalVector<List<Operation>::Element *> to_remove;
				for (List<Operation>::Element *E = actions.write[current_action + 1].do_ops.front(); E; E = E->next()) {
					if (!E->get().force_keep_in_merge_ends) {
						to_remove.push_back(E);
					}
				}

				for (List<Operation>::Element *E : to_remove) {
					// Delete all object references
					E->get().delete_reference();
					E->erase();
				}
			}

			if (p_mode == MERGE_ALL) {
				merge_total = actions.write[current_action + 1].do_ops.size();
			} else {
				merge_total = 0;
			}

			actions.write[actions.size() - 1].last_tick = ticks;

			// Revert reverse from previous commit.
			if (actions[actions.size() - 1].backward_undo_ops) {
				actions.write[actions.size() - 1].undo_ops.reverse();
			}

			merge_mode = p_mode;
			merging = true;
		} else {
			Action new_action;
			new_action.name = p_name;
			new_action.last_tick = ticks;
			new_action.backward_undo_ops = p_backward_undo_ops;
			actions.push_back(new_action);

			merge_mode = MERGE_DISABLE;
			merge_total = 0;
		}
	}

	action_level++;

	force_keep_in_merge_ends = false;
}

void UndoRedo::add_do_method(const Callable &p_callable) {
	ERR_FAIL_COND(!p_callable.is_valid());
	ERR_FAIL_COND(action_level <= 0);
	ERR_FAIL_COND((current_action + 1) >= actions.size());

	ObjectID object_id = p_callable.get_object_id();
	Object *object = ObjectDB::get_instance(object_id);
	ERR_FAIL_COND(object_id.is_valid() && object == nullptr);

	Operation do_op;
	do_op.callable = p_callable;
	do_op.object = object_id;
	if (Object::cast_to<RefCounted>(object)) {
		do_op.ref = Ref<RefCounted>(Object::cast_to<RefCounted>(object));
	}
	do_op.type = Operation::TYPE_METHOD;
	do_op.name = p_callable.get_method();
	if (do_op.name == StringName()) {
		// There's no `get_method()` for custom callables, so use `operator String()` instead.
		do_op.name = static_cast<String>(p_callable);
	}

	actions.write[current_action + 1].do_ops.push_back(do_op);
}

void UndoRedo::add_undo_method(const Callable &p_callable) {
	ERR_FAIL_COND(!p_callable.is_valid());
	ERR_FAIL_COND(action_level <= 0);
	ERR_FAIL_COND((current_action + 1) >= actions.size());

	// No undo if the merge mode is MERGE_ENDS
	if (!force_keep_in_merge_ends && merge_mode == MERGE_ENDS) {
		return;
	}

	ObjectID object_id = p_callable.get_object_id();
	Object *object = ObjectDB::get_instance(object_id);
	ERR_FAIL_COND(object_id.is_valid() && object == nullptr);

	Operation undo_op;
	undo_op.callable = p_callable;
	undo_op.object = object_id;
	if (Object::cast_to<RefCounted>(object)) {
		undo_op.ref = Ref<RefCounted>(Object::cast_to<RefCounted>(object));
	}
	undo_op.type = Operation::TYPE_METHOD;
	undo_op.force_keep_in_merge_ends = force_keep_in_merge_ends;
	undo_op.name = p_callable.get_method();
	if (undo_op.name == StringName()) {
		// There's no `get_method()` for custom callables, so use `operator String()` instead.
		undo_op.name = static_cast<String>(p_callable);
	}

	actions.write[current_action + 1].undo_ops.push_back(undo_op);
}

void UndoRedo::add_do_property(Object *p_object, const StringName &p_property, const Variant &p_value) {
	ERR_FAIL_NULL(p_object);
	ERR_FAIL_COND(action_level <= 0);
	ERR_FAIL_COND((current_action + 1) >= actions.size());
	Operation do_op;
	do_op.object = p_object->get_instance_id();
	if (Object::cast_to<RefCounted>(p_object)) {
		do_op.ref = Ref<RefCounted>(Object::cast_to<RefCounted>(p_object));
	}

	do_op.type = Operation::TYPE_PROPERTY;
	do_op.name = p_property;
	do_op.value = p_value;
	actions.write[current_action + 1].do_ops.push_back(do_op);
}

void UndoRedo::add_undo_property(Object *p_object, const StringName &p_property, const Variant &p_value) {
	ERR_FAIL_NULL(p_object);
	ERR_FAIL_COND(action_level <= 0);
	ERR_FAIL_COND((current_action + 1) >= actions.size());

	// No undo if the merge mode is MERGE_ENDS
	if (!force_keep_in_merge_ends && merge_mode == MERGE_ENDS) {
		return;
	}

	Operation undo_op;
	undo_op.object = p_object->get_instance_id();
	if (Object::cast_to<RefCounted>(p_object)) {
		undo_op.ref = Ref<RefCounted>(Object::cast_to<RefCounted>(p_object));
	}

	undo_op.type = Operation::TYPE_PROPERTY;
	undo_op.force_keep_in_merge_ends = force_keep_in_merge_ends;
	undo_op.name = p_property;
	undo_op.value = p_value;
	actions.write[current_action + 1].undo_ops.push_back(undo_op);
}

void UndoRedo::add_do_reference(Object *p_object) {
	ERR_FAIL_NULL(p_object);
	ERR_FAIL_COND(action_level <= 0);
	ERR_FAIL_COND((current_action + 1) >= actions.size());
	Operation do_op;
	do_op.object = p_object->get_instance_id();
	if (Object::cast_to<RefCounted>(p_object)) {
		do_op.ref = Ref<RefCounted>(Object::cast_to<RefCounted>(p_object));
	}

	do_op.type = Operation::TYPE_REFERENCE;
	actions.write[current_action + 1].do_ops.push_back(do_op);
}

void UndoRedo::add_undo_reference(Object *p_object) {
	ERR_FAIL_NULL(p_object);
	ERR_FAIL_COND(action_level <= 0);
	ERR_FAIL_COND((current_action + 1) >= actions.size());

	// No undo if the merge mode is MERGE_ENDS
	if (!force_keep_in_merge_ends && merge_mode == MERGE_ENDS) {
		return;
	}

	Operation undo_op;
	undo_op.object = p_object->get_instance_id();
	if (Object::cast_to<RefCounted>(p_object)) {
		undo_op.ref = Ref<RefCounted>(Object::cast_to<RefCounted>(p_object));
	}

	undo_op.type = Operation::TYPE_REFERENCE;
	undo_op.force_keep_in_merge_ends = force_keep_in_merge_ends;
	actions.write[current_action + 1].undo_ops.push_back(undo_op);
}

void UndoRedo::start_force_keep_in_merge_ends() {
	ERR_FAIL_COND(action_level <= 0);
	ERR_FAIL_COND((current_action + 1) >= actions.size());

	force_keep_in_merge_ends = true;
}

void UndoRedo::end_force_keep_in_merge_ends() {
	ERR_FAIL_COND(action_level <= 0);
	ERR_FAIL_COND((current_action + 1) >= actions.size());

	force_keep_in_merge_ends = false;
}

void UndoRedo::_pop_history_tail() {
	discard_redo();

	if (!actions.size()) {
		return;
	}

	for (Operation &E : actions.write[0].undo_ops) {
		E.delete_reference();
	}

	actions.remove_at(0);
	if (current_action >= 0) {
		current_action--;
	}
}

bool UndoRedo::is_committing_action() const {
	return committing > 0;
}

void UndoRedo::commit_action(bool p_execute) {
	ERR_FAIL_COND(action_level <= 0);
	action_level--;
	if (action_level > 0) {
		return; //still nested
	}

	bool add_message = !merging;

	if (merging) {
		version--;
		merging = false;
	}

	if (actions[actions.size() - 1].backward_undo_ops) {
		actions.write[actions.size() - 1].undo_ops.reverse();
	}

	committing++;
	_redo(p_execute); // perform action
	committing--;

	if (max_steps > 0) {
		// Clear early steps.

		while (actions.size() > max_steps) {
			_pop_history_tail();
		}
	}

	if (add_message && callback && actions.size() > 0) {
		callback(callback_ud, actions[actions.size() - 1].name);
	}
}

void UndoRedo::_process_operation_list(List<Operation>::Element *E, bool p_execute) {
	const int PREALLOCATE_ARGS_COUNT = 16;

	LocalVector<const Variant *> args;
	args.reserve(PREALLOCATE_ARGS_COUNT);

	for (; E; E = E->next()) {
		Operation &op = E->get();

		Object *obj = ObjectDB::get_instance(op.object);
		if (!obj) { //may have been deleted and this is fine
			continue;
		}

		switch (op.type) {
			case Operation::TYPE_METHOD: {
				if (p_execute) {
					Callable::CallError ce;
					Variant ret;
					op.callable.callp(nullptr, 0, ret, ce);
					if (ce.error != Callable::CallError::CALL_OK) {
						ERR_PRINT(vformat("Error calling UndoRedo method operation '%s': %s.", String(op.name), Variant::get_call_error_text(obj, op.name, nullptr, 0, ce)));
					}
#ifdef TOOLS_ENABLED
					Resource *res = Object::cast_to<Resource>(obj);
					if (res) {
						res->set_edited(true);
					}
#endif
				}

				if (method_callback) {
					Vector<Variant> binds;
					if (op.callable.is_custom()) {
						CallableCustomBind *ccb = dynamic_cast<CallableCustomBind *>(op.callable.get_custom());
						if (ccb) {
							binds = ccb->get_binds();
						}
					}

					if (binds.is_empty()) {
						method_callback(method_callback_ud, obj, op.name, nullptr, 0);
					} else {
						args.clear();

						for (int i = 0; i < binds.size(); i++) {
							args.push_back(&binds[i]);
						}

						method_callback(method_callback_ud, obj, op.name, args.ptr(), binds.size());
					}
				}
			} break;
			case Operation::TYPE_PROPERTY: {
				if (p_execute) {
					obj->set(op.name, op.value);
#ifdef TOOLS_ENABLED
					Resource *res = Object::cast_to<Resource>(obj);
					if (res) {
						res->set_edited(true);
					}
#endif
				}

				if (property_callback) {
					property_callback(prop_callback_ud, obj, op.name, op.value);
				}
			} break;
			case Operation::TYPE_REFERENCE: {
				//do nothing
			} break;
		}
	}
}

bool UndoRedo::redo() {
	return _redo(true);
}

bool UndoRedo::undo() {
	ERR_FAIL_COND_V(action_level > 0, false);
	if (current_action < 0) {
		return false; //nothing to redo
	}
	_process_operation_list(actions.write[current_action].undo_ops.front(), true);
	current_action--;
	version--;
	emit_signal(SNAME("version_changed"));

	return true;
}

int UndoRedo::get_history_count() {
	ERR_FAIL_COND_V(action_level > 0, -1);

	return actions.size();
}

int UndoRedo::get_current_action() {
	ERR_FAIL_COND_V(action_level > 0, -1);

	return current_action;
}

String UndoRedo::get_action_name(int p_id) {
	ERR_FAIL_INDEX_V(p_id, actions.size(), "");

	return actions[p_id].name;
}

void UndoRedo::clear_history(bool p_increase_version) {
	ERR_FAIL_COND(action_level > 0);
	discard_redo();

	while (actions.size()) {
		_pop_history_tail();
	}

	if (p_increase_version) {
		version++;
		emit_signal(SNAME("version_changed"));
	}
}

String UndoRedo::get_current_action_name() const {
	ERR_FAIL_COND_V(action_level > 0, "");
	if (current_action < 0) {
		return "";
	}
	return actions[current_action].name;
}

int UndoRedo::get_action_level() const {
	return action_level;
}

bool UndoRedo::has_undo() const {
	return current_action >= 0;
}

bool UndoRedo::has_redo() const {
	return (current_action + 1) < actions.size();
}

bool UndoRedo::is_merging() const {
	return merging;
}

uint64_t UndoRedo::get_version() const {
	return version;
}

void UndoRedo::set_max_steps(int p_max_steps) {
	max_steps = p_max_steps;
}

int UndoRedo::get_max_steps() const {
	return max_steps;
}

void UndoRedo::set_commit_notify_callback(CommitNotifyCallback p_callback, void *p_ud) {
	callback = p_callback;
	callback_ud = p_ud;
}

void UndoRedo::set_method_notify_callback(MethodNotifyCallback p_method_callback, void *p_ud) {
	method_callback = p_method_callback;
	method_callback_ud = p_ud;
}

void UndoRedo::set_property_notify_callback(PropertyNotifyCallback p_property_callback, void *p_ud) {
	property_callback = p_property_callback;
	prop_callback_ud = p_ud;
}

UndoRedo::~UndoRedo() {
	clear_history();
}

void UndoRedo::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create_action", "name", "merge_mode", "backward_undo_ops"), &UndoRedo::create_action, DEFVAL(MERGE_DISABLE), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("commit_action", "execute"), &UndoRedo::commit_action, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("cancel_action"), &UndoRedo::cancel_action);
	ClassDB::bind_method(D_METHOD("is_committing_action"), &UndoRedo::is_committing_action);

	ClassDB::bind_method(D_METHOD("add_do_method", "callable"), &UndoRedo::add_do_method);
	ClassDB::bind_method(D_METHOD("add_undo_method", "callable"), &UndoRedo::add_undo_method);
	ClassDB::bind_method(D_METHOD("add_do_property", "object", "property", "value"), &UndoRedo::add_do_property);
	ClassDB::bind_method(D_METHOD("add_undo_property", "object", "property", "value"), &UndoRedo::add_undo_property);
	ClassDB::bind_method(D_METHOD("add_do_reference", "object"), &UndoRedo::add_do_reference);
	ClassDB::bind_method(D_METHOD("add_undo_reference", "object"), &UndoRedo::add_undo_reference);

	ClassDB::bind_method(D_METHOD("start_force_keep_in_merge_ends"), &UndoRedo::start_force_keep_in_merge_ends);
	ClassDB::bind_method(D_METHOD("end_force_keep_in_merge_ends"), &UndoRedo::end_force_keep_in_merge_ends);

	ClassDB::bind_method(D_METHOD("get_history_count"), &UndoRedo::get_history_count);
	ClassDB::bind_method(D_METHOD("get_current_action"), &UndoRedo::get_current_action);
	ClassDB::bind_method(D_METHOD("get_action_name", "id"), &UndoRedo::get_action_name);
	ClassDB::bind_method(D_METHOD("clear_history", "increase_version"), &UndoRedo::clear_history, DEFVAL(true));

	ClassDB::bind_method(D_METHOD("get_current_action_name"), &UndoRedo::get_current_action_name);

	ClassDB::bind_method(D_METHOD("has_undo"), &UndoRedo::has_undo);
	ClassDB::bind_method(D_METHOD("has_redo"), &UndoRedo::has_redo);
	ClassDB::bind_method(D_METHOD("get_version"), &UndoRedo::get_version);
	ClassDB::bind_method(D_METHOD("set_max_steps", "max_steps"), &UndoRedo::set_max_steps);
	ClassDB::bind_method(D_METHOD("get_max_steps"), &UndoRedo::get_max_steps);
	ClassDB::bind_method(D_METHOD("redo"), &UndoRedo::redo);
	ClassDB::bind_method(D_METHOD("undo"), &UndoRedo::undo);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_steps", PROPERTY_HINT_RANGE, "0,50,1,or_greater"), "set_max_steps", "get_max_steps");

	ADD_SIGNAL(MethodInfo("version_changed"));

	BIND_ENUM_CONSTANT(MERGE_DISABLE);
	BIND_ENUM_CONSTANT(MERGE_ENDS);
	BIND_ENUM_CONSTANT(MERGE_ALL);
}

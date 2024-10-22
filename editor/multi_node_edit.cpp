/**************************************************************************/
/*  multi_node_edit.cpp                                                   */
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

#include "multi_node_edit.h"

#include "core/math/math_fieldwise.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"

bool MultiNodeEdit::_set(const StringName &p_name, const Variant &p_value) {
	return _set_impl(p_name, p_value, "");
}

bool MultiNodeEdit::_set_impl(const StringName &p_name, const Variant &p_value, const String &p_field) {
	Node *es = EditorNode::get_singleton()->get_edited_scene();
	if (!es) {
		return false;
	}

	String name = p_name;

	if (name == "scripts") { // Script set is intercepted at object level (check Variant Object::get()), so use a different name.
		name = "script";
	} else if (name.begins_with("Metadata/")) {
		name = name.replace_first("Metadata/", "metadata/");
	}

	Node *node_path_target = nullptr;
	if (p_value.get_type() == Variant::NODE_PATH && p_value != NodePath()) {
		node_path_target = es->get_node(p_value);
	}

	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();

	ur->create_action(vformat(TTR("Set %s on %d nodes"), name, get_node_count()), UndoRedo::MERGE_ENDS);
	for (const NodePath &E : nodes) {
		Node *n = es->get_node_or_null(E);
		if (!n) {
			continue;
		}

		if (p_value.get_type() == Variant::NODE_PATH) {
			NodePath path;
			if (node_path_target) {
				path = n->get_path_to(node_path_target);
			}
			ur->add_do_property(n, name, path);
		} else {
			Variant new_value;
			if (p_field.is_empty()) {
				// whole value
				new_value = p_value;
			} else {
				// only one field
				new_value = fieldwise_assign(n->get(name), p_value, p_field);
			}
			ur->add_do_property(n, name, new_value);
		}

		ur->add_undo_property(n, name, n->get(name));
	}

	ur->commit_action();
	return true;
}

bool MultiNodeEdit::_get(const StringName &p_name, Variant &r_ret) const {
	Node *es = EditorNode::get_singleton()->get_edited_scene();
	if (!es) {
		return false;
	}

	String name = p_name;
	if (name == "scripts") { // Script set is intercepted at object level (check Variant Object::get()), so use a different name.
		name = "script";
	} else if (name.begins_with("Metadata/")) {
		name = name.replace_first("Metadata/", "metadata/");
	}

	for (const NodePath &E : nodes) {
		const Node *n = es->get_node_or_null(E);
		if (!n) {
			continue;
		}

		bool found;
		r_ret = n->get(name, &found);
		if (found) {
			return true;
		}
	}

	return false;
}

void MultiNodeEdit::_get_property_list(List<PropertyInfo> *p_list) const {
	HashMap<String, PLData> usage;

	Node *es = EditorNode::get_singleton()->get_edited_scene();
	if (!es) {
		return;
	}

	int nc = 0;

	List<PLData *> data_list;

	for (const NodePath &E : nodes) {
		Node *n = es->get_node_or_null(E);
		if (!n) {
			continue;
		}

		List<PropertyInfo> plist;
		n->get_property_list(&plist, true);

		for (PropertyInfo F : plist) {
			if (F.name == "script") {
				continue; // Added later manually, since this is intercepted before being set (check Variant Object::get()).
			} else if (F.name.begins_with("metadata/")) {
				F.name = F.name.replace_first("metadata/", "Metadata/"); // Trick to not get actual metadata edited from MultiNodeEdit.
			}

			if (!usage.has(F.name)) {
				PLData pld;
				pld.uses = 0;
				pld.info = F;
				pld.info.name = F.name;
				usage[F.name] = pld;
				data_list.push_back(usage.getptr(F.name));
			}

			// Make sure only properties with the same exact PropertyInfo data will appear.
			if (usage[F.name].info == F) {
				usage[F.name].uses++;
			}
		}

		nc++;
	}

	for (const PLData *E : data_list) {
		if (nc == E->uses) {
			p_list->push_back(E->info);
		}
	}

	p_list->push_back(PropertyInfo(Variant::OBJECT, "scripts", PROPERTY_HINT_RESOURCE_TYPE, "Script"));
}

String MultiNodeEdit::_get_editor_name() const {
	return vformat(TTR("%s (%d Selected)"), get_edited_class_name(), get_node_count());
}

bool MultiNodeEdit::_property_can_revert(const StringName &p_name) const {
	Node *es = EditorNode::get_singleton()->get_edited_scene();
	if (!es) {
		return false;
	}

	if (ClassDB::has_property(get_edited_class_name(), p_name)) {
		for (const NodePath &E : nodes) {
			Node *node = es->get_node_or_null(E);
			if (node) {
				return true;
			}
		}

		return false;
	}

	// Don't show the revert button if the edited class doesn't have the property.
	return false;
}

bool MultiNodeEdit::_property_get_revert(const StringName &p_name, Variant &r_property) const {
	Node *es = EditorNode::get_singleton()->get_edited_scene();
	if (!es) {
		return false;
	}

	for (const NodePath &E : nodes) {
		Node *node = es->get_node_or_null(E);
		if (!node) {
			continue;
		}

		r_property = ClassDB::class_get_default_property_value(node->get_class_name(), p_name);
		return true;
	}

	return false;
}

void MultiNodeEdit::add_node(const NodePath &p_node) {
	nodes.push_back(p_node);
}

int MultiNodeEdit::get_node_count() const {
	return nodes.size();
}

NodePath MultiNodeEdit::get_node(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, get_node_count(), NodePath());
	return nodes[p_index];
}

StringName MultiNodeEdit::get_edited_class_name() const {
	Node *es = EditorNode::get_singleton()->get_edited_scene();
	if (!es) {
		return SNAME("Node");
	}

	// Get the class name of the first node.
	StringName class_name;
	for (const NodePath &E : nodes) {
		Node *node = es->get_node_or_null(E);
		if (!node) {
			continue;
		}

		class_name = node->get_class_name();
		break;
	}

	if (class_name == StringName()) {
		return SNAME("Node");
	}

	bool check_again = true;
	while (check_again) {
		check_again = false;

		if (class_name == SNAME("Node") || class_name == StringName()) {
			// All nodes inherit from Node, so no need to continue checking.
			return SNAME("Node");
		}

		// Check that all nodes inherit from class_name.
		for (const NodePath &E : nodes) {
			Node *node = es->get_node_or_null(E);
			if (!node) {
				continue;
			}

			const StringName node_class_name = node->get_class_name();
			if (class_name == node_class_name || ClassDB::is_parent_class(node_class_name, class_name)) {
				// class_name is the same or a parent of the node's class.
				continue;
			}

			// class_name is not a parent of the node's class, so check again with the parent class.
			class_name = ClassDB::get_parent_class(class_name);
			check_again = true;
			break;
		}
	}

	return class_name;
}

void MultiNodeEdit::set_property_field(const StringName &p_property, const Variant &p_value, const String &p_field) {
	_set_impl(p_property, p_value, p_field);
}

void MultiNodeEdit::_bind_methods() {
	ClassDB::bind_method("_hide_script_from_inspector", &MultiNodeEdit::_hide_script_from_inspector);
	ClassDB::bind_method("_hide_metadata_from_inspector", &MultiNodeEdit::_hide_metadata_from_inspector);
	ClassDB::bind_method("_get_editor_name", &MultiNodeEdit::_get_editor_name);
}

MultiNodeEdit::MultiNodeEdit() {
}

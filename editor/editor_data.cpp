/**************************************************************************/
/*  editor_data.cpp                                                       */
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

#include "editor_data.h"

#include "core/config/project_settings.h"
#include "core/extension/gdextension_manager.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/multi_node_edit.h"
#include "editor/plugins/editor_context_menu_plugin.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/resources/packed_scene.h"

void EditorSelectionHistory::cleanup_history() {
	for (int i = 0; i < history.size(); i++) {
		bool fail = false;

		for (int j = 0; j < history[i].path.size(); j++) {
			if (history[i].path[j].ref.is_valid()) {
				// If the node is a MultiNodeEdit node, examine it and see if anything is missing from it.
				Ref<MultiNodeEdit> multi_node_edit = history[i].path[j].ref;
				if (multi_node_edit.is_valid()) {
					Node *root = EditorNode::get_singleton()->get_edited_scene();
					if (root) {
						for (int k = 0; k < multi_node_edit->get_node_count(); k++) {
							NodePath np = multi_node_edit->get_node(k);
							Node *multi_node_selected_node = root->get_node_or_null(np);
							if (!multi_node_selected_node) {
								fail = true;
								break;
							}
						}
					} else {
						fail = true;
					}
				} else {
					// Reference is not null - object still alive.
					continue;
				}
			}

			if (!fail) {
				Object *obj = ObjectDB::get_instance(history[i].path[j].object);
				if (obj) {
					Node *n = Object::cast_to<Node>(obj);
					if (n && n->is_inside_tree()) {
						// Node valid and inside tree - object still alive.
						continue;
					}
					if (!n) {
						// Node possibly still alive.
						continue;
					}
				} // Else: object not valid - not alive.

				fail = true;
			}

			if (fail) {
				break;
			}
		}

		if (fail) {
			history.remove_at(i);
			i--;
		}
	}

	if (current_elem_idx >= history.size()) {
		current_elem_idx = history.size() - 1;
	}
}

void EditorSelectionHistory::add_object(ObjectID p_object, const String &p_property, bool p_inspector_only) {
	Object *obj = ObjectDB::get_instance(p_object);
	ERR_FAIL_NULL(obj);
	RefCounted *r = Object::cast_to<RefCounted>(obj);
	_Object o;
	if (r) {
		o.ref = Ref<RefCounted>(r);
	}
	o.object = p_object;
	o.property = p_property;
	o.inspector_only = p_inspector_only;

	bool has_prev = current_elem_idx >= 0 && current_elem_idx < history.size();

	if (has_prev) {
		history.resize(current_elem_idx + 1); // Clip history to next.
	}

	HistoryElement h;
	if (!p_property.is_empty() && has_prev) {
		// Add a sub property.
		HistoryElement &prev_element = history.write[current_elem_idx];
		h = prev_element;
		h.path.resize(h.level + 1);
		h.path.push_back(o);
		h.level++;

	} else {
		// Create a new history item.
		h.path.push_back(o);
		h.level = 0;
	}

	history.push_back(h);
	current_elem_idx++;
}

void EditorSelectionHistory::replace_object(ObjectID p_old_object, ObjectID p_new_object) {
	for (HistoryElement &element : history) {
		for (int index = 0; index < element.path.size(); index++) {
			if (element.path[index].object == p_old_object) {
				element.path.write[index].object = p_new_object;
			}
		}
	}
}

int EditorSelectionHistory::get_history_len() {
	return history.size();
}

int EditorSelectionHistory::get_history_pos() {
	return current_elem_idx;
}

ObjectID EditorSelectionHistory::get_history_obj(int p_obj) const {
	ERR_FAIL_INDEX_V(p_obj, history.size(), ObjectID());
	ERR_FAIL_INDEX_V(history[p_obj].level, history[p_obj].path.size(), ObjectID());
	return history[p_obj].path[history[p_obj].level].object;
}

bool EditorSelectionHistory::is_at_beginning() const {
	return current_elem_idx <= 0;
}

bool EditorSelectionHistory::is_at_end() const {
	return ((current_elem_idx + 1) >= history.size());
}

bool EditorSelectionHistory::next() {
	cleanup_history();

	if ((current_elem_idx + 1) < history.size()) {
		current_elem_idx++;
	} else {
		return false;
	}

	return true;
}

bool EditorSelectionHistory::previous() {
	cleanup_history();

	if (current_elem_idx > 0) {
		current_elem_idx--;
	} else {
		return false;
	}

	return true;
}

bool EditorSelectionHistory::is_current_inspector_only() const {
	if (current_elem_idx < 0 || current_elem_idx >= history.size()) {
		return false;
	}

	const HistoryElement &h = history[current_elem_idx];
	return h.path[h.level].inspector_only;
}

ObjectID EditorSelectionHistory::get_current() {
	if (current_elem_idx < 0 || current_elem_idx >= history.size()) {
		return ObjectID();
	}

	Object *obj = ObjectDB::get_instance(get_history_obj(current_elem_idx));
	return obj ? obj->get_instance_id() : ObjectID();
}

int EditorSelectionHistory::get_path_size() const {
	if (current_elem_idx < 0 || current_elem_idx >= history.size()) {
		return 0;
	}

	return history[current_elem_idx].path.size();
}

ObjectID EditorSelectionHistory::get_path_object(int p_index) const {
	if (current_elem_idx < 0 || current_elem_idx >= history.size()) {
		return ObjectID();
	}

	ERR_FAIL_INDEX_V(p_index, history[current_elem_idx].path.size(), ObjectID());

	Object *obj = ObjectDB::get_instance(history[current_elem_idx].path[p_index].object);
	return obj ? obj->get_instance_id() : ObjectID();
}

String EditorSelectionHistory::get_path_property(int p_index) const {
	if (current_elem_idx < 0 || current_elem_idx >= history.size()) {
		return "";
	}

	ERR_FAIL_INDEX_V(p_index, history[current_elem_idx].path.size(), "");
	return history[current_elem_idx].path[p_index].property;
}

void EditorSelectionHistory::clear() {
	history.clear();
	current_elem_idx = -1;
}

EditorSelectionHistory::EditorSelectionHistory() {
	current_elem_idx = -1;
}

////////////////////////////////////////////////////////////

EditorPlugin *EditorData::get_handling_main_editor(Object *p_object) {
	// We need to iterate backwards so that we can check user-created plugins first.
	// Otherwise, it would not be possible for plugins to handle CanvasItem and Spatial nodes.
	for (int i = editor_plugins.size() - 1; i > -1; i--) {
		if (editor_plugins[i]->has_main_screen() && editor_plugins[i]->handles(p_object)) {
			return editor_plugins[i];
		}
	}

	return nullptr;
}

Vector<EditorPlugin *> EditorData::get_handling_sub_editors(Object *p_object) {
	Vector<EditorPlugin *> sub_plugins;
	for (int i = editor_plugins.size() - 1; i > -1; i--) {
		if (!editor_plugins[i]->has_main_screen() && editor_plugins[i]->handles(p_object)) {
			sub_plugins.push_back(editor_plugins[i]);
		}
	}
	return sub_plugins;
}

EditorPlugin *EditorData::get_editor_by_name(const String &p_name) {
	for (int i = editor_plugins.size() - 1; i > -1; i--) {
		if (editor_plugins[i]->get_plugin_name() == p_name) {
			return editor_plugins[i];
		}
	}

	return nullptr;
}

void EditorData::copy_object_params(Object *p_object) {
	clipboard.clear();

	List<PropertyInfo> pinfo;
	p_object->get_property_list(&pinfo);

	for (const PropertyInfo &E : pinfo) {
		if (!(E.usage & PROPERTY_USAGE_EDITOR) || E.name == "script" || E.name == "scripts" || E.name == "resource_path") {
			continue;
		}

		PropertyData pd;
		pd.name = E.name;
		pd.value = p_object->get(pd.name);
		clipboard.push_back(pd);
	}
}

void EditorData::get_editor_breakpoints(List<String> *p_breakpoints) {
	for (int i = 0; i < editor_plugins.size(); i++) {
		editor_plugins[i]->get_breakpoints(p_breakpoints);
	}
}

Dictionary EditorData::get_editor_plugin_states() const {
	Dictionary metadata;
	for (int i = 0; i < editor_plugins.size(); i++) {
		Dictionary state = editor_plugins[i]->get_state();
		if (state.is_empty()) {
			continue;
		}
		metadata[editor_plugins[i]->get_plugin_name()] = state;
	}

	return metadata;
}

Dictionary EditorData::get_scene_editor_states(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), Dictionary());
	EditedScene es = edited_scene[p_idx];
	return es.editor_states;
}

void EditorData::set_editor_plugin_states(const Dictionary &p_states) {
	if (p_states.is_empty()) {
		for (EditorPlugin *ep : editor_plugins) {
			ep->clear();
		}
		return;
	}

	List<Variant> keys;
	p_states.get_key_list(&keys);

	List<Variant>::Element *E = keys.front();
	for (; E; E = E->next()) {
		String name = E->get();
		int idx = -1;
		for (int i = 0; i < editor_plugins.size(); i++) {
			if (editor_plugins[i]->get_plugin_name() == name) {
				idx = i;
				break;
			}
		}

		if (idx == -1) {
			continue;
		}
		editor_plugins[idx]->set_state(p_states[name]);
	}
}

void EditorData::notify_edited_scene_changed() {
	for (int i = 0; i < editor_plugins.size(); i++) {
		editor_plugins[i]->edited_scene_changed();
		editor_plugins[i]->notify_scene_changed(get_edited_scene_root());
	}
}

void EditorData::notify_resource_saved(const Ref<Resource> &p_resource) {
	for (int i = 0; i < editor_plugins.size(); i++) {
		editor_plugins[i]->notify_resource_saved(p_resource);
	}
}

void EditorData::notify_scene_saved(const String &p_path) {
	for (int i = 0; i < editor_plugins.size(); i++) {
		editor_plugins[i]->notify_scene_saved(p_path);
	}
}

void EditorData::clear_editor_states() {
	for (int i = 0; i < editor_plugins.size(); i++) {
		editor_plugins[i]->clear();
	}
}

void EditorData::save_editor_external_data() {
	for (int i = 0; i < editor_plugins.size(); i++) {
		editor_plugins[i]->save_external_data();
	}
}

void EditorData::apply_changes_in_editors() {
	for (int i = 0; i < editor_plugins.size(); i++) {
		editor_plugins[i]->apply_changes();
	}
}

void EditorData::paste_object_params(Object *p_object) {
	ERR_FAIL_NULL(p_object);
	undo_redo_manager->create_action(TTR("Paste Params"));
	for (const PropertyData &E : clipboard) {
		String name = E.name;
		undo_redo_manager->add_do_property(p_object, name, E.value);
		undo_redo_manager->add_undo_property(p_object, name, p_object->get(name));
	}
	undo_redo_manager->commit_action();
}

bool EditorData::call_build() {
	bool result = true;

	for (int i = 0; i < editor_plugins.size() && result; i++) {
		result &= editor_plugins[i]->build();
	}

	return result;
}

void EditorData::set_scene_as_saved(int p_idx) {
	if (p_idx == -1) {
		p_idx = current_edited_scene;
	}
	ERR_FAIL_INDEX(p_idx, edited_scene.size());

	undo_redo_manager->set_history_as_saved(edited_scene[p_idx].history_id);
}

bool EditorData::is_scene_changed(int p_idx) {
	if (p_idx == -1) {
		p_idx = current_edited_scene;
	}
	ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), false);

	uint64_t current_scene_version = undo_redo_manager->get_or_create_history(edited_scene[p_idx].history_id).undo_redo->get_version();
	bool is_changed = edited_scene[p_idx].last_checked_version != current_scene_version;
	edited_scene.write[p_idx].last_checked_version = current_scene_version;
	return is_changed;
}

int EditorData::get_scene_history_id_from_path(const String &p_path) const {
	for (const EditedScene &E : edited_scene) {
		if (E.path == p_path) {
			return E.history_id;
		}
	}
	return 0;
}

int EditorData::get_current_edited_scene_history_id() const {
	if (current_edited_scene != -1) {
		return edited_scene[current_edited_scene].history_id;
	}
	return 0;
}

int EditorData::get_scene_history_id(int p_idx) const {
	return edited_scene[p_idx].history_id;
}

void EditorData::add_undo_redo_inspector_hook_callback(Callable p_callable) {
	undo_redo_callbacks.push_back(p_callable);
}

void EditorData::remove_undo_redo_inspector_hook_callback(Callable p_callable) {
	undo_redo_callbacks.erase(p_callable);
}

const Vector<Callable> EditorData::get_undo_redo_inspector_hook_callback() {
	return undo_redo_callbacks;
}

void EditorData::add_move_array_element_function(const StringName &p_class, Callable p_callable) {
	move_element_functions.insert(p_class, p_callable);
}

void EditorData::remove_move_array_element_function(const StringName &p_class) {
	move_element_functions.erase(p_class);
}

Callable EditorData::get_move_array_element_function(const StringName &p_class) const {
	if (move_element_functions.has(p_class)) {
		return move_element_functions[p_class];
	}
	return Callable();
}

void EditorData::remove_editor_plugin(EditorPlugin *p_plugin) {
	editor_plugins.erase(p_plugin);
}

void EditorData::add_editor_plugin(EditorPlugin *p_plugin) {
	editor_plugins.push_back(p_plugin);
}

int EditorData::get_editor_plugin_count() const {
	return editor_plugins.size();
}

EditorPlugin *EditorData::get_editor_plugin(int p_idx) {
	ERR_FAIL_INDEX_V(p_idx, editor_plugins.size(), nullptr);
	return editor_plugins[p_idx];
}

void EditorData::add_extension_editor_plugin(const StringName &p_class_name, EditorPlugin *p_plugin) {
	ERR_FAIL_COND(extension_editor_plugins.has(p_class_name));
	extension_editor_plugins.insert(p_class_name, p_plugin);
}

void EditorData::remove_extension_editor_plugin(const StringName &p_class_name) {
	extension_editor_plugins.erase(p_class_name);
}

bool EditorData::has_extension_editor_plugin(const StringName &p_class_name) {
	return extension_editor_plugins.has(p_class_name);
}

EditorPlugin *EditorData::get_extension_editor_plugin(const StringName &p_class_name) {
	EditorPlugin **plugin = extension_editor_plugins.getptr(p_class_name);
	return plugin == nullptr ? nullptr : *plugin;
}

void EditorData::add_custom_type(const String &p_type, const String &p_inherits, const Ref<Script> &p_script, const Ref<Texture2D> &p_icon) {
	ERR_FAIL_COND_MSG(p_script.is_null(), "It's not a reference to a valid Script object.");
	CustomType ct;
	ct.name = p_type;
	ct.icon = p_icon;
	ct.script = p_script;

	if (!custom_types.has(p_inherits)) {
		custom_types[p_inherits] = Vector<CustomType>();
	}
	custom_types[p_inherits].push_back(ct);
}

Variant EditorData::instantiate_custom_type(const String &p_type, const String &p_inherits) {
	if (get_custom_types().has(p_inherits)) {
		for (int i = 0; i < get_custom_types()[p_inherits].size(); i++) {
			if (get_custom_types()[p_inherits][i].name == p_type) {
				Ref<Script> script = get_custom_types()[p_inherits][i].script;

				Variant ob = ClassDB::instantiate(p_inherits);
				ERR_FAIL_COND_V(!ob, Variant());
				Node *n = Object::cast_to<Node>(ob);
				if (n) {
					n->set_name(p_type);
				}
				n->set_meta(SceneStringName(_custom_type_script), script);
				((Object *)ob)->set_script(script);
				return ob;
			}
		}
	}

	return Variant();
}

const EditorData::CustomType *EditorData::get_custom_type_by_name(const String &p_type) const {
	for (const KeyValue<String, Vector<CustomType>> &E : custom_types) {
		for (const CustomType &F : E.value) {
			if (F.name == p_type) {
				return &F;
			}
		}
	}
	return nullptr;
}

const EditorData::CustomType *EditorData::get_custom_type_by_path(const String &p_path) const {
	for (const KeyValue<String, Vector<CustomType>> &E : custom_types) {
		for (const CustomType &F : E.value) {
			if (F.script->get_path() == p_path) {
				return &F;
			}
		}
	}
	return nullptr;
}

bool EditorData::is_type_recognized(const String &p_type) const {
	return ClassDB::class_exists(p_type) || ScriptServer::is_global_class(p_type) || get_custom_type_by_name(p_type);
}

void EditorData::remove_custom_type(const String &p_type) {
	for (KeyValue<String, Vector<CustomType>> &E : custom_types) {
		for (int i = 0; i < E.value.size(); i++) {
			if (E.value[i].name == p_type) {
				E.value.remove_at(i);
				if (E.value.is_empty()) {
					custom_types.erase(E.key);
				}
				return;
			}
		}
	}
}

void EditorData::instantiate_object_properties(Object *p_object) {
	ERR_FAIL_NULL(p_object);
	// Check if any Object-type property should be instantiated.
	List<PropertyInfo> pinfo;
	p_object->get_property_list(&pinfo);

	for (List<PropertyInfo>::Element *E = pinfo.front(); E; E = E->next()) {
		PropertyInfo pi = E->get();
		if (pi.type == Variant::OBJECT && pi.usage & PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT) {
			Object *prop = ClassDB::instantiate(pi.class_name);
			p_object->set(pi.name, prop);
		}
	}
}

int EditorData::add_edited_scene(int p_at_pos) {
	if (p_at_pos < 0) {
		p_at_pos = edited_scene.size();
	}
	EditedScene es;
	es.root = nullptr;
	es.path = String();
	es.file_modified_time = 0;
	es.history_current = -1;
	es.live_edit_root = NodePath(String("/root"));
	es.history_id = last_created_scene++;

	if (p_at_pos == edited_scene.size()) {
		edited_scene.push_back(es);
	} else {
		edited_scene.insert(p_at_pos, es);
	}

	if (current_edited_scene < 0) {
		current_edited_scene = 0;
	}
	return p_at_pos;
}

void EditorData::move_edited_scene_index(int p_idx, int p_to_idx) {
	ERR_FAIL_INDEX(p_idx, edited_scene.size());
	ERR_FAIL_INDEX(p_to_idx, edited_scene.size());
	SWAP(edited_scene.write[p_idx], edited_scene.write[p_to_idx]);
}

void EditorData::remove_scene(int p_idx) {
	ERR_FAIL_INDEX(p_idx, edited_scene.size());
	if (edited_scene[p_idx].root) {
		for (int i = 0; i < editor_plugins.size(); i++) {
			editor_plugins[i]->notify_scene_closed(edited_scene[p_idx].root->get_scene_file_path());
		}

		memdelete(edited_scene[p_idx].root);
		edited_scene.write[p_idx].root = nullptr;
	}

	if (current_edited_scene > p_idx) {
		current_edited_scene--;
	} else if (current_edited_scene == p_idx && current_edited_scene > 0) {
		current_edited_scene--;
	}

	if (!edited_scene[p_idx].path.is_empty()) {
		EditorNode::get_singleton()->emit_signal("scene_closed", edited_scene[p_idx].path);
	}

	if (undo_redo_manager->has_history(edited_scene[p_idx].history_id)) { // Might not exist if scene failed to load.
		undo_redo_manager->discard_history(edited_scene[p_idx].history_id);
	}
	edited_scene.remove_at(p_idx);
}

bool EditorData::_find_updated_instances(Node *p_root, Node *p_node, HashSet<String> &checked_paths) {
	Ref<SceneState> ss;

	if (p_node == p_root) {
		ss = p_node->get_scene_inherited_state();
	} else if (!p_node->get_scene_file_path().is_empty()) {
		ss = p_node->get_scene_instance_state();
	}

	if (ss.is_valid()) {
		String path = ss->get_path();

		if (!checked_paths.has(path)) {
			uint64_t modified_time = FileAccess::get_modified_time(path);
			if (modified_time != ss->get_last_modified_time()) {
				return true; //external scene changed
			}

			checked_paths.insert(path);
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		bool found = _find_updated_instances(p_root, p_node->get_child(i), checked_paths);
		if (found) {
			return true;
		}
	}

	return false;
}

bool EditorData::check_and_update_scene(int p_idx) {
	ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), false);
	if (!edited_scene[p_idx].root) {
		return false;
	}

	HashSet<String> checked_scenes;

	bool must_reload = _find_updated_instances(edited_scene[p_idx].root, edited_scene[p_idx].root, checked_scenes);

	if (must_reload) {
		Ref<PackedScene> pscene;
		pscene.instantiate();

		EditorProgress ep("update_scene", TTR("Updating Scene"), 2);
		ep.step(TTR("Storing local changes..."), 0);
		// Pack first, so it stores diffs to previous version of saved scene.
		Error err = pscene->pack(edited_scene[p_idx].root);
		ERR_FAIL_COND_V(err != OK, false);
		ep.step(TTR("Updating scene..."), 1);
		Node *new_scene = pscene->instantiate(PackedScene::GEN_EDIT_STATE_MAIN);
		ERR_FAIL_NULL_V(new_scene, false);

		// Transfer selection.
		List<Node *> new_selection;
		for (const Node *E : edited_scene.write[p_idx].selection) {
			NodePath p = edited_scene[p_idx].root->get_path_to(E);
			Node *new_node = new_scene->get_node(p);
			if (new_node) {
				new_selection.push_back(new_node);
			}
		}

		new_scene->set_scene_file_path(edited_scene[p_idx].root->get_scene_file_path());
		Node *old_root = edited_scene[p_idx].root;
		EditorNode::get_singleton()->set_edited_scene(new_scene);
		memdelete(old_root);
		edited_scene.write[p_idx].selection = new_selection;

		return true;
	}

	return false;
}

int EditorData::get_edited_scene() const {
	return current_edited_scene;
}

int EditorData::get_edited_scene_from_path(const String &p_path) const {
	for (int i = 0; i < edited_scene.size(); i++) {
		if (edited_scene[i].path == p_path) {
			return i;
		}
	}

	return -1;
}

void EditorData::set_edited_scene(int p_idx) {
	ERR_FAIL_INDEX(p_idx, edited_scene.size());
	current_edited_scene = p_idx;
}

Node *EditorData::get_edited_scene_root(int p_idx) {
	if (p_idx < 0) {
		ERR_FAIL_INDEX_V(current_edited_scene, edited_scene.size(), nullptr);
		return edited_scene[current_edited_scene].root;
	} else {
		ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), nullptr);
		return edited_scene[p_idx].root;
	}
}

void EditorData::set_edited_scene_root(Node *p_root) {
	ERR_FAIL_INDEX(current_edited_scene, edited_scene.size());
	edited_scene.write[current_edited_scene].root = p_root;
	if (p_root) {
		if (!p_root->get_scene_file_path().is_empty()) {
			edited_scene.write[current_edited_scene].path = p_root->get_scene_file_path();
		} else {
			p_root->set_scene_file_path(edited_scene[current_edited_scene].path);
		}
	}

	if (!edited_scene[current_edited_scene].path.is_empty()) {
		edited_scene.write[current_edited_scene].file_modified_time = FileAccess::get_modified_time(edited_scene[current_edited_scene].path);
	}
}

int EditorData::get_edited_scene_count() const {
	return edited_scene.size();
}

Vector<EditorData::EditedScene> EditorData::get_edited_scenes() const {
	Vector<EditedScene> out_edited_scenes_list = Vector<EditedScene>();

	for (int i = 0; i < edited_scene.size(); i++) {
		out_edited_scenes_list.push_back(edited_scene[i]);
	}

	return out_edited_scenes_list;
}

void EditorData::set_scene_modified_time(int p_idx, uint64_t p_time) {
	if (p_idx == -1) {
		p_idx = current_edited_scene;
	}
	ERR_FAIL_INDEX(p_idx, edited_scene.size());

	edited_scene.write[p_idx].file_modified_time = p_time;
}

uint64_t EditorData::get_scene_modified_time(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), 0);
	return edited_scene[p_idx].file_modified_time;
}

String EditorData::get_scene_type(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), String());
	if (!edited_scene[p_idx].root) {
		return "";
	}
	return edited_scene[p_idx].root->get_class();
}

void EditorData::move_edited_scene_to_index(int p_idx) {
	ERR_FAIL_INDEX(current_edited_scene, edited_scene.size());
	ERR_FAIL_INDEX(p_idx, edited_scene.size());

	EditedScene es = edited_scene[current_edited_scene];
	edited_scene.remove_at(current_edited_scene);
	edited_scene.insert(p_idx, es);
	current_edited_scene = p_idx;
}

Ref<Script> EditorData::get_scene_root_script(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), Ref<Script>());
	if (!edited_scene[p_idx].root) {
		return Ref<Script>();
	}
	Ref<Script> s = edited_scene[p_idx].root->get_script();
	if (s.is_null() && edited_scene[p_idx].root->get_child_count()) {
		Node *n = edited_scene[p_idx].root->get_child(0);
		while (s.is_null() && n && n->get_scene_file_path().is_empty()) {
			s = n->get_script();
			n = n->get_parent();
		}
	}
	return s;
}

String EditorData::get_scene_title(int p_idx, bool p_always_strip_extension) const {
	ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), String());
	if (!edited_scene[p_idx].root) {
		return TTR("[empty]");
	}
	if (edited_scene[p_idx].root->get_scene_file_path().is_empty()) {
		return TTR("[unsaved]");
	}

	const String filename = edited_scene[p_idx].root->get_scene_file_path().get_file();
	const String basename = filename.get_basename();

	if (p_always_strip_extension) {
		return basename;
	}

	// Return the filename including the extension if there's ambiguity (e.g. both `foo.tscn` and `foo.scn` are being edited).
	for (int i = 0; i < edited_scene.size(); i++) {
		if (i == p_idx) {
			// Don't compare the edited scene against itself.
			continue;
		}

		if (edited_scene[i].root && basename == edited_scene[i].root->get_scene_file_path().get_file().get_basename()) {
			return filename;
		}
	}

	// Else, return just the basename as there's no ambiguity.
	return basename;
}

void EditorData::set_scene_path(int p_idx, const String &p_path) {
	ERR_FAIL_INDEX(p_idx, edited_scene.size());
	edited_scene.write[p_idx].path = p_path;

	if (!edited_scene[p_idx].root) {
		return;
	}
	edited_scene[p_idx].root->set_scene_file_path(p_path);
}

String EditorData::get_scene_path(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), String());

	if (edited_scene[p_idx].root) {
		if (edited_scene[p_idx].root->get_scene_file_path().is_empty()) {
			edited_scene[p_idx].root->set_scene_file_path(edited_scene[p_idx].path);
		} else {
			return edited_scene[p_idx].root->get_scene_file_path();
		}
	}

	return edited_scene[p_idx].path;
}

void EditorData::set_edited_scene_live_edit_root(const NodePath &p_root) {
	ERR_FAIL_INDEX(current_edited_scene, edited_scene.size());

	edited_scene.write[current_edited_scene].live_edit_root = p_root;
}

NodePath EditorData::get_edited_scene_live_edit_root() {
	ERR_FAIL_INDEX_V(current_edited_scene, edited_scene.size(), String());

	return edited_scene[current_edited_scene].live_edit_root;
}

void EditorData::save_edited_scene_state(EditorSelection *p_selection, EditorSelectionHistory *p_history, const Dictionary &p_custom) {
	ERR_FAIL_INDEX(current_edited_scene, edited_scene.size());

	EditedScene &es = edited_scene.write[current_edited_scene];
	es.selection = p_selection->get_full_selected_node_list();
	es.history_current = p_history->current_elem_idx;
	es.history_stored = p_history->history;
	es.editor_states = get_editor_plugin_states();
	es.custom_state = p_custom;
}

Dictionary EditorData::restore_edited_scene_state(EditorSelection *p_selection, EditorSelectionHistory *p_history) {
	ERR_FAIL_INDEX_V(current_edited_scene, edited_scene.size(), Dictionary());

	const EditedScene &es = edited_scene.write[current_edited_scene];

	p_history->current_elem_idx = es.history_current;
	p_history->history = es.history_stored;

	p_selection->clear();
	for (Node *E : es.selection) {
		p_selection->add_node(E);
	}
	set_editor_plugin_states(es.editor_states);

	return es.custom_state;
}

void EditorData::clear_edited_scenes() {
	for (int i = 0; i < edited_scene.size(); i++) {
		if (edited_scene[i].root) {
			memdelete(edited_scene[i].root);
		}
	}
	edited_scene.clear();
}

void EditorData::set_plugin_window_layout(Ref<ConfigFile> p_layout) {
	for (int i = 0; i < editor_plugins.size(); i++) {
		editor_plugins[i]->set_window_layout(p_layout);
	}
}

void EditorData::get_plugin_window_layout(Ref<ConfigFile> p_layout) {
	for (int i = 0; i < editor_plugins.size(); i++) {
		editor_plugins[i]->get_window_layout(p_layout);
	}
}

bool EditorData::script_class_is_parent(const String &p_class, const String &p_inherits) {
	if (!ScriptServer::is_global_class(p_class)) {
		return false;
	}

	String base = p_class;
	while (base != p_inherits) {
		if (ClassDB::class_exists(base)) {
			return ClassDB::is_parent_class(base, p_inherits);
		} else if (ScriptServer::is_global_class(base)) {
			base = ScriptServer::get_global_class_base(base);
		} else {
			return false;
		}
	}
	return true;
}

StringName EditorData::script_class_get_base(const String &p_class) const {
	Ref<Script> script = script_class_load_script(p_class);
	if (script.is_null()) {
		return StringName();
	}

	Ref<Script> base_script = script->get_base_script();
	if (base_script.is_null()) {
		return ScriptServer::get_global_class_base(p_class);
	}

	return script->get_language()->get_global_class_name(base_script->get_path());
}

Variant EditorData::script_class_instance(const String &p_class) {
	if (ScriptServer::is_global_class(p_class)) {
		Ref<Script> script = script_class_load_script(p_class);
		if (script.is_valid()) {
			// Store in a variant to initialize the refcount if needed.
			Variant obj = ClassDB::instantiate(script->get_instance_base_type());
			if (obj) {
				Object::cast_to<Object>(obj)->set_meta(SceneStringName(_custom_type_script), script);
				obj.operator Object *()->set_script(script);
			}
			return obj;
		}
	}
	return Variant();
}

Ref<Script> EditorData::script_class_load_script(const String &p_class) const {
	if (!ScriptServer::is_global_class(p_class)) {
		return Ref<Script>();
	}

	String path = ScriptServer::get_global_class_path(p_class);
	return ResourceLoader::load(path, "Script");
}

void EditorData::script_class_set_icon_path(const String &p_class, const String &p_icon_path) {
	_script_class_icon_paths[p_class] = p_icon_path;
}

String EditorData::script_class_get_icon_path(const String &p_class) const {
	if (!ScriptServer::is_global_class(p_class)) {
		return String();
	}

	String current = p_class;
	String ret = _script_class_icon_paths[current];
	while (ret.is_empty()) {
		current = script_class_get_base(current);
		if (!ScriptServer::is_global_class(current)) {
			return String();
		}
		ret = _script_class_icon_paths.has(current) ? _script_class_icon_paths[current] : String();
	}

	return ret;
}

StringName EditorData::script_class_get_name(const String &p_path) const {
	return _script_class_file_to_path.has(p_path) ? _script_class_file_to_path[p_path] : StringName();
}

void EditorData::script_class_set_name(const String &p_path, const StringName &p_class) {
	_script_class_file_to_path[p_path] = p_class;
}

void EditorData::script_class_save_icon_paths() {
	Array script_classes = ProjectSettings::get_singleton()->get_global_class_list();

	Dictionary d;
	for (const KeyValue<StringName, String> &E : _script_class_icon_paths) {
		if (ScriptServer::is_global_class(E.key)) {
			d[E.key] = E.value;
		}
	}

	for (int i = 0; i < script_classes.size(); i++) {
		Dictionary d2 = script_classes[i];
		if (!d2.has("class")) {
			continue;
		}
		d2["icon"] = d.get(d2["class"], "");
	}
	ProjectSettings::get_singleton()->store_global_class_list(script_classes);
}

void EditorData::script_class_load_icon_paths() {
	script_class_clear_icon_paths();

#ifndef DISABLE_DEPRECATED
	if (ProjectSettings::get_singleton()->has_setting("_global_script_class_icons")) {
		Dictionary d = GLOBAL_GET("_global_script_class_icons");
		List<Variant> keys;
		d.get_key_list(&keys);

		for (const Variant &E : keys) {
			String name = E.operator String();
			_script_class_icon_paths[name] = d[name];

			String path = ScriptServer::get_global_class_path(name);
			script_class_set_name(path, name);
		}
		ProjectSettings::get_singleton()->clear("_global_script_class_icons");
	}
#endif

	Array script_classes = ProjectSettings::get_singleton()->get_global_class_list();
	for (int i = 0; i < script_classes.size(); i++) {
		Dictionary d = script_classes[i];
		if (!d.has("class") || !d.has("path") || !d.has("icon")) {
			continue;
		}

		String name = d["class"];
		_script_class_icon_paths[name] = d["icon"];
		script_class_set_name(d["path"], name);
	}
}

Ref<Texture2D> EditorData::extension_class_get_icon(const String &p_class) const {
	if (GDExtensionManager::get_singleton()->class_has_icon_path(p_class)) {
		String icon_path = GDExtensionManager::get_singleton()->class_get_icon_path(p_class);
		Ref<Texture2D> icon = _load_script_icon(icon_path);
		if (icon.is_valid()) {
			return icon;
		}
	}
	return nullptr;
}

Ref<Texture2D> EditorData::_load_script_icon(const String &p_path) const {
	if (!p_path.is_empty() && ResourceLoader::exists(p_path)) {
		Ref<Texture2D> icon = ResourceLoader::load(p_path);
		if (icon.is_valid()) {
			return icon;
		}
	}
	return nullptr;
}

Ref<Texture2D> EditorData::get_script_icon(const Ref<Script> &p_script) {
	// Take from the local cache, if available.
	if (_script_icon_cache.has(p_script)) {
		// Can be an empty value if we can't resolve any icon for this script.
		// An empty value is still cached to avoid unnecessary attempts at resolving it again.
		return _script_icon_cache[p_script];
	}

	Ref<Script> base_scr = p_script;
	while (base_scr.is_valid()) {
		// Check for scripted classes.
		String icon_path;
		StringName class_name = script_class_get_name(base_scr->get_path());
		if (base_scr->is_built_in() || class_name == StringName()) {
			icon_path = base_scr->get_class_icon_path();
		} else {
			icon_path = script_class_get_icon_path(class_name);
		}

		Ref<Texture2D> icon = _load_script_icon(icon_path);
		if (icon.is_valid()) {
			_script_icon_cache[p_script] = icon;
			return icon;
		}

		// Check for legacy custom classes defined by plugins.
		// TODO: Should probably be deprecated in 4.x
		const EditorData::CustomType *ctype = get_custom_type_by_path(base_scr->get_path());
		if (ctype && ctype->icon.is_valid()) {
			_script_icon_cache[p_script] = ctype->icon;
			return ctype->icon;
		}

		// Move to the base class.
		base_scr = base_scr->get_base_script();
	}

	// No custom icon was found in the inheritance chain, so check the base
	// class of the script instead.
	String base_type;
	p_script->get_language()->get_global_class_name(p_script->get_path(), &base_type);

	// Check if the base type is an extension-defined type.
	Ref<Texture2D> ext_icon = extension_class_get_icon(base_type);
	if (ext_icon.is_valid()) {
		_script_icon_cache[p_script] = ext_icon;
		return ext_icon;
	}

	// If no icon found, cache it as null.
	_script_icon_cache[p_script] = Ref<Texture>();
	return nullptr;
}

void EditorData::clear_script_icon_cache() {
	_script_icon_cache.clear();
}

EditorData::EditorData() {
	undo_redo_manager = memnew(EditorUndoRedoManager);
	script_class_load_icon_paths();
}

EditorData::~EditorData() {
	memdelete(undo_redo_manager);
}

///////////////////////////////////////////////////////////////////////////////

void EditorSelection::_node_removed(Node *p_node) {
	if (!selection.has(p_node)) {
		return;
	}

	Object *meta = selection[p_node];
	if (meta) {
		memdelete(meta);
	}
	selection.erase(p_node);
	changed = true;
	node_list_changed = true;
}

void EditorSelection::add_node(Node *p_node) {
	ERR_FAIL_NULL(p_node);
	ERR_FAIL_COND(!p_node->is_inside_tree());
	if (selection.has(p_node)) {
		return;
	}

	changed = true;
	node_list_changed = true;
	Object *meta = nullptr;
	for (Object *E : editor_plugins) {
		meta = E->call("_get_editor_data", p_node);
		if (meta) {
			break;
		}
	}
	selection[p_node] = meta;

	p_node->connect(SceneStringName(tree_exiting), callable_mp(this, &EditorSelection::_node_removed).bind(p_node), CONNECT_ONE_SHOT);
}

void EditorSelection::remove_node(Node *p_node) {
	ERR_FAIL_NULL(p_node);
	if (!selection.has(p_node)) {
		return;
	}

	changed = true;
	node_list_changed = true;
	Object *meta = selection[p_node];
	if (meta) {
		memdelete(meta);
	}
	selection.erase(p_node);

	p_node->disconnect(SceneStringName(tree_exiting), callable_mp(this, &EditorSelection::_node_removed));
}

bool EditorSelection::is_selected(Node *p_node) const {
	return selection.has(p_node);
}

void EditorSelection::_bind_methods() {
	ClassDB::bind_method(D_METHOD("clear"), &EditorSelection::clear);
	ClassDB::bind_method(D_METHOD("add_node", "node"), &EditorSelection::add_node);
	ClassDB::bind_method(D_METHOD("remove_node", "node"), &EditorSelection::remove_node);
	ClassDB::bind_method(D_METHOD("get_selected_nodes"), &EditorSelection::get_selected_nodes);
	ClassDB::bind_method(D_METHOD("get_transformable_selected_nodes"), &EditorSelection::_get_transformable_selected_nodes);
	ADD_SIGNAL(MethodInfo("selection_changed"));
}

void EditorSelection::add_editor_plugin(Object *p_object) {
	editor_plugins.push_back(p_object);
}

void EditorSelection::_update_node_list() {
	if (!node_list_changed) {
		return;
	}

	selected_node_list.clear();

	// If the selection does not have the parent of the selected node, then add the node to the node list.
	// However, if the parent is already selected, then adding this node is redundant as
	// it is included with the parent, so skip it.
	for (const KeyValue<Node *, Object *> &E : selection) {
		Node *parent = E.key;
		parent = parent->get_parent();
		bool skip = false;
		while (parent) {
			if (selection.has(parent)) {
				skip = true;
				break;
			}
			parent = parent->get_parent();
		}

		if (skip) {
			continue;
		}
		selected_node_list.push_back(E.key);
	}

	node_list_changed = true;
}

void EditorSelection::update() {
	_update_node_list();

	if (!changed) {
		return;
	}
	changed = false;
	if (!emitted) {
		emitted = true;
		callable_mp(this, &EditorSelection::_emit_change).call_deferred();
	}
}

void EditorSelection::_emit_change() {
	emit_signal(SNAME("selection_changed"));
	emitted = false;
}

TypedArray<Node> EditorSelection::_get_transformable_selected_nodes() {
	TypedArray<Node> ret;

	for (const Node *E : selected_node_list) {
		ret.push_back(E);
	}

	return ret;
}

TypedArray<Node> EditorSelection::get_selected_nodes() {
	TypedArray<Node> ret;

	for (const KeyValue<Node *, Object *> &E : selection) {
		ret.push_back(E.key);
	}

	return ret;
}

List<Node *> &EditorSelection::get_selected_node_list() {
	if (changed) {
		update();
	} else {
		_update_node_list();
	}
	return selected_node_list;
}

List<Node *> EditorSelection::get_full_selected_node_list() {
	List<Node *> node_list;
	for (const KeyValue<Node *, Object *> &E : selection) {
		node_list.push_back(E.key);
	}

	return node_list;
}

void EditorSelection::clear() {
	while (!selection.is_empty()) {
		remove_node(selection.begin()->key);
	}

	changed = true;
	node_list_changed = true;
}

EditorSelection::EditorSelection() {
}

EditorSelection::~EditorSelection() {
	clear();
}

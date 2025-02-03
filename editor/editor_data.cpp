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

#include "core/io/resource_loader.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/project_settings.h"
#include "editor_node.h"
#include "editor_settings.h"
#include "scene/resources/packed_scene.h"

void EditorHistory::cleanup_history() {
	for (int i = 0; i < history.size(); i++) {
		bool fail = false;

		for (int j = 0; j < history[i].path.size(); j++) {
			if (!history[i].path[j].ref.is_null()) {
				continue;
			}

			Object *obj = ObjectDB::get_instance(history[i].path[j].object);
			if (obj) {
				Node *n = Object::cast_to<Node>(obj);
				if (n && n->is_inside_tree()) {
					continue;
				}
				if (!n) { // Possibly still alive
					continue;
				}
			}

			if (j <= history[i].level) {
				//before or equal level, complete fail
				fail = true;
			} else {
				//after level, clip
				history.write[i].path.resize(j);
			}

			break;
		}

		if (fail) {
			history.remove(i);
			i--;
		}
	}

	if (current >= history.size()) {
		current = history.size() - 1;
	}
}

void EditorHistory::_add_object(ObjectID p_object, const String &p_property, int p_level_change, bool p_inspector_only) {
	Object *obj = ObjectDB::get_instance(p_object);
	ERR_FAIL_COND(!obj);
	Reference *r = Object::cast_to<Reference>(obj);
	Obj o;
	if (r) {
		o.ref = REF(r);
	}
	o.object = p_object;
	o.property = p_property;
	o.inspector_only = p_inspector_only;

	History h;

	bool has_prev = current >= 0 && current < history.size();

	if (has_prev) {
		history.resize(current + 1); //clip history to next
	}

	if (p_property != "" && has_prev) {
		//add a sub property
		History &pr = history.write[current];
		h = pr;
		h.path.resize(h.level + 1);
		h.path.push_back(o);
		h.level++;
	} else if (p_level_change != -1 && has_prev) {
		//add a sub property
		History &pr = history.write[current];
		h = pr;
		ERR_FAIL_INDEX(p_level_change, h.path.size());
		h.level = p_level_change;
	} else {
		//add a new node
		h.path.push_back(o);
		h.level = 0;
	}

	history.push_back(h);
	current++;
}

void EditorHistory::add_object_inspector_only(ObjectID p_object) {
	_add_object(p_object, "", -1, true);
}

void EditorHistory::add_object(ObjectID p_object) {
	_add_object(p_object, "", -1);
}

void EditorHistory::add_object(ObjectID p_object, const String &p_subprop) {
	_add_object(p_object, p_subprop, -1);
}

void EditorHistory::add_object(ObjectID p_object, int p_relevel) {
	_add_object(p_object, "", p_relevel);
}

int EditorHistory::get_history_len() {
	return history.size();
}
int EditorHistory::get_history_pos() {
	return current;
}

bool EditorHistory::is_history_obj_inspector_only(int p_obj) const {
	ERR_FAIL_INDEX_V(p_obj, history.size(), false);
	ERR_FAIL_INDEX_V(history[p_obj].level, history[p_obj].path.size(), false);
	return history[p_obj].path[history[p_obj].level].inspector_only;
}

ObjectID EditorHistory::get_history_obj(int p_obj) const {
	ERR_FAIL_INDEX_V(p_obj, history.size(), 0);
	ERR_FAIL_INDEX_V(history[p_obj].level, history[p_obj].path.size(), 0);
	return history[p_obj].path[history[p_obj].level].object;
}

bool EditorHistory::is_at_beginning() const {
	return current <= 0;
}
bool EditorHistory::is_at_end() const {
	return ((current + 1) >= history.size());
}

bool EditorHistory::next() {
	cleanup_history();

	if ((current + 1) < history.size()) {
		current++;
	} else {
		return false;
	}

	return true;
}

bool EditorHistory::previous() {
	cleanup_history();

	if (current > 0) {
		current--;
	} else {
		return false;
	}

	return true;
}

bool EditorHistory::is_current_inspector_only() const {
	if (current < 0 || current >= history.size()) {
		return false;
	}

	const History &h = history[current];
	return h.path[h.level].inspector_only;
}
ObjectID EditorHistory::get_current() {
	if (current < 0 || current >= history.size()) {
		return 0;
	}

	History &h = history.write[current];
	Object *obj = ObjectDB::get_instance(h.path[h.level].object);
	if (!obj) {
		return 0;
	}

	return obj->get_instance_id();
}

int EditorHistory::get_path_size() const {
	if (current < 0 || current >= history.size()) {
		return 0;
	}

	const History &h = history[current];
	return h.path.size();
}

ObjectID EditorHistory::get_path_object(int p_index) const {
	if (current < 0 || current >= history.size()) {
		return 0;
	}

	const History &h = history[current];

	ERR_FAIL_INDEX_V(p_index, h.path.size(), 0);

	Object *obj = ObjectDB::get_instance(h.path[p_index].object);
	if (!obj) {
		return 0;
	}

	return obj->get_instance_id();
}

String EditorHistory::get_path_property(int p_index) const {
	if (current < 0 || current >= history.size()) {
		return "";
	}

	const History &h = history[current];

	ERR_FAIL_INDEX_V(p_index, h.path.size(), "");

	return h.path[p_index].property;
}

void EditorHistory::clear() {
	history.clear();
	current = -1;
}

EditorHistory::EditorHistory() {
	current = -1;
}

EditorPlugin *EditorData::get_editor(Object *p_object) {
	for (int i = 0; i < editor_plugins.size(); i++) {
		if (editor_plugins[i]->has_main_screen() && editor_plugins[i]->handles(p_object)) {
			return editor_plugins[i];
		}
	}

	return nullptr;
}

EditorPlugin *EditorData::get_subeditor(Object *p_object) {
	for (int i = 0; i < editor_plugins.size(); i++) {
		if (!editor_plugins[i]->has_main_screen() && editor_plugins[i]->handles(p_object)) {
			return editor_plugins[i];
		}
	}

	return nullptr;
}

Vector<EditorPlugin *> EditorData::get_subeditors(Object *p_object) {
	Vector<EditorPlugin *> sub_plugins;
	for (int i = 0; i < editor_plugins.size(); i++) {
		if (!editor_plugins[i]->has_main_screen() && editor_plugins[i]->handles(p_object)) {
			sub_plugins.push_back(editor_plugins[i]);
		}
	}
	return sub_plugins;
}

EditorPlugin *EditorData::get_editor(String p_name) {
	for (int i = 0; i < editor_plugins.size(); i++) {
		if (editor_plugins[i]->get_name() == p_name) {
			return editor_plugins[i];
		}
	}

	return nullptr;
}

void EditorData::copy_object_params(Object *p_object) {
	clipboard.clear();

	List<PropertyInfo> pinfo;
	p_object->get_property_list(&pinfo);

	for (List<PropertyInfo>::Element *E = pinfo.front(); E; E = E->next()) {
		if (!(E->get().usage & PROPERTY_USAGE_EDITOR) || E->get().name == "script" || E->get().name == "scripts") {
			continue;
		}

		PropertyData pd;
		pd.name = E->get().name;
		pd.value = p_object->get(pd.name);
		clipboard.push_back(pd);
	}
}

void EditorData::get_editor_breakpoints(List<String> *p_breakpoints) {
	for (int i = 0; i < editor_plugins.size(); i++) {
		editor_plugins[i]->get_breakpoints(p_breakpoints);
	}
}

Dictionary EditorData::get_editor_states() const {
	Dictionary metadata;
	for (int i = 0; i < editor_plugins.size(); i++) {
		Dictionary state = editor_plugins[i]->get_state();
		if (state.empty()) {
			continue;
		}
		metadata[editor_plugins[i]->get_name()] = state;
	}

	return metadata;
}

Dictionary EditorData::get_scene_editor_states(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), Dictionary());
	EditedScene es = edited_scene[p_idx];
	return es.editor_states;
}

void EditorData::set_editor_states(const Dictionary &p_states) {
	List<Variant> keys;
	p_states.get_key_list(&keys);

	List<Variant>::Element *E = keys.front();
	for (; E; E = E->next()) {
		String name = E->get();
		int idx = -1;
		for (int i = 0; i < editor_plugins.size(); i++) {
			if (editor_plugins[i]->get_name() == name) {
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

void EditorData::save_editor_global_states() {
	for (int i = 0; i < editor_plugins.size(); i++) {
		editor_plugins[i]->save_global_state();
	}
}

void EditorData::restore_editor_global_states() {
	for (int i = 0; i < editor_plugins.size(); i++) {
		editor_plugins[i]->restore_global_state();
	}
}

void EditorData::paste_object_params(Object *p_object) {
	ERR_FAIL_NULL(p_object);
	undo_redo.create_action(TTR("Paste Params"));
	for (List<PropertyData>::Element *E = clipboard.front(); E; E = E->next()) {
		String name = E->get().name;
		undo_redo.add_do_property(p_object, name, E->get().value);
		undo_redo.add_undo_property(p_object, name, p_object->get(name));
	}
	undo_redo.commit_action();
}

bool EditorData::call_build() {
	bool result = true;

	for (int i = 0; i < editor_plugins.size() && result; i++) {
		result &= editor_plugins[i]->build();
	}

	return result;
}

UndoRedo &EditorData::get_undo_redo() {
	return undo_redo;
}

void EditorData::remove_editor_plugin(EditorPlugin *p_plugin) {
	p_plugin->undo_redo = nullptr;
	editor_plugins.erase(p_plugin);
}

void EditorData::add_editor_plugin(EditorPlugin *p_plugin) {
	p_plugin->undo_redo = &undo_redo;
	editor_plugins.push_back(p_plugin);
}

int EditorData::get_editor_plugin_count() const {
	return editor_plugins.size();
}
EditorPlugin *EditorData::get_editor_plugin(int p_idx) {
	ERR_FAIL_INDEX_V(p_idx, editor_plugins.size(), nullptr);
	return editor_plugins[p_idx];
}

void EditorData::add_custom_type(const String &p_type, const String &p_inherits, const Ref<Script> &p_script, const Ref<Texture> &p_icon) {
	ERR_FAIL_COND_MSG(p_script.is_null(), "It's not a reference to a valid Script object.");
	ERR_FAIL_COND_MSG(!ClassDB::class_exists(p_inherits), "class" + p_inherits + " does not exist");
	ERR_FAIL_COND_MSG(!ClassDB::can_instance(p_inherits), p_inherits + " can not be inherited for custom type");
	CustomType ct;
	ct.name = p_type;
	ct.icon = p_icon;
	ct.script = p_script;
	if (!custom_types.has(p_inherits)) {
		custom_types[p_inherits] = Vector<CustomType>();
	}

	custom_types[p_inherits].push_back(ct);
}

Variant EditorData::instance_custom_type(const String &p_type, const String &p_inherits) {
	if (get_custom_types().has(p_inherits)) {
		for (int i = 0; i < get_custom_types()[p_inherits].size(); i++) {
			if (get_custom_types()[p_inherits][i].name == p_type) {
				Ref<Script> script = get_custom_types()[p_inherits][i].script;

				Variant ob = ClassDB::instance(p_inherits);
				ERR_FAIL_COND_V(!ob, Variant());
				Node *n = Object::cast_to<Node>(ob);
				if (n) {
					n->set_name(p_type);
				}
				((Object *)ob)->set_script(script.get_ref_ptr());
				return ob;
			}
		}
	}

	return Variant();
}

void EditorData::remove_custom_type(const String &p_type) {
	for (Map<String, Vector<CustomType>>::Element *E = custom_types.front(); E; E = E->next()) {
		for (int i = 0; i < E->get().size(); i++) {
			if (E->get()[i].name == p_type) {
				E->get().remove(i);
				if (E->get().empty()) {
					custom_types.erase(E->key());
				}
				return;
			}
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
	es.version = 0;
	es.live_edit_root = NodePath(String("/root"));

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
			editor_plugins[i]->notify_scene_closed(edited_scene[p_idx].root->get_filename());
		}

		memdelete(edited_scene[p_idx].root);
		edited_scene.write[p_idx].root = nullptr;
	}

	if (current_edited_scene > p_idx) {
		current_edited_scene--;
	} else if (current_edited_scene == p_idx && current_edited_scene > 0) {
		current_edited_scene--;
	}

	if (edited_scene[p_idx].path != String()) {
		ScriptEditor::get_singleton()->close_builtin_scripts_from_scene(edited_scene[p_idx].path);
	}

	edited_scene.remove(p_idx);
}

bool EditorData::_find_updated_instances(Node *p_root, Node *p_node, Set<String> &checked_paths) {
	/*
	if (p_root!=p_node && p_node->get_owner()!=p_root && !p_root->is_editable_instance(p_node->get_owner()))
		return false;
	*/

	Ref<SceneState> ss;

	if (p_node == p_root) {
		ss = p_node->get_scene_inherited_state();
	} else if (p_node->get_filename() != String()) {
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

	Set<String> checked_scenes;

	bool must_reload = _find_updated_instances(edited_scene[p_idx].root, edited_scene[p_idx].root, checked_scenes);

	if (must_reload) {
		Ref<PackedScene> pscene;
		pscene.instance();

		EditorProgress ep("update_scene", TTR("Updating Scene"), 2);
		ep.step(TTR("Storing local changes..."), 0);
		//pack first, so it stores diffs to previous version of saved scene
		Error err = pscene->pack(edited_scene[p_idx].root);
		ERR_FAIL_COND_V(err != OK, false);
		ep.step(TTR("Updating scene..."), 1);
		Node *new_scene = pscene->instance(PackedScene::GEN_EDIT_STATE_MAIN);
		ERR_FAIL_COND_V(!new_scene, false);

		//transfer selection
		List<Node *> new_selection;
		for (List<Node *>::Element *E = edited_scene.write[p_idx].selection.front(); E; E = E->next()) {
			NodePath p = edited_scene[p_idx].root->get_path_to(E->get());
			Node *new_node = new_scene->get_node(p);
			if (new_node) {
				new_selection.push_back(new_node);
			}
		}

		new_scene->set_filename(edited_scene[p_idx].root->get_filename());

		memdelete(edited_scene[p_idx].root);
		edited_scene.write[p_idx].root = new_scene;
		if (new_scene->get_filename() != "") {
			edited_scene.write[p_idx].path = new_scene->get_filename();
		}
		edited_scene.write[p_idx].selection = new_selection;

		return true;
	}

	return false;
}

int EditorData::get_edited_scene() const {
	return current_edited_scene;
}
void EditorData::set_edited_scene(int p_idx) {
	ERR_FAIL_INDEX(p_idx, edited_scene.size());
	current_edited_scene = p_idx;
	//swap
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
		if (p_root->get_filename() != "") {
			edited_scene.write[current_edited_scene].path = p_root->get_filename();
		} else {
			p_root->set_filename(edited_scene[current_edited_scene].path);
		}
	}

	if (edited_scene[current_edited_scene].path != "") {
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

void EditorData::set_edited_scene_version(uint64_t version, int p_scene_idx) {
	ERR_FAIL_INDEX(current_edited_scene, edited_scene.size());
	if (p_scene_idx < 0) {
		edited_scene.write[current_edited_scene].version = version;
	} else {
		ERR_FAIL_INDEX(p_scene_idx, edited_scene.size());
		edited_scene.write[p_scene_idx].version = version;
	}
}

uint64_t EditorData::get_edited_scene_version() const {
	ERR_FAIL_INDEX_V(current_edited_scene, edited_scene.size(), 0);
	return edited_scene[current_edited_scene].version;
}
uint64_t EditorData::get_scene_version(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), 0);
	return edited_scene[p_idx].version;
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
	edited_scene.remove(current_edited_scene);
	edited_scene.insert(p_idx, es);
	current_edited_scene = p_idx;
}

Ref<Script> EditorData::get_scene_root_script(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), Ref<Script>());
	if (!edited_scene[p_idx].root) {
		return Ref<Script>();
	}
	Ref<Script> s = edited_scene[p_idx].root->get_script();
	if (!s.is_valid() && edited_scene[p_idx].root->get_child_count()) {
		Node *n = edited_scene[p_idx].root->get_child(0);
		while (!s.is_valid() && n && n->get_filename() == String()) {
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
	if (edited_scene[p_idx].root->get_filename() == "") {
		return TTR("[unsaved]");
	}

	const String filename = edited_scene[p_idx].root->get_filename().get_file();
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

		if (edited_scene[i].root && basename == edited_scene[i].root->get_filename().get_file().get_basename()) {
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
	edited_scene[p_idx].root->set_filename(p_path);
}

String EditorData::get_scene_path(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, edited_scene.size(), String());

	if (edited_scene[p_idx].root) {
		if (edited_scene[p_idx].root->get_filename() == "") {
			edited_scene[p_idx].root->set_filename(edited_scene[p_idx].path);
		} else {
			return edited_scene[p_idx].root->get_filename();
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

void EditorData::save_edited_scene_state(EditorSelection *p_selection, EditorHistory *p_history, const Dictionary &p_custom) {
	ERR_FAIL_INDEX(current_edited_scene, edited_scene.size());

	EditedScene &es = edited_scene.write[current_edited_scene];
	es.selection = p_selection->get_full_selected_node_list();
	es.history_current = p_history->current;
	es.history_stored = p_history->history;
	es.editor_states = get_editor_states();
	es.custom_state = p_custom;
}

Dictionary EditorData::restore_edited_scene_state(EditorSelection *p_selection, EditorHistory *p_history) {
	ERR_FAIL_INDEX_V(current_edited_scene, edited_scene.size(), Dictionary());

	EditedScene &es = edited_scene.write[current_edited_scene];

	p_history->current = es.history_current;
	p_history->history = es.history_stored;

	p_selection->clear();
	for (List<Node *>::Element *E = es.selection.front(); E; E = E->next()) {
		p_selection->add_node(E->get());
	}
	set_editor_states(es.editor_states);

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
		Variant obj = ClassDB::instance(ScriptServer::get_global_class_native_base(p_class));
		if (obj) {
			Ref<Script> script = script_class_load_script(p_class);
			if (script.is_valid()) {
				((Object *)obj)->set_script(script.get_ref_ptr());
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
	while (ret.empty()) {
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
	List<StringName> keys;
	_script_class_icon_paths.get_key_list(&keys);

	Dictionary d;
	for (List<StringName>::Element *E = keys.front(); E; E = E->next()) {
		if (ScriptServer::is_global_class(E->get())) {
			d[E->get()] = _script_class_icon_paths[E->get()];
		}
	}

	Dictionary old;
	if (ProjectSettings::get_singleton()->has_setting("_global_script_class_icons")) {
		old = ProjectSettings::get_singleton()->get("_global_script_class_icons");
	}
	if ((!old.empty() || d.empty()) && d.hash() == old.hash()) {
		return;
	}

	if (d.empty()) {
		if (ProjectSettings::get_singleton()->has_setting("_global_script_class_icons")) {
			ProjectSettings::get_singleton()->clear("_global_script_class_icons");
		}
	} else {
		ProjectSettings::get_singleton()->set("_global_script_class_icons", d);
	}
	ProjectSettings::get_singleton()->save();
}

void EditorData::script_class_load_icon_paths() {
	script_class_clear_icon_paths();

	if (ProjectSettings::get_singleton()->has_setting("_global_script_class_icons")) {
		Dictionary d = ProjectSettings::get_singleton()->get("_global_script_class_icons");
		List<Variant> keys;
		d.get_key_list(&keys);

		for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {
			String name = E->get().operator String();
			_script_class_icon_paths[name] = d[name];

			String path = ScriptServer::get_global_class_path(name);
			script_class_set_name(path, name);
		}
	}
}

EditorData::EditorData() {
	current_edited_scene = -1;

	//load_imported_scenes_from_globals();
	script_class_load_icon_paths();
}

///////////
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
	nl_changed = true;
}

void EditorSelection::add_node(Node *p_node) {
	ERR_FAIL_NULL(p_node);
	ERR_FAIL_COND(!p_node->is_inside_tree());
	if (selection.has(p_node)) {
		return;
	}

	changed = true;
	nl_changed = true;
	Object *meta = nullptr;
	for (List<Object *>::Element *E = editor_plugins.front(); E; E = E->next()) {
		meta = E->get()->call("_get_editor_data", p_node);
		if (meta) {
			break;
		}
	}
	selection[p_node] = meta;

	p_node->connect("tree_exiting", this, "_node_removed", varray(p_node), CONNECT_ONESHOT);

	//emit_signal("selection_changed");
}

void EditorSelection::remove_node(Node *p_node) {
	ERR_FAIL_NULL(p_node);

	if (!selection.has(p_node)) {
		return;
	}

	changed = true;
	nl_changed = true;
	Object *meta = selection[p_node];
	if (meta) {
		memdelete(meta);
	}
	selection.erase(p_node);
	p_node->disconnect("tree_exiting", this, "_node_removed");
	//emit_signal("selection_changed");
}
bool EditorSelection::is_selected(Node *p_node) const {
	return selection.has(p_node);
}

Array EditorSelection::_get_transformable_selected_nodes() {
	Array ret;

	for (List<Node *>::Element *E = selected_node_list.front(); E; E = E->next()) {
		ret.push_back(E->get());
	}

	return ret;
}

Array EditorSelection::get_selected_nodes() {
	Array ret;

	for (Map<Node *, Object *>::Element *E = selection.front(); E; E = E->next()) {
		ret.push_back(E->key());
	}

	return ret;
}

void EditorSelection::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_node_removed"), &EditorSelection::_node_removed);
	ClassDB::bind_method(D_METHOD("clear"), &EditorSelection::clear);
	ClassDB::bind_method(D_METHOD("add_node", "node"), &EditorSelection::add_node);
	ClassDB::bind_method(D_METHOD("remove_node", "node"), &EditorSelection::remove_node);
	ClassDB::bind_method(D_METHOD("get_selected_nodes"), &EditorSelection::get_selected_nodes);
	ClassDB::bind_method(D_METHOD("get_transformable_selected_nodes"), &EditorSelection::_get_transformable_selected_nodes);
	ClassDB::bind_method(D_METHOD("_emit_change"), &EditorSelection::_emit_change);
	ADD_SIGNAL(MethodInfo("selection_changed"));
}

void EditorSelection::add_editor_plugin(Object *p_object) {
	editor_plugins.push_back(p_object);
}

void EditorSelection::_update_nl() {
	if (!nl_changed) {
		return;
	}

	selected_node_list.clear();

	for (Map<Node *, Object *>::Element *E = selection.front(); E; E = E->next()) {
		Node *parent = E->key();
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
		selected_node_list.push_back(E->key());
	}

	nl_changed = true;
}

void EditorSelection::update() {
	_update_nl();

	if (!changed) {
		return;
	}
	changed = false;
	if (!emitted) {
		emitted = true;
		call_deferred("_emit_change");
	}
}

void EditorSelection::_emit_change() {
	emit_signal("selection_changed");
	emitted = false;
}

List<Node *> &EditorSelection::get_selected_node_list() {
	if (changed) {
		update();
	} else {
		_update_nl();
	}
	return selected_node_list;
}

List<Node *> EditorSelection::get_full_selected_node_list() {
	List<Node *> node_list;
	for (Map<Node *, Object *>::Element *E = selection.front(); E; E = E->next()) {
		node_list.push_back(E->key());
	}

	return node_list;
}

void EditorSelection::clear() {
	while (!selection.empty()) {
		remove_node(selection.front()->key());
	}

	changed = true;
	nl_changed = true;
}
EditorSelection::EditorSelection() {
	emitted = false;
	changed = false;
	nl_changed = false;
}

EditorSelection::~EditorSelection() {
	clear();
}

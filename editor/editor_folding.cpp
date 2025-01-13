/**************************************************************************/
/*  editor_folding.cpp                                                    */
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

#include "editor_folding.h"

#include "core/io/config_file.h"
#include "core/io/file_access.h"
#include "editor/editor_inspector.h"
#include "editor/editor_paths.h"

Vector<String> EditorFolding::_get_unfolds(const Object *p_object) {
	Vector<String> sections;
	sections.resize(p_object->editor_get_section_folding().size());
	if (sections.size()) {
		String *w = sections.ptrw();
		int idx = 0;
		for (const String &E : p_object->editor_get_section_folding()) {
			w[idx++] = E;
		}
	}

	return sections;
}

void EditorFolding::save_resource_folding(const Ref<Resource> &p_resource, const String &p_path) {
	Ref<ConfigFile> config;
	config.instantiate();
	Vector<String> unfolds = _get_unfolds(p_resource.ptr());
	config->set_value("folding", "sections_unfolded", unfolds);

	String file = p_path.get_file() + "-folding-" + p_path.md5_text() + ".cfg";
	file = EditorPaths::get_singleton()->get_project_settings_dir().path_join(file);
	config->save(file);
}

void EditorFolding::_set_unfolds(Object *p_object, const Vector<String> &p_unfolds) {
	int uc = p_unfolds.size();
	const String *r = p_unfolds.ptr();
	p_object->editor_clear_section_folding();
	for (int i = 0; i < uc; i++) {
		p_object->editor_set_section_unfold(r[i], true);
	}
}

void EditorFolding::load_resource_folding(Ref<Resource> p_resource, const String &p_path) {
	Ref<ConfigFile> config;
	config.instantiate();

	String file = p_path.get_file() + "-folding-" + p_path.md5_text() + ".cfg";
	file = EditorPaths::get_singleton()->get_project_settings_dir().path_join(file);

	if (config->load(file) != OK) {
		return;
	}

	Vector<String> unfolds;

	if (config->has_section_key("folding", "sections_unfolded")) {
		unfolds = config->get_value("folding", "sections_unfolded");
	}
	_set_unfolds(p_resource.ptr(), unfolds);
}

void EditorFolding::_fill_folds(const Node *p_root, const Node *p_node, Array &p_folds, Array &resource_folds, Array &nodes_folded, HashSet<Ref<Resource>> &resources) {
	if (p_root != p_node) {
		if (!p_node->get_owner()) {
			return; //not owned, bye
		}
		if (p_node->get_owner() != p_root && !p_root->is_editable_instance(p_node->get_owner())) {
			return;
		}
	}

	if (p_node->is_displayed_folded()) {
		nodes_folded.push_back(p_root->get_path_to(p_node));
	}
	Vector<String> unfolds = _get_unfolds(p_node);

	if (unfolds.size()) {
		p_folds.push_back(p_root->get_path_to(p_node));
		p_folds.push_back(unfolds);
	}

	List<PropertyInfo> plist;
	p_node->get_property_list(&plist);
	for (const PropertyInfo &E : plist) {
		if (E.usage & PROPERTY_USAGE_EDITOR) {
			if (E.type == Variant::OBJECT) {
				Ref<Resource> res = p_node->get(E.name);
				if (res.is_valid() && !resources.has(res) && !res->get_path().is_empty() && !res->get_path().is_resource_file()) {
					Vector<String> res_unfolds = _get_unfolds(res.ptr());
					resource_folds.push_back(res->get_path());
					resource_folds.push_back(res_unfolds);
					resources.insert(res);
				}
			}
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_fill_folds(p_root, p_node->get_child(i), p_folds, resource_folds, nodes_folded, resources);
	}
}

void EditorFolding::save_scene_folding(const Node *p_scene, const String &p_path) {
	ERR_FAIL_NULL(p_scene);

	Ref<FileAccess> file_check = FileAccess::create(FileAccess::ACCESS_RESOURCES);
	if (!file_check->file_exists(p_path)) { //This can happen when creating scene from FilesystemDock. It has path, but no file.
		return;
	}

	Ref<ConfigFile> config;
	config.instantiate();

	Array unfolds, res_unfolds;
	HashSet<Ref<Resource>> resources;
	Array nodes_folded;
	_fill_folds(p_scene, p_scene, unfolds, res_unfolds, nodes_folded, resources);

	config->set_value("folding", "node_unfolds", unfolds);
	config->set_value("folding", "resource_unfolds", res_unfolds);
	config->set_value("folding", "nodes_folded", nodes_folded);

	String file = p_path.get_file() + "-folding-" + p_path.md5_text() + ".cfg";
	file = EditorPaths::get_singleton()->get_project_settings_dir().path_join(file);
	config->save(file);
}

void EditorFolding::load_scene_folding(Node *p_scene, const String &p_path) {
	Ref<ConfigFile> config;
	config.instantiate();

	String path = EditorPaths::get_singleton()->get_project_settings_dir();
	String file = p_path.get_file() + "-folding-" + p_path.md5_text() + ".cfg";
	file = EditorPaths::get_singleton()->get_project_settings_dir().path_join(file);

	if (config->load(file) != OK) {
		return;
	}

	Array unfolds;
	if (config->has_section_key("folding", "node_unfolds")) {
		unfolds = config->get_value("folding", "node_unfolds");
	}
	Array res_unfolds;
	if (config->has_section_key("folding", "resource_unfolds")) {
		res_unfolds = config->get_value("folding", "resource_unfolds");
	}
	Array nodes_folded;
	if (config->has_section_key("folding", "nodes_folded")) {
		nodes_folded = config->get_value("folding", "nodes_folded");
	}

	ERR_FAIL_COND(unfolds.size() & 1);
	ERR_FAIL_COND(res_unfolds.size() & 1);

	for (int i = 0; i < unfolds.size(); i += 2) {
		NodePath path2 = unfolds[i];
		Vector<String> un = unfolds[i + 1];
		Node *node = p_scene->get_node_or_null(path2);
		if (!node) {
			continue;
		}
		_set_unfolds(node, un);
	}

	for (int i = 0; i < res_unfolds.size(); i += 2) {
		String path2 = res_unfolds[i];
		Ref<Resource> res = ResourceCache::get_ref(path2);
		if (res.is_null()) {
			continue;
		}

		Vector<String> unfolds2 = res_unfolds[i + 1];
		_set_unfolds(res.ptr(), unfolds2);
	}

	for (int i = 0; i < nodes_folded.size(); i++) {
		NodePath fold_path = nodes_folded[i];
		if (p_scene->has_node(fold_path)) {
			Node *node = p_scene->get_node(fold_path);
			node->set_display_folded(true);
		}
	}
}

bool EditorFolding::has_folding_data(const String &p_path) {
	String file = p_path.get_file() + "-folding-" + p_path.md5_text() + ".cfg";
	file = EditorPaths::get_singleton()->get_project_settings_dir().path_join(file);
	return FileAccess::exists(file);
}

void EditorFolding::_do_object_unfolds(Object *p_object, HashSet<Ref<Resource>> &resources) {
	List<PropertyInfo> plist;
	p_object->get_property_list(&plist);
	String group_base;
	String group;

	HashSet<String> unfold_group;

	for (const PropertyInfo &E : plist) {
		if (E.usage & PROPERTY_USAGE_CATEGORY) {
			group = "";
			group_base = "";
		}
		if (E.usage & PROPERTY_USAGE_GROUP) {
			group = E.name;
			group_base = E.hint_string;
			if (group_base.ends_with("_")) {
				group_base = group_base.substr(0, group_base.length() - 1);
			}
		}

		//can unfold
		if (E.usage & PROPERTY_USAGE_EDITOR) {
			if (!group.is_empty()) { //group
				if (group_base.is_empty() || E.name.begins_with(group_base)) {
					bool can_revert = EditorPropertyRevert::can_property_revert(p_object, E.name);
					if (can_revert) {
						unfold_group.insert(group);
					}
				}
			} else { //path
				int last = E.name.rfind_char('/');
				if (last != -1) {
					bool can_revert = EditorPropertyRevert::can_property_revert(p_object, E.name);
					if (can_revert) {
						unfold_group.insert(E.name.substr(0, last));
					}
				}
			}

			if (E.type == Variant::OBJECT) {
				Ref<Resource> res = p_object->get(E.name);
				if (res.is_valid() && !resources.has(res) && !res->get_path().is_empty() && !res->get_path().is_resource_file()) {
					resources.insert(res);
					_do_object_unfolds(res.ptr(), resources);
				}
			}
		}
	}

	for (const String &E : unfold_group) {
		p_object->editor_set_section_unfold(E, true);
	}
}

void EditorFolding::_do_node_unfolds(Node *p_root, Node *p_node, HashSet<Ref<Resource>> &resources) {
	if (p_root != p_node) {
		if (!p_node->get_owner()) {
			return; //not owned, bye
		}
		if (p_node->get_owner() != p_root && !p_root->is_editable_instance(p_node)) {
			return;
		}
	}

	_do_object_unfolds(p_node, resources);

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_do_node_unfolds(p_root, p_node->get_child(i), resources);
	}
}

void EditorFolding::unfold_scene(Node *p_scene) {
	HashSet<Ref<Resource>> resources;
	_do_node_unfolds(p_scene, p_scene, resources);
}

EditorFolding::EditorFolding() {
}

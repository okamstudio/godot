/**************************************************************************/
/*  code_completion.cpp                                                   */
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

#include "code_completion.h"

#include "core/config/project_settings.h"
#include "core/object/script_language.h"
#include "editor/editor_file_system.h"
#include "editor/editor_settings.h"
#include "scene/gui/control.h"
#include "scene/main/node.h"
#include "scene/theme/theme_db.h"

namespace gdmono {

// Almost everything here is taken from functions used by GDScript for code completion, adapted for C#.

_FORCE_INLINE_ String quoted(const String &p_str) {
	return "\"" + p_str + "\"";
}

void _add_nodes_suggestions(const Node *p_base, const Node *p_node, PackedStringArray &r_suggestions) {
	if (p_node != p_base && !p_node->get_owner()) {
		return;
	}

	String path_relative_to_orig = p_base->get_path_to(p_node);

	r_suggestions.push_back(quoted(path_relative_to_orig));

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_add_nodes_suggestions(p_base, p_node->get_child(i), r_suggestions);
	}
}

Node *_find_node_for_script(Node *p_base, Node *p_current, const Ref<Script> &p_script) {
	if (p_current->get_owner() != p_base && p_base != p_current) {
		return nullptr;
	}

	Ref<Script> c = p_current->get_script();

	if (c == p_script) {
		return p_current;
	}

	for (int i = 0; i < p_current->get_child_count(); i++) {
		Node *found = _find_node_for_script(p_base, p_current->get_child(i), p_script);
		if (found) {
			return found;
		}
	}

	return nullptr;
}

void _get_directory_contents(EditorFileSystemDirectory *p_dir, PackedStringArray &r_suggestions) {
	for (int i = 0; i < p_dir->get_file_count(); i++) {
		r_suggestions.push_back(quoted(p_dir->get_file_path(i)));
	}

	for (int i = 0; i < p_dir->get_subdir_count(); i++) {
		_get_directory_contents(p_dir->get_subdir(i), r_suggestions);
	}
}

Node *_try_find_owner_node_in_tree(const Ref<Script> p_script) {
	SceneTree *tree = SceneTree::get_singleton();
	if (!tree) {
		return nullptr;
	}
	Node *base = tree->get_edited_scene_root();
	if (base) {
		base = _find_node_for_script(base, base, p_script);
	}
	return base;
}

PackedStringArray get_code_completion(CompletionKind p_kind, const String &p_script_file) {
	PackedStringArray suggestions;

	switch (p_kind) {
		case CompletionKind::INPUT_ACTIONS: {
			List<PropertyInfo> project_props;
			ProjectSettings::get_singleton()->get_property_list(&project_props);

			for (const PropertyInfo &prop : project_props) {
				if (!prop.name.begins_with("input/")) {
					continue;
				}

				String name = prop.name.substr(prop.name.find_char('/') + 1, prop.name.length());
				suggestions.push_back(quoted(name));
			}
		} break;
		case CompletionKind::NODE_PATHS: {
			{
				// Autoloads.
				HashMap<StringName, ProjectSettings::AutoloadInfo> autoloads = ProjectSettings::get_singleton()->get_autoload_list();

				for (const KeyValue<StringName, ProjectSettings::AutoloadInfo> &E : autoloads) {
					const ProjectSettings::AutoloadInfo &info = E.value;
					suggestions.push_back(quoted("/root/" + String(info.name)));
				}
			}

			{
				// Current edited scene tree
				Ref<Script> script = ResourceLoader::load(p_script_file.simplify_path());
				Node *base = _try_find_owner_node_in_tree(script);
				if (base) {
					_add_nodes_suggestions(base, base, suggestions);
				}
			}
		} break;
		case CompletionKind::RESOURCE_PATHS: {
			if (bool(EDITOR_GET("text_editor/completion/complete_file_paths"))) {
				_get_directory_contents(EditorFileSystem::get_singleton()->get_filesystem(), suggestions);
			}
		} break;
		case CompletionKind::SCENE_PATHS: {
			Ref<DirAccess> dir_access = DirAccess::create(DirAccess::ACCESS_RESOURCES);
			List<String> directories;
			directories.push_back(dir_access->get_current_dir());

			while (!directories.is_empty()) {
				dir_access->change_dir(directories.back()->get());
				directories.pop_back();

				dir_access->list_dir_begin();
				String filename = dir_access->get_next();

				while (!filename.is_empty()) {
					if (filename == "." || filename == "..") {
						filename = dir_access->get_next();
						continue;
					}

					if (dir_access->dir_exists(filename)) {
						directories.push_back(dir_access->get_current_dir().path_join(filename));
					} else if (filename.ends_with(".tscn") || filename.ends_with(".scn")) {
						suggestions.push_back(quoted(dir_access->get_current_dir().path_join(filename)));
					}

					filename = dir_access->get_next();
				}
			}
		} break;
		case CompletionKind::SHADER_PARAMS: {
			print_verbose("Shader uniforms completion for C# is not implemented yet.");
		} break;
		case CompletionKind::SIGNALS: {
			Ref<Script> script = ResourceLoader::load(p_script_file.simplify_path());

			List<MethodInfo> signals;
			script->get_script_signal_list(&signals);

			StringName native = script->get_instance_base_type();
			if (native != StringName()) {
				ClassDB::get_signal_list(native, &signals, /* p_no_inheritance: */ false);
			}

			for (const MethodInfo &E : signals) {
				const String &signal = E.name;
				suggestions.push_back(quoted(signal));
			}
		} break;
		case CompletionKind::THEME_COLORS: {
			Ref<Script> script = ResourceLoader::load(p_script_file.simplify_path());
			Node *base = _try_find_owner_node_in_tree(script);
			if (base && Object::cast_to<Control>(base)) {
				List<StringName> sn;
				ThemeDB::get_singleton()->get_default_theme()->get_color_list(base->get_class(), &sn);

				for (const StringName &E : sn) {
					suggestions.push_back(quoted(E));
				}
			}
		} break;
		case CompletionKind::THEME_CONSTANTS: {
			Ref<Script> script = ResourceLoader::load(p_script_file.simplify_path());
			Node *base = _try_find_owner_node_in_tree(script);
			if (base && Object::cast_to<Control>(base)) {
				List<StringName> sn;
				ThemeDB::get_singleton()->get_default_theme()->get_constant_list(base->get_class(), &sn);

				for (const StringName &E : sn) {
					suggestions.push_back(quoted(E));
				}
			}
		} break;
		case CompletionKind::THEME_FONTS: {
			Ref<Script> script = ResourceLoader::load(p_script_file.simplify_path());
			Node *base = _try_find_owner_node_in_tree(script);
			if (base && Object::cast_to<Control>(base)) {
				List<StringName> sn;
				ThemeDB::get_singleton()->get_default_theme()->get_font_list(base->get_class(), &sn);

				for (const StringName &E : sn) {
					suggestions.push_back(quoted(E));
				}
			}
		} break;
		case CompletionKind::THEME_FONT_SIZES: {
			Ref<Script> script = ResourceLoader::load(p_script_file.simplify_path());
			Node *base = _try_find_owner_node_in_tree(script);
			if (base && Object::cast_to<Control>(base)) {
				List<StringName> sn;
				ThemeDB::get_singleton()->get_default_theme()->get_font_size_list(base->get_class(), &sn);

				for (const StringName &E : sn) {
					suggestions.push_back(quoted(E));
				}
			}
		} break;
		case CompletionKind::THEME_STYLES: {
			Ref<Script> script = ResourceLoader::load(p_script_file.simplify_path());
			Node *base = _try_find_owner_node_in_tree(script);
			if (base && Object::cast_to<Control>(base)) {
				List<StringName> sn;
				ThemeDB::get_singleton()->get_default_theme()->get_stylebox_list(base->get_class(), &sn);

				for (const StringName &E : sn) {
					suggestions.push_back(quoted(E));
				}
			}
		} break;
		default:
			ERR_FAIL_V_MSG(suggestions, "Invalid completion kind.");
	}

	return suggestions;
}
} // namespace gdmono

/**************************************************************************/
/*  packed_scene_translation_parser_plugin.cpp                            */
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

#include "packed_scene_translation_parser_plugin.h"

#include "core/io/resource_loader.h"
#include "core/object/script_language.h"
#include "scene/resources/packed_scene.h"

void PackedSceneEditorTranslationParserPlugin::get_recognized_extensions(List<String> *r_extensions) const {
	ResourceLoader::get_recognized_extensions_for_type("PackedScene", r_extensions);
}

Error PackedSceneEditorTranslationParserPlugin::parse_file(const String &p_path, Vector<String> *r_ids, Vector<Vector<String>> *r_ids_ctx_plural) {
	// Parse specific scene Node's properties (see in constructor) that are auto-translated by the engine when set. E.g Label's text property.
	// These properties are translated with the tr() function in the C++ code when being set or updated.

	Error err;
	Ref<Resource> loaded_res = ResourceLoader::load(p_path, "PackedScene", ResourceFormatLoader::CACHE_MODE_REUSE, &err);
	if (err) {
		ERR_PRINT("Failed to load " + p_path);
		return err;
	}
	Ref<PackedScene> packed_scene = loaded_res;
	ERR_FAIL_COND_V_MSG(packed_scene.is_null(), ERR_FILE_UNRECOGNIZED, vformat("'%s' is not a valid PackedScene resource.", p_path));
	Ref<SceneState> state = packed_scene->get_state();

	Vector<String> parsed_strings;
	Vector<Pair<NodePath, bool>> atr_owners;
	Vector<String> tabcontainer_paths;
	for (int i = 0; i < state->get_node_count(); i++) {
		String node_type = state->get_node_type(i);
		String parent_path = state->get_node_path(i, true);

		// Handle instanced scenes.
		if (node_type.is_empty()) {
			Ref<PackedScene> instance = state->get_node_instance(i);
			if (instance.is_valid()) {
				Ref<SceneState> _state = instance->get_state();
				node_type = _state->get_node_type(0);
			}
		}

		// Find the `auto_translate_mode` property.
		bool auto_translating = true;
		bool auto_translate_mode_found = false;
		for (int j = 0; j < state->get_node_property_count(i); j++) {
			String property = state->get_node_property_name(i, j);
			// If an old scene wasn't saved in the new version, the `auto_translate` property won't be converted into `auto_translate_mode`,
			// so the deprecated property still needs to be checked as well.
			// TODO: Remove the check for "auto_translate" once the property if fully removed.
			if (property != "auto_translate_mode" && property != "auto_translate") {
				continue;
			}

			auto_translate_mode_found = true;

			int idx_last = atr_owners.size() - 1;
			if (idx_last > 0 && !parent_path.begins_with(atr_owners[idx_last].first)) {
				// Exit from the current owner nesting into the previous one.
				atr_owners.remove_at(idx_last);
			}

			if (property == "auto_translate_mode") {
				int auto_translate_mode = (int)state->get_node_property_value(i, j);
				if (auto_translate_mode == Node::AUTO_TRANSLATE_MODE_DISABLED) {
					auto_translating = false;
				}
			} else {
				// TODO: Remove this once `auto_translate` is fully removed.
				auto_translating = (bool)state->get_node_property_value(i, j);
			}

			atr_owners.push_back(Pair(state->get_node_path(i), auto_translating));

			break;
		}

		// If `auto_translate_mode` wasn't found, that means it is set to its default value (`AUTO_TRANSLATE_MODE_INHERIT`).
		if (!auto_translate_mode_found) {
			int idx_last = atr_owners.size() - 1;
			if (idx_last > 0 && parent_path.begins_with(atr_owners[idx_last].first)) {
				auto_translating = atr_owners[idx_last].second;
			} else {
				atr_owners.push_back(Pair(state->get_node_path(i), true));
			}
		}

		// Parse the names of children of `TabContainer`s, as they are used for tab titles.
		if (!tabcontainer_paths.is_empty()) {
			if (!parent_path.begins_with(tabcontainer_paths[tabcontainer_paths.size() - 1])) {
				// Switch to the previous `TabContainer` this was nested in, if that was the case.
				tabcontainer_paths.remove_at(tabcontainer_paths.size() - 1);
			}

			if (auto_translating && !tabcontainer_paths.is_empty() && ClassDB::is_parent_class(node_type, "Control") &&
					parent_path == tabcontainer_paths[tabcontainer_paths.size() - 1]) {
				parsed_strings.push_back(state->get_node_name(i));
			}
		}

		if (!auto_translating) {
			continue;
		}

		if (node_type == "TabContainer") {
			tabcontainer_paths.push_back(state->get_node_path(i));
		}

		for (int j = 0; j < state->get_node_property_count(i); j++) {
			String property_name = state->get_node_property_name(i, j);

			if (!match_property(property_name, node_type)) {
				continue;
			}

			Variant property_value = state->get_node_property_value(i, j);
			if (property_name == "script" && property_value.get_type() == Variant::OBJECT && !property_value.is_null()) {
				// Parse built-in script.
				Ref<Script> s = Object::cast_to<Script>(property_value);
				if (!s->is_built_in()) {
					continue;
				}

				String extension = s->get_language()->get_extension();
				if (EditorTranslationParser::get_singleton()->can_parse(extension)) {
					Vector<String> temp;
					Vector<Vector<String>> ids_context_plural;
					EditorTranslationParser::get_singleton()->get_parser(extension)->parse_file(s->get_path(), &temp, &ids_context_plural);
					parsed_strings.append_array(temp);
					r_ids_ctx_plural->append_array(ids_context_plural);
				}
			} else if (node_type == "FileDialog" && property_name == "filters") {
				// Extract FileDialog's filters property with values in format "*.png ; PNG Images","*.gd ; GDScript Files".
				Vector<String> str_values = property_value;
				for (int k = 0; k < str_values.size(); k++) {
					String desc = str_values[k].get_slice(";", 1).strip_edges();
					if (!desc.is_empty()) {
						parsed_strings.push_back(desc);
					}
				}
			} else if (property_value.get_type() == Variant::STRING) {
				String str_value = String(property_value);
				// Prevent reading text containing only spaces.
				if (!str_value.strip_edges().is_empty()) {
					parsed_strings.push_back(str_value);
				}
			}
		}
	}

	r_ids->append_array(parsed_strings);

	return OK;
}

bool PackedSceneEditorTranslationParserPlugin::match_property(const String &p_property_name, const String &p_node_type) {
	for (const KeyValue<String, Vector<String>> &exception : exception_list) {
		const String &exception_node_type = exception.key;
		if (ClassDB::is_parent_class(p_node_type, exception_node_type)) {
			const Vector<String> &exception_properties = exception.value;
			for (const String &exception_property : exception_properties) {
				if (p_property_name.match(exception_property)) {
					return false;
				}
			}
		}
	}
	for (const String &lookup_property : lookup_properties) {
		if (p_property_name.match(lookup_property)) {
			return true;
		}
	}
	return false;
}

PackedSceneEditorTranslationParserPlugin::PackedSceneEditorTranslationParserPlugin() {
	// Scene Node's properties containing strings that will be fetched for translation.
	lookup_properties.insert("text");
	lookup_properties.insert("*_text");
	lookup_properties.insert("popup/*/text");
	lookup_properties.insert("title");
	lookup_properties.insert("filters");
	lookup_properties.insert("script");
	lookup_properties.insert("item_*/text");

	// Exception list (to prevent false positives).
	exception_list.insert("LineEdit", { "text" });
	exception_list.insert("TextEdit", { "text" });
	exception_list.insert("CodeEdit", { "text" });
}

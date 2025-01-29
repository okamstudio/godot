/**************************************************************************/
/*  resource_format_text.cpp                                              */
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

#include "resource_format_text.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/missing_resource.h"
#include "core/object/script_language.h"

void ResourceLoaderText::_printerr() {
	ERR_PRINT(vformat("%s:%d - Parse Error: %s.", res_path, lines, error_text));
}

Ref<Resource> ResourceLoaderText::get_resource() {
	return resource;
}

Error ResourceLoaderText::_parse_sub_resource_dummy(DummyReadData *p_data, VariantParser::Stream *p_stream, Ref<Resource> &r_res, int &line, String &r_err_str) {
	VariantParser::Token token;
	VariantParser::get_token(p_stream, token, line, r_err_str);
	if (token.type != VariantParser::TK_NUMBER && token.type != VariantParser::TK_STRING) {
		r_err_str = "Expected number (old style) or string (sub-resource index)";
		return ERR_PARSE_ERROR;
	}

	if (p_data->no_placeholders) {
		r_res.unref();
	} else {
		String unique_id = token.value;

		if (!p_data->resource_map.has(unique_id)) {
			r_err_str = "Found unique_id reference before mapping, sub-resources stored out of order in resource file";
			return ERR_PARSE_ERROR;
		}

		r_res = p_data->resource_map[unique_id];
	}

	VariantParser::get_token(p_stream, token, line, r_err_str);
	if (token.type != VariantParser::TK_PARENTHESIS_CLOSE) {
		r_err_str = "Expected ')'";
		return ERR_PARSE_ERROR;
	}

	return OK;
}

Error ResourceLoaderText::_parse_ext_resource_dummy(DummyReadData *p_data, VariantParser::Stream *p_stream, Ref<Resource> &r_res, int &line, String &r_err_str) {
	VariantParser::Token token;
	VariantParser::get_token(p_stream, token, line, r_err_str);
	if (token.type != VariantParser::TK_NUMBER && token.type != VariantParser::TK_STRING) {
		r_err_str = "Expected number (old style sub-resource index) or String (ext-resource ID)";
		return ERR_PARSE_ERROR;
	}

	if (p_data->no_placeholders) {
		r_res.unref();
	} else {
		String id = token.value;

		ERR_FAIL_COND_V(!p_data->rev_external_resources.has(id), ERR_PARSE_ERROR);

		r_res = p_data->rev_external_resources[id];
	}

	VariantParser::get_token(p_stream, token, line, r_err_str);
	if (token.type != VariantParser::TK_PARENTHESIS_CLOSE) {
		r_err_str = "Expected ')'";
		return ERR_PARSE_ERROR;
	}

	return OK;
}

Error ResourceLoaderText::_parse_sub_resource(VariantParser::Stream *p_stream, Ref<Resource> &r_res, int &line, String &r_err_str) {
	VariantParser::Token token;
	VariantParser::get_token(p_stream, token, line, r_err_str);
	if (token.type != VariantParser::TK_NUMBER && token.type != VariantParser::TK_STRING) {
		r_err_str = "Expected number (old style sub-resource index) or string";
		return ERR_PARSE_ERROR;
	}

	String id = token.value;
	ERR_FAIL_COND_V(!int_resources.has(id), ERR_INVALID_PARAMETER);
	r_res = int_resources[id];

	VariantParser::get_token(p_stream, token, line, r_err_str);
	if (token.type != VariantParser::TK_PARENTHESIS_CLOSE) {
		r_err_str = "Expected ')'";
		return ERR_PARSE_ERROR;
	}

	return OK;
}

Error ResourceLoaderText::_parse_ext_resource(VariantParser::Stream *p_stream, Ref<Resource> &r_res, int &line, String &r_err_str) {
	VariantParser::Token token;
	VariantParser::get_token(p_stream, token, line, r_err_str);
	if (token.type != VariantParser::TK_NUMBER && token.type != VariantParser::TK_STRING) {
		r_err_str = "Expected number (old style sub-resource index) or String (ext-resource ID)";
		return ERR_PARSE_ERROR;
	}

	String id = token.value;
	Error err = OK;

	if (!ignore_resource_parsing) {
		if (!ext_resources.has(id)) {
			r_err_str = "Can't load cached ext-resource id: " + id;
			return ERR_PARSE_ERROR;
		}

		String path = ext_resources[id].path;
		String type = ext_resources[id].type;
		Ref<ResourceLoader::LoadToken> &load_token = ext_resources[id].load_token;

		if (load_token.is_valid()) { // If not valid, it's OK since then we know this load accepts broken dependencies.
			Ref<Resource> res = ResourceLoader::_load_complete(*load_token.ptr(), &err);
			if (res.is_null()) {
				if (!ResourceLoader::is_cleaning_tasks()) {
					if (ResourceLoader::get_abort_on_missing_resources()) {
						error = ERR_FILE_MISSING_DEPENDENCIES;
						error_text = "[ext_resource] referenced non-existent resource at: " + path;
						_printerr();
						err = error;
					} else {
						ResourceLoader::notify_dependency_error(local_path, path, type);
					}
				}
			} else {
#ifdef TOOLS_ENABLED
				//remember ID for saving
				res->set_id_for_path(local_path, id);
#endif
				r_res = res;
			}
		} else {
			r_res = Ref<Resource>();
		}
#ifdef TOOLS_ENABLED
		if (r_res.is_null()) {
			// Hack to allow checking original path.
			r_res.instantiate();
			r_res->set_meta("__load_path__", ext_resources[id].path);
		}
#endif
	}

	VariantParser::get_token(p_stream, token, line, r_err_str);
	if (token.type != VariantParser::TK_PARENTHESIS_CLOSE) {
		r_err_str = "Expected ')'";
		return ERR_PARSE_ERROR;
	}

	return err;
}

Ref<PackedScene> ResourceLoaderText::_parse_node_tag(VariantParser::ResourceParser &parser) {
	Ref<PackedScene> packed_scene = ResourceLoader::get_resource_ref_override(local_path);
	if (packed_scene.is_null()) {
		packed_scene.instantiate();
	}

	while (true) {
		if (next_tag.name == "node") {
			int parent = -1;
			int owner = -1;
			int type = -1;
			int name = -1;
			int instance = -1;
			int index = -1;
			//int base_scene=-1;

			if (next_tag.fields.has("name")) {
				name = packed_scene->get_state()->add_name(next_tag.fields["name"]);
			}

			if (next_tag.fields.has("parent")) {
				NodePath np = next_tag.fields["parent"];
				np.prepend_period(); //compatible to how it manages paths internally
				parent = packed_scene->get_state()->add_node_path(np);
			}

			if (next_tag.fields.has("type")) {
				type = packed_scene->get_state()->add_name(next_tag.fields["type"]);
			} else {
				type = SceneState::TYPE_INSTANTIATED; //no type? assume this was instantiated
			}

			HashSet<StringName> path_properties;

			if (next_tag.fields.has("node_paths")) {
				Vector<String> paths = next_tag.fields["node_paths"];
				for (int i = 0; i < paths.size(); i++) {
					path_properties.insert(paths[i]);
				}
			}

			if (next_tag.fields.has("instance")) {
				instance = packed_scene->get_state()->add_value(next_tag.fields["instance"]);

				if (packed_scene->get_state()->get_node_count() == 0 && parent == -1) {
					packed_scene->get_state()->set_base_scene(instance);
					instance = -1;
				}
			}

			if (next_tag.fields.has("instance_placeholder")) {
				String path = next_tag.fields["instance_placeholder"];

				int path_v = packed_scene->get_state()->add_value(path);

				if (packed_scene->get_state()->get_node_count() == 0) {
					error = ERR_FILE_CORRUPT;
					error_text = "Instance Placeholder can't be used for inheritance";
					_printerr();
					return Ref<PackedScene>();
				}

				instance = path_v | SceneState::FLAG_INSTANCE_IS_PLACEHOLDER;
			}

			if (next_tag.fields.has("owner")) {
				owner = packed_scene->get_state()->add_node_path(next_tag.fields["owner"]);
			} else {
				if (parent != -1 && !(type == SceneState::TYPE_INSTANTIATED && instance == -1)) {
					owner = 0; //if no owner, owner is root
				}
			}

			if (next_tag.fields.has("index")) {
				index = next_tag.fields["index"];
			}

			int node_id = packed_scene->get_state()->add_node(parent, owner, type, name, instance, index);

			if (next_tag.fields.has("groups")) {
				Array groups = next_tag.fields["groups"];
				for (const Variant &group : groups) {
					packed_scene->get_state()->add_node_group(node_id, packed_scene->get_state()->add_name(group));
				}
			}

			while (true) {
				String assign;
				Variant value;

				error = VariantParser::parse_tag_assign_eof(&stream, lines, error_text, next_tag, assign, value, &parser);

				if (error) {
					if (error == ERR_FILE_MISSING_DEPENDENCIES) {
						// Resource loading error, just skip it.
					} else if (error != ERR_FILE_EOF) {
						ERR_PRINT(vformat("Parse Error: %s. [Resource file %s:%d]", error_names[error], res_path, lines));
						return Ref<PackedScene>();
					} else {
						error = OK;
						return packed_scene;
					}
				}

				if (!assign.is_empty()) {
					StringName assign_name = assign;
					int nameidx = packed_scene->get_state()->add_name(assign_name);
					int valueidx = packed_scene->get_state()->add_value(value);
					packed_scene->get_state()->add_node_property(node_id, nameidx, valueidx, path_properties.has(assign_name));
					//it's assignment
				} else if (!next_tag.name.is_empty()) {
					break;
				}
			}
		} else if (next_tag.name == "connection") {
			if (!next_tag.fields.has("from")) {
				error = ERR_FILE_CORRUPT;
				error_text = "missing 'from' field from connection tag";
				return Ref<PackedScene>();
			}

			if (!next_tag.fields.has("to")) {
				error = ERR_FILE_CORRUPT;
				error_text = "missing 'to' field from connection tag";
				return Ref<PackedScene>();
			}

			if (!next_tag.fields.has("signal")) {
				error = ERR_FILE_CORRUPT;
				error_text = "missing 'signal' field from connection tag";
				return Ref<PackedScene>();
			}

			if (!next_tag.fields.has("method")) {
				error = ERR_FILE_CORRUPT;
				error_text = "missing 'method' field from connection tag";
				return Ref<PackedScene>();
			}

			NodePath from = next_tag.fields["from"];
			NodePath to = next_tag.fields["to"];
			StringName method = next_tag.fields["method"];
			StringName signal = next_tag.fields["signal"];
			int flags = Object::CONNECT_PERSIST;
			int unbinds = 0;
			Array binds;

			if (next_tag.fields.has("flags")) {
				flags = next_tag.fields["flags"];
			}

			if (next_tag.fields.has("binds")) {
				binds = next_tag.fields["binds"];
			}

			if (next_tag.fields.has("unbinds")) {
				unbinds = next_tag.fields["unbinds"];
			}

			Vector<int> bind_ints;
			for (const Variant &bind : binds) {
				bind_ints.push_back(packed_scene->get_state()->add_value(bind));
			}

			packed_scene->get_state()->add_connection(
					packed_scene->get_state()->add_node_path(from.simplified()),
					packed_scene->get_state()->add_node_path(to.simplified()),
					packed_scene->get_state()->add_name(signal),
					packed_scene->get_state()->add_name(method),
					flags,
					unbinds,
					bind_ints);

			error = VariantParser::parse_tag(&stream, lines, error_text, next_tag, &parser);

			if (error) {
				if (error != ERR_FILE_EOF) {
					_printerr();
					return Ref<PackedScene>();
				} else {
					error = OK;
					return packed_scene;
				}
			}
		} else if (next_tag.name == "editable") {
			if (!next_tag.fields.has("path")) {
				error = ERR_FILE_CORRUPT;
				error_text = "Missing 'path' field from editable tag";
				_printerr();
				return Ref<PackedScene>();
			}

			NodePath path = next_tag.fields["path"];

			packed_scene->get_state()->add_editable_instance(path.simplified());

			error = VariantParser::parse_tag(&stream, lines, error_text, next_tag, &parser);

			if (error) {
				if (error != ERR_FILE_EOF) {
					_printerr();
					return Ref<PackedScene>();
				} else {
					error = OK;
					return packed_scene;
				}
			}
		} else {
			error = ERR_FILE_CORRUPT;
			error_text = vformat("Unknown tag '%s' in file", next_tag.name);
			_printerr();
			return Ref<PackedScene>();
		}
	}
}

Error ResourceLoaderText::load() {
	if (error != OK) {
		return error;
	}

	while (true) {
		if (next_tag.name != "ext_resource") {
			break;
		}

		if (!next_tag.fields.has("path")) {
			error = ERR_FILE_CORRUPT;
			error_text = "Missing 'path' in external resource tag";
			_printerr();
			return error;
		}

		if (!next_tag.fields.has("type")) {
			error = ERR_FILE_CORRUPT;
			error_text = "Missing 'type' in external resource tag";
			_printerr();
			return error;
		}

		if (!next_tag.fields.has("id")) {
			error = ERR_FILE_CORRUPT;
			error_text = "Missing 'id' in external resource tag";
			_printerr();
			return error;
		}

		String path = next_tag.fields["path"];
		String type = next_tag.fields["type"];
		String id = next_tag.fields["id"];

		if (next_tag.fields.has("uid")) {
			String uidt = next_tag.fields["uid"];
			ResourceUID::ID uid = ResourceUID::get_singleton()->text_to_id(uidt);
			if (uid != ResourceUID::INVALID_ID && ResourceUID::get_singleton()->has_id(uid)) {
				// If a UID is found and the path is valid, it will be used, otherwise, it falls back to the path.
				path = ResourceUID::get_singleton()->get_id_path(uid);
			} else {
#ifdef TOOLS_ENABLED
				// Silence a warning that can happen during the initial filesystem scan due to cache being regenerated.
				if (ResourceLoader::get_resource_uid(path) != uid) {
					WARN_PRINT(String(res_path + ":" + itos(lines) + " - ext_resource, invalid UID: " + uidt + " - using text path instead: " + path).utf8().get_data());
				}
#else
				WARN_PRINT(String(res_path + ":" + itos(lines) + " - ext_resource, invalid UID: " + uidt + " - using text path instead: " + path).utf8().get_data());
#endif
			}
		}

		if (!path.contains("://") && path.is_relative_path()) {
			// path is relative to file being loaded, so convert to a resource path
			path = ProjectSettings::get_singleton()->localize_path(local_path.get_base_dir().path_join(path));
		}

		if (remaps.has(path)) {
			path = remaps[path];
		}

		ext_resources[id].path = path;
		ext_resources[id].type = type;
		ext_resources[id].load_token = ResourceLoader::_load_start(path, type, use_sub_threads ? ResourceLoader::LOAD_THREAD_DISTRIBUTE : ResourceLoader::LOAD_THREAD_FROM_CURRENT, cache_mode_for_external);
		if (ext_resources[id].load_token.is_null()) {
			if (ResourceLoader::get_abort_on_missing_resources()) {
				error = ERR_FILE_CORRUPT;
				error_text = "[ext_resource] referenced non-existent resource at: " + path;
				_printerr();
				return error;
			} else {
				ResourceLoader::notify_dependency_error(local_path, path, type);
			}
		}

		error = VariantParser::parse_tag(&stream, lines, error_text, next_tag, &rp);

		if (error) {
			_printerr();
			return error;
		}

		resource_current++;
	}

	//these are the ones that count
	resources_total -= resource_current;
	resource_current = 0;

	while (true) {
		if (next_tag.name != "sub_resource") {
			break;
		}

		if (!next_tag.fields.has("type")) {
			error = ERR_FILE_CORRUPT;
			error_text = "Missing 'type' in external resource tag";
			_printerr();
			return error;
		}

		if (!next_tag.fields.has("id")) {
			error = ERR_FILE_CORRUPT;
			error_text = "Missing 'id' in external resource tag";
			_printerr();
			return error;
		}

		String type = next_tag.fields["type"];
		String id = next_tag.fields["id"];

		String path = local_path + "::" + id;

		//bool exists=ResourceCache::has(path);

		Ref<Resource> res;
		bool do_assign = false;

		if (cache_mode == ResourceFormatLoader::CACHE_MODE_REPLACE && ResourceCache::has(path)) {
			//reuse existing
			Ref<Resource> cache = ResourceCache::get_ref(path);
			if (cache.is_valid() && cache->get_class() == type) {
				res = cache;
				res->reset_state();
				do_assign = true;
			}
		}

		MissingResource *missing_resource = nullptr;

		if (res.is_null()) { //not reuse
			Ref<Resource> cache = ResourceCache::get_ref(path);
			if (cache_mode != ResourceFormatLoader::CACHE_MODE_IGNORE && cache.is_valid()) { //only if it doesn't exist
				//cached, do not assign
				res = cache;
			} else {
				//create

				Object *obj = ClassDB::instantiate(type);
				if (!obj) {
					if (ResourceLoader::is_creating_missing_resources_if_class_unavailable_enabled()) {
						missing_resource = memnew(MissingResource);
						missing_resource->set_original_class(type);
						missing_resource->set_recording_properties(true);
						obj = missing_resource;
					} else {
						error_text = vformat("Can't create sub resource of type '%s'", type);
						_printerr();
						error = ERR_FILE_CORRUPT;
						return error;
					}
				}

				Resource *r = Object::cast_to<Resource>(obj);
				if (!r) {
					error_text = vformat("Can't create sub resource of type '%s' as it's not a resource type", type);
					_printerr();
					error = ERR_FILE_CORRUPT;
					return error;
				}

				res = Ref<Resource>(r);
				do_assign = true;
			}
		}

		resource_current++;

		if (progress && resources_total > 0) {
			*progress = resource_current / float(resources_total);
		}

		int_resources[id] = res; // Always assign int resources.
		if (do_assign) {
			if (cache_mode != ResourceFormatLoader::CACHE_MODE_IGNORE) {
				res->set_path(path, cache_mode == ResourceFormatLoader::CACHE_MODE_REPLACE);
			} else {
				res->set_path_cache(path);
			}
			res->set_scene_unique_id(id);
		}

		Dictionary missing_resource_properties;

		while (true) {
			String assign;
			Variant value;

			error = VariantParser::parse_tag_assign_eof(&stream, lines, error_text, next_tag, assign, value, &rp);

			if (error) {
				_printerr();
				return error;
			}

			if (!assign.is_empty()) {
				if (do_assign) {
					bool set_valid = true;

					if (value.get_type() == Variant::OBJECT && missing_resource == nullptr && ResourceLoader::is_creating_missing_resources_if_class_unavailable_enabled()) {
						// If the property being set is a missing resource (and the parent is not),
						// then setting it will most likely not work.
						// Instead, save it as metadata.

						Ref<MissingResource> mr = value;
						if (mr.is_valid()) {
							missing_resource_properties[assign] = mr;
							set_valid = false;
						}
					}

					if (value.get_type() == Variant::ARRAY) {
						Array set_array = value;
						bool is_get_valid = false;
						Variant get_value = res->get(assign, &is_get_valid);
						if (is_get_valid && get_value.get_type() == Variant::ARRAY) {
							Array get_array = get_value;
							if (!set_array.is_same_typed(get_array)) {
								value = Array(set_array, get_array.get_typed_builtin(), get_array.get_typed_class_name(), get_array.get_typed_script());
							}
						}
					}

					if (value.get_type() == Variant::DICTIONARY) {
						Dictionary set_dict = value;
						bool is_get_valid = false;
						Variant get_value = res->get(assign, &is_get_valid);
						if (is_get_valid && get_value.get_type() == Variant::DICTIONARY) {
							Dictionary get_dict = get_value;
							if (!set_dict.is_same_typed(get_dict)) {
								value = Dictionary(set_dict, get_dict.get_typed_key_builtin(), get_dict.get_typed_key_class_name(), get_dict.get_typed_key_script(),
										get_dict.get_typed_value_builtin(), get_dict.get_typed_value_class_name(), get_dict.get_typed_value_script());
							}
						}
					}

					if (set_valid) {
						res->set(assign, value);
					}
				}
				//it's assignment
			} else if (!next_tag.name.is_empty()) {
				error = OK;
				break;
			} else {
				error = ERR_FILE_CORRUPT;
				error_text = "Premature end of file while parsing [sub_resource]";
				_printerr();
				return error;
			}
		}

		if (missing_resource) {
			missing_resource->set_recording_properties(false);
		}

		if (!missing_resource_properties.is_empty()) {
			res->set_meta(META_MISSING_RESOURCES, missing_resource_properties);
		}
	}

	while (true) {
		if (next_tag.name != "resource") {
			break;
		}

		if (is_scene) {
			error_text = "Unexpected 'resource' tag in a scene file";
			_printerr();
			error = ERR_FILE_CORRUPT;
			return error;
		}

		MissingResource *missing_resource = nullptr;

		resource = ResourceLoader::get_resource_ref_override(local_path);
		if (resource.is_null()) {
			Ref<Resource> cache = ResourceCache::get_ref(local_path);
			if (cache_mode == ResourceFormatLoader::CACHE_MODE_REPLACE && cache.is_valid() && cache->get_class() == res_type) {
				cache->reset_state();
				resource = cache;
			}

			if (resource.is_null()) {
				Object *obj = ClassDB::instantiate(res_type);
				if (!obj) {
					if (ResourceLoader::is_creating_missing_resources_if_class_unavailable_enabled()) {
						missing_resource = memnew(MissingResource);
						missing_resource->set_original_class(res_type);
						missing_resource->set_recording_properties(true);
						obj = missing_resource;
					} else {
						error_text = vformat("Can't create sub resource of type '%s'", res_type);
						_printerr();
						error = ERR_FILE_CORRUPT;
						return error;
					}
				}

				Resource *r = Object::cast_to<Resource>(obj);
				if (!r) {
					error_text = vformat("Can't create sub resource of type '%s' as it's not a resource type", res_type);
					_printerr();
					error = ERR_FILE_CORRUPT;
					return error;
				}

				resource = Ref<Resource>(r);
			}
		}

		Dictionary missing_resource_properties;

		while (true) {
			String assign;
			Variant value;

			error = VariantParser::parse_tag_assign_eof(&stream, lines, error_text, next_tag, assign, value, &rp);

			if (error) {
				if (error != ERR_FILE_EOF) {
					_printerr();
					return error;
				}
				// EOF, Done parsing.
				error = OK;
				if (cache_mode != ResourceFormatLoader::CACHE_MODE_IGNORE) {
					if (!ResourceCache::has(res_path)) {
						resource->set_path(res_path);
					}
					resource->set_as_translation_remapped(translation_remapped);
				} else {
					resource->set_path_cache(res_path);
				}
				break;
			}

			if (!assign.is_empty()) {
				bool set_valid = true;

				if (value.get_type() == Variant::OBJECT && missing_resource == nullptr && ResourceLoader::is_creating_missing_resources_if_class_unavailable_enabled()) {
					// If the property being set is a missing resource (and the parent is not),
					// then setting it will most likely not work.
					// Instead, save it as metadata.

					Ref<MissingResource> mr = value;
					if (mr.is_valid()) {
						missing_resource_properties[assign] = mr;
						set_valid = false;
					}
				}

				if (value.get_type() == Variant::ARRAY) {
					Array set_array = value;
					bool is_get_valid = false;
					Variant get_value = resource->get(assign, &is_get_valid);
					if (is_get_valid && get_value.get_type() == Variant::ARRAY) {
						Array get_array = get_value;
						if (!set_array.is_same_typed(get_array)) {
							value = Array(set_array, get_array.get_typed_builtin(), get_array.get_typed_class_name(), get_array.get_typed_script());
						}
					}
				}

				if (value.get_type() == Variant::DICTIONARY) {
					Dictionary set_dict = value;
					bool is_get_valid = false;
					Variant get_value = resource->get(assign, &is_get_valid);
					if (is_get_valid && get_value.get_type() == Variant::DICTIONARY) {
						Dictionary get_dict = get_value;
						if (!set_dict.is_same_typed(get_dict)) {
							value = Dictionary(set_dict, get_dict.get_typed_key_builtin(), get_dict.get_typed_key_class_name(), get_dict.get_typed_key_script(),
									get_dict.get_typed_value_builtin(), get_dict.get_typed_value_class_name(), get_dict.get_typed_value_script());
						}
					}
				}

				if (set_valid) {
					resource->set(assign, value);
				}
				//it's assignment
			} else if (!next_tag.name.is_empty()) {
				error = ERR_FILE_CORRUPT;
				error_text = "Extra tag found when parsing main resource file";
				_printerr();
				return error;
			} else {
				break;
			}
		}

		resource_current++;

		if (progress && resources_total > 0) {
			*progress = resource_current / float(resources_total);
		}

		if (missing_resource) {
			missing_resource->set_recording_properties(false);
		}

		if (!missing_resource_properties.is_empty()) {
			resource->set_meta(META_MISSING_RESOURCES, missing_resource_properties);
		}

		error = OK;

		return error;
	}

	//for scene files

	if (next_tag.name == "node") {
		if (!is_scene) {
			error_text = "Unexpected 'node' tag in a resource file";
			_printerr();
			error = ERR_FILE_CORRUPT;
			return error;
		}

		Ref<PackedScene> packed_scene = _parse_node_tag(rp);

		if (packed_scene.is_null()) {
			return error;
		}

		error = OK;
		//get it here
		resource = packed_scene;
		if (cache_mode != ResourceFormatLoader::CACHE_MODE_IGNORE) {
			if (!ResourceCache::has(res_path)) {
				packed_scene->set_path(res_path);
			}
		} else {
			packed_scene->get_state()->set_path(res_path);
			packed_scene->set_path_cache(res_path);
		}

		resource_current++;

		if (progress && resources_total > 0) {
			*progress = resource_current / float(resources_total);
		}

		return error;
	} else {
		error_text = vformat("Unknown tag '%s' in file", next_tag.name);
		_printerr();
		error = ERR_FILE_CORRUPT;
		return error;
	}
}

int ResourceLoaderText::get_stage() const {
	return resource_current;
}

int ResourceLoaderText::get_stage_count() const {
	return resources_total; //+ext_resources;
}

void ResourceLoaderText::set_translation_remapped(bool p_remapped) {
	translation_remapped = p_remapped;
}

ResourceLoaderText::ResourceLoaderText() :
		stream(false), format_version(FORMAT_VERSION) {}

void ResourceLoaderText::get_dependencies(Ref<FileAccess> p_f, List<String> *p_dependencies, bool p_add_types) {
	open(p_f);
	ignore_resource_parsing = true;
	ERR_FAIL_COND(error != OK);

	while (next_tag.name == "ext_resource") {
		if (!next_tag.fields.has("type")) {
			error = ERR_FILE_CORRUPT;
			error_text = "Missing 'type' in external resource tag";
			_printerr();
			return;
		}

		if (!next_tag.fields.has("id")) {
			error = ERR_FILE_CORRUPT;
			error_text = "Missing 'id' in external resource tag";
			_printerr();
			return;
		}

		String path = next_tag.fields["path"];
		String type = next_tag.fields["type"];
		String fallback_path;

		bool using_uid = false;
		if (next_tag.fields.has("uid")) {
			// If uid exists, return uid in text format, not the path.
			String uidt = next_tag.fields["uid"];
			ResourceUID::ID uid = ResourceUID::get_singleton()->text_to_id(uidt);
			if (uid != ResourceUID::INVALID_ID) {
				fallback_path = path; // Used by Dependency Editor, in case uid path fails.
				path = ResourceUID::get_singleton()->id_to_text(uid);
				using_uid = true;
			}
		}

		if (!using_uid && !path.contains("://") && path.is_relative_path()) {
			// Path is relative to file being loaded, so convert to a resource path.
			path = ProjectSettings::get_singleton()->localize_path(local_path.get_base_dir().path_join(path));
		}

		if (p_add_types) {
			path += "::" + type;
		}
		if (!fallback_path.is_empty()) {
			if (!p_add_types) {
				path += "::"; // Ensure that path comes third, even if there is no type.
			}
			path += "::" + fallback_path;
		}

		p_dependencies->push_back(path);

		Error err = VariantParser::parse_tag(&stream, lines, error_text, next_tag, &rp);

		if (err) {
			print_line(error_text + " - " + itos(lines));
			error_text = "Unexpected end of file";
			_printerr();
			error = ERR_FILE_CORRUPT;
			return;
		}
	}
}

Error ResourceLoaderText::rename_dependencies(Ref<FileAccess> p_f, const String &p_path, const HashMap<String, String> &p_map) {
	open(p_f, true);
	ERR_FAIL_COND_V(error != OK, error);
	ignore_resource_parsing = true;
	//FileAccess

	Ref<FileAccess> fw;

	String base_path = local_path.get_base_dir();

	uint64_t tag_end = f->get_position();

	while (true) {
		Error err = VariantParser::parse_tag(&stream, lines, error_text, next_tag, &rp);

		if (err != OK) {
			error = ERR_FILE_CORRUPT;
			ERR_FAIL_V(error);
		}

		if (next_tag.name != "ext_resource") {
			//nothing was done
			if (fw.is_null()) {
				return OK;
			}

			break;

		} else {
			if (fw.is_null()) {
				fw = FileAccess::open(p_path + ".depren", FileAccess::WRITE);

				if (res_uid == ResourceUID::INVALID_ID) {
					res_uid = ResourceSaver::get_resource_id_for_path(p_path);
				}

				String uid_text = "";
				if (res_uid != ResourceUID::INVALID_ID) {
					uid_text = " uid=\"" + ResourceUID::get_singleton()->id_to_text(res_uid) + "\"";
				}

				if (is_scene) {
					fw->store_line("[gd_scene load_steps=" + itos(resources_total) + " format=" + itos(format_version) + uid_text + "]\n");
				} else {
					String script_res_text;
					if (!script_class.is_empty()) {
						script_res_text = "script_class=\"" + script_class + "\" ";
					}
					fw->store_line("[gd_resource type=\"" + res_type + "\" " + script_res_text + "load_steps=" + itos(resources_total) + " format=" + itos(format_version) + uid_text + "]\n");
				}
			}

			if (!next_tag.fields.has("path") || !next_tag.fields.has("id") || !next_tag.fields.has("type")) {
				error = ERR_FILE_CORRUPT;
				ERR_FAIL_V(error);
			}

			String path = next_tag.fields["path"];
			String id = next_tag.fields["id"];
			String type = next_tag.fields["type"];

			if (next_tag.fields.has("uid")) {
				String uidt = next_tag.fields["uid"];
				ResourceUID::ID uid = ResourceUID::get_singleton()->text_to_id(uidt);
				if (uid != ResourceUID::INVALID_ID && ResourceUID::get_singleton()->has_id(uid)) {
					// If a UID is found and the path is valid, it will be used, otherwise, it falls back to the path.
					path = ResourceUID::get_singleton()->get_id_path(uid);
				}
			}
			bool relative = false;
			if (!path.begins_with("res://")) {
				path = base_path.path_join(path).simplify_path();
				relative = true;
			}

			if (p_map.has(path)) {
				path = p_map[path];
			}

			if (relative) {
				//restore relative
				path = base_path.path_to_file(path);
			}

			String s = "[ext_resource type=\"" + type + "\"";

			ResourceUID::ID uid = ResourceSaver::get_resource_id_for_path(path);
			if (uid != ResourceUID::INVALID_ID) {
				s += " uid=\"" + ResourceUID::get_singleton()->id_to_text(uid) + "\"";
			}
			s += " path=\"" + path + "\" id=\"" + id + "\"]";
			fw->store_line(s); // Bundled.

			tag_end = f->get_position();
		}
	}

	f->seek(tag_end);

	const uint32_t buffer_size = 2048;
	uint8_t *buffer = (uint8_t *)alloca(buffer_size);
	uint32_t num_read;

	num_read = f->get_buffer(buffer, buffer_size);
	ERR_FAIL_COND_V_MSG(num_read == UINT32_MAX, ERR_CANT_CREATE, "Failed to allocate memory for buffer.");
	ERR_FAIL_COND_V(num_read == 0, ERR_FILE_CORRUPT);

	if (*buffer == '\n') {
		// Skip first newline character since we added one.
		if (num_read > 1) {
			fw->store_buffer(buffer + 1, num_read - 1);
		}
	} else {
		fw->store_buffer(buffer, num_read);
	}

	while (!f->eof_reached()) {
		num_read = f->get_buffer(buffer, buffer_size);
		fw->store_buffer(buffer, num_read);
	}

	bool all_ok = fw->get_error() == OK;

	if (!all_ok) {
		return ERR_CANT_CREATE;
	}

	return OK;
}

void ResourceLoaderText::open(Ref<FileAccess> p_f, bool p_skip_first_tag) {
	error = OK;

	lines = 1;
	f = p_f;

	stream.f = f;
	is_scene = false;
	ignore_resource_parsing = false;
	resource_current = 0;

	VariantParser::Tag tag;
	Error err = VariantParser::parse_tag(&stream, lines, error_text, tag);

	if (err) {
		error = err;
		_printerr();
		return;
	}

	if (tag.fields.has("format")) {
		format_version = tag.fields["format"];
		if (format_version > FORMAT_VERSION) {
			error_text = "Saved with newer format version";
			_printerr();
			error = ERR_FILE_UNRECOGNIZED;
			return;
		}
	} else {
		format_version = FORMAT_VERSION;
	}

	if (tag.name == "gd_scene") {
		is_scene = true;

	} else if (tag.name == "gd_resource") {
		if (!tag.fields.has("type")) {
			error_text = "Missing 'type' field in 'gd_resource' tag";
			_printerr();
			error = ERR_PARSE_ERROR;
			return;
		}

		if (tag.fields.has("script_class")) {
			script_class = tag.fields["script_class"];
		}

		res_type = tag.fields["type"];

	} else {
		error_text = vformat("Unrecognized file type '%s'", tag.name);
		_printerr();
		error = ERR_PARSE_ERROR;
		return;
	}

	if (tag.fields.has("uid")) {
		res_uid = ResourceUID::get_singleton()->text_to_id(tag.fields["uid"]);
	} else {
		res_uid = ResourceUID::INVALID_ID;
	}

	if (tag.fields.has("load_steps")) {
		resources_total = tag.fields["load_steps"];
	} else {
		resources_total = 0;
	}

	if (!p_skip_first_tag) {
		err = VariantParser::parse_tag(&stream, lines, error_text, next_tag, &rp);

		if (err) {
			error_text = "Unexpected end of file";
			_printerr();
			error = ERR_FILE_CORRUPT;
		}
	}

	rp.ext_func = _parse_ext_resources;
	rp.sub_func = _parse_sub_resources;
	rp.userdata = this;
}

Error ResourceLoaderText::get_classes_used(HashSet<StringName> *r_classes) {
	if (error) {
		return error;
	}

	ignore_resource_parsing = true;

	DummyReadData dummy_read;
	dummy_read.no_placeholders = true;
	VariantParser::ResourceParser rp_new;
	rp_new.ext_func = _parse_ext_resource_dummys;
	rp_new.sub_func = _parse_sub_resource_dummys;
	rp_new.userdata = &dummy_read;

	while (next_tag.name == "ext_resource") {
		error = VariantParser::parse_tag(&stream, lines, error_text, next_tag, &rp_new);

		if (error) {
			_printerr();
			return error;
		}
	}

	while (next_tag.name == "sub_resource" || next_tag.name == "resource") {
		if (next_tag.name == "sub_resource") {
			if (!next_tag.fields.has("type")) {
				error = ERR_FILE_CORRUPT;
				error_text = "Missing 'type' in external resource tag";
				_printerr();
				return error;
			}

			r_classes->insert(next_tag.fields["type"]);

		} else {
			r_classes->insert(next_tag.fields["res_type"]);
		}

		while (true) {
			String assign;
			Variant value;

			error = VariantParser::parse_tag_assign_eof(&stream, lines, error_text, next_tag, assign, value, &rp_new);

			if (error) {
				if (error == ERR_FILE_EOF) {
					return OK;
				}

				_printerr();
				return error;
			}

			if (!assign.is_empty()) {
				continue;
			} else if (!next_tag.name.is_empty()) {
				error = OK;
				break;
			} else {
				error = ERR_FILE_CORRUPT;
				error_text = "Premature end of file while parsing [sub_resource]";
				_printerr();
				return error;
			}
		}
	}

	while (next_tag.name == "node") {
		// This is a node, must save one more!

		if (!is_scene) {
			error = ERR_FILE_CORRUPT;
			error_text = "Unexpected 'node' tag in a resource file";
			_printerr();
			return error;
		}

		if (!next_tag.fields.has("type")) {
			error = ERR_FILE_CORRUPT;
			error_text = "Missing 'type' in external resource tag";
			_printerr();
			return error;
		}

		r_classes->insert(next_tag.fields["type"]);

		while (true) {
			String assign;
			Variant value;

			error = VariantParser::parse_tag_assign_eof(&stream, lines, error_text, next_tag, assign, value, &rp_new);

			if (error) {
				if (error == ERR_FILE_MISSING_DEPENDENCIES) {
					// Resource loading error, just skip it.
				} else if (error != ERR_FILE_EOF) {
					_printerr();
					return error;
				} else {
					return OK;
				}
			}

			if (!assign.is_empty()) {
				continue;
			} else if (!next_tag.name.is_empty()) {
				error = OK;
				break;
			} else {
				error = ERR_FILE_CORRUPT;
				error_text = "Premature end of file while parsing [sub_resource]";
				_printerr();
				return error;
			}
		}
	}

	return OK;
}

String ResourceLoaderText::recognize_script_class(Ref<FileAccess> p_f) {
	error = OK;

	lines = 1;
	f = p_f;

	stream.f = f;

	ignore_resource_parsing = true;

	VariantParser::Tag tag;
	Error err = VariantParser::parse_tag(&stream, lines, error_text, tag);

	if (err) {
		_printerr();
		return "";
	}

	if (tag.fields.has("format")) {
		int fmt = tag.fields["format"];
		if (fmt > FORMAT_VERSION) {
			error_text = "Saved with newer format version";
			_printerr();
			return "";
		}
	}

	if (tag.name != "gd_resource") {
		return "";
	}

	if (tag.fields.has("script_class")) {
		return tag.fields["script_class"];
	}

	return "";
}

String ResourceLoaderText::recognize(Ref<FileAccess> p_f) {
	error = OK;

	lines = 1;
	f = p_f;

	stream.f = f;

	ignore_resource_parsing = true;

	VariantParser::Tag tag;
	Error err = VariantParser::parse_tag(&stream, lines, error_text, tag);

	if (err) {
		_printerr();
		return "";
	}

	if (tag.fields.has("format")) {
		int fmt = tag.fields["format"];
		if (fmt > FORMAT_VERSION) {
			error_text = "Saved with newer format version";
			_printerr();
			return "";
		}
	}

	if (tag.name == "gd_scene") {
		return "PackedScene";
	}

	if (tag.name != "gd_resource") {
		return "";
	}

	if (!tag.fields.has("type")) {
		error_text = "Missing 'type' field in 'gd_resource' tag";
		_printerr();
		return "";
	}

	return tag.fields["type"];
}

ResourceUID::ID ResourceLoaderText::get_uid(Ref<FileAccess> p_f) {
	error = OK;

	lines = 1;
	f = p_f;

	stream.f = f;

	ignore_resource_parsing = true;

	VariantParser::Tag tag;
	Error err = VariantParser::parse_tag(&stream, lines, error_text, tag);

	if (err) {
		_printerr();
		return ResourceUID::INVALID_ID;
	}

	if (tag.fields.has("uid")) { //field is optional
		String uidt = tag.fields["uid"];
		return ResourceUID::get_singleton()->text_to_id(uidt);
	}

	return ResourceUID::INVALID_ID;
}

/////////////////////

Ref<Resource> ResourceFormatLoaderText::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	if (r_error) {
		*r_error = ERR_CANT_OPEN;
	}

	Error err;

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);

	ERR_FAIL_COND_V_MSG(err != OK, Ref<Resource>(), "Cannot open file '" + p_path + "'.");

	ResourceLoaderText loader;
	String path = !p_original_path.is_empty() ? p_original_path : p_path;
	switch (p_cache_mode) {
		case CACHE_MODE_IGNORE:
		case CACHE_MODE_REUSE:
		case CACHE_MODE_REPLACE:
			loader.cache_mode = p_cache_mode;
			loader.cache_mode_for_external = CACHE_MODE_REUSE;
			break;
		case CACHE_MODE_IGNORE_DEEP:
			loader.cache_mode = ResourceFormatLoader::CACHE_MODE_IGNORE;
			loader.cache_mode_for_external = p_cache_mode;
			break;
		case CACHE_MODE_REPLACE_DEEP:
			loader.cache_mode = ResourceFormatLoader::CACHE_MODE_REPLACE;
			loader.cache_mode_for_external = p_cache_mode;
			break;
	}
	loader.use_sub_threads = p_use_sub_threads;
	loader.local_path = ProjectSettings::get_singleton()->localize_path(path);
	loader.progress = r_progress;
	loader.res_path = loader.local_path;
	loader.open(f);
	err = loader.load();
	if (r_error) {
		*r_error = err;
	}
	if (err == OK) {
		return loader.get_resource();
	} else {
		return Ref<Resource>();
	}
}

void ResourceFormatLoaderText::get_recognized_extensions_for_type(const String &p_type, List<String> *p_extensions) const {
	if (p_type.is_empty()) {
		get_recognized_extensions(p_extensions);
		return;
	}

	if (ClassDB::is_parent_class("PackedScene", p_type)) {
		p_extensions->push_back("tscn");
	}

	// Don't allow .tres for PackedScenes or GDExtension.
	if (p_type != "PackedScene" && p_type != "GDExtension") {
		p_extensions->push_back("tres");
	}
}

void ResourceFormatLoaderText::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("tscn");
	p_extensions->push_back("tres");
}

bool ResourceFormatLoaderText::handles_type(const String &p_type) const {
	return true;
}

void ResourceFormatLoaderText::get_classes_used(const String &p_path, HashSet<StringName> *r_classes) {
	String ext = p_path.get_extension().to_lower();
	if (ext == "tscn") {
		r_classes->insert("PackedScene");
	}

	// ...for anything else must test...

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return; // Could not read.
	}

	ResourceLoaderText loader;
	loader.local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	loader.res_path = loader.local_path;
	loader.open(f);
	loader.get_classes_used(r_classes);
}

String ResourceFormatLoaderText::get_resource_type(const String &p_path) const {
	String ext = p_path.get_extension().to_lower();
	if (ext == "tscn") {
		return "PackedScene";
	} else if (ext != "tres") {
		return String();
	}

	// ...for anything else must test...

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return ""; //could not read
	}

	ResourceLoaderText loader;
	loader.local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	loader.res_path = loader.local_path;
	String r = loader.recognize(f);
	return ClassDB::get_compatibility_remapped_class(r);
}

String ResourceFormatLoaderText::get_resource_script_class(const String &p_path) const {
	String ext = p_path.get_extension().to_lower();
	if (ext != "tres") {
		return String();
	}

	// ...for anything else must test...

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return ""; //could not read
	}

	ResourceLoaderText loader;
	loader.local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	loader.res_path = loader.local_path;
	return loader.recognize_script_class(f);
}

ResourceUID::ID ResourceFormatLoaderText::get_resource_uid(const String &p_path) const {
	String ext = p_path.get_extension().to_lower();

	if (ext != "tscn" && ext != "tres") {
		return ResourceUID::INVALID_ID;
	}

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return ResourceUID::INVALID_ID; //could not read
	}

	ResourceLoaderText loader;
	loader.local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	loader.res_path = loader.local_path;
	return loader.get_uid(f);
}

bool ResourceFormatLoaderText::has_custom_uid_support() const {
	return true;
}

void ResourceFormatLoaderText::get_dependencies(const String &p_path, List<String> *p_dependencies, bool p_add_types) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		ERR_FAIL();
	}

	ResourceLoaderText loader;
	loader.local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	loader.res_path = loader.local_path;
	loader.get_dependencies(f, p_dependencies, p_add_types);
}

Error ResourceFormatLoaderText::rename_dependencies(const String &p_path, const HashMap<String, String> &p_map) {
	Error err = OK;
	{
		Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
		if (f.is_null()) {
			ERR_FAIL_V(ERR_CANT_OPEN);
		}

		ResourceLoaderText loader;
		loader.local_path = ProjectSettings::get_singleton()->localize_path(p_path);
		loader.res_path = loader.local_path;
		err = loader.rename_dependencies(f, p_path, p_map);
	}

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (err == OK && da->file_exists(p_path + ".depren")) {
		da->remove(p_path);
		da->rename(p_path + ".depren", p_path);
	}

	return err;
}

ResourceFormatLoaderText *ResourceFormatLoaderText::singleton = nullptr;

/*****************************************************************************************************/

String ResourceFormatSaverTextInstance::_write_resources(void *ud, const Ref<Resource> &p_resource) {
	ResourceFormatSaverTextInstance *rsi = static_cast<ResourceFormatSaverTextInstance *>(ud);
	return rsi->_write_resource(p_resource);
}

String ResourceFormatSaverTextInstance::_write_resource(const Ref<Resource> &res) {
	if (res->get_meta(SNAME("_skip_save_"), false)) {
		return "null";
	}

	if (external_resources.has(res)) {
		return "ExtResource(\"" + external_resources[res] + "\")";
	} else {
		if (internal_resources.has(res)) {
			return "SubResource(\"" + internal_resources[res] + "\")";
		} else if (!res->is_built_in()) {
			if (res->get_path() == local_path) { //circular reference attempt
				return "null";
			}
			//external resource
			String path = relative_paths ? local_path.path_to_file(res->get_path()) : res->get_path();
			return "Resource(\"" + path + "\")";
		} else {
			ERR_FAIL_V_MSG("null", "Resource was not pre cached for the resource section, bug?");
			//internal resource
		}
	}
}

void ResourceFormatSaverTextInstance::_find_resources(const Variant &p_variant, bool p_main) {
	switch (p_variant.get_type()) {
		case Variant::OBJECT: {
			Ref<Resource> res = p_variant;

			if (res.is_null() || external_resources.has(res) || res->get_meta(SNAME("_skip_save_"), false)) {
				return;
			}

			if (!p_main && (!bundle_resources) && !res->is_built_in()) {
				if (res->get_path() == local_path) {
					ERR_PRINT("Circular reference to resource being saved found: '" + local_path + "' will be null next time it's loaded.");
					return;
				}

				// Use a numeric ID as a base, because they are sorted in natural order before saving.
				// This increases the chances of thread loading to fetch them first.
				String id = itos(external_resources.size() + 1) + "_" + Resource::generate_scene_unique_id();
				external_resources[res] = id;
				return;
			}

			if (resource_set.has(res)) {
				return;
			}

			resource_set.insert(res);

			List<PropertyInfo> property_list;

			res->get_property_list(&property_list);
			property_list.sort();

			List<PropertyInfo>::Element *I = property_list.front();

			while (I) {
				PropertyInfo pi = I->get();

				if (pi.usage & PROPERTY_USAGE_STORAGE) {
					Variant v = res->get(I->get().name);

					if (pi.usage & PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT) {
						NonPersistentKey npk;
						npk.base = res;
						npk.property = pi.name;
						non_persistent_map[npk] = v;

						Ref<Resource> sres = v;
						if (sres.is_valid()) {
							resource_set.insert(sres);
							saved_resources.push_back(sres);
						} else {
							_find_resources(v);
						}
					} else {
						_find_resources(v);
					}
				}

				I = I->next();
			}

			saved_resources.push_back(res); // Saved after, so the children it needs are available when loaded

		} break;
		case Variant::ARRAY: {
			Array varray = p_variant;
			_find_resources(varray.get_typed_script());
			for (const Variant &var : varray) {
				_find_resources(var);
			}

		} break;
		case Variant::DICTIONARY: {
			Dictionary d = p_variant;
			_find_resources(d.get_typed_key_script());
			_find_resources(d.get_typed_value_script());
			List<Variant> keys;
			d.get_key_list(&keys);
			for (const Variant &E : keys) {
				// Of course keys should also be cached, after all we can't prevent users from using resources as keys, right?
				// See also ResourceFormatSaverBinaryInstance::_find_resources (when p_variant is of type Variant::DICTIONARY)
				_find_resources(E);
				Variant v = d[E];
				_find_resources(v);
			}
		} break;
		case Variant::PACKED_BYTE_ARRAY: {
			// Balance between compatibility and performance.
			if (use_compat && p_variant.operator PackedByteArray().size() > 64) {
				use_compat = false;
			}
		} break;
		case Variant::PACKED_VECTOR4_ARRAY: {
			use_compat = false;
		} break;
		default: {
		}
	}
}

static String _resource_get_class(Ref<Resource> p_resource) {
	Ref<MissingResource> missing_resource = p_resource;
	if (missing_resource.is_valid()) {
		return missing_resource->get_original_class();
	} else {
		return p_resource->get_class();
	}
}

Error ResourceFormatSaverTextInstance::save(const String &p_path, const Ref<Resource> &p_resource, uint32_t p_flags) {
	Resource::seed_scene_unique_id(p_path.hash()); // Seeding for save path should make it deterministic for importers.

	if (p_path.ends_with(".tscn")) {
		packed_scene = p_resource;
	}

	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err, ERR_CANT_OPEN, "Cannot save file '" + p_path + "'.");
	Ref<FileAccess> _fref(f);

	local_path = ProjectSettings::get_singleton()->localize_path(p_path);

	relative_paths = p_flags & ResourceSaver::FLAG_RELATIVE_PATHS;
	skip_editor = p_flags & ResourceSaver::FLAG_OMIT_EDITOR_PROPERTIES;
	bundle_resources = p_flags & ResourceSaver::FLAG_BUNDLE_RESOURCES;
	takeover_paths = p_flags & ResourceSaver::FLAG_REPLACE_SUBRESOURCE_PATHS;
	if (!p_path.begins_with("res://")) {
		takeover_paths = false;
	}

	// Save resources.
	use_compat = true; // _find_resources() changes this.
	_find_resources(p_resource, true);

	if (packed_scene.is_valid()) {
		// Add instances to external resources if saving a packed scene.
		for (int i = 0; i < packed_scene->get_state()->get_node_count(); i++) {
			if (packed_scene->get_state()->is_node_instance_placeholder(i)) {
				continue;
			}

			Ref<PackedScene> instance = packed_scene->get_state()->get_node_instance(i);
			if (instance.is_valid() && !external_resources.has(instance)) {
				int index = external_resources.size() + 1;
				external_resources[instance] = itos(index) + "_" + Resource::generate_scene_unique_id(); // Keep the order for improved thread loading performance.
			}
		}
	}

	{
		String title = packed_scene.is_valid() ? "[gd_scene " : "[gd_resource ";
		if (packed_scene.is_null()) {
			title += "type=\"" + _resource_get_class(p_resource) + "\" ";
			Ref<Script> script = p_resource->get_script();
			if (script.is_valid() && script->get_global_name()) {
				title += "script_class=\"" + String(script->get_global_name()) + "\" ";
			}
		}

		int load_steps = saved_resources.size() + external_resources.size();

		if (load_steps > 1) {
			title += "load_steps=" + itos(load_steps) + " ";
		}
		title += "format=" + itos(use_compat ? ResourceLoaderText::FORMAT_VERSION_COMPAT : ResourceLoaderText::FORMAT_VERSION) + "";

		ResourceUID::ID uid = ResourceSaver::get_resource_id_for_path(local_path, true);

		if (uid != ResourceUID::INVALID_ID) {
			title += " uid=\"" + ResourceUID::get_singleton()->id_to_text(uid) + "\"";
		}

		f->store_string(title);
		f->store_line("]\n"); // One empty line.
	}

#ifdef TOOLS_ENABLED
	// Keep order from cached ids.
	HashSet<String> cached_ids_found;
	for (KeyValue<Ref<Resource>, String> &E : external_resources) {
		String cached_id = E.key->get_id_for_path(local_path);
		if (cached_id.is_empty() || cached_ids_found.has(cached_id)) {
			int sep_pos = E.value.find_char('_');
			if (sep_pos != -1) {
				E.value = E.value.substr(0, sep_pos + 1); // Keep the order found, for improved thread loading performance.
			} else {
				E.value = "";
			}

		} else {
			E.value = cached_id;
			cached_ids_found.insert(cached_id);
		}
	}
	// Create IDs for non cached resources.
	for (KeyValue<Ref<Resource>, String> &E : external_resources) {
		if (cached_ids_found.has(E.value)) { // Already cached, go on.
			continue;
		}

		String attempt;
		while (true) {
			attempt = E.value + Resource::generate_scene_unique_id();
			if (!cached_ids_found.has(attempt)) {
				break;
			}
		}

		cached_ids_found.insert(attempt);
		E.value = attempt;
		// Update also in resource.
		Ref<Resource> res = E.key;
		res->set_id_for_path(local_path, attempt);
	}
#else
	// Make sure to start from one, as it makes format more readable.
	int counter = 1;
	for (KeyValue<Ref<Resource>, String> &E : external_resources) {
		E.value = itos(counter++);
	}
#endif

	Vector<ResourceSort> sorted_er;

	for (const KeyValue<Ref<Resource>, String> &E : external_resources) {
		ResourceSort rs;
		rs.resource = E.key;
		rs.id = E.value;
		sorted_er.push_back(rs);
	}

	sorted_er.sort();

	for (int i = 0; i < sorted_er.size(); i++) {
		String p = sorted_er[i].resource->get_path();

		String s = "[ext_resource type=\"" + sorted_er[i].resource->get_save_class() + "\"";

		ResourceUID::ID uid = ResourceSaver::get_resource_id_for_path(p, false);
		if (uid != ResourceUID::INVALID_ID) {
			s += " uid=\"" + ResourceUID::get_singleton()->id_to_text(uid) + "\"";
		}
		s += " path=\"" + p + "\" id=\"" + sorted_er[i].id + "\"]\n";
		f->store_string(s); // Bundled.
	}

	if (external_resources.size()) {
		f->store_line(String()); // Separate.
	}

	HashSet<String> used_unique_ids;

	for (List<Ref<Resource>>::Element *E = saved_resources.front(); E; E = E->next()) {
		Ref<Resource> res = E->get();
		if (E->next() && res->is_built_in()) {
			if (!res->get_scene_unique_id().is_empty()) {
				if (used_unique_ids.has(res->get_scene_unique_id())) {
					res->set_scene_unique_id(""); // Repeated.
				} else {
					used_unique_ids.insert(res->get_scene_unique_id());
				}
			}
		}
	}

	for (List<Ref<Resource>>::Element *E = saved_resources.front(); E; E = E->next()) {
		Ref<Resource> res = E->get();
		ERR_CONTINUE(!resource_set.has(res));
		bool main = (E->next() == nullptr);

		if (main && packed_scene.is_valid()) {
			break; // Save as a scene.
		}

		if (main) {
			f->store_line("[resource]");
		} else {
			String line = "[sub_resource ";
			if (res->get_scene_unique_id().is_empty()) {
				String new_id;
				while (true) {
					new_id = _resource_get_class(res) + "_" + Resource::generate_scene_unique_id();

					if (!used_unique_ids.has(new_id)) {
						break;
					}
				}

				res->set_scene_unique_id(new_id);
				used_unique_ids.insert(new_id);
			}

			String id = res->get_scene_unique_id();
			line += "type=\"" + _resource_get_class(res) + "\" id=\"" + id;
			f->store_line(line + "\"]");
			if (takeover_paths) {
				res->set_path(p_path + "::" + id, true);
			}

			internal_resources[res] = id;
#ifdef TOOLS_ENABLED
			res->set_edited(false);
#endif
		}

		Dictionary missing_resource_properties = res->get_meta(META_MISSING_RESOURCES, Dictionary());

		List<PropertyInfo> property_list;
		res->get_property_list(&property_list);
		for (List<PropertyInfo>::Element *PE = property_list.front(); PE; PE = PE->next()) {
			if (skip_editor && PE->get().name.begins_with("__editor")) {
				continue;
			}
			if (PE->get().name == META_PROPERTY_MISSING_RESOURCES) {
				continue;
			}

			if (PE->get().usage & PROPERTY_USAGE_STORAGE || missing_resource_properties.has(PE->get().name)) {
				String name = PE->get().name;
				Variant value;
				if (PE->get().usage & PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT) {
					NonPersistentKey npk;
					npk.base = res;
					npk.property = name;
					if (non_persistent_map.has(npk)) {
						value = non_persistent_map[npk];
					}
				} else {
					value = res->get(name);
				}

				if (PE->get().type == Variant::OBJECT && missing_resource_properties.has(PE->get().name)) {
					// Was this missing resource overridden? If so do not save the old value.
					Ref<Resource> ures = value;
					if (ures.is_null()) {
						value = missing_resource_properties[PE->get().name];
					}
				}

				Variant default_value = ClassDB::class_get_default_property_value(res->get_class(), name);

				if (default_value.get_type() != Variant::NIL && bool(Variant::evaluate(Variant::OP_EQUAL, value, default_value))) {
					continue;
				}

				if (PE->get().type == Variant::OBJECT && value.is_zero() && !(PE->get().usage & PROPERTY_USAGE_STORE_IF_NULL)) {
					continue;
				}

				String vars;
				VariantWriter::write_to_string(value, vars, _write_resources, this, use_compat);
				f->store_string(name.property_name_encode() + " = " + vars + "\n");
			}
		}

		if (E->next()) {
			f->store_line(String());
		}
	}

	if (packed_scene.is_valid()) {
		// If this is a scene, save nodes and connections!
		Ref<SceneState> state = packed_scene->get_state();
		for (int i = 0; i < state->get_node_count(); i++) {
			StringName type = state->get_node_type(i);
			StringName name = state->get_node_name(i);
			int index = state->get_node_index(i);
			NodePath path = state->get_node_path(i, true);
			NodePath owner = state->get_node_owner_path(i);
			Ref<PackedScene> instance = state->get_node_instance(i);
			String instance_placeholder = state->get_node_instance_placeholder(i);
			Vector<StringName> groups = state->get_node_groups(i);
			Vector<String> deferred_node_paths = state->get_node_deferred_nodepath_properties(i);

			String header = "[node";
			header += " name=\"" + String(name).c_escape() + "\"";
			if (type != StringName()) {
				header += " type=\"" + String(type) + "\"";
			}
			if (path != NodePath()) {
				header += " parent=\"" + String(path.simplified()).c_escape() + "\"";
			}
			if (owner != NodePath() && owner != NodePath(".")) {
				header += " owner=\"" + String(owner.simplified()).c_escape() + "\"";
			}
			if (index >= 0) {
				header += " index=\"" + itos(index) + "\"";
			}

			if (deferred_node_paths.size()) {
				header += " node_paths=" + Variant(deferred_node_paths).get_construct_string();
			}

			if (groups.size()) {
				// Write all groups on the same line as they're part of a section header.
				// This improves readability while not impacting VCS friendliness too much,
				// since it's rare to have more than 5 groups assigned to a single node.
				groups.sort_custom<StringName::AlphCompare>();
				String sgroups = " groups=[";
				for (int j = 0; j < groups.size(); j++) {
					sgroups += "\"" + String(groups[j]).c_escape() + "\"";
					if (j < groups.size() - 1) {
						sgroups += ", ";
					}
				}
				sgroups += "]";
				header += sgroups;
			}

			f->store_string(header);

			if (!instance_placeholder.is_empty()) {
				String vars;
				f->store_string(" instance_placeholder=");
				VariantWriter::write_to_string(instance_placeholder, vars, _write_resources, this, use_compat);
				f->store_string(vars);
			}

			if (instance.is_valid()) {
				String vars;
				f->store_string(" instance=");
				VariantWriter::write_to_string(instance, vars, _write_resources, this, use_compat);
				f->store_string(vars);
			}

			f->store_line("]");

			for (int j = 0; j < state->get_node_property_count(i); j++) {
				String vars;
				VariantWriter::write_to_string(state->get_node_property_value(i, j), vars, _write_resources, this, use_compat);

				f->store_string(String(state->get_node_property_name(i, j)).property_name_encode() + " = " + vars + "\n");
			}

			if (i < state->get_node_count() - 1) {
				f->store_line(String());
			}
		}

		for (int i = 0; i < state->get_connection_count(); i++) {
			if (i == 0) {
				f->store_line("");
			}

			String connstr = "[connection";
			connstr += " signal=\"" + String(state->get_connection_signal(i)).c_escape() + "\"";
			connstr += " from=\"" + String(state->get_connection_source(i).simplified()).c_escape() + "\"";
			connstr += " to=\"" + String(state->get_connection_target(i).simplified()).c_escape() + "\"";
			connstr += " method=\"" + String(state->get_connection_method(i)).c_escape() + "\"";
			int flags = state->get_connection_flags(i);
			if (flags != Object::CONNECT_PERSIST) {
				connstr += " flags=" + itos(flags);
			}

			int unbinds = state->get_connection_unbinds(i);
			if (unbinds > 0) {
				connstr += " unbinds=" + itos(unbinds);
			}

			Array binds = state->get_connection_binds(i);
			f->store_string(connstr);
			if (binds.size()) {
				String vars;
				VariantWriter::write_to_string(binds, vars, _write_resources, this, use_compat);
				f->store_string(" binds= " + vars);
			}

			f->store_line("]");
		}

		Vector<NodePath> editable_instances = state->get_editable_instances();
		for (int i = 0; i < editable_instances.size(); i++) {
			if (i == 0) {
				f->store_line("");
			}
			f->store_line("[editable path=\"" + editable_instances[i].operator String().c_escape() + "\"]");
		}
	}

	if (f->get_error() != OK && f->get_error() != ERR_FILE_EOF) {
		return ERR_CANT_CREATE;
	}

	return OK;
}

Error ResourceLoaderText::set_uid(Ref<FileAccess> p_f, ResourceUID::ID p_uid) {
	open(p_f, true);
	ERR_FAIL_COND_V(error != OK, error);
	ignore_resource_parsing = true;

	Ref<FileAccess> fw;

	fw = FileAccess::open(local_path + ".uidren", FileAccess::WRITE);
	if (is_scene) {
		fw->store_string("[gd_scene load_steps=" + itos(resources_total) + " format=" + itos(format_version) + " uid=\"" + ResourceUID::get_singleton()->id_to_text(p_uid) + "\"]");
	} else {
		String script_res_text;
		if (!script_class.is_empty()) {
			script_res_text = "script_class=\"" + script_class + "\" ";
		}

		fw->store_string("[gd_resource type=\"" + res_type + "\" " + script_res_text + "load_steps=" + itos(resources_total) + " format=" + itos(format_version) + " uid=\"" + ResourceUID::get_singleton()->id_to_text(p_uid) + "\"]");
	}

	uint8_t c = f->get_8();
	while (!f->eof_reached()) {
		fw->store_8(c);
		c = f->get_8();
	}

	bool all_ok = fw->get_error() == OK;

	if (!all_ok) {
		return ERR_CANT_CREATE;
	}

	return OK;
}

Error ResourceFormatSaverText::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	if (p_path.ends_with(".tscn") && Ref<PackedScene>(p_resource).is_null()) {
		return ERR_FILE_UNRECOGNIZED;
	}

	ResourceFormatSaverTextInstance saver;
	return saver.save(p_path, p_resource, p_flags);
}

Error ResourceFormatSaverText::set_uid(const String &p_path, ResourceUID::ID p_uid) {
	String lc = p_path.to_lower();
	if (!lc.ends_with(".tscn") && !lc.ends_with(".tres")) {
		return ERR_FILE_UNRECOGNIZED;
	}

	String local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	Error err = OK;
	{
		Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
		if (file.is_null()) {
			ERR_FAIL_V(ERR_CANT_OPEN);
		}

		ResourceLoaderText loader;
		loader.local_path = local_path;
		loader.res_path = loader.local_path;
		err = loader.set_uid(file, p_uid);
	}

	if (err == OK) {
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
		da->remove(local_path);
		da->rename(local_path + ".uidren", local_path);
	}

	return err;
}

bool ResourceFormatSaverText::recognize(const Ref<Resource> &p_resource) const {
	return true; // All resources recognized!
}

void ResourceFormatSaverText::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	if (Ref<PackedScene>(p_resource).is_valid()) {
		p_extensions->push_back("tscn"); // Text scene.
	} else {
		p_extensions->push_back("tres"); // Text resource.
	}
}

ResourceFormatSaverText *ResourceFormatSaverText::singleton = nullptr;
ResourceFormatSaverText::ResourceFormatSaverText() {
	singleton = this;
}

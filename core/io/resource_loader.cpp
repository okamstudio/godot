/**************************************************************************/
/*  resource_loader.cpp                                                   */
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

#include "resource_loader.h"

#include "core/io/resource_importer.h"
#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/path_remap.h"
#include "core/print_string.h"
#include "core/project_settings.h"
#include "core/translation.h"
#include "core/variant_parser.h"

Ref<ResourceFormatLoader> ResourceLoader::loader[ResourceLoader::MAX_LOADERS];

int ResourceLoader::loader_count = 0;

Error ResourceInteractiveLoader::wait() {
	Error err = poll();
	while (err == OK) {
		err = poll();
	}

	return err;
}

void ResourceInteractiveLoader::set_no_subresource_cache(bool p_no_subresource_cache) {
	no_subresource_cache = p_no_subresource_cache;
}

bool ResourceInteractiveLoader::get_no_subresource_cache() {
	return no_subresource_cache;
}

ResourceInteractiveLoader::~ResourceInteractiveLoader() {
	if (path_loading != String()) {
		ResourceLoader::_remove_from_loading_map_and_thread(path_loading, path_loading_thread);
	}
}

bool ResourceFormatLoader::recognize_path(const String &p_path, const String &p_for_type) const {
	String extension = p_path.get_extension();

	List<String> extensions;
	if (p_for_type == String()) {
		get_recognized_extensions(&extensions);
	} else {
		get_recognized_extensions_for_type(p_for_type, &extensions);
	}

	for (List<String>::Element *E = extensions.front(); E; E = E->next()) {
		if (E->get().nocasecmp_to(extension) == 0) {
			return true;
		}
	}

	return false;
}

bool ResourceFormatLoader::handles_type(const String &p_type) const {
	if (get_script_instance() && get_script_instance()->has_method("handles_type")) {
		// I guess custom loaders for custom resources should use "Resource"
		return get_script_instance()->call("handles_type", p_type);
	}

	return false;
}

String ResourceFormatLoader::get_resource_type(const String &p_path) const {
	if (get_script_instance() && get_script_instance()->has_method("get_resource_type")) {
		return get_script_instance()->call("get_resource_type", p_path);
	}

	return "";
}

void ResourceFormatLoader::get_recognized_extensions_for_type(const String &p_type, List<String> *p_extensions) const {
	if (p_type == "" || handles_type(p_type)) {
		get_recognized_extensions(p_extensions);
	}
}

void ResourceLoader::get_recognized_extensions_for_type(const String &p_type, List<String> *p_extensions) {
	for (int i = 0; i < loader_count; i++) {
		loader[i]->get_recognized_extensions_for_type(p_type, p_extensions);
	}
}

void ResourceInteractiveLoader::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_resource"), &ResourceInteractiveLoader::get_resource);
	ClassDB::bind_method(D_METHOD("poll"), &ResourceInteractiveLoader::poll);
	ClassDB::bind_method(D_METHOD("wait"), &ResourceInteractiveLoader::wait);
	ClassDB::bind_method(D_METHOD("get_stage"), &ResourceInteractiveLoader::get_stage);
	ClassDB::bind_method(D_METHOD("get_stage_count"), &ResourceInteractiveLoader::get_stage_count);
	ClassDB::bind_method(D_METHOD("set_no_subresource_cache", "no_subresource_cache"), &ResourceInteractiveLoader::set_no_subresource_cache);
	ClassDB::bind_method(D_METHOD("get_no_subresource_cache"), &ResourceInteractiveLoader::get_no_subresource_cache);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "no_subresource_cache"), "set_no_subresource_cache", "get_no_subresource_cache");
}

class ResourceInteractiveLoaderDefault : public ResourceInteractiveLoader {
	GDCLASS(ResourceInteractiveLoaderDefault, ResourceInteractiveLoader);

public:
	Ref<Resource> resource;

	virtual void set_local_path(const String &p_local_path) { /*scene->set_filename(p_local_path);*/
	}
	virtual Ref<Resource> get_resource() { return resource; }
	virtual Error poll() { return ERR_FILE_EOF; }
	virtual int get_stage() const { return 1; }
	virtual int get_stage_count() const { return 1; }
	virtual void set_translation_remapped(bool p_remapped) { resource->set_as_translation_remapped(p_remapped); }

	ResourceInteractiveLoaderDefault() {}
};

bool ResourceFormatLoader::exists(const String &p_path) const {
	return FileAccess::exists(p_path); //by default just check file
}

void ResourceFormatLoader::get_recognized_extensions(List<String> *p_extensions) const {
	if (get_script_instance() && get_script_instance()->has_method("get_recognized_extensions")) {
		PoolStringArray exts = get_script_instance()->call("get_recognized_extensions");

		{
			PoolStringArray::Read r = exts.read();
			for (int i = 0; i < exts.size(); ++i) {
				p_extensions->push_back(r[i]);
			}
		}
	}
}

// Warning: Derived classes must override either `load` or `load_interactive`. The base code
// here can trigger an infinite recursion otherwise, since `load` calls `load_interactive`
// vice versa.

Ref<ResourceInteractiveLoader> ResourceFormatLoader::load_interactive(const String &p_path, const String &p_original_path, Error *r_error, bool p_no_subresource_cache) {
	// Warning: See previous note about the risk of infinite recursion.
	Ref<Resource> res = load(p_path, p_original_path, r_error, p_no_subresource_cache);
	if (res.is_null()) {
		return Ref<ResourceInteractiveLoader>();
	}

	Ref<ResourceInteractiveLoaderDefault> ril = Ref<ResourceInteractiveLoaderDefault>(memnew(ResourceInteractiveLoaderDefault));
	ril->set_no_subresource_cache(p_no_subresource_cache);
	ril->resource = res;
	return ril;
}

RES ResourceFormatLoader::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_no_subresource_cache) {
	// Check user-defined loader if there's any. Hard fail if it returns an error.
	if (get_script_instance() && get_script_instance()->has_method("load")) {
		Variant res = get_script_instance()->call("load", p_path, p_original_path, p_no_subresource_cache);

		if (res.get_type() == Variant::INT) { // Error code, abort.
			if (r_error) {
				*r_error = (Error)res.operator int64_t();
			}
			return RES();
		} else { // Success, pass on result.
			if (r_error) {
				*r_error = OK;
			}
			return res;
		}
	}

	// Warning: See previous note about the risk of infinite recursion.
	Ref<ResourceInteractiveLoader> ril = load_interactive(p_path, p_original_path, r_error, p_no_subresource_cache);
	if (!ril.is_valid()) {
		return RES();
	}
	ril->set_local_path(p_original_path);

	while (true) {
		Error err = ril->poll();

		if (err == ERR_FILE_EOF) {
			if (r_error) {
				*r_error = OK;
			}
			return ril->get_resource();
		}

		if (r_error) {
			*r_error = err;
		}

		ERR_FAIL_COND_V_MSG(err != OK, RES(), "Failed to load resource '" + p_path + "'.");
	}
}

void ResourceFormatLoader::get_dependencies(const String &p_path, List<String> *p_dependencies, bool p_add_types) {
	if (get_script_instance() && get_script_instance()->has_method("get_dependencies")) {
		PoolStringArray deps = get_script_instance()->call("get_dependencies", p_path, p_add_types);

		{
			PoolStringArray::Read r = deps.read();
			for (int i = 0; i < deps.size(); ++i) {
				p_dependencies->push_back(r[i]);
			}
		}
	}
}

Error ResourceFormatLoader::rename_dependencies(const String &p_path, const Map<String, String> &p_map) {
	if (get_script_instance() && get_script_instance()->has_method("rename_dependencies")) {
		Dictionary deps_dict;
		for (Map<String, String>::Element *E = p_map.front(); E; E = E->next()) {
			deps_dict[E->key()] = E->value();
		}

		int64_t res = get_script_instance()->call("rename_dependencies", deps_dict);
		return (Error)res;
	}

	return OK;
}

void ResourceFormatLoader::_bind_methods() {
	{
		MethodInfo info = MethodInfo(Variant::NIL, "load", PropertyInfo(Variant::STRING, "path"), PropertyInfo(Variant::STRING, "original_path"), PropertyInfo(Variant::BOOL, "no_subresource_cache"));
		info.return_val.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
		ClassDB::add_virtual_method(get_class_static(), info);
	}

	ClassDB::add_virtual_method(get_class_static(), MethodInfo(Variant::POOL_STRING_ARRAY, "get_recognized_extensions"));
	ClassDB::add_virtual_method(get_class_static(), MethodInfo(Variant::BOOL, "handles_type", PropertyInfo(Variant::STRING, "typename")));
	ClassDB::add_virtual_method(get_class_static(), MethodInfo(Variant::STRING, "get_resource_type", PropertyInfo(Variant::STRING, "path")));
	ClassDB::add_virtual_method(get_class_static(), MethodInfo("get_dependencies", PropertyInfo(Variant::STRING, "path"), PropertyInfo(Variant::STRING, "add_types")));
	ClassDB::add_virtual_method(get_class_static(), MethodInfo(Variant::INT, "rename_dependencies", PropertyInfo(Variant::STRING, "path"), PropertyInfo(Variant::STRING, "renames")));
}

///////////////////////////////////

RES ResourceLoader::_load(const String &p_path, const String &p_original_path, const String &p_type_hint, bool p_no_cache, Error *r_error) {
	bool found = false;

	// Try all loaders and pick the first match for the type hint
	for (int i = 0; i < loader_count; i++) {
		if (!loader[i]->recognize_path(p_path, p_type_hint)) {
			continue;
		}
		found = true;
		RES res = loader[i]->load(p_path, p_original_path != String() ? p_original_path : p_path, r_error, p_no_cache);
		if (res.is_null()) {
			continue;
		}

		return res;
	}

	ERR_FAIL_COND_V_MSG(found, RES(),
			vformat("Failed loading resource: %s. Make sure resources have been imported by opening the project in the editor at least once.", p_path));

#ifdef TOOLS_ENABLED
	FileAccessRef file_check = FileAccess::create(FileAccess::ACCESS_RESOURCES);
	ERR_FAIL_COND_V_MSG(!file_check->file_exists(p_path), RES(), "Resource file not found: " + p_path + ".");
#endif

	ERR_FAIL_V_MSG(RES(), "No loader found for resource: " + p_path + ".");
}

bool ResourceLoader::_add_to_loading_map(const String &p_path) {
	bool success;
	loading_map_mutex.lock();

	LoadingMapKey key;
	key.path = p_path;
	key.thread = Thread::get_caller_id();

	if (loading_map.has(key)) {
		success = false;
	} else {
		loading_map[key] = true;
		success = true;
	}

	loading_map_mutex.unlock();

	return success;
}

void ResourceLoader::_remove_from_loading_map(const String &p_path) {
	loading_map_mutex.lock();

	LoadingMapKey key;
	key.path = p_path;
	key.thread = Thread::get_caller_id();

	loading_map.erase(key);

	loading_map_mutex.unlock();
}

void ResourceLoader::_remove_from_loading_map_and_thread(const String &p_path, Thread::ID p_thread) {
	loading_map_mutex.lock();

	LoadingMapKey key;
	key.path = p_path;
	key.thread = p_thread;

	loading_map.erase(key);

	loading_map_mutex.unlock();
}

RES ResourceLoader::load(const String &p_path, const String &p_type_hint, bool p_no_cache, Error *r_error) {
	if (r_error) {
		*r_error = ERR_CANT_OPEN;
	}

	String local_path;
	if (p_path.is_rel_path()) {
		local_path = "res://" + p_path;
	} else {
		local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	}

	if (!p_no_cache) {
		{
			bool success = _add_to_loading_map(local_path);
			ERR_FAIL_COND_V_MSG(!success, RES(), "Resource: '" + local_path + "' is already being loaded. Cyclic reference?");
		}

		//lock first if possible
		ResourceCache::lock.read_lock();

		//get ptr
		Resource **rptr = ResourceCache::resources.getptr(local_path);

		if (rptr) {
			RES res(*rptr);
			//it is possible this resource was just freed in a thread. If so, this referencing will not work and resource is considered not cached
			if (res.is_valid()) {
				//referencing is fine
				if (r_error) {
					*r_error = OK;
				}
				ResourceCache::lock.read_unlock();
				_remove_from_loading_map(local_path);
				return res;
			}
		}
		ResourceCache::lock.read_unlock();
	} else if (ResourceCache::has(local_path)) {
		ResourceCache::forget(local_path);
	}

	bool xl_remapped = false;
	String path = _path_remap(local_path, &xl_remapped);

	if (path == "") {
		if (!p_no_cache) {
			_remove_from_loading_map(local_path);
		}
		ERR_FAIL_V_MSG(RES(), "Remapping '" + local_path + "' failed.");
	}

	print_verbose("Loading resource: " + path);
	RES res = _load(path, local_path, p_type_hint, p_no_cache, r_error);

	if (res.is_null()) {
		if (!p_no_cache) {
			_remove_from_loading_map(local_path);
		}
		return RES();
	}
	if (!p_no_cache) {
		res->set_path(local_path);
	}

	if (xl_remapped) {
		res->set_as_translation_remapped(true);
	}

#ifdef TOOLS_ENABLED

	res->set_edited(false);
	if (timestamp_on_load) {
		uint64_t mt = FileAccess::get_modified_time(path);
		//printf("mt %s: %lli\n",remapped_path.utf8().get_data(),mt);
		res->set_last_modified_time(mt);
	}
#endif

	if (!p_no_cache) {
		_remove_from_loading_map(local_path);
	}

	if (_loaded_callback) {
		_loaded_callback(res, p_path);
	}

	return res;
}

bool ResourceLoader::exists(const String &p_path, const String &p_type_hint) {
	String local_path;
	if (p_path.is_rel_path()) {
		local_path = "res://" + p_path;
	} else {
		local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	}

	if (ResourceCache::has(local_path)) {
		return true; // If cached, it probably exists
	}

	bool xl_remapped = false;
	String path = _path_remap(local_path, &xl_remapped);

	// Try all loaders and pick the first match for the type hint
	for (int i = 0; i < loader_count; i++) {
		if (!loader[i]->recognize_path(path, p_type_hint)) {
			continue;
		}

		if (loader[i]->exists(path)) {
			return true;
		}
	}

	return false;
}

Ref<ResourceInteractiveLoader> ResourceLoader::load_interactive(const String &p_path, const String &p_type_hint, bool p_no_cache, Error *r_error) {
	if (r_error) {
		*r_error = ERR_CANT_OPEN;
	}

	String local_path;
	if (p_path.is_rel_path()) {
		local_path = "res://" + p_path;
	} else {
		local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	}

	if (!p_no_cache) {
		bool success = _add_to_loading_map(local_path);
		ERR_FAIL_COND_V_MSG(!success, RES(), "Resource: '" + local_path + "' is already being loaded. Cyclic reference?");

		if (ResourceCache::has(local_path)) {
			print_verbose("Loading resource: " + local_path + " (cached)");
			Ref<Resource> res_cached = ResourceCache::get(local_path);
			Ref<ResourceInteractiveLoaderDefault> ril = Ref<ResourceInteractiveLoaderDefault>(memnew(ResourceInteractiveLoaderDefault));

			ril->resource = res_cached;
			ril->path_loading = local_path;
			ril->path_loading_thread = Thread::get_caller_id();
			return ril;
		}
	}

	bool xl_remapped = false;
	String path = _path_remap(local_path, &xl_remapped);
	if (path == "") {
		if (!p_no_cache) {
			_remove_from_loading_map(local_path);
		}
		ERR_FAIL_V_MSG(RES(), "Remapping '" + local_path + "' failed.");
	}

	print_verbose("Loading resource: " + path);

	bool found = false;
	for (int i = 0; i < loader_count; i++) {
		if (!loader[i]->recognize_path(path, p_type_hint)) {
			continue;
		}
		found = true;
		Ref<ResourceInteractiveLoader> ril = loader[i]->load_interactive(path, local_path, r_error, p_no_cache);
		if (ril.is_null()) {
			continue;
		}
		if (!p_no_cache) {
			ril->set_local_path(local_path);
			ril->path_loading = local_path;
			ril->path_loading_thread = Thread::get_caller_id();
		}

		if (xl_remapped) {
			ril->set_translation_remapped(true);
		}

		return ril;
	}

	if (!p_no_cache) {
		_remove_from_loading_map(local_path);
	}

	ERR_FAIL_COND_V_MSG(found, Ref<ResourceInteractiveLoader>(), "Failed loading resource: " + path + ".");

	ERR_FAIL_V_MSG(Ref<ResourceInteractiveLoader>(), "No loader found for resource: " + path + ".");
}

void ResourceLoader::add_resource_format_loader(Ref<ResourceFormatLoader> p_format_loader, bool p_at_front) {
	ERR_FAIL_COND(p_format_loader.is_null());
	ERR_FAIL_COND(loader_count >= MAX_LOADERS);

	if (p_at_front) {
		for (int i = loader_count; i > 0; i--) {
			loader[i] = loader[i - 1];
		}
		loader[0] = p_format_loader;
		loader_count++;
	} else {
		loader[loader_count++] = p_format_loader;
	}
}

void ResourceLoader::remove_resource_format_loader(Ref<ResourceFormatLoader> p_format_loader) {
	ERR_FAIL_COND(p_format_loader.is_null());

	// Find loader
	int i = 0;
	for (; i < loader_count; ++i) {
		if (loader[i] == p_format_loader) {
			break;
		}
	}

	ERR_FAIL_COND(i >= loader_count); // Not found

	// Shift next loaders up
	for (; i < loader_count - 1; ++i) {
		loader[i] = loader[i + 1];
	}
	loader[loader_count - 1].unref();
	--loader_count;
}

int ResourceLoader::get_import_order(const String &p_path) {
	String path = _path_remap(p_path);

	String local_path;
	if (path.is_rel_path()) {
		local_path = "res://" + path;
	} else {
		local_path = ProjectSettings::get_singleton()->localize_path(path);
	}

	for (int i = 0; i < loader_count; i++) {
		if (!loader[i]->recognize_path(local_path)) {
			continue;
		}
		/*
		if (p_type_hint!="" && !loader[i]->handles_type(p_type_hint))
			continue;
		*/

		return loader[i]->get_import_order(p_path);
	}

	return 0;
}

String ResourceLoader::get_import_group_file(const String &p_path) {
	String path = _path_remap(p_path);

	String local_path;
	if (path.is_rel_path()) {
		local_path = "res://" + path;
	} else {
		local_path = ProjectSettings::get_singleton()->localize_path(path);
	}

	for (int i = 0; i < loader_count; i++) {
		if (!loader[i]->recognize_path(local_path)) {
			continue;
		}
		/*
		if (p_type_hint!="" && !loader[i]->handles_type(p_type_hint))
			continue;
		*/

		return loader[i]->get_import_group_file(p_path);
	}

	return String(); //not found
}

bool ResourceLoader::is_import_valid(const String &p_path) {
	String path = _path_remap(p_path);

	String local_path;
	if (path.is_rel_path()) {
		local_path = "res://" + path;
	} else {
		local_path = ProjectSettings::get_singleton()->localize_path(path);
	}

	for (int i = 0; i < loader_count; i++) {
		if (!loader[i]->recognize_path(local_path)) {
			continue;
		}
		/*
		if (p_type_hint!="" && !loader[i]->handles_type(p_type_hint))
			continue;
		*/

		return loader[i]->is_import_valid(p_path);
	}

	return false; //not found
}

bool ResourceLoader::is_imported(const String &p_path) {
	String path = _path_remap(p_path);

	String local_path;
	if (path.is_rel_path()) {
		local_path = "res://" + path;
	} else {
		local_path = ProjectSettings::get_singleton()->localize_path(path);
	}

	for (int i = 0; i < loader_count; i++) {
		if (!loader[i]->recognize_path(local_path)) {
			continue;
		}
		/*
		if (p_type_hint!="" && !loader[i]->handles_type(p_type_hint))
			continue;
		*/

		return loader[i]->is_imported(p_path);
	}

	return false; //not found
}

void ResourceLoader::get_dependencies(const String &p_path, List<String> *p_dependencies, bool p_add_types) {
	String path = _path_remap(p_path);

	String local_path;
	if (path.is_rel_path()) {
		local_path = "res://" + path;
	} else {
		local_path = ProjectSettings::get_singleton()->localize_path(path);
	}

	for (int i = 0; i < loader_count; i++) {
		if (!loader[i]->recognize_path(local_path)) {
			continue;
		}
		/*
		if (p_type_hint!="" && !loader[i]->handles_type(p_type_hint))
			continue;
		*/

		loader[i]->get_dependencies(local_path, p_dependencies, p_add_types);
	}
}

Error ResourceLoader::rename_dependencies(const String &p_path, const Map<String, String> &p_map) {
	String path = _path_remap(p_path);

	String local_path;
	if (path.is_rel_path()) {
		local_path = "res://" + path;
	} else {
		local_path = ProjectSettings::get_singleton()->localize_path(path);
	}

	for (int i = 0; i < loader_count; i++) {
		if (!loader[i]->recognize_path(local_path)) {
			continue;
		}
		/*
		if (p_type_hint!="" && !loader[i]->handles_type(p_type_hint))
			continue;
		*/

		return loader[i]->rename_dependencies(local_path, p_map);
	}

	return OK; // ??
}

String ResourceLoader::get_resource_type(const String &p_path) {
	String local_path;
	if (p_path.is_rel_path()) {
		local_path = "res://" + p_path;
	} else {
		local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	}

	for (int i = 0; i < loader_count; i++) {
		String result = loader[i]->get_resource_type(local_path);
		if (result != "") {
			return result;
		}
	}

	return "";
}

String ResourceLoader::_path_remap(const String &p_path, bool *r_translation_remapped) {
	String new_path = p_path;

	if (translation_remaps.has(p_path)) {
		// translation_remaps has the following format:
		//   { "res://path.png": PoolStringArray( "res://path-ru.png:ru", "res://path-de.png:de" ) }

		// To find the path of the remapped resource, we extract the locale name after
		// the last ':' to match the project locale.

		// An extra remap may still be necessary afterwards due to the text -> binary converter on export.

		String locale = TranslationServer::get_singleton()->get_locale();
		ERR_FAIL_COND_V_MSG(locale.length() < 2, p_path, "Could not remap path '" + p_path + "' for translation as configured locale '" + locale + "' is invalid.");

		Vector<String> &res_remaps = *translation_remaps.getptr(new_path);

		int best_score = 0;
		for (int i = 0; i < res_remaps.size(); i++) {
			int split = res_remaps[i].rfind(":");
			if (split == -1) {
				continue;
			}
			String l = res_remaps[i].right(split + 1).strip_edges();
			int score = TranslationServer::get_singleton()->compare_locales(locale, l);
			if (score > 0 && score >= best_score) {
				new_path = res_remaps[i].left(split);
				best_score = score;
				if (score == 10) {
					break; // Exact match, skip the rest.
				}
			}
		}

		if (r_translation_remapped) {
			*r_translation_remapped = true;
		}
	}

	if (path_remaps.has(new_path)) {
		new_path = path_remaps[new_path];
	} else {
		// Try file remap.
		Error err;
		FileAccess *f = FileAccess::open(new_path + ".remap", FileAccess::READ, &err);

		if (f) {
			VariantParser::StreamFile stream;
			stream.f = f;

			String assign;
			Variant value;
			VariantParser::Tag next_tag;

			int lines = 0;
			String error_text;
			while (true) {
				assign = Variant();
				next_tag.fields.clear();
				next_tag.name = String();

				err = VariantParser::parse_tag_assign_eof(&stream, lines, error_text, next_tag, assign, value, nullptr, true);
				if (err == ERR_FILE_EOF) {
					break;
				} else if (err != OK) {
					ERR_PRINT("Parse error: " + p_path + ".remap:" + itos(lines) + " error: " + error_text + ".");
					break;
				}

				if (assign == "path") {
					new_path = value;
					break;
				} else if (next_tag.name != "remap") {
					break;
				}
			}

			memdelete(f);
		}
	}

	return new_path;
}

String ResourceLoader::import_remap(const String &p_path) {
	if (ResourceFormatImporter::get_singleton()->recognize_path(p_path)) {
		return ResourceFormatImporter::get_singleton()->get_internal_resource_path(p_path);
	}

	return p_path;
}

String ResourceLoader::path_remap(const String &p_path) {
	return _path_remap(p_path);
}

void ResourceLoader::reload_translation_remaps() {
	ResourceCache::lock.read_lock();

	List<Resource *> to_reload;
	SelfList<Resource> *E = remapped_list.first();

	while (E) {
		to_reload.push_back(E->self());
		E = E->next();
	}

	ResourceCache::lock.read_unlock();

	//now just make sure to not delete any of these resources while changing locale..
	while (to_reload.front()) {
		to_reload.front()->get()->reload_from_file();
		to_reload.pop_front();
	}
}

void ResourceLoader::load_translation_remaps() {
	if (!ProjectSettings::get_singleton()->has_setting("locale/translation_remaps")) {
		return;
	}

	Dictionary remaps = ProjectSettings::get_singleton()->get("locale/translation_remaps");
	List<Variant> keys;
	remaps.get_key_list(&keys);
	for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {
		Array langs = remaps[E->get()];
		Vector<String> lang_remaps;
		lang_remaps.resize(langs.size());
		for (int i = 0; i < langs.size(); i++) {
			lang_remaps.write[i] = langs[i];
		}

		translation_remaps[String(E->get())] = lang_remaps;
	}
}

void ResourceLoader::clear_translation_remaps() {
	translation_remaps.clear();
	while (remapped_list.first() != nullptr) {
		remapped_list.remove(remapped_list.first());
	}
}

void ResourceLoader::load_path_remaps() {
	if (!ProjectSettings::get_singleton()->has_setting("path_remap/remapped_paths")) {
		return;
	}

	PoolVector<String> remaps = ProjectSettings::get_singleton()->get("path_remap/remapped_paths");
	int rc = remaps.size();
	ERR_FAIL_COND(rc & 1); //must be even
	PoolVector<String>::Read r = remaps.read();

	for (int i = 0; i < rc; i += 2) {
		path_remaps[r[i]] = r[i + 1];
	}
}

void ResourceLoader::clear_path_remaps() {
	path_remaps.clear();
}

void ResourceLoader::set_load_callback(ResourceLoadedCallback p_callback) {
	_loaded_callback = p_callback;
}

ResourceLoadedCallback ResourceLoader::_loaded_callback = nullptr;

Ref<ResourceFormatLoader> ResourceLoader::_find_custom_resource_format_loader(String path) {
	for (int i = 0; i < loader_count; ++i) {
		if (loader[i]->get_script_instance() && loader[i]->get_script_instance()->get_script()->get_path() == path) {
			return loader[i];
		}
	}
	return Ref<ResourceFormatLoader>();
}

bool ResourceLoader::add_custom_resource_format_loader(String script_path) {
	if (_find_custom_resource_format_loader(script_path).is_valid()) {
		return false;
	}

	Ref<Resource> res = ResourceLoader::load(script_path);
	ERR_FAIL_COND_V(res.is_null(), false);
	ERR_FAIL_COND_V(!res->is_class("Script"), false);

	Ref<Script> s = res;
	StringName ibt = s->get_instance_base_type();
	bool valid_type = ClassDB::is_parent_class(ibt, "ResourceFormatLoader");
	ERR_FAIL_COND_V_MSG(!valid_type, false, "Script does not inherit a CustomResourceLoader: " + script_path + ".");

	Object *obj = ClassDB::instance(ibt);

	ERR_FAIL_COND_V_MSG(obj == nullptr, false, "Cannot instance script as custom resource loader, expected 'ResourceFormatLoader' inheritance, got: " + String(ibt) + ".");

	Ref<ResourceFormatLoader> crl = Object::cast_to<ResourceFormatLoader>(obj);
	crl->set_script(s.get_ref_ptr());
	ResourceLoader::add_resource_format_loader(crl);

	return true;
}

void ResourceLoader::remove_custom_resource_format_loader(String script_path) {
	Ref<ResourceFormatLoader> custom_loader = _find_custom_resource_format_loader(script_path);
	if (custom_loader.is_valid()) {
		remove_resource_format_loader(custom_loader);
	}
}

void ResourceLoader::add_custom_loaders() {
	// Custom loaders registration exploits global class names

	String custom_loader_base_class = ResourceFormatLoader::get_class_static();

	List<StringName> global_classes;
	ScriptServer::get_global_class_list(&global_classes);

	for (List<StringName>::Element *E = global_classes.front(); E; E = E->next()) {
		StringName class_name = E->get();
		StringName base_class = ScriptServer::get_global_class_native_base(class_name);

		if (base_class == custom_loader_base_class) {
			String path = ScriptServer::get_global_class_path(class_name);
			add_custom_resource_format_loader(path);
		}
	}
}

void ResourceLoader::remove_custom_loaders() {
	Vector<Ref<ResourceFormatLoader>> custom_loaders;
	for (int i = 0; i < loader_count; ++i) {
		if (loader[i]->get_script_instance()) {
			custom_loaders.push_back(loader[i]);
		}
	}

	for (int i = 0; i < custom_loaders.size(); ++i) {
		remove_resource_format_loader(custom_loaders[i]);
	}
}

Mutex ResourceLoader::loading_map_mutex;
HashMap<ResourceLoader::LoadingMapKey, int, ResourceLoader::LoadingMapKeyHasher> ResourceLoader::loading_map;

void ResourceLoader::finalize() {
#ifndef NO_THREADS
	const LoadingMapKey *K = nullptr;
	while ((K = loading_map.next(K))) {
		ERR_PRINT("Exited while resource is being loaded: " + K->path);
	}
	loading_map.clear();
#endif
}

ResourceLoadErrorNotify ResourceLoader::err_notify = nullptr;
void *ResourceLoader::err_notify_ud = nullptr;

DependencyErrorNotify ResourceLoader::dep_err_notify = nullptr;
void *ResourceLoader::dep_err_notify_ud = nullptr;

bool ResourceLoader::abort_on_missing_resource = true;
bool ResourceLoader::timestamp_on_load = false;

SelfList<Resource>::List ResourceLoader::remapped_list;
HashMap<String, Vector<String>> ResourceLoader::translation_remaps;
HashMap<String, String> ResourceLoader::path_remaps;

ResourceLoaderImport ResourceLoader::import = nullptr;

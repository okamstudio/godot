/**************************************************************************/
/*  resource_importer.cpp                                                 */
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

#include "resource_importer.h"

#include "core/config/project_settings.h"
#include "core/io/config_file.h"
#include "core/os/os.h"
#include "core/variant/variant_parser.h"

ResourceFormatImporterLoadOnStartup ResourceImporter::load_on_startup = nullptr;

bool ResourceFormatImporter::SortImporterByName::operator()(const Ref<ResourceImporter> &p_a, const Ref<ResourceImporter> &p_b) const {
	return p_a->get_importer_name() < p_b->get_importer_name();
}

Error ResourceFormatImporter::_get_path_and_type(const String &p_path, PathAndType &r_path_and_type, bool *r_valid) const {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path + ".import", FileAccess::READ, &err);

	if (f.is_null()) {
		if (r_valid) {
			*r_valid = false;
		}
		return err;
	}

	VariantParser::StreamFile stream;
	stream.f = f;

	String assign;
	Variant value;
	VariantParser::Tag next_tag;

	if (r_valid) {
		*r_valid = true;
	}

	int lines = 0;
	String error_text;
	bool path_found = false; //first match must have priority
	while (true) {
		assign = Variant();
		next_tag.fields.clear();
		next_tag.name = String();

		err = VariantParser::parse_tag_assign_eof(&stream, lines, error_text, next_tag, assign, value, nullptr, true);
		if (err == ERR_FILE_EOF) {
			return OK;
		} else if (err != OK) {
			ERR_PRINT(vformat("ResourceFormatImporter::load - %s.import:%d error: %s.", p_path, lines, error_text));
			return err;
		}

		if (!assign.is_empty()) {
			if (!path_found && assign.begins_with("path.") && r_path_and_type.path.is_empty()) {
				String feature = assign.get_slicec('.', 1);
				if (OS::get_singleton()->has_feature(feature)) {
					r_path_and_type.path = value;
					path_found = true; //first match must have priority
				}

			} else if (!path_found && assign == "path") {
				r_path_and_type.path = value;
				path_found = true; //first match must have priority
			} else if (assign == "type") {
				r_path_and_type.type = ClassDB::get_compatibility_remapped_class(value);
			} else if (assign == "importer") {
				r_path_and_type.importer = value;
			} else if (assign == "uid") {
				r_path_and_type.uid = ResourceUID::get_singleton()->text_to_id(value);
			} else if (assign == "group_file") {
				r_path_and_type.group_file = value;
			} else if (assign == "metadata") {
				r_path_and_type.metadata = value;
			} else if (assign == "valid") {
				if (r_valid) {
					*r_valid = value;
				}
			}

		} else if (next_tag.name != "remap") {
			break;
		}
	}

#ifdef TOOLS_ENABLED
	if (r_path_and_type.metadata && !r_path_and_type.path.is_empty()) {
		Dictionary meta = r_path_and_type.metadata;
		if (meta.has("has_editor_variant")) {
			r_path_and_type.path = r_path_and_type.path.get_basename() + ".editor." + r_path_and_type.path.get_extension();
		}
	}
#endif

	if (r_path_and_type.type.is_empty()) {
		return ERR_FILE_CORRUPT;
	}
	if (r_path_and_type.path.is_empty()) {
		// Some importers may not write files to the .godot folder, so the path can be empty.
		if (r_path_and_type.importer.is_empty()) {
			return ERR_FILE_CORRUPT;
		}

		// It's only invalid if the extension for the importer is not empty.
		Ref<ResourceImporter> importer = get_importer_by_name(r_path_and_type.importer);
		if (importer.is_null() || !importer->get_save_extension().is_empty()) {
			return ERR_FILE_CORRUPT;
		}
	}
	return OK;
}

Ref<Resource> ResourceFormatImporter::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
#ifdef TOOLS_ENABLED
	// When loading a resource on startup, we use the load_on_startup callback,
	// which executes the loading in the EditorFileSystem. It can reimport
	// the resource and retry the load, allowing the resource to be loaded
	// even if it is not yet imported.
	if (ResourceImporter::load_on_startup != nullptr) {
		return ResourceImporter::load_on_startup(this, p_path, r_error, p_use_sub_threads, r_progress, p_cache_mode);
	}
#endif

	return load_internal(p_path, r_error, p_use_sub_threads, r_progress, p_cache_mode, false);
}

Ref<Resource> ResourceFormatImporter::load_internal(const String &p_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode, bool p_silence_errors) {
	PathAndType pat;
	Error err = _get_path_and_type(p_path, pat);

	if (err != OK) {
		if (r_error) {
			*r_error = err;
		}

		return Ref<Resource>();
	}

	if (p_silence_errors) {
		// Note: Some importers do not create files in the .godot folder, so we need to check if the path is empty.
		if (!pat.path.is_empty() && !FileAccess::exists(pat.path)) {
			return Ref<Resource>();
		}
	}

	Ref<Resource> res = ResourceLoader::_load(pat.path, p_path, pat.type, p_cache_mode, r_error, p_use_sub_threads, r_progress);

#ifdef TOOLS_ENABLED
	if (res.is_valid()) {
		res->set_import_last_modified_time(res->get_last_modified_time()); //pass this, if used
		res->set_import_path(pat.path);
	}
#endif

	return res;
}

void ResourceFormatImporter::get_recognized_extensions(List<String> *p_extensions) const {
	HashSet<String> found;

	for (int i = 0; i < importers.size(); i++) {
		List<String> local_exts;
		importers[i]->get_recognized_extensions(&local_exts);
		for (const String &F : local_exts) {
			if (!found.has(F)) {
				p_extensions->push_back(F);
				found.insert(F);
			}
		}
	}
}

void ResourceFormatImporter::get_recognized_extensions_for_type(const String &p_type, List<String> *p_extensions) const {
	if (p_type.is_empty()) {
		get_recognized_extensions(p_extensions);
		return;
	}

	HashSet<String> found;

	for (int i = 0; i < importers.size(); i++) {
		String res_type = importers[i]->get_resource_type();
		if (res_type.is_empty()) {
			continue;
		}

		if (!ClassDB::is_parent_class(res_type, p_type)) {
			continue;
		}

		List<String> local_exts;
		importers[i]->get_recognized_extensions(&local_exts);
		for (const String &F : local_exts) {
			if (!found.has(F)) {
				p_extensions->push_back(F);
				found.insert(F);
			}
		}
	}
}

bool ResourceFormatImporter::exists(const String &p_path) const {
	return FileAccess::exists(p_path + ".import");
}

bool ResourceFormatImporter::recognize_path(const String &p_path, const String &p_for_type) const {
	return FileAccess::exists(p_path + ".import");
}

Error ResourceFormatImporter::get_import_order_threads_and_importer(const String &p_path, int &r_order, bool &r_can_threads, String &r_importer) const {
	r_order = 0;
	r_importer = "";

	r_can_threads = false;
	Ref<ResourceImporter> importer;

	if (FileAccess::exists(p_path + ".import")) {
		PathAndType pat;
		Error err = _get_path_and_type(p_path, pat);

		if (err == OK) {
			importer = get_importer_by_name(pat.importer);
		}
	} else {
		importer = get_importer_by_file(p_path);
	}

	if (importer.is_valid()) {
		r_order = importer->get_import_order();
		r_importer = importer->get_importer_name();
		r_can_threads = importer->can_import_threaded();
		return OK;
	} else {
		return ERR_INVALID_PARAMETER;
	}
}

int ResourceFormatImporter::get_import_order(const String &p_path) const {
	Ref<ResourceImporter> importer;

	if (FileAccess::exists(p_path + ".import")) {
		PathAndType pat;
		Error err = _get_path_and_type(p_path, pat);

		if (err == OK) {
			importer = get_importer_by_name(pat.importer);
		}
	} else {
		importer = get_importer_by_file(p_path);
	}

	if (importer.is_valid()) {
		return importer->get_import_order();
	}

	return 0;
}

bool ResourceFormatImporter::handles_type(const String &p_type) const {
	for (int i = 0; i < importers.size(); i++) {
		String res_type = importers[i]->get_resource_type();
		if (res_type.is_empty()) {
			continue;
		}
		if (ClassDB::is_parent_class(res_type, p_type)) {
			return true;
		}
	}

	return true;
}

String ResourceFormatImporter::get_internal_resource_path(const String &p_path) const {
	PathAndType pat;
	Error err = _get_path_and_type(p_path, pat);

	if (err != OK) {
		return String();
	}

	return pat.path;
}

void ResourceFormatImporter::get_internal_resource_path_list(const String &p_path, List<String> *r_paths) {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path + ".import", FileAccess::READ, &err);

	if (f.is_null()) {
		return;
	}

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
			return;
		} else if (err != OK) {
			ERR_PRINT(vformat("ResourceFormatImporter::get_internal_resource_path_list - %s.import:%d error: %s.", p_path, lines, error_text));
			return;
		}

		if (!assign.is_empty()) {
			if (assign.begins_with("path.")) {
				r_paths->push_back(value);
			} else if (assign == "path") {
				r_paths->push_back(value);
			}
		} else if (next_tag.name != "remap") {
			break;
		}
	}
}

String ResourceFormatImporter::get_import_group_file(const String &p_path) const {
	bool valid = true;
	PathAndType pat;
	_get_path_and_type(p_path, pat, &valid);
	return valid ? pat.group_file : String();
}

bool ResourceFormatImporter::is_import_valid(const String &p_path) const {
	bool valid = true;
	PathAndType pat;
	_get_path_and_type(p_path, pat, &valid);
	return valid;
}

String ResourceFormatImporter::get_resource_type(const String &p_path) const {
	PathAndType pat;
	Error err = _get_path_and_type(p_path, pat);

	if (err != OK) {
		return "";
	}

	return pat.type;
}

ResourceUID::ID ResourceFormatImporter::get_resource_uid(const String &p_path) const {
	PathAndType pat;
	Error err = _get_path_and_type(p_path, pat);

	if (err != OK) {
		return ResourceUID::INVALID_ID;
	}

	return pat.uid;
}

bool ResourceFormatImporter::has_custom_uid_support() const {
	return true;
}

Error ResourceFormatImporter::get_resource_import_info(const String &p_path, StringName &r_type, ResourceUID::ID &r_uid, String &r_import_group_file) const {
	PathAndType pat;
	Error err = _get_path_and_type(p_path, pat);

	if (err == OK) {
		r_type = pat.type;
		r_uid = pat.uid;
		r_import_group_file = pat.group_file;
	} else {
		r_type = "";
		r_uid = ResourceUID::INVALID_ID;
		r_import_group_file = "";
	}

	return err;
}

Variant ResourceFormatImporter::get_resource_metadata(const String &p_path) const {
	PathAndType pat;
	Error err = _get_path_and_type(p_path, pat);

	if (err != OK) {
		return Variant();
	}

	return pat.metadata;
}
void ResourceFormatImporter::get_classes_used(const String &p_path, HashSet<StringName> *r_classes) {
	PathAndType pat;
	Error err = _get_path_and_type(p_path, pat);

	if (err != OK) {
		return;
	}

	ResourceLoader::get_classes_used(pat.path, r_classes);
}

void ResourceFormatImporter::get_dependencies(const String &p_path, List<String> *p_dependencies, bool p_add_types) {
	PathAndType pat;
	Error err = _get_path_and_type(p_path, pat);

	if (err != OK) {
		return;
	}

	ResourceLoader::get_dependencies(pat.path, p_dependencies, p_add_types);
}

Ref<ResourceImporter> ResourceFormatImporter::get_importer_by_name(const String &p_name) const {
	for (int i = 0; i < importers.size(); i++) {
		if (importers[i]->get_importer_name() == p_name) {
			return importers[i];
		}
	}

	return Ref<ResourceImporter>();
}

void ResourceFormatImporter::add_importer(const Ref<ResourceImporter> &p_importer, bool p_first_priority) {
	ERR_FAIL_COND(p_importer.is_null());
	if (p_first_priority) {
		importers.insert(0, p_importer);
	} else {
		importers.push_back(p_importer);
	}
}

void ResourceFormatImporter::get_importers_for_file(const String &p_file, List<Ref<ResourceImporter>> *r_importers) {
	for (int i = 0; i < importers.size(); i++) {
		List<String> local_exts;
		importers[i]->get_recognized_extensions(&local_exts);
		for (const String &F : local_exts) {
			if (p_file.right(F.length()).nocasecmp_to(F) == 0) {
				r_importers->push_back(importers[i]);
				break;
			}
		}
	}
}

void ResourceFormatImporter::get_importers(List<Ref<ResourceImporter>> *r_importers) {
	for (int i = 0; i < importers.size(); i++) {
		r_importers->push_back(importers[i]);
	}
}

Ref<ResourceImporter> ResourceFormatImporter::get_importer_by_file(const String &p_file) const {
	Ref<ResourceImporter> importer;
	float priority = 0;

	for (int i = 0; i < importers.size(); i++) {
		List<String> local_exts;
		importers[i]->get_recognized_extensions(&local_exts);
		for (const String &F : local_exts) {
			if (p_file.right(F.length()).nocasecmp_to(F) == 0 && importers[i]->get_priority() > priority) {
				importer = importers[i];
				priority = importers[i]->get_priority();
				break;
			}
		}
	}

	return importer;
}

String ResourceFormatImporter::get_import_base_path(const String &p_for_file) const {
	return ProjectSettings::get_singleton()->get_imported_files_path().path_join(p_for_file.get_file() + "-" + p_for_file.md5_text());
}

bool ResourceFormatImporter::are_import_settings_valid(const String &p_path) const {
	bool valid = true;
	PathAndType pat;
	_get_path_and_type(p_path, pat, &valid);

	if (!valid) {
		return false;
	}

	for (int i = 0; i < importers.size(); i++) {
		if (importers[i]->get_importer_name() == pat.importer) {
			if (!importers[i]->are_import_settings_valid(p_path, pat.metadata)) { //importer thinks this is not valid
				return false;
			}
		}
	}

	return true;
}

String ResourceFormatImporter::get_import_settings_hash() const {
	Vector<Ref<ResourceImporter>> sorted_importers = importers;

	sorted_importers.sort_custom<SortImporterByName>();

	String hash;
	for (int i = 0; i < sorted_importers.size(); i++) {
		hash += ":" + sorted_importers[i]->get_importer_name() + ":" + sorted_importers[i]->get_import_settings_string();
	}
	return hash.md5_text();
}

ResourceFormatImporter *ResourceFormatImporter::singleton = nullptr;

ResourceFormatImporter::ResourceFormatImporter() {
	singleton = this;
}

//////////////

void ResourceImporter::_bind_methods() {
	BIND_ENUM_CONSTANT(IMPORT_ORDER_DEFAULT);
	BIND_ENUM_CONSTANT(IMPORT_ORDER_SCENE);
}

/////

Error ResourceFormatImporterSaver::set_uid(const String &p_path, ResourceUID::ID p_uid) {
	Ref<ConfigFile> cf;
	cf.instantiate();
	Error err = cf->load(p_path + ".import");
	if (err != OK) {
		return err;
	}
	cf->set_value("remap", "uid", ResourceUID::get_singleton()->id_to_text(p_uid));
	cf->save(p_path + ".import");

	return OK;
}

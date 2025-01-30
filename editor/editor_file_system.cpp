/**************************************************************************/
/*  editor_file_system.cpp                                                */
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

#include "editor_file_system.h"

#include "core/config/project_settings.h"
#include "core/extension/gdextension_manager.h"
#include "core/io/file_access.h"
#include "core/io/resource_importer.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/worker_thread_pool.h"
#include "core/os/os.h"
#include "core/variant/variant_parser.h"
#include "editor/editor_help.h"
#include "editor/editor_node.h"
#include "editor/editor_paths.h"
#include "editor/editor_resource_preview.h"
#include "editor/editor_settings.h"
#include "editor/project_settings_editor.h"
#include "scene/resources/packed_scene.h"

EditorFileSystem *EditorFileSystem::singleton = nullptr;
//the name is the version, to keep compatibility with different versions of Godot
#define CACHE_FILE_NAME "filesystem_cache9"

int EditorFileSystemDirectory::find_file_index(const String &p_file) const {
	for (int i = 0; i < files.size(); i++) {
		if (files[i]->file == p_file) {
			return i;
		}
	}
	return -1;
}

int EditorFileSystemDirectory::find_dir_index(const String &p_dir) const {
	for (int i = 0; i < subdirs.size(); i++) {
		if (subdirs[i]->name == p_dir) {
			return i;
		}
	}

	return -1;
}

void EditorFileSystemDirectory::force_update() {
	// We set modified_time to 0 to force `EditorFileSystem::_scan_fs_changes` to search changes in the directory
	modified_time = 0;
}

int EditorFileSystemDirectory::get_subdir_count() const {
	return subdirs.size();
}

EditorFileSystemDirectory *EditorFileSystemDirectory::get_subdir(int p_idx) {
	ERR_FAIL_INDEX_V(p_idx, subdirs.size(), nullptr);
	return subdirs[p_idx];
}

int EditorFileSystemDirectory::get_file_count() const {
	return files.size();
}

String EditorFileSystemDirectory::get_file(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, files.size(), "");

	return files[p_idx]->file;
}

String EditorFileSystemDirectory::get_path() const {
	String p;
	const EditorFileSystemDirectory *d = this;
	while (d->parent) {
		p = d->name.path_join(p);
		d = d->parent;
	}

	return "res://" + p;
}

String EditorFileSystemDirectory::get_file_path(int p_idx) const {
	String file = get_file(p_idx);
	const EditorFileSystemDirectory *d = this;
	while (d->parent) {
		file = d->name.path_join(file);
		d = d->parent;
	}

	return "res://" + file;
}

Vector<String> EditorFileSystemDirectory::get_file_deps(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, files.size(), Vector<String>());
	Vector<String> deps;

	for (int i = 0; i < files[p_idx]->deps.size(); i++) {
		String dep = files[p_idx]->deps[i];
		int sep_idx = dep.find("::"); //may contain type information, unwanted
		if (sep_idx != -1) {
			dep = dep.substr(0, sep_idx);
		}
		ResourceUID::ID uid = ResourceUID::get_singleton()->text_to_id(dep);
		if (uid != ResourceUID::INVALID_ID) {
			//return proper dependency resource from uid
			if (ResourceUID::get_singleton()->has_id(uid)) {
				dep = ResourceUID::get_singleton()->get_id_path(uid);
			} else {
				continue;
			}
		}
		deps.push_back(dep);
	}
	return deps;
}

bool EditorFileSystemDirectory::get_file_import_is_valid(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, files.size(), false);
	return files[p_idx]->import_valid;
}

uint64_t EditorFileSystemDirectory::get_file_modified_time(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, files.size(), 0);
	return files[p_idx]->modified_time;
}

uint64_t EditorFileSystemDirectory::get_file_import_modified_time(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, files.size(), 0);
	return files[p_idx]->import_modified_time;
}

String EditorFileSystemDirectory::get_file_script_class_name(int p_idx) const {
	return files[p_idx]->class_info.name;
}

String EditorFileSystemDirectory::get_file_script_class_extends(int p_idx) const {
	return files[p_idx]->class_info.extends;
}

String EditorFileSystemDirectory::get_file_script_class_icon_path(int p_idx) const {
	return files[p_idx]->class_info.icon_path;
}

String EditorFileSystemDirectory::get_file_icon_path(int p_idx) const {
	return files[p_idx]->class_info.icon_path;
}

StringName EditorFileSystemDirectory::get_file_type(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, files.size(), "");
	return files[p_idx]->type;
}

StringName EditorFileSystemDirectory::get_file_resource_script_class(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, files.size(), "");
	return files[p_idx]->resource_script_class;
}

String EditorFileSystemDirectory::get_name() {
	return name;
}

EditorFileSystemDirectory *EditorFileSystemDirectory::get_parent() {
	return parent;
}

void EditorFileSystemDirectory::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_subdir_count"), &EditorFileSystemDirectory::get_subdir_count);
	ClassDB::bind_method(D_METHOD("get_subdir", "idx"), &EditorFileSystemDirectory::get_subdir);
	ClassDB::bind_method(D_METHOD("get_file_count"), &EditorFileSystemDirectory::get_file_count);
	ClassDB::bind_method(D_METHOD("get_file", "idx"), &EditorFileSystemDirectory::get_file);
	ClassDB::bind_method(D_METHOD("get_file_path", "idx"), &EditorFileSystemDirectory::get_file_path);
	ClassDB::bind_method(D_METHOD("get_file_type", "idx"), &EditorFileSystemDirectory::get_file_type);
	ClassDB::bind_method(D_METHOD("get_file_script_class_name", "idx"), &EditorFileSystemDirectory::get_file_script_class_name);
	ClassDB::bind_method(D_METHOD("get_file_script_class_extends", "idx"), &EditorFileSystemDirectory::get_file_script_class_extends);
	ClassDB::bind_method(D_METHOD("get_file_import_is_valid", "idx"), &EditorFileSystemDirectory::get_file_import_is_valid);
	ClassDB::bind_method(D_METHOD("get_name"), &EditorFileSystemDirectory::get_name);
	ClassDB::bind_method(D_METHOD("get_path"), &EditorFileSystemDirectory::get_path);
	ClassDB::bind_method(D_METHOD("get_parent"), &EditorFileSystemDirectory::get_parent);
	ClassDB::bind_method(D_METHOD("find_file_index", "name"), &EditorFileSystemDirectory::find_file_index);
	ClassDB::bind_method(D_METHOD("find_dir_index", "name"), &EditorFileSystemDirectory::find_dir_index);
}

EditorFileSystemDirectory::EditorFileSystemDirectory() {
	modified_time = 0;
	parent = nullptr;
}

EditorFileSystemDirectory::~EditorFileSystemDirectory() {
	for (EditorFileSystemDirectory::FileInfo *fi : files) {
		memdelete(fi);
	}

	for (EditorFileSystemDirectory *dir : subdirs) {
		memdelete(dir);
	}
}

EditorFileSystem::ScannedDirectory::~ScannedDirectory() {
	for (ScannedDirectory *dir : subdirs) {
		memdelete(dir);
	}
}

void EditorFileSystem::_first_scan_filesystem() {
	Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	first_scan_root_dir = memnew(ScannedDirectory);
	first_scan_root_dir->full_path = "res://";
	HashSet<String> existing_class_names;

	nb_files_total = _scan_new_dir(first_scan_root_dir, d);

	// This loads the global class names from the scripts and ensures that even if the
	// global_script_class_cache.cfg was missing or invalid, the global class names are valid in ScriptServer.
	_first_scan_process_scripts(first_scan_root_dir, existing_class_names);

	// Removing invalid global class to prevent having invalid paths in ScriptServer.
	_remove_invalid_global_class_names(existing_class_names);

	// Now that all the global class names should be loaded, create autoloads and plugins.
	// This is done after loading the global class names because autoloads and plugins can use
	// global class names.
	ProjectSettingsEditor::get_singleton()->init_autoloads();
	EditorNode::get_singleton()->init_plugins();
}

void EditorFileSystem::_first_scan_process_scripts(const ScannedDirectory *p_scan_dir, HashSet<String> &p_existing_class_names) {
	for (ScannedDirectory *scan_sub_dir : p_scan_dir->subdirs) {
		_first_scan_process_scripts(scan_sub_dir, p_existing_class_names);
	}

	for (const String &scan_file : p_scan_dir->files) {
		String path = p_scan_dir->full_path.path_join(scan_file);
		StringName type = ResourceLoader::get_resource_type(path);

		if (ClassDB::is_parent_class(type, SNAME("Script"))) {
			const ScriptClassInfo &info = _get_global_script_class(type, path);
			ScriptClassInfoUpdate update(info);
			update.type = type;
			_register_global_class_script(path, path, update);

			if (!info.name.is_empty()) {
				p_existing_class_names.insert(info.name);
			}
		}
	}
}

void EditorFileSystem::_scan_filesystem() {
	// On the first scan, the first_scan_root_dir is created in _first_scan_filesystem.
	ERR_FAIL_COND(!scanning || new_filesystem || (first_scan && !first_scan_root_dir));

	//read .fscache
	String cpath;

	sources_changed.clear();
	file_cache.clear();

	String project = ProjectSettings::get_singleton()->get_resource_path();

	String fscache = EditorPaths::get_singleton()->get_project_settings_dir().path_join(CACHE_FILE_NAME);
	{
		Ref<FileAccess> f = FileAccess::open(fscache, FileAccess::READ);

		bool first = true;
		if (f.is_valid()) {
			//read the disk cache
			while (!f->eof_reached()) {
				String l = f->get_line().strip_edges();
				if (first) {
					if (first_scan) {
						// only use this on first scan, afterwards it gets ignored
						// this is so on first reimport we synchronize versions, then
						// we don't care until editor restart. This is for usability mainly so
						// your workflow is not killed after changing a setting by forceful reimporting
						// everything there is.
						filesystem_settings_version_for_import = l.strip_edges();
						if (filesystem_settings_version_for_import != ResourceFormatImporter::get_singleton()->get_import_settings_hash()) {
							revalidate_import_files = true;
						}
					}
					first = false;
					continue;
				}
				if (l.is_empty()) {
					continue;
				}

				if (l.begins_with("::")) {
					Vector<String> split = l.split("::");
					ERR_CONTINUE(split.size() != 3);
					const String &name = split[1];

					cpath = name;

				} else {
					// The last section (deps) may contain the same splitter, so limit the maxsplit to 8 to get the complete deps.
					Vector<String> split = l.split("::", true, 8);
					ERR_CONTINUE(split.size() < 9);
					String name = split[0];
					String file;

					file = name;
					name = cpath.path_join(name);

					FileCache fc;
					fc.type = split[1].get_slice("/", 0);
					fc.resource_script_class = split[1].get_slice("/", 1);
					fc.uid = split[2].to_int();
					fc.modification_time = split[3].to_int();
					fc.import_modification_time = split[4].to_int();
					fc.import_valid = split[5].to_int() != 0;
					fc.import_group_file = split[6].strip_edges();
					{
						const Vector<String> &slices = split[7].split("<>");
						ERR_CONTINUE(slices.size() < 4);
						fc.class_info.name = slices[0];
						fc.class_info.extends = slices[1];
						fc.class_info.icon_path = slices[2];
						fc.class_info.is_abstract = slices[3].to_int();
					}
					fc.deps = split[8].strip_edges().split("<>");

					file_cache[name] = fc;
				}
			}
		}
	}

	const String update_cache = EditorPaths::get_singleton()->get_project_settings_dir().path_join("filesystem_update4");
	if (first_scan && FileAccess::exists(update_cache)) {
		{
			Ref<FileAccess> f2 = FileAccess::open(update_cache, FileAccess::READ);
			String l = f2->get_line().strip_edges();
			while (!l.is_empty()) {
				dep_update_list.insert(l);
				file_cache.erase(l); // Erase cache for this, so it gets updated.
				l = f2->get_line().strip_edges();
			}
		}

		Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
		d->remove(update_cache); // Bye bye update cache.
	}

	EditorProgressBG scan_progress("efs", "ScanFS", 1000);
	ScanProgress sp;
	sp.hi = nb_files_total;
	sp.progress = &scan_progress;

	new_filesystem = memnew(EditorFileSystemDirectory);
	new_filesystem->parent = nullptr;

	ScannedDirectory *sd;
	// On the first scan, the first_scan_root_dir is created in _first_scan_filesystem.
	if (first_scan) {
		sd = first_scan_root_dir;
		// Will be updated on scan.
		ResourceUID::get_singleton()->clear();
	} else {
		Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
		sd = memnew(ScannedDirectory);
		sd->full_path = "res://";
		nb_files_total = _scan_new_dir(sd, d);
	}

	_process_file_system(sd, new_filesystem, sp);

	dep_update_list.clear();
	file_cache.clear(); //clear caches, no longer needed

	if (first_scan) {
		memdelete(first_scan_root_dir);
		first_scan_root_dir = nullptr;
	} else {
		//on the first scan this is done from the main thread after re-importing
		_save_filesystem_cache();
	}

	scanning = false;
}

void EditorFileSystem::_save_filesystem_cache() {
	group_file_cache.clear();

	String fscache = EditorPaths::get_singleton()->get_project_settings_dir().path_join(CACHE_FILE_NAME);

	Ref<FileAccess> f = FileAccess::open(fscache, FileAccess::WRITE);
	ERR_FAIL_COND_MSG(f.is_null(), "Cannot create file '" + fscache + "'. Check user write permissions.");

	f->store_line(filesystem_settings_version_for_import);
	_save_filesystem_cache(filesystem, f);
}

void EditorFileSystem::_thread_func(void *_userdata) {
	EditorFileSystem *sd = (EditorFileSystem *)_userdata;
	sd->_scan_filesystem();
}

bool EditorFileSystem::_test_for_reimport(const String &p_path, bool p_only_imported_files) {
	if (!reimport_on_missing_imported_files && p_only_imported_files) {
		return false;
	}

	if (!FileAccess::exists(p_path + ".import")) {
		return true;
	}

	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path + ".import", FileAccess::READ, &err);

	if (f.is_null()) { // No import file, reimport.
		return true;
	}

	VariantParser::StreamFile stream;
	stream.f = f;

	String assign;
	Variant value;
	VariantParser::Tag next_tag;

	int lines = 0;
	String error_text;

	List<String> to_check;

	String importer_name;
	String source_file = "";
	String source_md5 = "";
	Vector<String> dest_files;
	String dest_md5 = "";
	int version = 0;
	bool found_uid = false;

	while (true) {
		assign = Variant();
		next_tag.fields.clear();
		next_tag.name = String();

		err = VariantParser::parse_tag_assign_eof(&stream, lines, error_text, next_tag, assign, value, nullptr, true);
		if (err == ERR_FILE_EOF) {
			break;
		} else if (err != OK) {
			ERR_PRINT("ResourceFormatImporter::load - '" + p_path + ".import:" + itos(lines) + "' error '" + error_text + "'.");
			// Parse error, skip and let user attempt manual reimport to avoid reimport loop.
			return false;
		}

		if (!assign.is_empty()) {
			if (assign == "valid" && value.operator bool() == false) {
				// Invalid import (failed previous import), skip and let user attempt manual reimport to avoid reimport loop.
				return false;
			}
			if (assign.begins_with("path")) {
				to_check.push_back(value);
			} else if (assign == "files") {
				Array fa = value;
				for (int i = 0; i < fa.size(); i++) {
					to_check.push_back(fa[i]);
				}
			} else if (assign == "importer_version") {
				version = value;
			} else if (assign == "importer") {
				importer_name = value;
			} else if (assign == "uid") {
				found_uid = true;
			} else if (!p_only_imported_files) {
				if (assign == "source_file") {
					source_file = value;
				} else if (assign == "dest_files") {
					dest_files = value;
				}
			}

		} else if (next_tag.name != "remap" && next_tag.name != "deps") {
			break;
		}
	}

	if (!ResourceFormatImporter::get_singleton()->are_import_settings_valid(p_path)) {
		// Reimport settings are out of sync with project settings, reimport.
		return true;
	}

	if (importer_name == "keep" || importer_name == "skip") {
		return false; //keep mode, do not reimport
	}

	if (!found_uid) {
		return true; //UID not found, old format, reimport.
	}

	Ref<ResourceImporter> importer = ResourceFormatImporter::get_singleton()->get_importer_by_name(importer_name);

	if (importer.is_null()) {
		return true; // the importer has possibly changed, try to reimport.
	}

	if (importer->get_format_version() > version) {
		return true; // version changed, reimport
	}

	// Read the md5's from a separate file (so the import parameters aren't dependent on the file version
	String base_path = ResourceFormatImporter::get_singleton()->get_import_base_path(p_path);
	Ref<FileAccess> md5s = FileAccess::open(base_path + ".md5", FileAccess::READ, &err);
	if (md5s.is_null()) { // No md5's stored for this resource
		return true;
	}

	VariantParser::StreamFile md5_stream;
	md5_stream.f = md5s;

	while (true) {
		assign = Variant();
		next_tag.fields.clear();
		next_tag.name = String();

		err = VariantParser::parse_tag_assign_eof(&md5_stream, lines, error_text, next_tag, assign, value, nullptr, true);

		if (err == ERR_FILE_EOF) {
			break;
		} else if (err != OK) {
			ERR_PRINT("ResourceFormatImporter::load - '" + p_path + ".import.md5:" + itos(lines) + "' error '" + error_text + "'.");
			return false; // parse error
		}
		if (!assign.is_empty()) {
			if (!p_only_imported_files) {
				if (assign == "source_md5") {
					source_md5 = value;
				} else if (assign == "dest_md5") {
					dest_md5 = value;
				}
			}
		}
	}

	//imported files are gone, reimport
	for (const String &E : to_check) {
		if (!FileAccess::exists(E)) {
			return true;
		}
	}

	//check source md5 matching
	if (!p_only_imported_files) {
		if (!source_file.is_empty() && source_file != p_path) {
			return true; //file was moved, reimport
		}

		if (source_md5.is_empty()) {
			return true; //lacks md5, so just reimport
		}

		String md5 = FileAccess::get_md5(p_path);
		if (md5 != source_md5) {
			return true;
		}

		if (dest_files.size() && !dest_md5.is_empty()) {
			md5 = FileAccess::get_multiple_md5(dest_files);
			if (md5 != dest_md5) {
				return true;
			}
		}
	}

	return false; //nothing changed
}

bool EditorFileSystem::_scan_import_support(const Vector<String> &reimports) {
	if (import_support_queries.size() == 0) {
		return false;
	}
	HashMap<String, int> import_support_test;
	Vector<bool> import_support_tested;
	import_support_tested.resize(import_support_queries.size());
	for (int i = 0; i < import_support_queries.size(); i++) {
		import_support_tested.write[i] = false;
		if (import_support_queries[i]->is_active()) {
			Vector<String> extensions = import_support_queries[i]->get_file_extensions();
			for (int j = 0; j < extensions.size(); j++) {
				import_support_test.insert(extensions[j], i);
			}
		}
	}

	if (import_support_test.size() == 0) {
		return false; //well nothing to do
	}

	for (int i = 0; i < reimports.size(); i++) {
		HashMap<String, int>::Iterator E = import_support_test.find(reimports[i].get_extension().to_lower());
		if (E) {
			import_support_tested.write[E->value] = true;
		}
	}

	for (int i = 0; i < import_support_tested.size(); i++) {
		if (import_support_tested[i]) {
			if (import_support_queries.write[i]->query()) {
				return true;
			}
		}
	}

	return false;
}

bool EditorFileSystem::_update_scan_actions() {
	sources_changed.clear();

	// We need to update the script global class names before the reimports to be sure that
	// all the importer classes that depends on class names will work.
	_update_script_classes();

	bool fs_changed = false;

	Vector<String> reimports;
	Vector<String> reloads;

	for (const ItemAction &ia : scan_actions) {
		switch (ia.action) {
			case ItemAction::ACTION_NONE: {
			} break;
			case ItemAction::ACTION_DIR_ADD: {
				int idx = 0;
				for (int i = 0; i < ia.dir->subdirs.size(); i++) {
					if (ia.new_dir->name.filenocasecmp_to(ia.dir->subdirs[i]->name) < 0) {
						break;
					}
					idx++;
				}
				if (idx == ia.dir->subdirs.size()) {
					ia.dir->subdirs.push_back(ia.new_dir);
				} else {
					ia.dir->subdirs.insert(idx, ia.new_dir);
				}

				fs_changed = true;
			} break;
			case ItemAction::ACTION_DIR_REMOVE: {
				ERR_CONTINUE(!ia.dir->parent);
				ia.dir->parent->subdirs.erase(ia.dir);
				memdelete(ia.dir);
				fs_changed = true;
			} break;
			case ItemAction::ACTION_FILE_ADD: {
				int idx = 0;
				for (int i = 0; i < ia.dir->files.size(); i++) {
					if (ia.new_file->file.filenocasecmp_to(ia.dir->files[i]->file) < 0) {
						break;
					}
					idx++;
				}
				if (idx == ia.dir->files.size()) {
					ia.dir->files.push_back(ia.new_file);
				} else {
					ia.dir->files.insert(idx, ia.new_file);
				}

				fs_changed = true;

				if (ClassDB::is_parent_class(ia.new_file->type, SNAME("Script"))) {
					_queue_update_script_class(ia.dir->get_file_path(idx), ScriptClassInfoUpdate::from_file_info(ia.new_file));
				}
				if (ia.new_file->type == SNAME("PackedScene")) {
					_queue_update_scene_groups(ia.dir->get_file_path(idx));
				}

			} break;
			case ItemAction::ACTION_FILE_REMOVE: {
				int idx = ia.dir->find_file_index(ia.file);
				ERR_CONTINUE(idx == -1);

				String class_name = ia.dir->files[idx]->class_info.name;
				if (ClassDB::is_parent_class(ia.dir->files[idx]->type, SNAME("Script"))) {
					_queue_update_script_class(ia.dir->get_file_path(idx), ScriptClassInfoUpdate());
				}
				if (ia.dir->files[idx]->type == SNAME("PackedScene")) {
					_queue_update_scene_groups(ia.dir->get_file_path(idx));
				}

				_delete_internal_files(ia.dir->files[idx]->file);
				memdelete(ia.dir->files[idx]);
				ia.dir->files.remove_at(idx);

				// Restore another script with the same global class name if it exists.
				if (!class_name.is_empty()) {
					EditorFileSystemDirectory::FileInfo *old_fi = nullptr;
					String old_file = _get_file_by_class_name(filesystem, class_name, old_fi);
					if (!old_file.is_empty() && old_fi) {
						_queue_update_script_class(old_file, ScriptClassInfoUpdate::from_file_info(old_fi));
					}
				}

				fs_changed = true;

			} break;
			case ItemAction::ACTION_FILE_TEST_REIMPORT: {
				int idx = ia.dir->find_file_index(ia.file);
				ERR_CONTINUE(idx == -1);
				String full_path = ia.dir->get_file_path(idx);

				bool need_reimport = _test_for_reimport(full_path, false);
				// Workaround GH-94416 for the Android editor for now.
				// `import_mt` seems to always be 0 and force a reimport on any fs scan.
#ifndef ANDROID_ENABLED
				if (!need_reimport && FileAccess::exists(full_path + ".import")) {
					uint64_t import_mt = ia.dir->get_file_import_modified_time(idx);
					if (import_mt != FileAccess::get_modified_time(full_path + ".import")) {
						need_reimport = true;
					}
				}
#endif

				if (need_reimport) {
					//must reimport
					reimports.push_back(full_path);
					Vector<String> dependencies = _get_dependencies(full_path);
					for (const String &dep : dependencies) {
						const String &dependency_path = dep.contains("::") ? dep.get_slice("::", 0) : dep;
						if (import_extensions.has(dep.get_extension())) {
							reimports.push_back(dependency_path);
						}
					}
				} else {
					//must not reimport, all was good
					//update modified times, to avoid reimport
					ia.dir->files[idx]->modified_time = FileAccess::get_modified_time(full_path);
					ia.dir->files[idx]->import_modified_time = FileAccess::get_modified_time(full_path + ".import");
				}

				fs_changed = true;
			} break;
			case ItemAction::ACTION_FILE_RELOAD: {
				int idx = ia.dir->find_file_index(ia.file);
				ERR_CONTINUE(idx == -1);
				String full_path = ia.dir->get_file_path(idx);

				const EditorFileSystemDirectory::FileInfo *fi = ia.dir->files[idx];
				if (ClassDB::is_parent_class(fi->type, SNAME("Script"))) {
					_queue_update_script_class(full_path, ScriptClassInfoUpdate::from_file_info(fi));
				}
				if (fi->type == SNAME("PackedScene")) {
					_queue_update_scene_groups(full_path);
				}

				reloads.push_back(full_path);

			} break;
		}
	}

	if (_scan_extensions()) {
		//needs editor restart
		//extensions also may provide filetypes to be imported, so they must run before importing
		if (EditorNode::immediate_confirmation_dialog(TTR("Some extensions need the editor to restart to take effect."), first_scan ? TTR("Restart") : TTR("Save & Restart"), TTR("Continue"))) {
			if (!first_scan) {
				EditorNode::get_singleton()->save_all_scenes();
			}
			EditorNode::get_singleton()->restart_editor();
			//do not import
			return true;
		}
	}

	if (reimports.size()) {
		if (_scan_import_support(reimports)) {
			return true;
		}

		reimport_files(reimports);
	} else {
		//reimport files will update the uid cache file so if nothing was reimported, update it manually
		ResourceUID::get_singleton()->update_cache();
	}

	if (first_scan) {
		//only on first scan this is valid and updated, then settings changed.
		revalidate_import_files = false;
		filesystem_settings_version_for_import = ResourceFormatImporter::get_singleton()->get_import_settings_hash();
		_save_filesystem_cache();
	}

	// Moving the processing of pending updates before the resources_reload event to be sure all global class names
	// are updated. Script.cpp listens on resources_reload and reloads updated scripts.
	_process_update_pending();

	if (reloads.size()) {
		emit_signal(SNAME("resources_reload"), reloads);
	}
	scan_actions.clear();

	return fs_changed;
}

void EditorFileSystem::scan() {
	if (false /*&& bool(Globals::get_singleton()->get("debug/disable_scan"))*/) {
		return;
	}

	if (scanning || scanning_changes || thread.is_started()) {
		return;
	}

	// The first scan must be on the main thread because, after the first scan and update
	// of global class names, we load the plugins and autoloads. These need to
	// be added on the main thread because they are nodes, and we need to wait for them
	// to be loaded to continue the scan and reimportations.
	if (first_scan) {
		_first_scan_filesystem();
	}

	_update_extensions();

	if (!use_threads) {
		scanning = true;
		scan_total = 0;
		_scan_filesystem();
		if (filesystem) {
			memdelete(filesystem);
		}
		//file_type_cache.clear();
		filesystem = new_filesystem;
		new_filesystem = nullptr;
		_update_scan_actions();
		// Update all icons so they are loaded for the FileSystemDock.
		_update_files_icon_path();
		scanning = false;
		// Set first_scan to false before the signals so the function doing_first_scan can return false
		// in editor_node to start the export if needed.
		first_scan = false;
		emit_signal(SNAME("filesystem_changed"));
		emit_signal(SNAME("sources_changed"), sources_changed.size() > 0);
	} else {
		ERR_FAIL_COND(thread.is_started());
		set_process(true);
		Thread::Settings s;
		scanning = true;
		scan_total = 0;
		s.priority = Thread::PRIORITY_LOW;
		thread.start(_thread_func, this, s);
		//tree->hide();
		//progress->show();
	}
}

void EditorFileSystem::ScanProgress::increment() {
	current++;
	float ratio = current / MAX(hi, 1.0f);
	progress->step(ratio * 1000.0f);
	EditorFileSystem::singleton->scan_total = ratio;
}

int EditorFileSystem::_scan_new_dir(ScannedDirectory *p_dir, Ref<DirAccess> &da) {
	List<String> dirs;
	List<String> files;

	String cd = da->get_current_dir();

	da->list_dir_begin();
	while (true) {
		String f = da->get_next();
		if (f.is_empty()) {
			break;
		}

		if (da->current_is_hidden()) {
			continue;
		}

		if (da->current_is_dir()) {
			if (f.begins_with(".")) { // Ignore special and . / ..
				continue;
			}

			if (_should_skip_directory(cd.path_join(f))) {
				continue;
			}

			dirs.push_back(f);

		} else {
			files.push_back(f);
		}
	}

	da->list_dir_end();

	dirs.sort_custom<FileNoCaseComparator>();
	files.sort_custom<FileNoCaseComparator>();

	int nb_files_total_scan = 0;

	for (List<String>::Element *E = dirs.front(); E; E = E->next()) {
		if (da->change_dir(E->get()) == OK) {
			String d = da->get_current_dir();

			if (d == cd || !d.begins_with(cd)) {
				da->change_dir(cd); //avoid recursion
			} else {
				ScannedDirectory *sd = memnew(ScannedDirectory);
				sd->name = E->get();
				sd->full_path = p_dir->full_path.path_join(sd->name);

				nb_files_total_scan += _scan_new_dir(sd, da);

				p_dir->subdirs.push_back(sd);

				da->change_dir("..");
			}
		} else {
			ERR_PRINT("Cannot go into subdir '" + E->get() + "'.");
		}
	}

	p_dir->files = files;
	nb_files_total_scan += files.size();

	return nb_files_total_scan;
}

void EditorFileSystem::_process_file_system(const ScannedDirectory *p_scan_dir, EditorFileSystemDirectory *p_dir, ScanProgress &p_progress) {
	p_dir->modified_time = FileAccess::get_modified_time(p_scan_dir->full_path);

	for (ScannedDirectory *scan_sub_dir : p_scan_dir->subdirs) {
		EditorFileSystemDirectory *sub_dir = memnew(EditorFileSystemDirectory);
		sub_dir->parent = p_dir;
		sub_dir->name = scan_sub_dir->name;
		p_dir->subdirs.push_back(sub_dir);
		_process_file_system(scan_sub_dir, sub_dir, p_progress);
	}

	for (const String &scan_file : p_scan_dir->files) {
		String ext = scan_file.get_extension().to_lower();
		if (!valid_extensions.has(ext)) {
			p_progress.increment();
			continue; //invalid
		}

		String path = p_scan_dir->full_path.path_join(scan_file);

		EditorFileSystemDirectory::FileInfo *fi = memnew(EditorFileSystemDirectory::FileInfo);
		fi->file = scan_file;
		p_dir->files.push_back(fi);

		FileCache *fc = file_cache.getptr(path);
		uint64_t mt = FileAccess::get_modified_time(path);

		if (import_extensions.has(ext)) {
			//is imported
			uint64_t import_mt = 0;
			if (FileAccess::exists(path + ".import")) {
				import_mt = FileAccess::get_modified_time(path + ".import");
			}

			if (fc && fc->modification_time == mt && fc->import_modification_time == import_mt && !_test_for_reimport(path, true)) {
				fi->type = fc->type;
				fi->resource_script_class = fc->resource_script_class;
				fi->uid = fc->uid;
				fi->deps = fc->deps;
				fi->modified_time = fc->modification_time;
				fi->import_modified_time = fc->import_modification_time;

				fi->import_valid = fc->import_valid;
				fi->import_group_file = fc->import_group_file;
				fi->class_info = fc->class_info;

				if (revalidate_import_files && !ResourceFormatImporter::get_singleton()->are_import_settings_valid(path)) {
					ItemAction ia;
					ia.action = ItemAction::ACTION_FILE_TEST_REIMPORT;
					ia.dir = p_dir;
					ia.file = fi->file;
					scan_actions.push_back(ia);
				}

				if (fc->type == StringName()) {
					fi->type = ResourceLoader::get_resource_type(path);
					fi->resource_script_class = ResourceLoader::get_resource_script_class(path);
					fi->import_group_file = ResourceLoader::get_import_group_file(path);
					//there is also the chance that file type changed due to reimport, must probably check this somehow here (or kind of note it for next time in another file?)
					//note: I think this should not happen any longer..
				}

				if (fc->uid == ResourceUID::INVALID_ID) {
					// imported files should always have a UID, so attempt to fetch it.
					fi->uid = ResourceLoader::get_resource_uid(path);
				}

			} else {
				fi->type = ResourceFormatImporter::get_singleton()->get_resource_type(path);
				fi->uid = ResourceFormatImporter::get_singleton()->get_resource_uid(path);
				fi->import_group_file = ResourceFormatImporter::get_singleton()->get_import_group_file(path);
				fi->class_info = _get_global_script_class(fi->type, path);
				fi->modified_time = 0;
				fi->import_modified_time = 0;
				fi->import_valid = fi->type == "TextFile" ? true : ResourceLoader::is_import_valid(path);

				ItemAction ia;
				ia.action = ItemAction::ACTION_FILE_TEST_REIMPORT;
				ia.dir = p_dir;
				ia.file = fi->file;
				scan_actions.push_back(ia);
			}
		} else {
			if (fc && fc->modification_time == mt) {
				//not imported, so just update type if changed
				fi->type = fc->type;
				fi->resource_script_class = fc->resource_script_class;
				fi->uid = fc->uid;
				fi->modified_time = fc->modification_time;
				fi->deps = fc->deps;
				fi->import_modified_time = 0;
				fi->import_valid = true;
				fi->class_info = fc->class_info;

				if (first_scan && ClassDB::is_parent_class(fi->type, SNAME("Script"))) {
					bool update_script = false;
					String old_class_name = fi->class_info.name;
					fi->class_info = _get_global_script_class(fi->type, path);
					if (old_class_name != fi->class_info.name) {
						update_script = true;
					} else if (!fi->class_info.name.is_empty() && (!ScriptServer::is_global_class(fi->class_info.name) || ScriptServer::get_global_class_path(fi->class_info.name) != path)) {
						// This script has a class name but is not in the global class names or the path of the class has changed.
						update_script = true;
					}
					if (update_script) {
						_queue_update_script_class(path, ScriptClassInfoUpdate::from_file_info(fi));
					}
				}
			} else {
				//new or modified time
				fi->type = ResourceLoader::get_resource_type(path);
				fi->resource_script_class = ResourceLoader::get_resource_script_class(path);
				if (fi->type == "" && textfile_extensions.has(ext)) {
					fi->type = "TextFile";
				}
				fi->uid = ResourceLoader::get_resource_uid(path);
				fi->class_info = _get_global_script_class(fi->type, path);
				fi->deps = _get_dependencies(path);
				fi->modified_time = mt;
				fi->import_modified_time = 0;
				fi->import_valid = true;

				// Files in dep_update_list are forced for rescan to update dependencies. They don't need other updates.
				if (!dep_update_list.has(path)) {
					if (ClassDB::is_parent_class(fi->type, SNAME("Script"))) {
						_queue_update_script_class(path, ScriptClassInfoUpdate::from_file_info(fi));
					}
					if (fi->type == SNAME("PackedScene")) {
						_queue_update_scene_groups(path);
					}
				}
			}
		}

		if (fi->uid != ResourceUID::INVALID_ID) {
			if (ResourceUID::get_singleton()->has_id(fi->uid)) {
				ResourceUID::get_singleton()->set_id(fi->uid, path);
			} else {
				ResourceUID::get_singleton()->add_id(fi->uid, path);
			}
		}

		p_progress.increment();
	}
}

void EditorFileSystem::_scan_fs_changes(EditorFileSystemDirectory *p_dir, ScanProgress &p_progress) {
	uint64_t current_mtime = FileAccess::get_modified_time(p_dir->get_path());

	bool updated_dir = false;
	String cd = p_dir->get_path();
	int diff_nb_files = 0;

	if (current_mtime != p_dir->modified_time || using_fat32_or_exfat) {
		updated_dir = true;
		p_dir->modified_time = current_mtime;
		//ooooops, dir changed, see what's going on

		//first mark everything as verified

		for (int i = 0; i < p_dir->files.size(); i++) {
			p_dir->files[i]->verified = false;
		}

		for (int i = 0; i < p_dir->subdirs.size(); i++) {
			p_dir->get_subdir(i)->verified = false;
		}

		diff_nb_files -= p_dir->files.size();

		//then scan files and directories and check what's different

		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);

		Error ret = da->change_dir(cd);
		ERR_FAIL_COND_MSG(ret != OK, "Cannot change to '" + cd + "' folder.");

		da->list_dir_begin();
		while (true) {
			String f = da->get_next();
			if (f.is_empty()) {
				break;
			}

			if (da->current_is_hidden()) {
				continue;
			}

			if (da->current_is_dir()) {
				if (f.begins_with(".")) { // Ignore special and . / ..
					continue;
				}

				int idx = p_dir->find_dir_index(f);
				if (idx == -1) {
					String dir_path = cd.path_join(f);
					if (_should_skip_directory(dir_path)) {
						continue;
					}

					ScannedDirectory sd;
					sd.name = f;
					sd.full_path = dir_path;

					EditorFileSystemDirectory *efd = memnew(EditorFileSystemDirectory);
					efd->parent = p_dir;
					efd->name = f;

					Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
					d->change_dir(dir_path);
					int nb_files_dir = _scan_new_dir(&sd, d);
					p_progress.hi += nb_files_dir;
					diff_nb_files += nb_files_dir;
					_process_file_system(&sd, efd, p_progress);

					ItemAction ia;
					ia.action = ItemAction::ACTION_DIR_ADD;
					ia.dir = p_dir;
					ia.file = f;
					ia.new_dir = efd;
					scan_actions.push_back(ia);
				} else {
					p_dir->subdirs[idx]->verified = true;
				}

			} else {
				String ext = f.get_extension().to_lower();
				if (!valid_extensions.has(ext)) {
					continue; //invalid
				}

				int idx = p_dir->find_file_index(f);

				if (idx == -1) {
					//never seen this file, add actition to add it
					EditorFileSystemDirectory::FileInfo *fi = memnew(EditorFileSystemDirectory::FileInfo);
					fi->file = f;

					String path = cd.path_join(fi->file);
					fi->modified_time = FileAccess::get_modified_time(path);
					fi->import_modified_time = 0;
					fi->type = ResourceLoader::get_resource_type(path);
					fi->resource_script_class = ResourceLoader::get_resource_script_class(path);
					if (fi->type == "" && textfile_extensions.has(ext)) {
						fi->type = "TextFile";
					}
					fi->class_info = _get_global_script_class(fi->type, path);
					fi->import_modified_time = 0;
					fi->import_valid = fi->type == "TextFile" ? true : ResourceLoader::is_import_valid(path);
					fi->import_group_file = ResourceLoader::get_import_group_file(path);

					{
						ItemAction ia;
						ia.action = ItemAction::ACTION_FILE_ADD;
						ia.dir = p_dir;
						ia.file = f;
						ia.new_file = fi;
						scan_actions.push_back(ia);
					}

					if (import_extensions.has(ext)) {
						//if it can be imported, and it was added, it needs to be reimported
						ItemAction ia;
						ia.action = ItemAction::ACTION_FILE_TEST_REIMPORT;
						ia.dir = p_dir;
						ia.file = f;
						scan_actions.push_back(ia);
					}
					diff_nb_files++;
				} else {
					p_dir->files[idx]->verified = true;
				}
			}
		}

		da->list_dir_end();
	}

	for (int i = 0; i < p_dir->files.size(); i++) {
		if (updated_dir && !p_dir->files[i]->verified) {
			//this file was removed, add action to remove it
			ItemAction ia;
			ia.action = ItemAction::ACTION_FILE_REMOVE;
			ia.dir = p_dir;
			ia.file = p_dir->files[i]->file;
			scan_actions.push_back(ia);
			diff_nb_files--;
			continue;
		}

		String path = cd.path_join(p_dir->files[i]->file);

		if (import_extensions.has(p_dir->files[i]->file.get_extension().to_lower())) {
			//check here if file must be imported or not

			uint64_t mt = FileAccess::get_modified_time(path);

			bool reimport = false;

			if (mt != p_dir->files[i]->modified_time) {
				reimport = true; //it was modified, must be reimported.
			} else if (!FileAccess::exists(path + ".import")) {
				reimport = true; //no .import file, obviously reimport
			} else {
				uint64_t import_mt = FileAccess::get_modified_time(path + ".import");
				if (import_mt != p_dir->files[i]->import_modified_time) {
					reimport = true;
				} else if (_test_for_reimport(path, true)) {
					reimport = true;
				}
			}

			if (reimport) {
				ItemAction ia;
				ia.action = ItemAction::ACTION_FILE_TEST_REIMPORT;
				ia.dir = p_dir;
				ia.file = p_dir->files[i]->file;
				scan_actions.push_back(ia);
			}
		} else if (ResourceCache::has(path)) { //test for potential reload

			uint64_t mt = FileAccess::get_modified_time(path);

			if (mt != p_dir->files[i]->modified_time) {
				p_dir->files[i]->modified_time = mt; //save new time, but test for reload

				ItemAction ia;
				ia.action = ItemAction::ACTION_FILE_RELOAD;
				ia.dir = p_dir;
				ia.file = p_dir->files[i]->file;
				scan_actions.push_back(ia);
			}
		}

		p_progress.increment();
	}

	for (int i = 0; i < p_dir->subdirs.size(); i++) {
		if ((updated_dir && !p_dir->subdirs[i]->verified) || _should_skip_directory(p_dir->subdirs[i]->get_path())) {
			// Add all the files of the folder to be sure _update_scan_actions process the removed files
			// for global class names.
			diff_nb_files += _insert_actions_delete_files_directory(p_dir->subdirs[i]);

			//this directory was removed or ignored, add action to remove it
			ItemAction ia;
			ia.action = ItemAction::ACTION_DIR_REMOVE;
			ia.dir = p_dir->subdirs[i];
			scan_actions.push_back(ia);
			continue;
		}
		_scan_fs_changes(p_dir->get_subdir(i), p_progress);
	}

	nb_files_total = MAX(nb_files_total + diff_nb_files, 0);
}

void EditorFileSystem::_delete_internal_files(const String &p_file) {
	if (FileAccess::exists(p_file + ".import")) {
		List<String> paths;
		ResourceFormatImporter::get_singleton()->get_internal_resource_path_list(p_file, &paths);
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
		for (const String &E : paths) {
			da->remove(E);
		}
		da->remove(p_file + ".import");
	}
}

int EditorFileSystem::_insert_actions_delete_files_directory(EditorFileSystemDirectory *p_dir) {
	int nb_files = 0;
	for (EditorFileSystemDirectory::FileInfo *fi : p_dir->files) {
		ItemAction ia;
		ia.action = ItemAction::ACTION_FILE_REMOVE;
		ia.dir = p_dir;
		ia.file = fi->file;
		scan_actions.push_back(ia);
		nb_files++;
	}

	for (EditorFileSystemDirectory *sub_dir : p_dir->subdirs) {
		nb_files += _insert_actions_delete_files_directory(sub_dir);
	}

	return nb_files;
}

void EditorFileSystem::_thread_func_sources(void *_userdata) {
	EditorFileSystem *efs = (EditorFileSystem *)_userdata;
	if (efs->filesystem) {
		EditorProgressBG pr("sources", TTR("ScanSources"), 1000);
		ScanProgress sp;
		sp.progress = &pr;
		sp.hi = efs->nb_files_total;
		efs->_scan_fs_changes(efs->filesystem, sp);
	}
	efs->scanning_changes_done.set();
}

void EditorFileSystem::_remove_invalid_global_class_names(const HashSet<String> &p_existing_class_names) {
	List<StringName> global_classes;
	bool must_save = false;
	ScriptServer::get_global_class_list(&global_classes);
	for (const StringName &class_name : global_classes) {
		if (!p_existing_class_names.has(class_name)) {
			ScriptServer::remove_global_class(class_name);
			must_save = true;
		}
	}
	if (must_save) {
		ScriptServer::save_global_classes();
	}
}

String EditorFileSystem::_get_file_by_class_name(EditorFileSystemDirectory *p_dir, const String &p_class_name, EditorFileSystemDirectory::FileInfo *&r_file_info) {
	for (EditorFileSystemDirectory::FileInfo *fi : p_dir->files) {
		if (fi->class_info.name == p_class_name) {
			r_file_info = fi;
			return p_dir->get_path().path_join(fi->file);
		}
	}

	for (EditorFileSystemDirectory *sub_dir : p_dir->subdirs) {
		String file = _get_file_by_class_name(sub_dir, p_class_name, r_file_info);
		if (!file.is_empty()) {
			return file;
		}
	}
	r_file_info = nullptr;
	return "";
}

void EditorFileSystem::scan_changes() {
	if (first_scan || // Prevent a premature changes scan from inhibiting the first full scan
			scanning || scanning_changes || thread.is_started()) {
		scan_changes_pending = true;
		set_process(true);
		return;
	}

	_update_extensions();
	sources_changed.clear();
	scanning_changes = true;
	scanning_changes_done.clear();

	if (!use_threads) {
		if (filesystem) {
			EditorProgressBG pr("sources", TTR("ScanSources"), 1000);
			ScanProgress sp;
			sp.progress = &pr;
			sp.hi = nb_files_total;
			scan_total = 0;
			_scan_fs_changes(filesystem, sp);
			if (_update_scan_actions()) {
				emit_signal(SNAME("filesystem_changed"));
			}
		}
		scanning_changes = false;
		scanning_changes_done.set();
		emit_signal(SNAME("sources_changed"), sources_changed.size() > 0);
	} else {
		ERR_FAIL_COND(thread_sources.is_started());
		set_process(true);
		scan_total = 0;
		Thread::Settings s;
		s.priority = Thread::PRIORITY_LOW;
		thread_sources.start(_thread_func_sources, this, s);
	}
}

void EditorFileSystem::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_EXIT_TREE: {
			Thread &active_thread = thread.is_started() ? thread : thread_sources;
			if (use_threads && active_thread.is_started()) {
				while (scanning) {
					OS::get_singleton()->delay_usec(1000);
				}
				active_thread.wait_to_finish();
				WARN_PRINT("Scan thread aborted...");
				set_process(false);
			}

			if (filesystem) {
				memdelete(filesystem);
			}
			if (new_filesystem) {
				memdelete(new_filesystem);
			}
			filesystem = nullptr;
			new_filesystem = nullptr;
		} break;

		case NOTIFICATION_PROCESS: {
			if (use_threads) {
				/** This hack exists because of the EditorProgress nature
				 *  of processing events recursively. This needs to be rewritten
				 *  at some point entirely, but in the meantime the following
				 *  hack prevents deadlock on import.
				 */

				static bool prevent_recursive_process_hack = false;
				if (prevent_recursive_process_hack) {
					break;
				}

				prevent_recursive_process_hack = true;

				bool done_importing = false;

				if (scanning_changes) {
					if (scanning_changes_done.is_set()) {
						set_process(false);

						if (thread_sources.is_started()) {
							thread_sources.wait_to_finish();
						}
						bool changed = _update_scan_actions();
						// Set first_scan to false before the signals so the function doing_first_scan can return false
						// in editor_node to start the export if needed.
						first_scan = false;
						if (changed) {
							emit_signal(SNAME("filesystem_changed"));
						}
						emit_signal(SNAME("sources_changed"), sources_changed.size() > 0);
						scanning_changes = false; // Changed to false here to prevent recursive triggering of scan thread.
						done_importing = true;
					}
				} else if (!scanning && thread.is_started()) {
					set_process(false);

					if (filesystem) {
						memdelete(filesystem);
					}
					filesystem = new_filesystem;
					new_filesystem = nullptr;
					thread.wait_to_finish();
					_update_scan_actions();
					// Update all icons so they are loaded for the FileSystemDock.
					_update_files_icon_path();
					// Set first_scan to false before the signals so the function doing_first_scan can return false
					// in editor_node to start the export if needed.
					first_scan = false;
					emit_signal(SNAME("filesystem_changed"));
					emit_signal(SNAME("sources_changed"), sources_changed.size() > 0);
				}

				if (done_importing && scan_changes_pending) {
					scan_changes_pending = false;
					scan_changes();
				}

				prevent_recursive_process_hack = false;
			}
		} break;
	}
}

bool EditorFileSystem::is_scanning() const {
	return scanning || scanning_changes || first_scan;
}

float EditorFileSystem::get_scanning_progress() const {
	return scan_total;
}

EditorFileSystemDirectory *EditorFileSystem::get_filesystem() {
	return filesystem;
}

void EditorFileSystem::_save_filesystem_cache(EditorFileSystemDirectory *p_dir, Ref<FileAccess> p_file) {
	if (!p_dir) {
		return; //none
	}
	p_file->store_line("::" + p_dir->get_path() + "::" + String::num(p_dir->modified_time));

	for (int i = 0; i < p_dir->files.size(); i++) {
		const EditorFileSystemDirectory::FileInfo *file_info = p_dir->files[i];
		if (!file_info->import_group_file.is_empty()) {
			group_file_cache.insert(file_info->import_group_file);
		}

		String type = file_info->type;
		if (file_info->resource_script_class) {
			type += "/" + String(file_info->resource_script_class);
		}

		PackedStringArray cache_string;
		cache_string.append(file_info->file);
		cache_string.append(type);
		cache_string.append(itos(file_info->uid));
		cache_string.append(itos(file_info->modified_time));
		cache_string.append(itos(file_info->import_modified_time));
		cache_string.append(itos(file_info->import_valid));
		cache_string.append(file_info->import_group_file);
		cache_string.append(String("<>").join({ file_info->class_info.name, file_info->class_info.extends, file_info->class_info.icon_path, itos(file_info->class_info.is_abstract) }));
		cache_string.append(String("<>").join(file_info->deps));

		p_file->store_line(String("::").join(cache_string));
	}

	for (int i = 0; i < p_dir->subdirs.size(); i++) {
		_save_filesystem_cache(p_dir->subdirs[i], p_file);
	}
}

bool EditorFileSystem::_find_file(const String &p_file, EditorFileSystemDirectory **r_d, int &r_file_pos) const {
	//todo make faster

	if (!filesystem || scanning) {
		return false;
	}

	String f = ProjectSettings::get_singleton()->localize_path(p_file);

	// Note: Only checks if base directory is case sensitive.
	Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	bool fs_case_sensitive = dir->is_case_sensitive("res://");

	if (!f.begins_with("res://")) {
		return false;
	}
	f = f.substr(6, f.length());
	f = f.replace("\\", "/");

	Vector<String> path = f.split("/");

	if (path.size() == 0) {
		return false;
	}
	String file = path[path.size() - 1];
	path.resize(path.size() - 1);

	EditorFileSystemDirectory *fs = filesystem;

	for (int i = 0; i < path.size(); i++) {
		if (path[i].begins_with(".")) {
			return false;
		}

		int idx = -1;
		for (int j = 0; j < fs->get_subdir_count(); j++) {
			if (fs_case_sensitive) {
				if (fs->get_subdir(j)->get_name() == path[i]) {
					idx = j;
					break;
				}
			} else {
				if (fs->get_subdir(j)->get_name().to_lower() == path[i].to_lower()) {
					idx = j;
					break;
				}
			}
		}

		if (idx == -1) {
			// Only create a missing directory in memory when it exists on disk.
			if (!dir->dir_exists(fs->get_path().path_join(path[i]))) {
				return false;
			}
			EditorFileSystemDirectory *efsd = memnew(EditorFileSystemDirectory);

			efsd->name = path[i];
			efsd->parent = fs;

			int idx2 = 0;
			for (int j = 0; j < fs->get_subdir_count(); j++) {
				if (efsd->name.filenocasecmp_to(fs->get_subdir(j)->get_name()) < 0) {
					break;
				}
				idx2++;
			}

			if (idx2 == fs->get_subdir_count()) {
				fs->subdirs.push_back(efsd);
			} else {
				fs->subdirs.insert(idx2, efsd);
			}
			fs = efsd;
		} else {
			fs = fs->get_subdir(idx);
		}
	}

	int cpos = -1;
	for (int i = 0; i < fs->files.size(); i++) {
		if (fs->files[i]->file == file) {
			cpos = i;
			break;
		}
	}

	r_file_pos = cpos;
	*r_d = fs;

	return cpos != -1;
}

String EditorFileSystem::get_file_type(const String &p_file) const {
	EditorFileSystemDirectory *fs = nullptr;
	int cpos = -1;

	if (!_find_file(p_file, &fs, cpos)) {
		return "";
	}

	return fs->files[cpos]->type;
}

EditorFileSystemDirectory *EditorFileSystem::find_file(const String &p_file, int *r_index) const {
	if (!filesystem || scanning) {
		return nullptr;
	}

	EditorFileSystemDirectory *fs = nullptr;
	int cpos = -1;
	if (!_find_file(p_file, &fs, cpos)) {
		return nullptr;
	}

	if (r_index) {
		*r_index = cpos;
	}

	return fs;
}

EditorFileSystemDirectory *EditorFileSystem::get_filesystem_path(const String &p_path) {
	if (!filesystem || scanning) {
		return nullptr;
	}

	String f = ProjectSettings::get_singleton()->localize_path(p_path);

	if (!f.begins_with("res://")) {
		return nullptr;
	}

	f = f.substr(6, f.length());
	f = f.replace("\\", "/");
	if (f.is_empty()) {
		return filesystem;
	}

	if (f.ends_with("/")) {
		f = f.substr(0, f.length() - 1);
	}

	Vector<String> path = f.split("/");

	if (path.size() == 0) {
		return nullptr;
	}

	EditorFileSystemDirectory *fs = filesystem;

	for (int i = 0; i < path.size(); i++) {
		int idx = -1;
		for (int j = 0; j < fs->get_subdir_count(); j++) {
			if (fs->get_subdir(j)->get_name() == path[i]) {
				idx = j;
				break;
			}
		}

		if (idx == -1) {
			return nullptr;
		} else {
			fs = fs->get_subdir(idx);
		}
	}

	return fs;
}

void EditorFileSystem::_save_late_updated_files() {
	//files that already existed, and were modified, need re-scanning for dependencies upon project restart. This is done via saving this special file
	String fscache = EditorPaths::get_singleton()->get_project_settings_dir().path_join("filesystem_update4");
	Ref<FileAccess> f = FileAccess::open(fscache, FileAccess::WRITE);
	ERR_FAIL_COND_MSG(f.is_null(), "Cannot create file '" + fscache + "'. Check user write permissions.");
	for (const String &E : late_update_files) {
		f->store_line(E);
	}
}

Vector<String> EditorFileSystem::_get_dependencies(const String &p_path) {
	// Avoid error spam on first opening of a not yet imported project by treating the following situation
	// as a benign one, not letting the file open error happen: the resource is of an importable type but
	// it has not been imported yet.
	if (ResourceFormatImporter::get_singleton()->recognize_path(p_path)) {
		const String &internal_path = ResourceFormatImporter::get_singleton()->get_internal_resource_path(p_path);
		if (!internal_path.is_empty() && !FileAccess::exists(internal_path)) { // If path is empty (error), keep the code flow to the error.
			return Vector<String>();
		}
	}

	List<String> deps;
	ResourceLoader::get_dependencies(p_path, &deps);

	Vector<String> ret;
	for (const String &E : deps) {
		ret.push_back(E);
	}

	return ret;
}

EditorFileSystem::ScriptClassInfo EditorFileSystem::_get_global_script_class(const String &p_type, const String &p_path) const {
	ScriptClassInfo info;
	for (int i = 0; i < ScriptServer::get_language_count(); i++) {
		if (ScriptServer::get_language(i)->handles_global_class_type(p_type)) {
			info.name = ScriptServer::get_language(i)->get_global_class_name(p_path, &info.extends, &info.icon_path, &info.is_abstract);
		}
	}
	return info;
}

void EditorFileSystem::_update_file_icon_path(EditorFileSystemDirectory::FileInfo *file_info) {
	String icon_path;
	if (file_info->resource_script_class != StringName()) {
		icon_path = EditorNode::get_editor_data().script_class_get_icon_path(file_info->resource_script_class);
	} else if (file_info->class_info.icon_path.is_empty() && !file_info->deps.is_empty()) {
		const String &script_dep = file_info->deps[0]; // Assuming the first dependency is a script.
		const String &script_path = script_dep.contains("::") ? script_dep.get_slice("::", 2) : script_dep;
		if (!script_path.is_empty()) {
			String *cached = file_icon_cache.getptr(script_path);
			if (cached) {
				icon_path = *cached;
			} else {
				if (ClassDB::is_parent_class(ResourceLoader::get_resource_type(script_path), SNAME("Script"))) {
					int script_file;
					EditorFileSystemDirectory *efsd = find_file(script_path, &script_file);
					if (efsd) {
						icon_path = efsd->files[script_file]->class_info.icon_path;
					}
				}
				file_icon_cache.insert(script_path, icon_path);
			}
		}
	}

	file_info->class_info.icon_path = icon_path;
}

void EditorFileSystem::_update_files_icon_path(EditorFileSystemDirectory *edp) {
	if (!edp) {
		edp = filesystem;
		file_icon_cache.clear();
	}
	for (EditorFileSystemDirectory *sub_dir : edp->subdirs) {
		_update_files_icon_path(sub_dir);
	}
	for (EditorFileSystemDirectory::FileInfo *fi : edp->files) {
		_update_file_icon_path(fi);
	}
}

void EditorFileSystem::_update_script_classes() {
	if (update_script_paths.is_empty()) {
		// Ensure the global class file is always present; it's essential for exports to work.
		if (!FileAccess::exists(ProjectSettings::get_singleton()->get_global_class_list_path())) {
			ScriptServer::save_global_classes();
		}
		return;
	}

	update_script_mutex.lock();

	for (const KeyValue<String, ScriptClassInfoUpdate> &E : update_script_paths) {
		_register_global_class_script(E.key, E.key, E.value);
	}

	update_script_paths.clear();
	update_script_mutex.unlock();

	ScriptServer::save_global_classes();
	EditorNode::get_editor_data().script_class_save_icon_paths();

	emit_signal("script_classes_updated");

	// Rescan custom loaders and savers.
	// Doing the following here because the `filesystem_changed` signal fires multiple times and isn't always followed by script classes update.
	// So I thought it's better to do this when script classes really get updated
	ResourceLoader::remove_custom_loaders();
	ResourceLoader::add_custom_loaders();
	ResourceSaver::remove_custom_savers();
	ResourceSaver::add_custom_savers();
}

void EditorFileSystem::_update_script_documentation() {
	if (update_script_paths_documentation.is_empty()) {
		return;
	}

	update_script_mutex.lock();

	for (const String &path : update_script_paths_documentation) {
		int index = -1;
		EditorFileSystemDirectory *efd = find_file(path, &index);

		if (!efd || index < 0) {
			// The file was removed
			continue;
		}

		for (int i = 0; i < ScriptServer::get_language_count(); i++) {
			ScriptLanguage *lang = ScriptServer::get_language(i);
			if (lang->supports_documentation() && efd->files[index]->type == lang->get_type()) {
				Ref<Script> scr = ResourceLoader::load(path);
				if (scr.is_null()) {
					continue;
				}
				Vector<DocData::ClassDoc> docs = scr->get_documentation();
				for (int j = 0; j < docs.size(); j++) {
					EditorHelp::get_doc_data()->add_doc(docs[j]);
				}
			}
		}
	}

	update_script_paths_documentation.clear();
	update_script_mutex.unlock();
}

void EditorFileSystem::_process_update_pending() {
	_update_script_classes();
	// Parse documentation second, as it requires the class names to be loaded
	// because _update_script_documentation loads the scripts completely.
	_update_script_documentation();
	_update_pending_scene_groups();
}

void EditorFileSystem::_queue_update_script_class(const String &p_path, const ScriptClassInfoUpdate &p_script_update) {
	update_script_mutex.lock();
	update_script_paths.insert(p_path, p_script_update);
	update_script_paths_documentation.insert(p_path);
	update_script_mutex.unlock();
}

void EditorFileSystem::_update_scene_groups() {
	if (update_scene_paths.is_empty()) {
		return;
	}

	EditorProgress *ep = nullptr;
	if (update_scene_paths.size() > 20) {
		ep = memnew(EditorProgress("update_scene_groups", TTR("Update Scene Groups"), update_scene_paths.size()));
	}
	int step_count = 0;

	update_scene_mutex.lock();
	for (const String &path : update_scene_paths) {
		ProjectSettings::get_singleton()->remove_scene_groups_cache(path);

		int index = -1;
		EditorFileSystemDirectory *efd = find_file(path, &index);

		if (!efd || index < 0) {
			// The file was removed.
			continue;
		}

		const HashSet<StringName> scene_groups = PackedScene::get_scene_groups(path);
		if (!scene_groups.is_empty()) {
			ProjectSettings::get_singleton()->add_scene_groups_cache(path, scene_groups);
		}

		if (ep) {
			ep->step(TTR("Updating Scene Groups..."), step_count++);
		}
	}

	memdelete_notnull(ep);
	update_scene_paths.clear();
	update_scene_mutex.unlock();

	ProjectSettings::get_singleton()->save_scene_groups_cache();
}

void EditorFileSystem::_update_pending_scene_groups() {
	if (!FileAccess::exists(ProjectSettings::get_singleton()->get_scene_groups_cache_path())) {
		_get_all_scenes(get_filesystem(), update_scene_paths);
		_update_scene_groups();
	} else if (!update_scene_paths.is_empty()) {
		_update_scene_groups();
	}
}

void EditorFileSystem::_queue_update_scene_groups(const String &p_path) {
	update_scene_mutex.lock();
	update_scene_paths.insert(p_path);
	update_scene_mutex.unlock();
}

void EditorFileSystem::_get_all_scenes(EditorFileSystemDirectory *p_dir, HashSet<String> &r_list) {
	for (int i = 0; i < p_dir->get_file_count(); i++) {
		if (p_dir->get_file_type(i) == SNAME("PackedScene")) {
			r_list.insert(p_dir->get_file_path(i));
		}
	}

	for (int i = 0; i < p_dir->get_subdir_count(); i++) {
		_get_all_scenes(p_dir->get_subdir(i), r_list);
	}
}

void EditorFileSystem::update_file(const String &p_file) {
	ERR_FAIL_COND(p_file.is_empty());
	update_files({ p_file });
}

void EditorFileSystem::update_files(const Vector<String> &p_script_paths) {
	bool updated = false;
	bool update_files_icon_cache = false;
	Vector<EditorFileSystemDirectory::FileInfo *> files_to_update_icon_path;
	for (const String &file : p_script_paths) {
		ERR_CONTINUE(file.is_empty());
		EditorFileSystemDirectory *fs = nullptr;
		int cpos = -1;

		if (!_find_file(file, &fs, cpos)) {
			if (!fs) {
				continue;
			}
		}

		if (!FileAccess::exists(file)) {
			//was removed
			_delete_internal_files(file);
			if (cpos != -1) { // Might've never been part of the editor file system (*.* files deleted in Open dialog).
				if (fs->files[cpos]->uid != ResourceUID::INVALID_ID) {
					if (ResourceUID::get_singleton()->has_id(fs->files[cpos]->uid)) {
						ResourceUID::get_singleton()->remove_id(fs->files[cpos]->uid);
					}
				}
				if (ClassDB::is_parent_class(fs->files[cpos]->type, SNAME("Script"))) {
					ScriptClassInfoUpdate update;
					update.type = fs->files[cpos]->type;
					_queue_update_script_class(file, update);
					if (!fs->files[cpos]->class_info.icon_path.is_empty()) {
						update_files_icon_cache = true;
					}
				}
				if (fs->files[cpos]->type == SNAME("PackedScene")) {
					_queue_update_scene_groups(file);
				}

				memdelete(fs->files[cpos]);
				fs->files.remove_at(cpos);
				updated = true;
			}
		} else {
			String type = ResourceLoader::get_resource_type(file);
			if (type.is_empty() && textfile_extensions.has(file.get_extension())) {
				type = "TextFile";
			}
			String script_class = ResourceLoader::get_resource_script_class(file);

			ResourceUID::ID uid = ResourceLoader::get_resource_uid(file);

			if (cpos == -1) {
				// The file did not exist, it was added.
				int idx = 0;
				String file_name = file.get_file();

				for (int i = 0; i < fs->files.size(); i++) {
					if (file.filenocasecmp_to(fs->files[i]->file) < 0) {
						break;
					}
					idx++;
				}

				EditorFileSystemDirectory::FileInfo *fi = memnew(EditorFileSystemDirectory::FileInfo);
				fi->file = file_name;
				fi->import_modified_time = 0;
				fi->import_valid = type == "TextFile" ? true : ResourceLoader::is_import_valid(file);

				if (idx == fs->files.size()) {
					fs->files.push_back(fi);
				} else {
					fs->files.insert(idx, fi);
				}
				cpos = idx;
			} else {
				//the file exists and it was updated, and was not added in this step.
				//this means we must force upon next restart to scan it again, to get proper type and dependencies
				late_update_files.insert(file);
				_save_late_updated_files(); //files need to be updated in the re-scan
			}

			const String old_script_class_icon_path = fs->files[cpos]->class_info.icon_path;
			const String old_class_name = fs->files[cpos]->class_info.name;
			fs->files[cpos]->type = type;
			fs->files[cpos]->resource_script_class = script_class;
			fs->files[cpos]->uid = uid;
			fs->files[cpos]->class_info = _get_global_script_class(type, file);
			fs->files[cpos]->import_group_file = ResourceLoader::get_import_group_file(file);
			fs->files[cpos]->modified_time = FileAccess::get_modified_time(file);
			fs->files[cpos]->deps = _get_dependencies(file);
			fs->files[cpos]->import_valid = type == "TextFile" ? true : ResourceLoader::is_import_valid(file);

			if (uid != ResourceUID::INVALID_ID) {
				if (ResourceUID::get_singleton()->has_id(uid)) {
					ResourceUID::get_singleton()->set_id(uid, file);
				} else {
					ResourceUID::get_singleton()->add_id(uid, file);
				}

				ResourceUID::get_singleton()->update_cache();
			}
			// Update preview
			EditorResourcePreview::get_singleton()->check_for_invalidation(file);

			if (ClassDB::is_parent_class(fs->files[cpos]->type, SNAME("Script"))) {
				_queue_update_script_class(file, ScriptClassInfoUpdate::from_file_info(fs->files[cpos]));
			}
			if (fs->files[cpos]->type == SNAME("PackedScene")) {
				_queue_update_scene_groups(file);
			}

			if (fs->files[cpos]->type == SNAME("Resource")) {
				files_to_update_icon_path.push_back(fs->files[cpos]);
			} else if (old_script_class_icon_path != fs->files[cpos]->class_info.icon_path) {
				update_files_icon_cache = true;
			}

			// Restore another script as the global class name if multiple scripts had the same old class name.
			if (!old_class_name.is_empty() && fs->files[cpos]->class_info.name != old_class_name && ClassDB::is_parent_class(type, SNAME("Script"))) {
				EditorFileSystemDirectory::FileInfo *old_fi = nullptr;
				String old_file = _get_file_by_class_name(filesystem, old_class_name, old_fi);
				if (!old_file.is_empty() && old_fi) {
					_queue_update_script_class(old_file, ScriptClassInfoUpdate::from_file_info(old_fi));
				}
			}
			updated = true;
		}
	}

	if (updated) {
		if (update_files_icon_cache) {
			_update_files_icon_path();
		} else {
			for (EditorFileSystemDirectory::FileInfo *fi : files_to_update_icon_path) {
				_update_file_icon_path(fi);
			}
		}
		if (!is_scanning()) {
			_process_update_pending();
		}
		call_deferred(SNAME("emit_signal"), "filesystem_changed"); // Update later
	}
}

HashSet<String> EditorFileSystem::get_valid_extensions() const {
	return valid_extensions;
}

void EditorFileSystem::_register_global_class_script(const String &p_search_path, const String &p_target_path, const ScriptClassInfoUpdate &p_script_update) {
	ScriptServer::remove_global_class_by_path(p_search_path); // First remove, just in case it changed

	if (p_script_update.name.is_empty()) {
		return;
	}

	String lang;
	for (int j = 0; j < ScriptServer::get_language_count(); j++) {
		if (ScriptServer::get_language(j)->handles_global_class_type(p_script_update.type)) {
			lang = ScriptServer::get_language(j)->get_name();
			break;
		}
	}
	if (lang.is_empty()) {
		return; // No lang found that can handle this global class
	}

	ScriptServer::add_global_class(p_script_update.name, p_script_update.extends, lang, p_target_path, p_script_update.is_abstract);
	EditorNode::get_editor_data().script_class_set_icon_path(p_script_update.name, p_script_update.icon_path);
	EditorNode::get_editor_data().script_class_set_name(p_target_path, p_script_update.name);
}

void EditorFileSystem::register_global_class_script(const String &p_search_path, const String &p_target_path) {
	int index_file;
	EditorFileSystemDirectory *efsd = find_file(p_search_path, &index_file);
	if (efsd) {
		const EditorFileSystemDirectory::FileInfo *fi = efsd->files[index_file];
		EditorFileSystem::get_singleton()->_register_global_class_script(p_search_path, p_target_path, ScriptClassInfoUpdate::from_file_info(fi));
	} else {
		ScriptServer::remove_global_class_by_path(p_search_path);
	}
}

Error EditorFileSystem::_reimport_group(const String &p_group_file, const Vector<String> &p_files) {
	String importer_name;

	HashMap<String, HashMap<StringName, Variant>> source_file_options;
	HashMap<String, ResourceUID::ID> uids;
	HashMap<String, String> base_paths;
	for (int i = 0; i < p_files.size(); i++) {
		Ref<ConfigFile> config;
		config.instantiate();
		Error err = config->load(p_files[i] + ".import");
		ERR_CONTINUE(err != OK);
		ERR_CONTINUE(!config->has_section_key("remap", "importer"));
		String file_importer_name = config->get_value("remap", "importer");
		ERR_CONTINUE(file_importer_name.is_empty());

		if (!importer_name.is_empty() && importer_name != file_importer_name) {
			EditorNode::get_singleton()->show_warning(vformat(TTR("There are multiple importers for different types pointing to file %s, import aborted"), p_group_file));
			ERR_FAIL_V(ERR_FILE_CORRUPT);
		}

		ResourceUID::ID uid = ResourceUID::INVALID_ID;

		if (config->has_section_key("remap", "uid")) {
			String uidt = config->get_value("remap", "uid");
			uid = ResourceUID::get_singleton()->text_to_id(uidt);
		}

		uids[p_files[i]] = uid;

		source_file_options[p_files[i]] = HashMap<StringName, Variant>();
		importer_name = file_importer_name;

		if (importer_name == "keep" || importer_name == "skip") {
			continue; //do nothing
		}

		Ref<ResourceImporter> importer = ResourceFormatImporter::get_singleton()->get_importer_by_name(importer_name);
		ERR_FAIL_COND_V(!importer.is_valid(), ERR_FILE_CORRUPT);
		List<ResourceImporter::ImportOption> options;
		importer->get_import_options(p_files[i], &options);
		//set default values
		for (const ResourceImporter::ImportOption &E : options) {
			source_file_options[p_files[i]][E.option.name] = E.default_value;
		}

		if (config->has_section("params")) {
			List<String> sk;
			config->get_section_keys("params", &sk);
			for (const String &param : sk) {
				Variant value = config->get_value("params", param);
				//override with whatever is in file
				source_file_options[p_files[i]][param] = value;
			}
		}

		base_paths[p_files[i]] = ResourceFormatImporter::get_singleton()->get_import_base_path(p_files[i]);
	}

	if (importer_name == "keep" || importer_name == "skip") {
		return OK; // (do nothing)
	}

	ERR_FAIL_COND_V(importer_name.is_empty(), ERR_UNCONFIGURED);

	Ref<ResourceImporter> importer = ResourceFormatImporter::get_singleton()->get_importer_by_name(importer_name);

	Error err = importer->import_group_file(p_group_file, source_file_options, base_paths);

	//all went well, overwrite config files with proper remaps and md5s
	for (const KeyValue<String, HashMap<StringName, Variant>> &E : source_file_options) {
		const String &file = E.key;
		String base_path = ResourceFormatImporter::get_singleton()->get_import_base_path(file);
		Vector<String> dest_paths;
		ResourceUID::ID uid = uids[file];
		{
			Ref<FileAccess> f = FileAccess::open(file + ".import", FileAccess::WRITE);
			ERR_FAIL_COND_V_MSG(f.is_null(), ERR_FILE_CANT_OPEN, "Cannot open import file '" + file + ".import'.");

			//write manually, as order matters ([remap] has to go first for performance).
			f->store_line("[remap]");
			f->store_line("");
			f->store_line("importer=\"" + importer->get_importer_name() + "\"");
			int version = importer->get_format_version();
			if (version > 0) {
				f->store_line("importer_version=" + itos(version));
			}
			if (!importer->get_resource_type().is_empty()) {
				f->store_line("type=\"" + importer->get_resource_type() + "\"");
			}

			if (uid == ResourceUID::INVALID_ID) {
				uid = ResourceUID::get_singleton()->create_id();
			}

			f->store_line("uid=\"" + ResourceUID::get_singleton()->id_to_text(uid) + "\""); // Store in readable format.

			if (err == OK) {
				String path = base_path + "." + importer->get_save_extension();
				f->store_line("path=\"" + path + "\"");
				dest_paths.push_back(path);
			}

			f->store_line("group_file=" + Variant(p_group_file).get_construct_string());

			if (err == OK) {
				f->store_line("valid=true");
			} else {
				f->store_line("valid=false");
			}
			f->store_line("[deps]\n");

			f->store_line("");

			f->store_line("source_file=" + Variant(file).get_construct_string());
			if (dest_paths.size()) {
				Array dp;
				for (int i = 0; i < dest_paths.size(); i++) {
					dp.push_back(dest_paths[i]);
				}
				f->store_line("dest_files=" + Variant(dp).get_construct_string() + "\n");
			}
			f->store_line("[params]");
			f->store_line("");

			//store options in provided order, to avoid file changing. Order is also important because first match is accepted first.

			List<ResourceImporter::ImportOption> options;
			importer->get_import_options(file, &options);
			//set default values
			for (const ResourceImporter::ImportOption &F : options) {
				String base = F.option.name;
				Variant v = F.default_value;
				if (source_file_options[file].has(base)) {
					v = source_file_options[file][base];
				}
				String value;
				VariantWriter::write_to_string(v, value);
				f->store_line(base + "=" + value);
			}
		}

		// Store the md5's of the various files. These are stored separately so that the .import files can be version controlled.
		{
			Ref<FileAccess> md5s = FileAccess::open(base_path + ".md5", FileAccess::WRITE);
			ERR_FAIL_COND_V_MSG(md5s.is_null(), ERR_FILE_CANT_OPEN, "Cannot open MD5 file '" + base_path + ".md5'.");

			md5s->store_line("source_md5=\"" + FileAccess::get_md5(file) + "\"");
			if (dest_paths.size()) {
				md5s->store_line("dest_md5=\"" + FileAccess::get_multiple_md5(dest_paths) + "\"\n");
			}
		}

		EditorFileSystemDirectory *fs = nullptr;
		int cpos = -1;
		bool found = _find_file(file, &fs, cpos);
		ERR_FAIL_COND_V_MSG(!found, ERR_UNCONFIGURED, "Can't find file '" + file + "'.");

		//update modified times, to avoid reimport
		fs->files[cpos]->modified_time = FileAccess::get_modified_time(file);
		fs->files[cpos]->import_modified_time = FileAccess::get_modified_time(file + ".import");
		fs->files[cpos]->deps = _get_dependencies(file);
		fs->files[cpos]->uid = uid;
		fs->files[cpos]->type = importer->get_resource_type();
		if (fs->files[cpos]->type == "" && textfile_extensions.has(file.get_extension())) {
			fs->files[cpos]->type = "TextFile";
		}
		fs->files[cpos]->import_valid = err == OK;

		if (ResourceUID::get_singleton()->has_id(uid)) {
			ResourceUID::get_singleton()->set_id(uid, file);
		} else {
			ResourceUID::get_singleton()->add_id(uid, file);
		}

		//if file is currently up, maybe the source it was loaded from changed, so import math must be updated for it
		//to reload properly
		Ref<Resource> r = ResourceCache::get_ref(file);

		if (r.is_valid()) {
			if (!r->get_import_path().is_empty()) {
				String dst_path = ResourceFormatImporter::get_singleton()->get_internal_resource_path(file);
				r->set_import_path(dst_path);
				r->set_import_last_modified_time(0);
			}
		}

		EditorResourcePreview::get_singleton()->check_for_invalidation(file);
	}

	return err;
}

Error EditorFileSystem::_reimport_file(const String &p_file, const HashMap<StringName, Variant> &p_custom_options, const String &p_custom_importer, Variant *p_generator_parameters) {
	print_verbose(vformat("EditorFileSystem: Importing file: %s", p_file));
	uint64_t start_time = OS::get_singleton()->get_ticks_msec();

	EditorFileSystemDirectory *fs = nullptr;
	int cpos = -1;
	bool found = _find_file(p_file, &fs, cpos);
	ERR_FAIL_COND_V_MSG(!found, ERR_FILE_NOT_FOUND, "Can't find file '" + p_file + "'.");

	//try to obtain existing params

	HashMap<StringName, Variant> params = p_custom_options;
	String importer_name; //empty by default though

	if (!p_custom_importer.is_empty()) {
		importer_name = p_custom_importer;
	}

	ResourceUID::ID uid = ResourceUID::INVALID_ID;
	Variant generator_parameters;
	if (p_generator_parameters) {
		generator_parameters = *p_generator_parameters;
	}

	if (FileAccess::exists(p_file + ".import")) {
		//use existing
		Ref<ConfigFile> cf;
		cf.instantiate();
		Error err = cf->load(p_file + ".import");
		if (err == OK) {
			if (cf->has_section("params")) {
				List<String> sk;
				cf->get_section_keys("params", &sk);
				for (const String &E : sk) {
					if (!params.has(E)) {
						params[E] = cf->get_value("params", E);
					}
				}
			}

			if (cf->has_section("remap")) {
				if (p_custom_importer.is_empty()) {
					importer_name = cf->get_value("remap", "importer");
				}

				if (cf->has_section_key("remap", "uid")) {
					String uidt = cf->get_value("remap", "uid");
					uid = ResourceUID::get_singleton()->text_to_id(uidt);
				}

				if (!p_generator_parameters) {
					if (cf->has_section_key("remap", "generator_parameters")) {
						generator_parameters = cf->get_value("remap", "generator_parameters");
					}
				}
			}
		}
	}

	if (importer_name == "keep" || importer_name == "skip") {
		//keep files, do nothing.
		fs->files[cpos]->modified_time = FileAccess::get_modified_time(p_file);
		fs->files[cpos]->import_modified_time = FileAccess::get_modified_time(p_file + ".import");
		fs->files[cpos]->deps.clear();
		fs->files[cpos]->type = "";
		fs->files[cpos]->import_valid = false;
		EditorResourcePreview::get_singleton()->check_for_invalidation(p_file);
		return OK;
	}
	Ref<ResourceImporter> importer;
	bool load_default = false;
	//find the importer
	if (!importer_name.is_empty()) {
		importer = ResourceFormatImporter::get_singleton()->get_importer_by_name(importer_name);
	}

	if (importer.is_null()) {
		//not found by name, find by extension
		importer = ResourceFormatImporter::get_singleton()->get_importer_by_extension(p_file.get_extension());
		load_default = true;
		if (importer.is_null()) {
			ERR_FAIL_V_MSG(ERR_FILE_CANT_OPEN, "BUG: File queued for import, but can't be imported, importer for type '" + importer_name + "' not found.");
		}
	}

	if (FileAccess::exists(p_file + ".import")) {
		// We only want to handle compat for existing files, not new ones.
		importer->handle_compatibility_options(params);
	}

	//mix with default params, in case a parameter is missing

	List<ResourceImporter::ImportOption> opts;
	importer->get_import_options(p_file, &opts);
	for (const ResourceImporter::ImportOption &E : opts) {
		if (!params.has(E.option.name)) { //this one is not present
			params[E.option.name] = E.default_value;
		}
	}

	if (load_default && ProjectSettings::get_singleton()->has_setting("importer_defaults/" + importer->get_importer_name())) {
		//use defaults if exist
		Dictionary d = GLOBAL_GET("importer_defaults/" + importer->get_importer_name());
		List<Variant> v;
		d.get_key_list(&v);

		for (const Variant &E : v) {
			params[E] = d[E];
		}
	}

	//finally, perform import!!
	String base_path = ResourceFormatImporter::get_singleton()->get_import_base_path(p_file);

	List<String> import_variants;
	List<String> gen_files;
	Variant meta;
	Error err = importer->import(p_file, base_path, params, &import_variants, &gen_files, &meta);

	// As import is complete, save the .import file.

	Vector<String> dest_paths;
	{
		Ref<FileAccess> f = FileAccess::open(p_file + ".import", FileAccess::WRITE);
		ERR_FAIL_COND_V_MSG(f.is_null(), ERR_FILE_CANT_OPEN, "Cannot open file from path '" + p_file + ".import'.");

		// Write manually, as order matters ([remap] has to go first for performance).
		f->store_line("[remap]");
		f->store_line("");
		f->store_line("importer=\"" + importer->get_importer_name() + "\"");
		int version = importer->get_format_version();
		if (version > 0) {
			f->store_line("importer_version=" + itos(version));
		}
		if (!importer->get_resource_type().is_empty()) {
			f->store_line("type=\"" + importer->get_resource_type() + "\"");
		}

		if (uid == ResourceUID::INVALID_ID) {
			uid = ResourceUID::get_singleton()->create_id();
		}

		f->store_line("uid=\"" + ResourceUID::get_singleton()->id_to_text(uid) + "\""); // Store in readable format.

		if (err == OK) {
			if (importer->get_save_extension().is_empty()) {
				//no path
			} else if (import_variants.size()) {
				//import with variants
				for (const String &E : import_variants) {
					String path = base_path.c_escape() + "." + E + "." + importer->get_save_extension();

					f->store_line("path." + E + "=\"" + path + "\"");
					dest_paths.push_back(path);
				}
			} else {
				String path = base_path + "." + importer->get_save_extension();
				f->store_line("path=\"" + path + "\"");
				dest_paths.push_back(path);
			}

		} else {
			f->store_line("valid=false");
		}

		if (meta != Variant()) {
			f->store_line("metadata=" + meta.get_construct_string());
		}

		if (generator_parameters != Variant()) {
			f->store_line("generator_parameters=" + generator_parameters.get_construct_string());
		}

		f->store_line("");

		f->store_line("[deps]\n");

		if (gen_files.size()) {
			Array genf;
			for (const String &E : gen_files) {
				genf.push_back(E);
				dest_paths.push_back(E);
			}

			String value;
			VariantWriter::write_to_string(genf, value);
			f->store_line("files=" + value);
			f->store_line("");
		}

		f->store_line("source_file=" + Variant(p_file).get_construct_string());

		if (dest_paths.size()) {
			Array dp;
			for (int i = 0; i < dest_paths.size(); i++) {
				dp.push_back(dest_paths[i]);
			}
			f->store_line("dest_files=" + Variant(dp).get_construct_string());
		}
		f->store_line("");

		f->store_line("[params]");
		f->store_line("");

		// Store options in provided order, to avoid file changing. Order is also important because first match is accepted first.

		for (const ResourceImporter::ImportOption &E : opts) {
			String base = E.option.name;
			String value;
			VariantWriter::write_to_string(params[base], value);
			f->store_line(base + "=" + value);
		}
	}

	// Store the md5's of the various files. These are stored separately so that the .import files can be version controlled.
	{
		Ref<FileAccess> md5s = FileAccess::open(base_path + ".md5", FileAccess::WRITE);
		ERR_FAIL_COND_V_MSG(md5s.is_null(), ERR_FILE_CANT_OPEN, "Cannot open MD5 file '" + base_path + ".md5'.");

		md5s->store_line("source_md5=\"" + FileAccess::get_md5(p_file) + "\"");
		if (dest_paths.size()) {
			md5s->store_line("dest_md5=\"" + FileAccess::get_multiple_md5(dest_paths) + "\"\n");
		}
	}

	// Update cpos, newly created files could've changed the index of the reimported p_file.
	_find_file(p_file, &fs, cpos);

	// Update modified times, to avoid reimport.
	fs->files[cpos]->modified_time = FileAccess::get_modified_time(p_file);
	fs->files[cpos]->import_modified_time = FileAccess::get_modified_time(p_file + ".import");
	fs->files[cpos]->deps = _get_dependencies(p_file);
	fs->files[cpos]->type = importer->get_resource_type();
	fs->files[cpos]->uid = uid;
	fs->files[cpos]->import_valid = fs->files[cpos]->type == "TextFile" ? true : ResourceLoader::is_import_valid(p_file);

	if (ResourceUID::get_singleton()->has_id(uid)) {
		ResourceUID::get_singleton()->set_id(uid, p_file);
	} else {
		ResourceUID::get_singleton()->add_id(uid, p_file);
	}

	// If file is currently up, maybe the source it was loaded from changed, so import math must be updated for it
	// to reload properly.
	Ref<Resource> r = ResourceCache::get_ref(p_file);
	if (r.is_valid()) {
		if (!r->get_import_path().is_empty()) {
			String dst_path = ResourceFormatImporter::get_singleton()->get_internal_resource_path(p_file);
			r->set_import_path(dst_path);
			r->set_import_last_modified_time(0);
		}
	}

	EditorResourcePreview::get_singleton()->check_for_invalidation(p_file);

	print_verbose(vformat("EditorFileSystem: \"%s\" import took %d ms.", p_file, OS::get_singleton()->get_ticks_msec() - start_time));

	ERR_FAIL_COND_V_MSG(err != OK, ERR_FILE_UNRECOGNIZED, "Error importing '" + p_file + "'.");
	return OK;
}

void EditorFileSystem::_find_group_files(EditorFileSystemDirectory *efd, HashMap<String, Vector<String>> &group_files, HashSet<String> &groups_to_reimport) {
	int fc = efd->files.size();
	const EditorFileSystemDirectory::FileInfo *const *files = efd->files.ptr();
	for (int i = 0; i < fc; i++) {
		if (groups_to_reimport.has(files[i]->import_group_file)) {
			if (!group_files.has(files[i]->import_group_file)) {
				group_files[files[i]->import_group_file] = Vector<String>();
			}
			group_files[files[i]->import_group_file].push_back(efd->get_file_path(i));
		}
	}

	for (int i = 0; i < efd->get_subdir_count(); i++) {
		_find_group_files(efd->get_subdir(i), group_files, groups_to_reimport);
	}
}

void EditorFileSystem::reimport_file_with_custom_parameters(const String &p_file, const String &p_importer, const HashMap<StringName, Variant> &p_custom_params) {
	Vector<String> reloads;
	reloads.append(p_file);

	// Emit the resource_reimporting signal for the single file before the actual importation.
	emit_signal(SNAME("resources_reimporting"), reloads);

	_reimport_file(p_file, p_custom_params, p_importer);

	// Emit the resource_reimported signal for the single file we just reimported.
	emit_signal(SNAME("resources_reimported"), reloads);
}

void EditorFileSystem::_reimport_thread(uint32_t p_index, ImportThreadData *p_import_data) {
	int current_max = p_import_data->reimport_from + int(p_index);
	p_import_data->max_index.exchange_if_greater(current_max);
	_reimport_file(p_import_data->reimport_files[current_max].path);
}

void EditorFileSystem::reimport_files(const Vector<String> &p_files) {
	ERR_FAIL_COND_MSG(importing, "Attempted to call reimport_files() recursively, this is not allowed.");
	importing = true;

	Vector<String> reloads;

	EditorProgress pr("reimport", TTR("(Re)Importing Assets"), p_files.size());

	Vector<ImportFile> reimport_files;

	HashSet<String> groups_to_reimport;

	for (int i = 0; i < p_files.size(); i++) {
		String file = p_files[i];

		ResourceUID::ID uid = ResourceUID::get_singleton()->text_to_id(file);
		if (uid != ResourceUID::INVALID_ID && ResourceUID::get_singleton()->has_id(uid)) {
			file = ResourceUID::get_singleton()->get_id_path(uid);
		}

		String group_file = ResourceFormatImporter::get_singleton()->get_import_group_file(file);

		if (group_file_cache.has(file)) {
			// Maybe the file itself is a group!
			groups_to_reimport.insert(file);
			// Groups do not belong to groups.
			group_file = String();
		} else if (groups_to_reimport.has(file)) {
			// Groups do not belong to groups.
			group_file = String();
		} else if (!group_file.is_empty()) {
			// It's a group file, add group to import and skip this file.
			groups_to_reimport.insert(group_file);
		} else {
			// It's a regular file.
			ImportFile ifile;
			ifile.path = file;
			ResourceFormatImporter::get_singleton()->get_import_order_threads_and_importer(file, ifile.order, ifile.threaded, ifile.importer);
			reloads.push_back(file);
			reimport_files.push_back(ifile);
		}

		// Group may have changed, so also update group reference.
		EditorFileSystemDirectory *fs = nullptr;
		int cpos = -1;
		if (_find_file(file, &fs, cpos)) {
			fs->files.write[cpos]->import_group_file = group_file;
		}
	}

	reimport_files.sort();

	// Emit the resource_reimporting signal for the single file before the actual importation.
	emit_signal(SNAME("resources_reimporting"), reloads);

#ifdef THREADS_ENABLED
	bool use_multiple_threads = GLOBAL_GET("editor/import/use_multiple_threads");
#else
	bool use_multiple_threads = false;
#endif

	int from = 0;
	for (int i = 0; i < reimport_files.size(); i++) {
		if (groups_to_reimport.has(reimport_files[i].path)) {
			from = i + 1;
			continue;
		}

		if (use_multiple_threads && reimport_files[i].threaded) {
			if (i + 1 == reimport_files.size() || reimport_files[i + 1].importer != reimport_files[from].importer || groups_to_reimport.has(reimport_files[i + 1].path)) {
				if (from - i == 0) {
					// Single file, do not use threads.
					pr.step(reimport_files[i].path.get_file(), i);
					_reimport_file(reimport_files[i].path);
				} else {
					Ref<ResourceImporter> importer = ResourceFormatImporter::get_singleton()->get_importer_by_name(reimport_files[from].importer);
					if (importer.is_null()) {
						ERR_PRINT(vformat("Invalid importer for \"%s\".", reimport_files[from].importer));
						from = i + 1;
						continue;
					}

					importer->import_threaded_begin();

					ImportThreadData tdata;
					tdata.max_index.set(from);
					tdata.reimport_from = from;
					tdata.reimport_files = reimport_files.ptr();

					WorkerThreadPool::GroupID group_task = WorkerThreadPool::get_singleton()->add_template_group_task(this, &EditorFileSystem::_reimport_thread, &tdata, i - from + 1, -1, false, vformat(TTR("Import resources of type: %s"), reimport_files[from].importer));
					int current_index = from - 1;
					do {
						if (current_index < tdata.max_index.get()) {
							current_index = tdata.max_index.get();
							pr.step(reimport_files[current_index].path.get_file(), current_index);
						}
						OS::get_singleton()->delay_usec(1);
					} while (!WorkerThreadPool::get_singleton()->is_group_task_completed(group_task));

					WorkerThreadPool::get_singleton()->wait_for_group_task_completion(group_task);

					importer->import_threaded_end();
				}

				from = i + 1;
			}

		} else {
			pr.step(reimport_files[i].path.get_file(), i);
			_reimport_file(reimport_files[i].path);

			// We need to increment the counter, maybe the next file is multithreaded
			// and doesn't have the same importer.
			from = i + 1;
		}
	}

	// Reimport groups.

	from = reimport_files.size();

	if (groups_to_reimport.size()) {
		HashMap<String, Vector<String>> group_files;
		_find_group_files(filesystem, group_files, groups_to_reimport);
		for (const KeyValue<String, Vector<String>> &E : group_files) {
			pr.step(E.key.get_file(), from++);
			Error err = _reimport_group(E.key, E.value);
			reloads.push_back(E.key);
			reloads.append_array(E.value);
			if (err == OK) {
				_reimport_file(E.key);
			}
		}
	}

	ResourceUID::get_singleton()->update_cache(); // After reimporting, update the cache.

	_save_filesystem_cache();
	_process_update_pending();
	importing = false;
	if (!is_scanning()) {
		emit_signal(SNAME("filesystem_changed"));
	}

	emit_signal(SNAME("resources_reimported"), reloads);
}

Error EditorFileSystem::reimport_append(const String &p_file, const HashMap<StringName, Variant> &p_custom_options, const String &p_custom_importer, Variant p_generator_parameters) {
	ERR_FAIL_COND_V_MSG(!importing, ERR_INVALID_PARAMETER, "Can only append files to import during a current reimport process.");
	Vector<String> reloads;
	reloads.append(p_file);

	// Emit the resource_reimporting signal for the single file before the actual importation.
	emit_signal(SNAME("resources_reimporting"), reloads);

	Error ret = _reimport_file(p_file, p_custom_options, p_custom_importer, &p_generator_parameters);

	// Emit the resource_reimported signal for the single file we just reimported.
	emit_signal(SNAME("resources_reimported"), reloads);
	return ret;
}

Error EditorFileSystem::_resource_import(const String &p_path) {
	Vector<String> files;
	files.push_back(p_path);

	singleton->update_file(p_path);
	singleton->reimport_files(files);

	return OK;
}

bool EditorFileSystem::_should_skip_directory(const String &p_path) {
	String project_data_path = ProjectSettings::get_singleton()->get_project_data_path();
	if (p_path == project_data_path || p_path.begins_with(project_data_path + "/")) {
		return true;
	}

	if (FileAccess::exists(p_path.path_join("project.godot"))) {
		// Skip if another project inside this.
		if (EditorFileSystem::get_singleton()->first_scan) {
			WARN_PRINT_ONCE(vformat("Detected another project.godot at %s. The folder will be ignored.", p_path));
		}
		return true;
	}

	if (FileAccess::exists(p_path.path_join(".gdignore"))) {
		// Skip if a `.gdignore` file is inside this.
		return true;
	}

	return false;
}

bool EditorFileSystem::is_group_file(const String &p_path) const {
	return group_file_cache.has(p_path);
}

void EditorFileSystem::_move_group_files(EditorFileSystemDirectory *efd, const String &p_group_file, const String &p_new_location) {
	int fc = efd->files.size();
	EditorFileSystemDirectory::FileInfo *const *files = efd->files.ptrw();
	for (int i = 0; i < fc; i++) {
		if (files[i]->import_group_file == p_group_file) {
			files[i]->import_group_file = p_new_location;

			Ref<ConfigFile> config;
			config.instantiate();
			String path = efd->get_file_path(i) + ".import";
			Error err = config->load(path);
			if (err != OK) {
				continue;
			}
			if (config->has_section_key("remap", "group_file")) {
				config->set_value("remap", "group_file", p_new_location);
			}

			List<String> sk;
			config->get_section_keys("params", &sk);
			for (const String &param : sk) {
				//not very clean, but should work
				String value = config->get_value("params", param);
				if (value == p_group_file) {
					config->set_value("params", param, p_new_location);
				}
			}

			config->save(path);
		}
	}

	for (int i = 0; i < efd->get_subdir_count(); i++) {
		_move_group_files(efd->get_subdir(i), p_group_file, p_new_location);
	}
}

void EditorFileSystem::move_group_file(const String &p_path, const String &p_new_path) {
	if (get_filesystem()) {
		_move_group_files(get_filesystem(), p_path, p_new_path);
		if (group_file_cache.has(p_path)) {
			group_file_cache.erase(p_path);
			group_file_cache.insert(p_new_path);
		}
	}
}

ResourceUID::ID EditorFileSystem::_resource_saver_get_resource_id_for_path(const String &p_path, bool p_generate) {
	if (!p_path.is_resource_file() || p_path.begins_with(ProjectSettings::get_singleton()->get_project_data_path())) {
		// Saved externally (configuration file) or internal file, do not assign an ID.
		return ResourceUID::INVALID_ID;
	}

	EditorFileSystemDirectory *fs = nullptr;
	int cpos = -1;

	if (!singleton->_find_file(p_path, &fs, cpos)) {
		// Fallback to ResourceLoader if filesystem cache fails (can happen during scanning etc.).
		ResourceUID::ID fallback = ResourceLoader::get_resource_uid(p_path);
		if (fallback != ResourceUID::INVALID_ID) {
			return fallback;
		}

		if (p_generate) {
			return ResourceUID::get_singleton()->create_id(); // Just create a new one, we will be notified of save anyway and fetch the right UID at that time, to keep things simple.
		} else {
			return ResourceUID::INVALID_ID;
		}
	} else if (fs->files[cpos]->uid != ResourceUID::INVALID_ID) {
		return fs->files[cpos]->uid;
	} else if (p_generate) {
		return ResourceUID::get_singleton()->create_id(); // Just create a new one, we will be notified of save anyway and fetch the right UID at that time, to keep things simple.
	} else {
		return ResourceUID::INVALID_ID;
	}
}

static void _scan_extensions_dir(EditorFileSystemDirectory *d, HashSet<String> &extensions) {
	int fc = d->get_file_count();
	for (int i = 0; i < fc; i++) {
		if (d->get_file_type(i) == SNAME("GDExtension")) {
			extensions.insert(d->get_file_path(i));
		}
	}
	int dc = d->get_subdir_count();
	for (int i = 0; i < dc; i++) {
		_scan_extensions_dir(d->get_subdir(i), extensions);
	}
}
bool EditorFileSystem::_scan_extensions() {
	EditorFileSystemDirectory *d = get_filesystem();
	HashSet<String> extensions;

	_scan_extensions_dir(d, extensions);

	//verify against loaded extensions

	Vector<String> extensions_added;
	Vector<String> extensions_removed;

	for (const String &E : extensions) {
		if (!GDExtensionManager::get_singleton()->is_extension_loaded(E)) {
			extensions_added.push_back(E);
		}
	}

	Vector<String> loaded_extensions = GDExtensionManager::get_singleton()->get_loaded_extensions();
	for (int i = 0; i < loaded_extensions.size(); i++) {
		if (!extensions.has(loaded_extensions[i])) {
			extensions_removed.push_back(loaded_extensions[i]);
		}
	}

	String extension_list_config_file = GDExtension::get_extension_list_config_file();
	if (extensions.size()) {
		if (extensions_added.size() || extensions_removed.size()) { //extensions were added or removed
			Ref<FileAccess> f = FileAccess::open(extension_list_config_file, FileAccess::WRITE);
			for (const String &E : extensions) {
				f->store_line(E);
			}
		}
	} else {
		if (loaded_extensions.size() || FileAccess::exists(extension_list_config_file)) { //extensions were removed
			Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
			da->remove(extension_list_config_file);
		}
	}

	bool needs_restart = false;
	for (int i = 0; i < extensions_added.size(); i++) {
		GDExtensionManager::LoadStatus st = GDExtensionManager::get_singleton()->load_extension(extensions_added[i]);
		if (st == GDExtensionManager::LOAD_STATUS_NEEDS_RESTART) {
			needs_restart = true;
		}
	}
	for (int i = 0; i < extensions_removed.size(); i++) {
		GDExtensionManager::LoadStatus st = GDExtensionManager::get_singleton()->unload_extension(extensions_removed[i]);
		if (st == GDExtensionManager::LOAD_STATUS_NEEDS_RESTART) {
			needs_restart = true;
		}
	}

	return needs_restart;
}

void EditorFileSystem::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_filesystem"), &EditorFileSystem::get_filesystem);
	ClassDB::bind_method(D_METHOD("is_scanning"), &EditorFileSystem::is_scanning);
	ClassDB::bind_method(D_METHOD("get_scanning_progress"), &EditorFileSystem::get_scanning_progress);
	ClassDB::bind_method(D_METHOD("scan"), &EditorFileSystem::scan);
	ClassDB::bind_method(D_METHOD("scan_sources"), &EditorFileSystem::scan_changes);
	ClassDB::bind_method(D_METHOD("update_file", "path"), &EditorFileSystem::update_file);
	ClassDB::bind_method(D_METHOD("get_filesystem_path", "path"), &EditorFileSystem::get_filesystem_path);
	ClassDB::bind_method(D_METHOD("get_file_type", "path"), &EditorFileSystem::get_file_type);
	ClassDB::bind_method(D_METHOD("reimport_files", "files"), &EditorFileSystem::reimport_files);

	ADD_SIGNAL(MethodInfo("filesystem_changed"));
	ADD_SIGNAL(MethodInfo("script_classes_updated"));
	ADD_SIGNAL(MethodInfo("sources_changed", PropertyInfo(Variant::BOOL, "exist")));
	ADD_SIGNAL(MethodInfo("resources_reimporting", PropertyInfo(Variant::PACKED_STRING_ARRAY, "resources")));
	ADD_SIGNAL(MethodInfo("resources_reimported", PropertyInfo(Variant::PACKED_STRING_ARRAY, "resources")));
	ADD_SIGNAL(MethodInfo("resources_reload", PropertyInfo(Variant::PACKED_STRING_ARRAY, "resources")));
}

void EditorFileSystem::_update_extensions() {
	valid_extensions.clear();
	import_extensions.clear();
	textfile_extensions.clear();

	List<String> extensionsl;
	ResourceLoader::get_recognized_extensions_for_type("", &extensionsl);
	for (const String &E : extensionsl) {
		valid_extensions.insert(E);
	}

	const Vector<String> textfile_ext = ((String)(EDITOR_GET("docks/filesystem/textfile_extensions"))).split(",", false);
	for (const String &E : textfile_ext) {
		if (valid_extensions.has(E)) {
			continue;
		}
		valid_extensions.insert(E);
		textfile_extensions.insert(E);
	}

	extensionsl.clear();
	ResourceFormatImporter::get_singleton()->get_recognized_extensions(&extensionsl);
	for (const String &E : extensionsl) {
		import_extensions.insert(E);
	}
}

void EditorFileSystem::add_import_format_support_query(Ref<EditorFileSystemImportFormatSupportQuery> p_query) {
	ERR_FAIL_COND(import_support_queries.has(p_query));
	import_support_queries.push_back(p_query);
}
void EditorFileSystem::remove_import_format_support_query(Ref<EditorFileSystemImportFormatSupportQuery> p_query) {
	import_support_queries.erase(p_query);
}

EditorFileSystem::EditorFileSystem() {
#ifdef THREADS_ENABLED
	use_threads = true;
#endif

	ResourceLoader::import = _resource_import;
	reimport_on_missing_imported_files = GLOBAL_GET("editor/import/reimport_missing_imported_files");
	singleton = this;
	filesystem = memnew(EditorFileSystemDirectory); //like, empty
	filesystem->parent = nullptr;

	new_filesystem = nullptr;

	// This should probably also work on Unix and use the string it returns for FAT32 or exFAT
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	using_fat32_or_exfat = (da->get_filesystem_type() == "FAT32" || da->get_filesystem_type() == "exFAT");

	scan_total = 0;
	ResourceSaver::set_get_resource_id_for_path(_resource_saver_get_resource_id_for_path);
}

EditorFileSystem::~EditorFileSystem() {
	ResourceSaver::set_get_resource_id_for_path(nullptr);
}

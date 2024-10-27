/**************************************************************************/
/*  editor_file_system.h                                                  */
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

#ifndef EDITOR_FILE_SYSTEM_H
#define EDITOR_FILE_SYSTEM_H

#include "core/io/dir_access.h"
#include "core/io/resource_importer.h"
#include "core/io/resource_loader.h"
#include "core/os/thread.h"
#include "core/os/thread_safe.h"
#include "core/templates/hash_set.h"
#include "core/templates/safe_refcount.h"
#include "scene/main/node.h"

class FileAccess;

struct EditorProgressBG;
class EditorFileSystemDirectory : public Object {
	GDCLASS(EditorFileSystemDirectory, Object);

	String name;
	uint64_t modified_time;
	bool verified = false; //used for checking changes

	EditorFileSystemDirectory *parent = nullptr;
	Vector<EditorFileSystemDirectory *> subdirs;

	struct FileInfo {
		String file;
		StringName type;
		StringName resource_script_class; // If any resource has script with a global class name, its found here.
		ResourceUID::ID uid = ResourceUID::INVALID_ID;
		uint64_t modified_time = 0;
		uint64_t import_modified_time = 0;
		String import_md5;
		Vector<String> import_dest_paths;
		bool import_valid = false;
		String import_group_file;
		Vector<String> deps;
		bool verified = false; //used for checking changes
		// These are for script resources only.
		String script_class_name;
		String script_class_extends;
		String script_class_icon_path;
		String icon_path;
	};

	Vector<FileInfo *> files;

	static void _bind_methods();

	friend class EditorFileSystem;

public:
	String get_name();
	String get_path() const;

	int get_subdir_count() const;
	EditorFileSystemDirectory *get_subdir(int p_idx);
	int get_file_count() const;
	String get_file(int p_idx) const;
	String get_file_path(int p_idx) const;
	StringName get_file_type(int p_idx) const;
	StringName get_file_resource_script_class(int p_idx) const;
	Vector<String> get_file_deps(int p_idx) const;
	bool get_file_import_is_valid(int p_idx) const;
	uint64_t get_file_modified_time(int p_idx) const;
	uint64_t get_file_import_modified_time(int p_idx) const;
	String get_file_script_class_name(int p_idx) const; //used for scripts
	String get_file_script_class_extends(int p_idx) const; //used for scripts
	String get_file_script_class_icon_path(int p_idx) const; //used for scripts
	String get_file_icon_path(int p_idx) const; //used for FileSystemDock

	EditorFileSystemDirectory *get_parent();

	int find_file_index(const String &p_file) const;
	int find_dir_index(const String &p_dir) const;

	void force_update();

	EditorFileSystemDirectory();
	~EditorFileSystemDirectory();
};

class EditorFileSystemImportFormatSupportQuery : public RefCounted {
	GDCLASS(EditorFileSystemImportFormatSupportQuery, RefCounted);

protected:
	GDVIRTUAL0RC_REQUIRED(bool, _is_active)
	GDVIRTUAL0RC_REQUIRED(Vector<String>, _get_file_extensions)
	GDVIRTUAL0RC_REQUIRED(bool, _query)
	static void _bind_methods() {
		GDVIRTUAL_BIND(_is_active);
		GDVIRTUAL_BIND(_get_file_extensions);
		GDVIRTUAL_BIND(_query);
	}

public:
	virtual bool is_active() const {
		bool ret = false;
		GDVIRTUAL_CALL(_is_active, ret);
		return ret;
	}
	virtual Vector<String> get_file_extensions() const {
		Vector<String> ret;
		GDVIRTUAL_CALL(_get_file_extensions, ret);
		return ret;
	}
	virtual bool query() {
		bool ret = false;
		GDVIRTUAL_CALL(_query, ret);
		return ret;
	}
};

class EditorFileSystem : public Node {
	GDCLASS(EditorFileSystem, Node);

	_THREAD_SAFE_CLASS_

	struct ItemAction {
		enum Action {
			ACTION_NONE,
			ACTION_DIR_ADD,
			ACTION_DIR_REMOVE,
			ACTION_FILE_ADD,
			ACTION_FILE_REMOVE,
			ACTION_FILE_TEST_REIMPORT,
			ACTION_FILE_RELOAD
		};

		Action action = ACTION_NONE;
		EditorFileSystemDirectory *dir = nullptr;
		String file;
		EditorFileSystemDirectory *new_dir = nullptr;
		EditorFileSystemDirectory::FileInfo *new_file = nullptr;
	};

	struct ScannedDirectory {
		String name;
		String full_path;
		Vector<ScannedDirectory *> subdirs;
		List<String> files;

		~ScannedDirectory();
	};

	bool use_threads = false;
	Thread thread;
	static void _thread_func(void *_userdata);

	EditorFileSystemDirectory *new_filesystem = nullptr;
	ScannedDirectory *first_scan_root_dir = nullptr;

	bool scanning = false;
	bool importing = false;
	bool first_scan = true;
	bool scan_changes_pending = false;
	float scan_total;
	String filesystem_settings_version_for_import;
	bool revalidate_import_files = false;
	int nb_files_total = 0;

	void _scan_filesystem();
	void _first_scan_filesystem();
	void _first_scan_process_scripts(const ScannedDirectory *p_scan_dir, List<String> &p_gdextension_extensions, HashSet<String> &p_existing_class_names, HashSet<String> &p_extensions);

	HashSet<String> late_update_files;

	void _save_late_updated_files();

	EditorFileSystemDirectory *filesystem = nullptr;

	static EditorFileSystem *singleton;

	/* Used for reading the filesystem cache file */
	struct FileCache {
		String type;
		String resource_script_class;
		ResourceUID::ID uid = ResourceUID::INVALID_ID;
		uint64_t modification_time = 0;
		uint64_t import_modification_time = 0;
		String import_md5;
		Vector<String> import_dest_paths;
		Vector<String> deps;
		bool import_valid = false;
		String import_group_file;
		String script_class_name;
		String script_class_extends;
		String script_class_icon_path;
	};

	HashMap<String, FileCache> file_cache;
	HashSet<String> dep_update_list;

	struct ScanProgress {
		float hi = 0;
		int current = 0;
		EditorProgressBG *progress = nullptr;
		void increment();
	};

	struct DirectoryComparator {
		bool operator()(const EditorFileSystemDirectory *p_a, const EditorFileSystemDirectory *p_b) const {
			return p_a->name.filenocasecmp_to(p_b->name) < 0;
		}
	};

	void _save_filesystem_cache();
	void _save_filesystem_cache(EditorFileSystemDirectory *p_dir, Ref<FileAccess> p_file);

	bool _find_file(const String &p_file, EditorFileSystemDirectory **r_d, int &r_file_pos) const;

	void _scan_fs_changes(EditorFileSystemDirectory *p_dir, ScanProgress &p_progress, bool p_recursive = true);

	void _delete_internal_files(const String &p_file);
	int _insert_actions_delete_files_directory(EditorFileSystemDirectory *p_dir);

	HashSet<String> textfile_extensions;
	HashSet<String> other_file_extensions;
	HashSet<String> valid_extensions;
	HashSet<String> import_extensions;

	int _scan_new_dir(ScannedDirectory *p_dir, Ref<DirAccess> &da);
	void _process_file_system(const ScannedDirectory *p_scan_dir, EditorFileSystemDirectory *p_dir, ScanProgress &p_progress, HashSet<String> *p_processed_files);

	Thread thread_sources;
	bool scanning_changes = false;
	SafeFlag scanning_changes_done;

	static void _thread_func_sources(void *_userdata);

	List<String> sources_changed;
	List<ItemAction> scan_actions;

	bool _update_scan_actions();

	void _update_extensions();

	Error _reimport_file(const String &p_file, const HashMap<StringName, Variant> &p_custom_options = HashMap<StringName, Variant>(), const String &p_custom_importer = String(), Variant *generator_parameters = nullptr, bool p_update_file_system = true);
	Error _reimport_group(const String &p_group_file, const Vector<String> &p_files);

	bool _test_for_reimport(const String &p_path, const String &p_expected_import_md5);
	bool _is_test_for_reimport_needed(const String &p_path, uint64_t p_last_modification_time, uint64_t p_modification_time, uint64_t p_last_import_modification_time, uint64_t p_import_modification_time, const Vector<String> &p_import_dest_paths);
	Vector<String> _get_import_dest_paths(const String &p_path);

	bool reimport_on_missing_imported_files;

	Vector<String> _get_dependencies(const String &p_path);

	struct ImportFile {
		String path;
		String importer;
		bool threaded = false;
		int order = 0;
		bool operator<(const ImportFile &p_if) const {
			return order == p_if.order ? (importer < p_if.importer) : (order < p_if.order);
		}
	};

	struct ScriptInfo {
		String type;
		String script_class_name;
		String script_class_extends;
		String script_class_icon_path;
	};

	Mutex update_script_mutex;
	HashMap<String, ScriptInfo> update_script_paths;
	HashSet<String> update_script_paths_documentation;
	void _queue_update_script_class(const String &p_path, const String &p_type, const String &p_script_class_name, const String &p_script_class_extends, const String &p_script_class_icon_path);
	void _update_script_classes();
	void _update_script_documentation();
	void _process_update_pending();
	void _process_removed_files(const HashSet<String> &p_processed_files);
	bool _should_reload_script(const String &p_path);

	Mutex update_scene_mutex;
	HashSet<String> update_scene_paths;
	void _queue_update_scene_groups(const String &p_path);
	void _update_scene_groups();
	void _update_pending_scene_groups();
	void _get_all_scenes(EditorFileSystemDirectory *p_dir, HashSet<String> &r_list);

	String _get_global_script_class(const String &p_type, const String &p_path, String *r_extends, String *r_icon_path) const;

	static Error _resource_import(const String &p_path);
	static Ref<Resource> _load_resource_on_startup(ResourceFormatImporter *p_importer, const String &p_path, Error *r_error, bool p_use_sub_threads, float *r_progress, ResourceFormatLoader::CacheMode p_cache_mode);

	bool using_fat32_or_exfat; // Workaround for projects in FAT32 or exFAT filesystem (pendrives, most of the time)

	void _find_group_files(EditorFileSystemDirectory *efd, HashMap<String, Vector<String>> &group_files, HashSet<String> &groups_to_reimport);

	void _move_group_files(EditorFileSystemDirectory *efd, const String &p_group_file, const String &p_new_location);

	HashSet<String> group_file_cache;
	HashMap<String, String> file_icon_cache;

	struct CopiedFile {
		String from;
		String to;
	};

	bool refresh_queued = false;
	HashSet<ObjectID> folders_to_sort;

	Error _copy_file(const String &p_from, const String &p_to);
	bool _copy_directory(const String &p_from, const String &p_to, List<CopiedFile> *p_files);
	void _queue_refresh_filesystem();
	void _refresh_filesystem();

	struct ImportThreadData {
		const ImportFile *reimport_files;
		int reimport_from;
		SafeNumeric<int> max_index;
	};

	void _reimport_thread(uint32_t p_index, ImportThreadData *p_import_data);

	static ResourceUID::ID _resource_saver_get_resource_id_for_path(const String &p_path, bool p_generate);

	bool _scan_extensions();
	bool _scan_import_support(const Vector<String> &reimports);

	Vector<Ref<EditorFileSystemImportFormatSupportQuery>> import_support_queries;

	void _update_file_icon_path(EditorFileSystemDirectory::FileInfo *file_info);
	void _update_files_icon_path(EditorFileSystemDirectory *edp = nullptr);
	void _remove_invalid_global_class_names(const HashSet<String> &p_existing_class_names);
	String _get_file_by_class_name(EditorFileSystemDirectory *p_dir, const String &p_class_name, EditorFileSystemDirectory::FileInfo *&r_file_info);

	void _register_global_class_script(const String &p_search_path, const String &p_target_path, const String &p_type, const String &p_script_class_name, const String &p_script_class_extends, const String &p_script_class_icon_path);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	static EditorFileSystem *get_singleton() { return singleton; }

	EditorFileSystemDirectory *get_filesystem();
	bool is_scanning() const;
	bool is_importing() const { return importing; }
	bool doing_first_scan() const { return first_scan; }
	float get_scanning_progress() const;
	void scan();
	void scan_changes();
	void update_file(const String &p_file);
	void update_files(const Vector<String> &p_script_paths);
	HashSet<String> get_valid_extensions() const;
	void register_global_class_script(const String &p_search_path, const String &p_target_path);

	EditorFileSystemDirectory *get_filesystem_path(const String &p_path);
	String get_file_type(const String &p_file) const;
	EditorFileSystemDirectory *find_file(const String &p_file, int *r_index) const;

	void reimport_files(const Vector<String> &p_files);
	Error reimport_append(const String &p_file, const HashMap<StringName, Variant> &p_custom_options, const String &p_custom_importer, Variant p_generator_parameters);

	void reimport_file_with_custom_parameters(const String &p_file, const String &p_importer, const HashMap<StringName, Variant> &p_custom_params);

	bool is_group_file(const String &p_path) const;
	void move_group_file(const String &p_path, const String &p_new_path);

	Error make_dir_recursive(const String &p_path, const String &p_base_path = String());
	Error copy_file(const String &p_from, const String &p_to);
	Error copy_directory(const String &p_from, const String &p_to);

	static bool _should_skip_directory(const String &p_path);

	void add_import_format_support_query(Ref<EditorFileSystemImportFormatSupportQuery> p_query);
	void remove_import_format_support_query(Ref<EditorFileSystemImportFormatSupportQuery> p_query);
	EditorFileSystem();
	~EditorFileSystem();
};

#endif // EDITOR_FILE_SYSTEM_H

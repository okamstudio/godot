/**************************************************************************/
/*  dir_access.cpp                                                        */
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

#include "dir_access.h"

#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "core/templates/local_vector.h"

thread_local Error DirAccess::last_dir_open_error = OK;

String DirAccess::_get_root_path() const {
	switch (_access_type) {
		case ACCESS_RESOURCES:
			return ProjectSettings::get_singleton()->get_resource_path();
		case ACCESS_USERDATA:
			return OS::get_singleton()->get_user_data_dir();
		default:
			return "";
	}
}

String DirAccess::_get_root_string() const {
	switch (_access_type) {
		case ACCESS_RESOURCES:
			return "res://";
		case ACCESS_USERDATA:
			return "user://";
		default:
			return "";
	}
}

int DirAccess::get_current_drive() {
	String path = get_current_dir().to_lower();
	for (int i = 0; i < get_drive_count(); i++) {
		String d = get_drive(i).to_lower();
		if (path.begins_with(d)) {
			return i;
		}
	}

	return 0;
}

bool DirAccess::drives_are_shortcuts() {
	return false;
}

static Error _erase_recursive(DirAccess *da) {
	List<String> dirs;
	List<String> files;

	da->list_dir_begin();
	String n = da->get_next();
	while (!n.is_empty()) {
		if (n != "." && n != "..") {
			if (da->current_is_dir() && !da->is_link(n)) {
				dirs.push_back(n);
			} else {
				files.push_back(n);
			}
		}

		n = da->get_next();
	}

	da->list_dir_end();

	for (const String &E : dirs) {
		Error err = da->change_dir(E);
		if (err == OK) {
			err = _erase_recursive(da);
			if (err) {
				da->change_dir("..");
				return err;
			}
			err = da->change_dir("..");
			if (err) {
				return err;
			}
			err = da->remove(da->get_current_dir().path_join(E));
			if (err) {
				return err;
			}
		} else {
			return err;
		}
	}

	for (const String &E : files) {
		Error err = da->remove(da->get_current_dir().path_join(E));
		if (err) {
			return err;
		}
	}

	return OK;
}

Error DirAccess::erase_contents_recursive() {
	return _erase_recursive(this);
}

Error DirAccess::make_dir_recursive(const String &p_dir) {
	if (p_dir.length() < 1) {
		return OK;
	}

	String full_dir;

	if (p_dir.is_relative_path()) {
		//append current
		full_dir = get_current_dir().path_join(p_dir);

	} else {
		full_dir = p_dir;
	}

	full_dir = full_dir.replace("\\", "/");

	String base;

	if (full_dir.begins_with("res://")) {
		base = "res://";
	} else if (full_dir.begins_with("user://")) {
		base = "user://";
	} else if (full_dir.is_network_share_path()) {
		int pos = full_dir.find_char('/', 2);
		ERR_FAIL_COND_V(pos < 0, ERR_INVALID_PARAMETER);
		pos = full_dir.find_char('/', pos + 1);
		ERR_FAIL_COND_V(pos < 0, ERR_INVALID_PARAMETER);
		base = full_dir.substr(0, pos + 1);
	} else if (full_dir.begins_with("/")) {
		base = "/";
	} else if (full_dir.contains(":/")) {
		base = full_dir.substr(0, full_dir.find(":/") + 2);
	} else {
		ERR_FAIL_V(ERR_INVALID_PARAMETER);
	}

	full_dir = full_dir.replace_first(base, "").simplify_path();

	Vector<String> subdirs = full_dir.split("/");

	String curpath = base;
	for (int i = 0; i < subdirs.size(); i++) {
		curpath = curpath.path_join(subdirs[i]);
		Error err = make_dir(curpath);
		if (err != OK && err != ERR_ALREADY_EXISTS) {
			ERR_FAIL_V_MSG(err, vformat("Could not create directory: '%s'.", curpath));
		}
	}

	return OK;
}

DirAccess::AccessType DirAccess::get_access_type() const {
	return _access_type;
}

String DirAccess::fix_path(const String &p_path) const {
	switch (_access_type) {
		case ACCESS_RESOURCES: {
			if (ProjectSettings::get_singleton()) {
				if (p_path.begins_with("res://")) {
					String resource_path = ProjectSettings::get_singleton()->get_resource_path();
					if (!resource_path.is_empty()) {
						return p_path.replace_first("res:/", resource_path);
					}
					return p_path.replace_first("res://", "");
				}
			}

		} break;
		case ACCESS_USERDATA: {
			if (p_path.begins_with("user://")) {
				String data_dir = OS::get_singleton()->get_user_data_dir();
				if (!data_dir.is_empty()) {
					return p_path.replace_first("user:/", data_dir);
				}
				return p_path.replace_first("user://", "");
			}

		} break;
		case ACCESS_FILESYSTEM: {
			return p_path;
		} break;
		case ACCESS_MAX:
			break; // Can't happen, but silences warning
	}

	return p_path;
}

DirAccess::CreateFunc DirAccess::create_func[ACCESS_MAX] = { nullptr, nullptr, nullptr };

Ref<DirAccess> DirAccess::create_for_path(const String &p_path) {
	Ref<DirAccess> da;
	if (p_path.begins_with("res://")) {
		da = create(ACCESS_RESOURCES);
	} else if (p_path.begins_with("user://")) {
		da = create(ACCESS_USERDATA);
	} else {
		da = create(ACCESS_FILESYSTEM);
	}

	return da;
}

Ref<DirAccess> DirAccess::open(const String &p_path, Error *r_error) {
	Ref<DirAccess> da = create_for_path(p_path);
	ERR_FAIL_COND_V_MSG(da.is_null(), nullptr, vformat("Cannot create DirAccess for path '%s'.", p_path));
	Error err = da->change_dir(p_path);
	if (r_error) {
		*r_error = err;
	}
	if (err != OK) {
		return nullptr;
	}

	return da;
}

Ref<DirAccess> DirAccess::_open(const String &p_path) {
	Error err = OK;
	Ref<DirAccess> da = open(p_path, &err);
	last_dir_open_error = err;
	if (err) {
		return Ref<DirAccess>();
	}
	return da;
}

int DirAccess::_get_drive_count() {
	Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	return d->get_drive_count();
}

String DirAccess::get_drive_name(int p_idx) {
	Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	return d->get_drive(p_idx);
}

Error DirAccess::make_dir_absolute(const String &p_dir) {
	Ref<DirAccess> d = DirAccess::create_for_path(p_dir);
	return d->make_dir(p_dir);
}

Error DirAccess::make_dir_recursive_absolute(const String &p_dir) {
	Ref<DirAccess> d = DirAccess::create_for_path(p_dir);
	return d->make_dir_recursive(p_dir);
}

bool DirAccess::dir_exists_absolute(const String &p_dir) {
	Ref<DirAccess> d = DirAccess::create_for_path(p_dir);
	return d->dir_exists(p_dir);
}

Error DirAccess::copy_absolute(const String &p_from, const String &p_to, int p_chmod_flags) {
	Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	// Support copying from res:// to user:// etc.
	String from = ProjectSettings::get_singleton()->globalize_path(p_from);
	String to = ProjectSettings::get_singleton()->globalize_path(p_to);
	return d->copy(from, to, p_chmod_flags);
}

Error DirAccess::rename_absolute(const String &p_from, const String &p_to) {
	Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	String from = ProjectSettings::get_singleton()->globalize_path(p_from);
	String to = ProjectSettings::get_singleton()->globalize_path(p_to);
	return d->rename(from, to);
}

Error DirAccess::remove_absolute(const String &p_path) {
	Ref<DirAccess> d = DirAccess::create_for_path(p_path);
	return d->remove(p_path);
}

Ref<DirAccess> DirAccess::create(AccessType p_access) {
	Ref<DirAccess> da = create_func[p_access] ? create_func[p_access]() : nullptr;
	if (da.is_valid()) {
		da->_access_type = p_access;

		// for ACCESS_RESOURCES and ACCESS_FILESYSTEM, current_dir already defaults to where game was started
		// in case current directory is force changed elsewhere for ACCESS_RESOURCES
		if (p_access == ACCESS_RESOURCES) {
			da->change_dir("res://");
		} else if (p_access == ACCESS_USERDATA) {
			da->change_dir("user://");
		}
	}

	return da;
}

Ref<DirAccess> DirAccess::create_temp(const String &p_prefix, bool p_keep, Error *r_error) {
	const String ERROR_COMMON_PREFIX = "Error while creating temporary directory";

	if (!p_prefix.is_valid_filename()) {
		*r_error = ERR_FILE_BAD_PATH;
		ERR_FAIL_V_MSG(Ref<FileAccess>(), vformat(R"(%s: "%s" is not a valid prefix.)", ERROR_COMMON_PREFIX, p_prefix));
	}

	Ref<DirAccess> dir_access = DirAccess::open(OS::get_singleton()->get_temp_path());

	uint32_t suffix_i = 0;
	String path;
	while (true) {
		String datetime = Time::get_singleton()->get_datetime_string_from_system().remove_chars("-T:");
		datetime += itos(Time::get_singleton()->get_ticks_usec());
		String suffix = datetime + (suffix_i > 0 ? itos(suffix_i) : "");
		path = (p_prefix.is_empty() ? "" : p_prefix + "-") + suffix;
		if (!path.is_valid_filename()) {
			*r_error = ERR_FILE_BAD_PATH;
			return Ref<DirAccess>();
		}
		if (!DirAccess::exists(path)) {
			break;
		}
		suffix_i += 1;
	}

	Error err = dir_access->make_dir(path);
	if (err != OK) {
		*r_error = err;
		ERR_FAIL_V_MSG(Ref<FileAccess>(), vformat(R"(%s: "%s" couldn't create directory "%s".)", ERROR_COMMON_PREFIX, path));
	}
	err = dir_access->change_dir(path);
	if (err != OK) {
		*r_error = err;
		return Ref<DirAccess>();
	}

	dir_access->_is_temp = true;
	dir_access->_temp_keep_after_free = p_keep;
	dir_access->_temp_path = dir_access->get_current_dir();

	*r_error = OK;
	return dir_access;
}

Ref<DirAccess> DirAccess::_create_temp(const String &p_prefix, bool p_keep) {
	return create_temp(p_prefix, p_keep, &last_dir_open_error);
}

void DirAccess::_delete_temp() {
	if (!_is_temp || _temp_keep_after_free) {
		return;
	}

	if (!DirAccess::exists(_temp_path)) {
		return;
	}

	Error err;
	{
		Ref<DirAccess> dir_access = DirAccess::open(_temp_path, &err);
		if (err != OK) {
			return;
		}
		err = dir_access->erase_contents_recursive();
		if (err != OK) {
			return;
		}
	}

	DirAccess::remove_absolute(_temp_path);
}

Error DirAccess::get_open_error() {
	return last_dir_open_error;
}

String DirAccess::get_full_path(const String &p_path, AccessType p_access) {
	Ref<DirAccess> d = DirAccess::create(p_access);
	if (d.is_null()) {
		return p_path;
	}

	d->change_dir(p_path);
	String full = d->get_current_dir();
	return full;
}

Error DirAccess::copy(const String &p_from, const String &p_to, int p_chmod_flags) {
	ERR_FAIL_COND_V_MSG(p_from == p_to, ERR_INVALID_PARAMETER, "Source and destination path are equal.");

	//printf("copy %s -> %s\n",p_from.ascii().get_data(),p_to.ascii().get_data());
	Error err;
	{
		Ref<FileAccess> fsrc = FileAccess::open(p_from, FileAccess::READ, &err);
		ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Failed to open '%s'.", p_from));

		Ref<FileAccess> fdst = FileAccess::open(p_to, FileAccess::WRITE, &err);
		ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Failed to open '%s'.", p_to));

		const size_t copy_buffer_limit = 65536; // 64 KB

		fsrc->seek_end(0);
		uint64_t size = fsrc->get_position();
		fsrc->seek(0);
		err = OK;
		size_t buffer_size = MIN(size * sizeof(uint8_t), copy_buffer_limit);
		LocalVector<uint8_t> buffer;
		buffer.resize(buffer_size);
		while (size > 0) {
			if (fsrc->get_error() != OK) {
				err = fsrc->get_error();
				break;
			}
			if (fdst->get_error() != OK) {
				err = fdst->get_error();
				break;
			}

			int bytes_read = fsrc->get_buffer(buffer.ptr(), buffer_size);
			if (bytes_read <= 0) {
				err = FAILED;
				break;
			}
			fdst->store_buffer(buffer.ptr(), bytes_read);

			size -= bytes_read;
		}
	}

	if (err == OK && p_chmod_flags != -1) {
		err = FileAccess::set_unix_permissions(p_to, p_chmod_flags);
		// If running on a platform with no chmod support (i.e., Windows), don't fail
		if (err == ERR_UNAVAILABLE) {
			err = OK;
		}
	}

	return err;
}

// Changes dir for the current scope, returning back to the original dir
// when scope exits
class DirChanger {
	DirAccess *da;
	String original_dir;

public:
	DirChanger(DirAccess *p_da, const String &p_dir) :
			da(p_da),
			original_dir(p_da->get_current_dir()) {
		p_da->change_dir(p_dir);
	}

	~DirChanger() {
		da->change_dir(original_dir);
	}
};

Error DirAccess::_copy_dir(Ref<DirAccess> &p_target_da, const String &p_to, int p_chmod_flags, bool p_copy_links) {
	List<String> dirs;

	String curdir = get_current_dir();
	list_dir_begin();
	String n = get_next();
	while (!n.is_empty()) {
		if (n != "." && n != "..") {
			if (p_copy_links && is_link(get_current_dir().path_join(n))) {
				create_link(read_link(get_current_dir().path_join(n)), p_to + n);
			} else if (current_is_dir()) {
				dirs.push_back(n);
			} else {
				const String &rel_path = n;
				if (!n.is_relative_path()) {
					list_dir_end();
					return ERR_BUG;
				}
				Error err = copy(get_current_dir().path_join(n), p_to + rel_path, p_chmod_flags);
				if (err) {
					list_dir_end();
					return err;
				}
			}
		}

		n = get_next();
	}

	list_dir_end();

	for (const String &rel_path : dirs) {
		String target_dir = p_to + rel_path;
		if (!p_target_da->dir_exists(target_dir)) {
			Error err = p_target_da->make_dir(target_dir);
			ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Cannot create directory '%s'.", target_dir));
		}

		Error err = change_dir(rel_path);
		ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Cannot change current directory to '%s'.", rel_path));

		err = _copy_dir(p_target_da, p_to + rel_path + "/", p_chmod_flags, p_copy_links);
		if (err) {
			change_dir("..");
			ERR_FAIL_V_MSG(err, "Failed to copy recursively.");
		}
		err = change_dir("..");
		ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to go back.");
	}

	return OK;
}

Error DirAccess::copy_dir(const String &p_from, String p_to, int p_chmod_flags, bool p_copy_links) {
	ERR_FAIL_COND_V_MSG(!dir_exists(p_from), ERR_FILE_NOT_FOUND, "Source directory doesn't exist.");

	Ref<DirAccess> target_da = DirAccess::create_for_path(p_to);
	ERR_FAIL_COND_V_MSG(target_da.is_null(), ERR_CANT_CREATE, vformat("Cannot create DirAccess for path '%s'.", p_to));

	if (!target_da->dir_exists(p_to)) {
		Error err = target_da->make_dir_recursive(p_to);
		ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Cannot create directory '%s'.", p_to));
	}

	if (!p_to.ends_with("/")) {
		p_to = p_to + "/";
	}

	DirChanger dir_changer(this, p_from);
	Error err = _copy_dir(target_da, p_to, p_chmod_flags, p_copy_links);

	return err;
}

bool DirAccess::exists(const String &p_dir) {
	Ref<DirAccess> da = DirAccess::create_for_path(p_dir);
	return da->change_dir(p_dir) == OK;
}

PackedStringArray DirAccess::get_files() {
	return _get_contents(false);
}

PackedStringArray DirAccess::get_files_at(const String &p_path) {
	Ref<DirAccess> da = DirAccess::open(p_path);
	ERR_FAIL_COND_V_MSG(da.is_null(), PackedStringArray(), vformat("Couldn't open directory at path \"%s\".", p_path));
	return da->get_files();
}

PackedStringArray DirAccess::get_directories() {
	return _get_contents(true);
}

PackedStringArray DirAccess::get_directories_at(const String &p_path) {
	Ref<DirAccess> da = DirAccess::open(p_path);
	ERR_FAIL_COND_V_MSG(da.is_null(), PackedStringArray(), vformat("Couldn't open directory at path \"%s\".", p_path));
	return da->get_directories();
}

PackedStringArray DirAccess::_get_contents(bool p_directories) {
	PackedStringArray ret;

	list_dir_begin();
	String s = _get_next();
	while (!s.is_empty()) {
		if (current_is_dir() == p_directories) {
			ret.append(s);
		}
		s = _get_next();
	}

	ret.sort();
	return ret;
}

String DirAccess::_get_next() {
	String next = get_next();
	while (!next.is_empty() && ((!include_navigational && (next == "." || next == "..")) || (!include_hidden && current_is_hidden()))) {
		next = get_next();
	}
	return next;
}

void DirAccess::set_include_navigational(bool p_enable) {
	include_navigational = p_enable;
}

bool DirAccess::get_include_navigational() const {
	return include_navigational;
}

void DirAccess::set_include_hidden(bool p_enable) {
	include_hidden = p_enable;
}

bool DirAccess::get_include_hidden() const {
	return include_hidden;
}

bool DirAccess::is_case_sensitive(const String &p_path) const {
	return true;
}

void DirAccess::_bind_methods() {
	ClassDB::bind_static_method("DirAccess", D_METHOD("open", "path"), &DirAccess::_open);
	ClassDB::bind_static_method("DirAccess", D_METHOD("get_open_error"), &DirAccess::get_open_error);
	ClassDB::bind_static_method("DirAccess", D_METHOD("create_temp", "prefix", "keep"), &DirAccess::_create_temp, DEFVAL(""), DEFVAL(false));

	ClassDB::bind_method(D_METHOD("list_dir_begin"), &DirAccess::list_dir_begin);
	ClassDB::bind_method(D_METHOD("get_next"), &DirAccess::_get_next);
	ClassDB::bind_method(D_METHOD("current_is_dir"), &DirAccess::current_is_dir);
	ClassDB::bind_method(D_METHOD("list_dir_end"), &DirAccess::list_dir_end);
	ClassDB::bind_method(D_METHOD("get_files"), &DirAccess::get_files);
	ClassDB::bind_static_method("DirAccess", D_METHOD("get_files_at", "path"), &DirAccess::get_files_at);
	ClassDB::bind_method(D_METHOD("get_directories"), &DirAccess::get_directories);
	ClassDB::bind_static_method("DirAccess", D_METHOD("get_directories_at", "path"), &DirAccess::get_directories_at);
	ClassDB::bind_static_method("DirAccess", D_METHOD("get_drive_count"), &DirAccess::_get_drive_count);
	ClassDB::bind_static_method("DirAccess", D_METHOD("get_drive_name", "idx"), &DirAccess::get_drive_name);
	ClassDB::bind_method(D_METHOD("get_current_drive"), &DirAccess::get_current_drive);
	ClassDB::bind_method(D_METHOD("change_dir", "to_dir"), &DirAccess::change_dir);
	ClassDB::bind_method(D_METHOD("get_current_dir", "include_drive"), &DirAccess::get_current_dir, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("make_dir", "path"), &DirAccess::make_dir);
	ClassDB::bind_static_method("DirAccess", D_METHOD("make_dir_absolute", "path"), &DirAccess::make_dir_absolute);
	ClassDB::bind_method(D_METHOD("make_dir_recursive", "path"), &DirAccess::make_dir_recursive);
	ClassDB::bind_static_method("DirAccess", D_METHOD("make_dir_recursive_absolute", "path"), &DirAccess::make_dir_recursive_absolute);
	ClassDB::bind_method(D_METHOD("file_exists", "path"), &DirAccess::file_exists);
	ClassDB::bind_method(D_METHOD("dir_exists", "path"), &DirAccess::dir_exists);
	ClassDB::bind_static_method("DirAccess", D_METHOD("dir_exists_absolute", "path"), &DirAccess::dir_exists_absolute);
	ClassDB::bind_method(D_METHOD("get_space_left"), &DirAccess::get_space_left);
	ClassDB::bind_method(D_METHOD("copy", "from", "to", "chmod_flags"), &DirAccess::copy, DEFVAL(-1));
	ClassDB::bind_static_method("DirAccess", D_METHOD("copy_absolute", "from", "to", "chmod_flags"), &DirAccess::copy_absolute, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("rename", "from", "to"), &DirAccess::rename);
	ClassDB::bind_static_method("DirAccess", D_METHOD("rename_absolute", "from", "to"), &DirAccess::rename_absolute);
	ClassDB::bind_method(D_METHOD("remove", "path"), &DirAccess::remove);
	ClassDB::bind_static_method("DirAccess", D_METHOD("remove_absolute", "path"), &DirAccess::remove_absolute);

	ClassDB::bind_method(D_METHOD("is_link", "path"), &DirAccess::is_link);
	ClassDB::bind_method(D_METHOD("read_link", "path"), &DirAccess::read_link);
	ClassDB::bind_method(D_METHOD("create_link", "source", "target"), &DirAccess::create_link);

	ClassDB::bind_method(D_METHOD("is_bundle", "path"), &DirAccess::is_bundle);

	ClassDB::bind_method(D_METHOD("set_include_navigational", "enable"), &DirAccess::set_include_navigational);
	ClassDB::bind_method(D_METHOD("get_include_navigational"), &DirAccess::get_include_navigational);
	ClassDB::bind_method(D_METHOD("set_include_hidden", "enable"), &DirAccess::set_include_hidden);
	ClassDB::bind_method(D_METHOD("get_include_hidden"), &DirAccess::get_include_hidden);

	ClassDB::bind_method(D_METHOD("is_case_sensitive", "path"), &DirAccess::is_case_sensitive);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "include_navigational"), "set_include_navigational", "get_include_navigational");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "include_hidden"), "set_include_hidden", "get_include_hidden");
}

DirAccess::~DirAccess() {
	_delete_temp();
}

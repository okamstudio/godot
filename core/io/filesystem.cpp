/**************************************************************************/
/*  filesystem.cpp                                                        */
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

#include "filesystem.h"
#include "filesystem_protocol_resources.h"
#include "filesystem_protocol_uid.h"
#include "filesystem_protocol_user.h"

FileSystem *FileSystem::singleton = nullptr;
FileSystem *FileSystem::get_singleton() {
	return singleton;
}
FileSystem::FileSystem() {
	singleton = this;

	underlying_protocol_name_for_resources = protocol_name_os;
	underlying_protocol_name_for_user = protocol_name_os;
}
FileSystem::~FileSystem() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

const String FileSystem::protocol_name_os = "os";
const String FileSystem::protocol_name_pipe = "pipe";
const String FileSystem::protocol_name_resources = "res";
const String FileSystem::protocol_name_user = "user";
const String FileSystem::protocol_name_uid = "uid";
const String FileSystem::protocol_name_gdscript = "gdscript";
const String FileSystem::protocol_name_memory = "mem";

void FileSystem::register_protocols() {
	// Register user://
	Ref<FileSystemProtocol> protocol_os_for_user = get_protocol_or_null(underlying_protocol_name_for_user);
	ERR_FAIL_COND_MSG(protocol_os_for_user.is_null(), "Cannot get " + underlying_protocol_name_for_user + ":// protocol handler! Make sure you have registered it in OS_XXX::initialize_filesystem()!");

	Ref<FileSystemProtocolUser> protocol_user = Ref<FileSystemProtocolUser>();
	protocol_user.instantiate(protocol_os_for_user);
	add_protocol(protocol_name_user, protocol_user);

	// Register res://
	Ref<FileSystemProtocol> protocol_os_for_res = get_protocol_or_null(underlying_protocol_name_for_resources);
	ERR_FAIL_COND_MSG(protocol_os_for_res.is_null(), "Cannot get " + underlying_protocol_name_for_resources + ":// protocol handler! Make sure you have registered it in OS_XXX::initialize_filesystem()!");

	Ref<FileSystemProtocolResources> protocol_resources = Ref<FileSystemProtocolResources>();
	protocol_resources.instantiate(protocol_os_for_res);
	add_protocol(protocol_name_resources, protocol_resources);

	// Register uid://
	Ref<FileSystemProtocolUID> protocol_uid = Ref<FileSystemProtocolUID>();
	protocol_uid.instantiate(protocol_resources);
	add_protocol(protocol_name_uid, protocol_uid);

	// gdscript:// represents a script instance in memory.
	// We reserve it and make it a placeholder which fails all accesses silently
	// to prevent stepping in the old code.
	Ref<FileSystemProtocol> protocol_gdscript = Ref<FileSystemProtocol>();
	protocol_gdscript.instantiate();
	add_protocol(protocol_name_gdscript, protocol_gdscript);
}

bool FileSystem::has_protocol(const String &p_name) const {
	_THREAD_SAFE_METHOD_

	return protocols.has(p_name);
}
bool FileSystem::remove_protocol(const String &p_name) {
	_THREAD_SAFE_METHOD_

	bool erased = protocols.erase(p_name);
	ERR_FAIL_COND_V_MSG(!erased, false, "No FileSystemProtocol with the name " + p_name + " is registered.");
	return erased;
}
bool FileSystem::add_protocol(const String &p_name, const Ref<FileSystemProtocol> &p_protocol) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V_MSG(protocols.has(p_name), false, "FileSystemProtocol with the name " + p_name + " is already registered.");
	protocols[p_name] = p_protocol;
	return true;
}
Ref<FileSystemProtocol> FileSystem::get_protocol_or_null(const String &p_name) const {
	_THREAD_SAFE_METHOD_

	HashMap<String, Ref<FileSystemProtocol>>::ConstIterator iter = protocols.find(p_name);
	if (!iter) {
		return Ref<FileSystemProtocol>();
	}
	return iter->value;
}
Ref<FileSystemProtocol> FileSystem::get_protocol(const String &p_name) const {
	Ref<FileSystemProtocol> protocol = get_protocol_or_null(p_name);
	ERR_FAIL_COND_V_MSG(protocol.is_null(), Ref<FileSystemProtocol>(), "FileSystemProtocol with name " + p_name + " doesn't exist!");
	return protocol;
}

bool FileSystem::try_find_protocol_in_path(const String &p_path, int *r_protocol_name_end, int *r_file_path_start) {
	int index = p_path.find("://");
	if (index < 0) {
		return false;
	}

	for (int i = 0; i < index; i++) {
		if (!is_ascii_protocol_name_char(p_path[i])) {
			return false;
		}
	}

	if (r_protocol_name_end) {
		*r_protocol_name_end = index;
	}
	if (r_file_path_start) {
		*r_file_path_start = index + 3;
	}
	return true;
}

String FileSystem::fix_path(const String &p_path) {
	String r_path = p_path.replace("\\", "/");

	return r_path;
}

bool FileSystem::split_path(const String &p_path, String *r_protocol_name, String *r_file_path) {
	int protocol_name_end;
	int file_path_start;
	if (!try_find_protocol_in_path(p_path, &protocol_name_end, &file_path_start)) {
		if (r_protocol_name != nullptr) {
			*r_protocol_name = "";
		}
		if (r_file_path != nullptr) {
			*r_file_path = p_path;
		}
		return false;
	}

	if (r_protocol_name != nullptr) {
		*r_protocol_name = p_path.substr(0, protocol_name_end);
	}
	if (r_file_path != nullptr) {
		*r_file_path = p_path.substr(file_path_start);
	}
	return true;
}

void FileSystem::process_path(const String &p_path, String *r_protocol_name, Ref<FileSystemProtocol> *r_protocol, String *r_file_path) const {
	// protocol name is needed for fetching protocol objects
	String protocol_name = "";
	bool has_protcol_part = split_path(p_path, &protocol_name, r_file_path);
	if (!has_protcol_part) {
		protocol_name = protocol_name_os;
	}
	if (r_protocol_name != nullptr) {
		*r_protocol_name = protocol_name;
	}
	if (r_protocol != nullptr) {
		*r_protocol = get_protocol_or_null(protocol_name);
	}
	return;
}

String FileSystem::globalize_path(const String &p_path) const {
	String protocol_name = String();
	Ref<FileSystemProtocol> protocol = Ref<FileSystemProtocol>();
	String file_path = String();
	process_path(p_path, &protocol_name, &protocol, &file_path);
	ERR_FAIL_COND_V_MSG(protocol.is_null(), String(), "Unknown filesystem protocol " + protocol_name);

	return protocol->globalize_path(file_path);
}

String FileSystem::globalize_path_or_fallback(const String &p_path) const {
	String protocol_name = String();
	Ref<FileSystemProtocol> protocol = Ref<FileSystemProtocol>();
	String file_path = String();
	process_path(p_path, &protocol_name, &protocol, &file_path);
	ERR_FAIL_COND_V_MSG(protocol.is_null(), file_path, "Unknown filesystem protocol " + protocol_name);

	String result = protocol->globalize_path(file_path);
	if (result.is_empty()) {
		return file_path;
	}

	return result;
}

Ref<FileAccess> FileSystem::open_file(const String &p_path, int p_mode_flags, Error *r_error) const {
	String protocol_name = String();
	Ref<FileSystemProtocol> protocol = Ref<FileSystemProtocol>();
	String file_path = String();
	process_path(p_path, &protocol_name, &protocol, &file_path);
	ERR_FAIL_COND_V_MSG(protocol.is_null(), Ref<FileAccess>(), "Unknown filesystem protocol " + protocol_name);

	Error err;
	Ref<FileAccess> file = protocol->open_file(file_path, p_mode_flags, err);
	if (file.is_valid()) {
		protocol->disguise_file(file, protocol_name, file_path);
	}
	if (r_error) {
		*r_error = err;
	}
	return file;
}
bool FileSystem::file_exists(const String &p_path) const {
	String protocol_name = String();
	Ref<FileSystemProtocol> protocol = Ref<FileSystemProtocol>();
	String file_path = String();
	process_path(p_path, &protocol_name, &protocol, &file_path);
	ERR_FAIL_COND_V_MSG(protocol.is_null(), false, "Unknown filesystem protocol " + protocol_name);

	return protocol->file_exists(file_path);
}

Error FileSystem::get_open_error() const {
	return open_error;
}
Ref<FileAccess> FileSystem::_open_file(const String &p_path, int p_mode_flags) {
	return open_file(p_path, p_mode_flags, &open_error);
}

uint64_t FileSystem::get_modified_time(const String &p_path) const {
	String protocol_name = String();
	Ref<FileSystemProtocol> protocol = Ref<FileSystemProtocol>();
	String file_path = String();
	process_path(p_path, &protocol_name, &protocol, &file_path);
	ERR_FAIL_COND_V_MSG(protocol.is_null(), 0, "Unknown filesystem protocol " + protocol_name);

	return protocol->get_modified_time(file_path);
}
BitField<FileAccess::UnixPermissionFlags> FileSystem::get_unix_permissions(const String &p_path) const {
	String protocol_name = String();
	Ref<FileSystemProtocol> protocol = Ref<FileSystemProtocol>();
	String file_path = String();
	process_path(p_path, &protocol_name, &protocol, &file_path);
	ERR_FAIL_COND_V_MSG(protocol.is_null(), 0, "Unknown filesystem protocol " + protocol_name);

	return protocol->get_unix_permissions(file_path);
}
Error FileSystem::set_unix_permissions(const String &p_path, BitField<FileAccess::UnixPermissionFlags> p_permissions) const {
	String protocol_name = String();
	Ref<FileSystemProtocol> protocol = Ref<FileSystemProtocol>();
	String file_path = String();
	process_path(p_path, &protocol_name, &protocol, &file_path);
	ERR_FAIL_COND_V_MSG(protocol.is_null(), ERR_FILE_BAD_PATH, "Unknown filesystem protocol " + protocol_name);

	return protocol->set_unix_permissions(file_path, p_permissions);
}
bool FileSystem::get_hidden_attribute(const String &p_path) const {
	String protocol_name = String();
	Ref<FileSystemProtocol> protocol = Ref<FileSystemProtocol>();
	String file_path = String();
	process_path(p_path, &protocol_name, &protocol, &file_path);
	ERR_FAIL_COND_V_MSG(protocol.is_null(), false, "Unknown filesystem protocol " + protocol_name);

	return protocol->get_hidden_attribute(file_path);
}
Error FileSystem::set_hidden_attribute(const String &p_path, bool p_hidden) const {
	String protocol_name = String();
	Ref<FileSystemProtocol> protocol = Ref<FileSystemProtocol>();
	String file_path = String();
	process_path(p_path, &protocol_name, &protocol, &file_path);
	ERR_FAIL_COND_V_MSG(protocol.is_null(), ERR_FILE_BAD_PATH, "Unknown filesystem protocol " + protocol_name);

	return protocol->set_hidden_attribute(file_path, p_hidden);
}
bool FileSystem::get_read_only_attribute(const String &p_path) const {
	String protocol_name = String();
	Ref<FileSystemProtocol> protocol = Ref<FileSystemProtocol>();
	String file_path = String();
	process_path(p_path, &protocol_name, &protocol, &file_path);
	ERR_FAIL_COND_V_MSG(protocol.is_null(), false, "Unknown filesystem protocol " + protocol_name);

	return protocol->get_read_only_attribute(file_path);
}
Error FileSystem::set_read_only_attribute(const String &p_path, bool p_ro) const {
	String protocol_name = String();
	Ref<FileSystemProtocol> protocol = Ref<FileSystemProtocol>();
	String file_path = String();
	process_path(p_path, &protocol_name, &protocol, &file_path);
	ERR_FAIL_COND_V_MSG(protocol.is_null(), ERR_FILE_BAD_PATH, "Unknown filesystem protocol " + protocol_name);

	return protocol->set_read_only_attribute(file_path, p_ro);
}

void FileSystem::set_underlying_protocol_name_for_resources(const String &name) {
	underlying_protocol_name_for_resources = name;
}
void FileSystem::set_underlying_protocol_name_for_user(const String &name) {
	underlying_protocol_name_for_user = name;
}

void FileSystem::_bind_methods() {
	ClassDB::bind_method(D_METHOD("has_protocol", "name"), &FileSystem::has_protocol);
	ClassDB::bind_method(D_METHOD("remove_protocol", "name"), &FileSystem::remove_protocol);
	ClassDB::bind_method(D_METHOD("add_protocol", "name", "protocol"), &FileSystem::add_protocol);
	ClassDB::bind_method(D_METHOD("get_protocol_or_null", "name"), &FileSystem::get_protocol_or_null);
	ClassDB::bind_method(D_METHOD("get_protocol", "name"), &FileSystem::get_protocol);

	ClassDB::bind_method(D_METHOD("globalize_path", "path"), &FileSystem::globalize_path);

	ClassDB::bind_method(D_METHOD("get_open_error"), &FileSystem::get_open_error);
	ClassDB::bind_method(D_METHOD("open_file", "path", "mode_flags"), &FileSystem::_open_file);
	ClassDB::bind_method(D_METHOD("file_exists", "name"), &FileSystem::file_exists);

	ClassDB::bind_method(D_METHOD("get_modified_time", "path"), &FileSystem::get_modified_time);
	ClassDB::bind_method(D_METHOD("get_unix_permissions", "path"), &FileSystem::get_unix_permissions);
	ClassDB::bind_method(D_METHOD("set_unix_permissions", "path", "permissions"), &FileSystem::set_unix_permissions);
	ClassDB::bind_method(D_METHOD("get_hidden_attribute", "path"), &FileSystem::get_hidden_attribute);
	ClassDB::bind_method(D_METHOD("set_hidden_attribute", "path", "hidden"), &FileSystem::set_hidden_attribute);
	ClassDB::bind_method(D_METHOD("get_read_only_attribute", "path"), &FileSystem::get_read_only_attribute);
	ClassDB::bind_method(D_METHOD("set_read_only_attribute", "path", "ro"), &FileSystem::set_read_only_attribute);
}

/**************************************************************************/
/*  filesystem_protocol_resources.cpp                                     */
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

#include "filesystem_protocol_resources.h"
#include "core/config/project_settings.h"
#include "core/io/file_access_pack.h"
#include "core/io/filesystem.h"
#include "core/os/os.h"

FileSystemProtocolResources::FileSystemProtocolResources() {}
FileSystemProtocolResources::FileSystemProtocolResources(const Ref<FileSystemProtocol> &p_protocol_os) {
	set_protocol_os(p_protocol_os);
}
void FileSystemProtocolResources::set_protocol_os(const Ref<FileSystemProtocol> &p_protocol_os) {
	protocol_os = p_protocol_os;
}

String FileSystemProtocolResources::globalize_path(const String &p_path) const {
	String resource_path = ProjectSettings::get_singleton()->get_resource_path();
	if (!resource_path.is_empty()) {
		return resource_path + "/" + p_path;
	} else {
		return String();
	}
}
Ref<FileAccess> FileSystemProtocolResources::open_file(const String &p_path, int p_mode_flags, Error &r_error) const {
	// TODO: Replace this with resource mounting stack

	//try packed data first
	if (!(p_mode_flags & FileAccess::WRITE) && PackedData::get_singleton() && !PackedData::get_singleton()->is_disabled()) {
		Ref<FileAccess> file = PackedData::get_singleton()->try_open_path("res://" + p_path);
		if (file.is_valid()) {
			r_error = OK;
			return file;
		}
	}

	String path = globalize_path(p_path);
	return protocol_os->open_file(path, p_mode_flags, r_error);
}
bool FileSystemProtocolResources::file_exists(const String &p_path) const {
	// TODO: Replace this with resource mounting stack

	//try packed data first
	if (PackedData::get_singleton() && !PackedData::get_singleton()->is_disabled() && PackedData::get_singleton()->has_path("res://" + p_path)) {
		return true;
	}

	String path = globalize_path(p_path);
	return protocol_os->file_exists(path);
}

uint64_t FileSystemProtocolResources::get_modified_time(const String &p_path) const {
	if (PackedData::get_singleton() && !PackedData::get_singleton()->is_disabled() && (PackedData::get_singleton()->has_path(p_path) || PackedData::get_singleton()->has_directory(p_path))) {
		return 0;
	}

	String path = globalize_path(p_path);
	return protocol_os->get_modified_time(path);
}
BitField<FileAccess::UnixPermissionFlags> FileSystemProtocolResources::get_unix_permissions(const String &p_path) const {
	if (PackedData::get_singleton() && !PackedData::get_singleton()->is_disabled() && (PackedData::get_singleton()->has_path(p_path) || PackedData::get_singleton()->has_directory(p_path))) {
		return 0;
	}

	String path = globalize_path(p_path);
	return protocol_os->get_unix_permissions(path);
}
Error FileSystemProtocolResources::set_unix_permissions(const String &p_path, BitField<FileAccess::UnixPermissionFlags> p_permissions) const {
	if (PackedData::get_singleton() && !PackedData::get_singleton()->is_disabled() && (PackedData::get_singleton()->has_path(p_path) || PackedData::get_singleton()->has_directory(p_path))) {
		return ERR_UNAVAILABLE;
	}

	String path = globalize_path(p_path);
	return protocol_os->set_unix_permissions(path, p_permissions);
}
bool FileSystemProtocolResources::get_hidden_attribute(const String &p_path) const {
	if (PackedData::get_singleton() && !PackedData::get_singleton()->is_disabled() && (PackedData::get_singleton()->has_path(p_path) || PackedData::get_singleton()->has_directory(p_path))) {
		return false;
	}

	String path = globalize_path(p_path);
	return protocol_os->get_hidden_attribute(path);
}
Error FileSystemProtocolResources::set_hidden_attribute(const String &p_path, bool p_hidden) const {
	if (PackedData::get_singleton() && !PackedData::get_singleton()->is_disabled() && (PackedData::get_singleton()->has_path(p_path) || PackedData::get_singleton()->has_directory(p_path))) {
		return ERR_UNAVAILABLE;
	}

	String path = globalize_path(p_path);
	return protocol_os->set_hidden_attribute(path, p_hidden);
}
bool FileSystemProtocolResources::get_read_only_attribute(const String &p_path) const {
	if (PackedData::get_singleton() && !PackedData::get_singleton()->is_disabled() && (PackedData::get_singleton()->has_path(p_path) || PackedData::get_singleton()->has_directory(p_path))) {
		return false;
	}

	String path = globalize_path(p_path);
	return protocol_os->get_read_only_attribute(path);
}
Error FileSystemProtocolResources::set_read_only_attribute(const String &p_path, bool p_ro) const {
	if (PackedData::get_singleton() && !PackedData::get_singleton()->is_disabled() && (PackedData::get_singleton()->has_path(p_path) || PackedData::get_singleton()->has_directory(p_path))) {
		return ERR_UNAVAILABLE;
	}

	String path = globalize_path(p_path);
	return protocol_os->set_read_only_attribute(path, p_ro);
}

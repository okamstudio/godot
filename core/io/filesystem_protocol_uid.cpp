/**************************************************************************/
/*  filesystem_protocol_uid.cpp                                           */
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

#include "filesystem_protocol_uid.h"
#include "core/io/filesystem.h"
#include "core/os/os.h"

FileSystemProtocolUID::FileSystemProtocolUID() {}
FileSystemProtocolUID::FileSystemProtocolUID(const Ref<FileSystemProtocol> &p_protocol_res) {
	set_protocol_resources(p_protocol_res);
}

void FileSystemProtocolUID::set_protocol_resources(const Ref<FileSystemProtocol> &p_protocol_res) {
	this->protocol_res = p_protocol_res;
}

String FileSystemProtocolUID::get_next_path(const String &p_path) const {
	return ResourceUID::uid_to_path(p_path);
}

String FileSystemProtocolUID::globalize_path(const String &p_path) const {
	return protocol_res->globalize_path(get_next_path(p_path));
}
Ref<FileAccess> FileSystemProtocolUID::open_file(const String &p_path, int p_mode_flags, Error &r_error) const {
	String path = globalize_path(p_path);
	return protocol_res->open_file(path, p_mode_flags, r_error);
}
bool FileSystemProtocolUID::file_exists(const String &p_path) const {
	String path = globalize_path(p_path);
	return protocol_res->file_exists(path);
}

uint64_t FileSystemProtocolUID::get_modified_time(const String &p_path) const {
	String path = globalize_path(p_path);
	return protocol_res->get_modified_time(path);
}
BitField<FileAccess::UnixPermissionFlags> FileSystemProtocolUID::get_unix_permissions(const String &p_path) const {
	String path = globalize_path(p_path);
	return protocol_res->get_unix_permissions(path);
}
Error FileSystemProtocolUID::set_unix_permissions(const String &p_path, BitField<FileAccess::UnixPermissionFlags> p_permissions) const {
	String path = globalize_path(p_path);
	return protocol_res->set_unix_permissions(path, p_permissions);
}
bool FileSystemProtocolUID::get_hidden_attribute(const String &p_path) const {
	String path = globalize_path(p_path);
	return protocol_res->get_hidden_attribute(path);
}
Error FileSystemProtocolUID::set_hidden_attribute(const String &p_path, bool p_hidden) const {
	String path = globalize_path(p_path);
	return protocol_res->set_hidden_attribute(path, p_hidden);
}
bool FileSystemProtocolUID::get_read_only_attribute(const String &p_path) const {
	String path = globalize_path(p_path);
	return protocol_res->get_read_only_attribute(path);
}
Error FileSystemProtocolUID::set_read_only_attribute(const String &p_path, bool p_ro) const {
	String path = globalize_path(p_path);
	return protocol_res->set_read_only_attribute(path, p_ro);
}

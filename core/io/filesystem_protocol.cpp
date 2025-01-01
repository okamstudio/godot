/**************************************************************************/
/*  filesystem_protocol.cpp                                               */
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

#include "filesystem_protocol.h"

Error FileSystemProtocol::get_open_error() const {
	return open_error;
}
Ref<FileAccess> FileSystemProtocol::_open_file(const String &p_path, int p_mode_flags) {
	return open_file(p_path, p_mode_flags, open_error);
}

Ref<FileAccess> FileSystemProtocol::open_file(const String &p_path, int p_mode_flags, Error &r_error) const {
	r_error = ERR_FILE_NOT_FOUND;
	return Ref<FileAccess>();
}

void FileSystemProtocol::disguise_file(const Ref<FileAccess> &p_file, const String &p_protocol_name, const String &p_path) const {
	p_file->set_path_disguise(p_protocol_name + "://" + p_path);
}

void FileSystemProtocol::_bind_methods() {
	ClassDB::bind_method(D_METHOD("globalize_path", "path"), &FileSystemProtocol::globalize_path);

	ClassDB::bind_method(D_METHOD("get_open_error"), &FileSystemProtocol::get_open_error);
	ClassDB::bind_method(D_METHOD("open_file", "path", "mode_flags"), &FileSystemProtocol::_open_file);
	ClassDB::bind_method(D_METHOD("file_exists", "name"), &FileSystemProtocol::file_exists);

	ClassDB::bind_method(D_METHOD("get_modified_time", "path"), &FileSystemProtocol::get_modified_time);
	ClassDB::bind_method(D_METHOD("get_unix_permissions", "path"), &FileSystemProtocol::get_unix_permissions);
	ClassDB::bind_method(D_METHOD("set_unix_permissions", "path", "permissions"), &FileSystemProtocol::set_unix_permissions);
	ClassDB::bind_method(D_METHOD("get_hidden_attribute", "path"), &FileSystemProtocol::get_hidden_attribute);
	ClassDB::bind_method(D_METHOD("set_hidden_attribute", "path", "hidden"), &FileSystemProtocol::set_hidden_attribute);
	ClassDB::bind_method(D_METHOD("get_read_only_attribute", "path"), &FileSystemProtocol::get_read_only_attribute);
	ClassDB::bind_method(D_METHOD("set_read_only_attribute", "path", "ro"), &FileSystemProtocol::set_read_only_attribute);
}

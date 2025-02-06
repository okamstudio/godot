/**************************************************************************/
/*  filesystem.h                                                          */
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

#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "core/io/file_access.h"
#include "core/object/ref_counted.h"
#include "filesystem_protocol.h"

class FileSystem : public Object {
	GDCLASS(FileSystem, Object);

protected:
	static void _bind_methods();

private:
	static FileSystem *singleton;

	_THREAD_SAFE_CLASS_

	HashMap<String, Ref<FileSystemProtocol>> protocols;

	// for paths that doesn't have the protocol part, it fallsback to os://
	// if r_protocol returns null ref, the protocol of the path targeted is invalid.
	void process_path(const String &p_path, String *r_protocol_name, Ref<FileSystemProtocol> *r_protocol, String *r_file_path) const;

	Error open_error = OK;
	Ref<FileAccess> _open_file(const String &p_path, int p_mode_flags);

	friend class Main;

	void register_protocols();

	String underlying_protocol_name_for_resources;
	String underlying_protocol_name_for_user;

public:
	// out values are only valid when method returns true
	static bool try_find_protocol_in_path(const String &p_path, int *r_protocol_name_end, int *r_file_path_start);
	// returns whether it found a protocol present in the path
	static bool split_path(const String &p_path, String *r_protocol_name, String *r_file_path);

	static const String protocol_name_os;
	static const String protocol_name_pipe;
	static const String protocol_name_resources;
	static const String protocol_name_user;
	static const String protocol_name_uid;
	static const String protocol_name_gdscript;
	static const String protocol_name_memory;

	FileSystem();
	~FileSystem();
	static FileSystem *get_singleton();

	bool has_protocol(const String &p_protocol) const;
	bool add_protocol(const String &p_name, const Ref<FileSystemProtocol> &p_protocol);
	bool remove_protocol(const String &p_name);
	Ref<FileSystemProtocol> get_protocol(const String &p_name) const;
	Ref<FileSystemProtocol> get_protocol_or_null(const String &p_name) const;

	// Basic path fix. Replaces FileAccess::fix_path.
	static String fix_path(const String &p_path);

	Error get_open_error() const;

	// If failed, returns empty string
	// !! NOT !! the same behavior as the original ProjectSettings::globalize_path
	String globalize_path(const String &path) const;

	// If failed, returns file path without protocol part
	// The same behavior as the original ProjectSettings:globalize_path
	String globalize_path_or_fallback(const String &path) const;

	Ref<FileAccess> open_file(const String &p_path, int p_mode_flags, Error *r_error = nullptr) const;
	bool file_exists(const String &p_path) const;

	uint64_t get_modified_time(const String &p_path) const;
	BitField<FileAccess::UnixPermissionFlags> get_unix_permissions(const String &p_path) const;
	Error set_unix_permissions(const String &p_path, BitField<FileAccess::UnixPermissionFlags> p_permissions) const;
	bool get_hidden_attribute(const String &p_path) const;
	Error set_hidden_attribute(const String &p_path, bool p_hidden) const;
	bool get_read_only_attribute(const String &p_path) const;
	Error set_read_only_attribute(const String &p_path, bool p_ro) const;

	void set_underlying_protocol_name_for_resources(const String &name);
	void set_underlying_protocol_name_for_user(const String &name);
};

#endif // FILESYSTEM_H

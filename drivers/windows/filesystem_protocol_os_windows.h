/**************************************************************************/
/*  filesystem_protocol_os_windows.h                                      */
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

#ifndef FILESYSTEM_PROTOCOL_OS_WINDOWS_H
#define FILESYSTEM_PROTOCOL_OS_WINDOWS_H

#ifdef WINDOWS_ENABLED

#include "core/io/filesystem_protocol.h"

class FileSystemProtocolOSWindows : public FileSystemProtocol {
private:
	static HashSet<String> invalid_files;

public:
	static void initialize();
	static void finalize();

	static String fix_path(const String &p_path);
	static bool is_path_invalid(const String &p_path);
	static bool file_exists_static(const String &p_path);
	static uint64_t get_modified_time_static(const String &p_path);
	static BitField<FileAccess::UnixPermissionFlags> get_unix_permissions_static(const String &p_path);
	static Error set_unix_permissions_static(const String &p_path, BitField<FileAccess::UnixPermissionFlags> p_permissions);
	static bool get_hidden_attribute_static(const String &p_path);
	static Error set_hidden_attribute_static(const String &p_path, bool p_hidden);
	static bool get_read_only_attribute_static(const String &p_path);
	static Error set_read_only_attribute_static(const String &p_path, bool p_ro);

	virtual Ref<FileAccess> open_file(const String &p_path, int p_mode_flags, Error &r_error) const override;
	virtual bool file_exists(const String &p_path) const override;

	// OS files don't need to be disguised when they are opened directly.
	virtual void disguise_file(const Ref<FileAccess> &p_file, const String &p_protocol_name, const String &p_path) const override {}

	virtual uint64_t get_modified_time(const String &p_path) const override;
	virtual BitField<FileAccess::UnixPermissionFlags> get_unix_permissions(const String &p_path) const override;
	virtual Error set_unix_permissions(const String &p_path, BitField<FileAccess::UnixPermissionFlags> p_permissions) const override;
	virtual bool get_hidden_attribute(const String &p_path) const override;
	virtual Error set_hidden_attribute(const String &p_path, bool p_hidden) const override;
	virtual bool get_read_only_attribute(const String &p_path) const override;
	virtual Error set_read_only_attribute(const String &p_path, bool p_ro) const override;
};
#endif // WINDOWS_ENABLED

#endif // FILESYSTEM_PROTOCOL_OS_WINDOWS_H

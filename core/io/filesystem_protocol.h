/**************************************************************************/
/*  filesystem_protocol.h                                                 */
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

#ifndef FILESYSTEM_PROTOCOL_H
#define FILESYSTEM_PROTOCOL_H

#include "core/io/file_access.h"
#include "core/object/ref_counted.h"

// File paths without the protocol part are sent in.
class FileSystemProtocol : public RefCounted {
	GDCLASS(FileSystemProtocol, RefCounted);

private:
	Error open_error;
	Ref<FileAccess> _open_file(const String &p_path, int p_mode_flags);

protected:
	static void _bind_methods();

public:
	Error get_open_error() const;

	virtual String globalize_path(const String &p_path) const { return String(); }

	virtual Ref<FileAccess> open_file(const String &p_path, int p_mode_flags, Error &r_error) const;
	virtual bool file_exists(const String &p_path) const { return false; }

	virtual void disguise_file(const Ref<FileAccess> &p_file, const String &p_protocol_name, const String &p_path) const;

	virtual uint64_t get_modified_time(const String &p_path) const { return 0; }
	virtual BitField<FileAccess::UnixPermissionFlags> get_unix_permissions(const String &p_path) const { return 0; }
	virtual Error set_unix_permissions(const String &p_path, BitField<FileAccess::UnixPermissionFlags> p_permissions) const { return ERR_UNAVAILABLE; }
	virtual bool get_hidden_attribute(const String &p_path) const { return false; }
	virtual Error set_hidden_attribute(const String &p_path, bool p_hidden) const { return ERR_UNAVAILABLE; }
	virtual bool get_read_only_attribute(const String &p_path) const { return false; }
	virtual Error set_read_only_attribute(const String &p_path, bool p_ro) const { return ERR_UNAVAILABLE; }
};

#endif // FILESYSTEM_PROTOCOL_H

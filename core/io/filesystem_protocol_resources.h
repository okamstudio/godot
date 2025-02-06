/**************************************************************************/
/*  filesystem_protocol_resources.h                                       */
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

#ifndef FILESYSTEM_PROTOCOL_RESOURCES_H
#define FILESYSTEM_PROTOCOL_RESOURCES_H

#include "core/io/filesystem_protocol.h"

class FileSystemProtocolResources : public FileSystemProtocol {
	GDCLASS(FileSystemProtocolResources, FileSystemProtocol);

private:
	Ref<FileSystemProtocol> protocol_os;

public:
	FileSystemProtocolResources();
	FileSystemProtocolResources(const Ref<FileSystemProtocol> &p_protocol_os);

	void set_protocol_os(const Ref<FileSystemProtocol> &p_protocol_os);

	virtual String globalize_path(const String &p_path) const override;

	virtual Ref<FileAccess> open_file(const String &p_path, int p_mode_flags, Error &r_error) const override;
	virtual bool file_exists(const String &p_path) const override;

	virtual uint64_t get_modified_time(const String &p_path) const override;
	virtual BitField<FileAccess::UnixPermissionFlags> get_unix_permissions(const String &p_path) const override;
	virtual Error set_unix_permissions(const String &p_path, BitField<FileAccess::UnixPermissionFlags> p_permissions) const override;
	virtual bool get_hidden_attribute(const String &p_path) const override;
	virtual Error set_hidden_attribute(const String &p_path, bool p_hidden) const override;
	virtual bool get_read_only_attribute(const String &p_path) const override;
	virtual Error set_read_only_attribute(const String &p_path, bool p_ro) const override;
};

#endif // FILESYSTEM_PROTOCOL_RESOURCES_H

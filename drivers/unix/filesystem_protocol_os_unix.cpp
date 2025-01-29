/**************************************************************************/
/*  filesystem_protocol_os_unix.cpp                                       */
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

#ifdef UNIX_ENABLED

#include "filesystem_protocol_os_unix.h"
#include "core/io/filesystem.h"
#include "file_access_unix.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

String FileSystemProtocolOSUnix::fix_path(const String &p_path) {
	String r_path = FileSystem::fix_path(p_path);
	return r_path;
}

bool FileSystemProtocolOSUnix::file_exists_static(const String &p_path) {
	struct stat st = {};
	const CharString filename_utf8 = fix_path(p_path).utf8();

	// Does the name exist at all?
	if (stat(filename_utf8.get_data(), &st)) {
		return false;
	}

	// See if we have access to the file
	if (access(filename_utf8.get_data(), F_OK)) {
		return false;
	}

	// See if this is a regular file
	switch (st.st_mode & S_IFMT) {
		case S_IFLNK:
		case S_IFREG:
			return true;
		default:
			return false;
	}
}
BitField<FileAccess::UnixPermissionFlags> FileSystemProtocolOSUnix::get_unix_permissions_static(const String &p_path) {
	String path = fix_path(p_path);
	struct stat status = {};
	int err = stat(path.utf8().get_data(), &status);

	if (!err) {
		return status.st_mode & 0xFFF; //only permissions
	} else {
		ERR_FAIL_V_MSG(0, "Failed to get unix permissions for: " + p_path + ".");
	}
}
Error FileSystemProtocolOSUnix::set_unix_permissions_static(const String &p_path, BitField<FileAccess::UnixPermissionFlags> p_permissions) {
	String path = fix_path(p_path);

	int err = chmod(path.utf8().get_data(), p_permissions);
	if (!err) {
		return OK;
	}

	return FAILED;
}
uint64_t FileSystemProtocolOSUnix::get_modified_time_static(const String &p_path) {
	String path = fix_path(p_path);
	struct stat status = {};
	int err = stat(path.utf8().get_data(), &status);

	if (!err) {
		return status.st_mtime;
	} else {
		return 0;
	}
}

bool FileSystemProtocolOSUnix::get_hidden_attribute_static(const String &p_path) {
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__APPLE__)
	String path = fix_path(p_path);

	struct stat st = {};
	int err = stat(path.utf8().get_data(), &st);
	ERR_FAIL_COND_V_MSG(err, false, "Failed to get attributes for: " + p_path);

	return (st.st_flags & UF_HIDDEN);
#else
	return false;
#endif
}
Error FileSystemProtocolOSUnix::set_hidden_attribute_static(const String &p_path, bool p_hidden) {
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__APPLE__)
	String path = fix_path(p_path);

	struct stat st = {};
	int err = stat(path.utf8().get_data(), &st);
	ERR_FAIL_COND_V_MSG(err, FAILED, "Failed to get attributes for: " + p_path);

	if (p_hidden) {
		err = chflags(path.utf8().get_data(), st.st_flags | UF_HIDDEN);
	} else {
		err = chflags(path.utf8().get_data(), st.st_flags & ~UF_HIDDEN);
	}
	ERR_FAIL_COND_V_MSG(err, FAILED, "Failed to set attributes for: " + p_path);
	return OK;
#else
	return ERR_UNAVAILABLE;
#endif
}
bool FileSystemProtocolOSUnix::get_read_only_attribute_static(const String &p_path) {
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
	String path = fix_path(p_path);

	struct stat st = {};
	int err = stat(path.utf8().get_data(), &st);
	ERR_FAIL_COND_V_MSG(err, false, "Failed to get attributes for: " + p_path);

	return st.st_flags & UF_IMMUTABLE;
#else
	return false;
#endif
}
Error FileSystemProtocolOSUnix::set_read_only_attribute_static(const String &p_path, bool p_ro) {
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
	String path = fix_path(p_path);

	struct stat st = {};
	int err = stat(path.utf8().get_data(), &st);
	ERR_FAIL_COND_V_MSG(err, FAILED, "Failed to get attributes for: " + p_path);

	if (p_ro) {
		err = chflags(path.utf8().get_data(), st.st_flags | UF_IMMUTABLE);
	} else {
		err = chflags(path.utf8().get_data(), st.st_flags & ~UF_IMMUTABLE);
	}
	ERR_FAIL_COND_V_MSG(err, FAILED, "Failed to set attributes for: " + p_path);
	return OK;
#else
	return ERR_UNAVAILABLE;
#endif
}

Ref<FileAccess> FileSystemProtocolOSUnix::open_file(const String &p_path, int p_mode_flags, Error &r_error) const {
	Ref<FileAccessUnix> file = Ref<FileAccessUnix>();
	file.instantiate();

	r_error = file->open_internal(p_path, p_mode_flags);

	if (r_error != OK) {
		file.unref();
	}

	return file;
}
bool FileSystemProtocolOSUnix::file_exists(const String &p_path) const {
	return file_exists_static(p_path);
}

uint64_t FileSystemProtocolOSUnix::get_modified_time(const String &p_path) const {
	return get_modified_time_static(p_path);
}
BitField<FileAccess::UnixPermissionFlags> FileSystemProtocolOSUnix::get_unix_permissions(const String &p_path) const {
	return get_unix_permissions_static(p_path);
}
Error FileSystemProtocolOSUnix::set_unix_permissions(const String &p_path, BitField<FileAccess::UnixPermissionFlags> p_permissions) const {
	return set_unix_permissions_static(p_path, p_permissions);
}
bool FileSystemProtocolOSUnix::get_hidden_attribute(const String &p_path) const {
	return get_hidden_attribute_static(p_path);
}
Error FileSystemProtocolOSUnix::set_hidden_attribute(const String &p_path, bool p_hidden) const {
	return set_hidden_attribute_static(p_path, p_hidden);
}
bool FileSystemProtocolOSUnix::get_read_only_attribute(const String &p_path) const {
	return get_read_only_attribute_static(p_path);
}
Error FileSystemProtocolOSUnix::set_read_only_attribute(const String &p_path, bool p_ro) const {
	return set_read_only_attribute_static(p_path, p_ro);
}
#endif // WINDOWS_ENABLED

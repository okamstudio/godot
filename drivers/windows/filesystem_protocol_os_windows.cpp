/**************************************************************************/
/*  filesystem_protocol_os_windows.cpp                                    */
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

#ifdef WINDOWS_ENABLED

#include "filesystem_protocol_os_windows.h"
#include "core/io/filesystem.h"
#include "file_access_windows.h"

#include <share.h> // _SH_DENYNO
#include <shlwapi.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

HashSet<String> FileSystemProtocolOSWindows::invalid_files;
void FileSystemProtocolOSWindows::initialize() {
	static const char *reserved_files[]{
		"con", "prn", "aux", "nul", "com0", "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9", "lpt0", "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9", nullptr
	};
	int reserved_file_index = 0;
	while (reserved_files[reserved_file_index] != nullptr) {
		invalid_files.insert(reserved_files[reserved_file_index]);
		reserved_file_index++;
	}

	_setmaxstdio(8192);
	print_verbose(vformat("Maximum number of file handles: %d", _getmaxstdio()));
}
void FileSystemProtocolOSWindows::finalize() {
	invalid_files.clear();
}

bool FileSystemProtocolOSWindows::is_path_invalid(const String &p_path) {
	// Check for invalid operating system file.
	String fname = p_path.get_file().to_lower();

	int dot = fname.find_char('.');
	if (dot != -1) {
		fname = fname.substr(0, dot);
	}
	return invalid_files.has(fname);
}

String FileSystemProtocolOSWindows::fix_path(const String &p_path) {
	String r_path = FileSystem::fix_path(p_path);

	if (r_path.is_relative_path()) {
		Char16String current_dir_name;
		size_t str_len = GetCurrentDirectoryW(0, nullptr);
		current_dir_name.resize(str_len + 1);
		GetCurrentDirectoryW(current_dir_name.size(), (LPWSTR)current_dir_name.ptrw());
		r_path = String::utf16((const char16_t *)current_dir_name.get_data()).trim_prefix(R"(\\?\)").replace("\\", "/").path_join(r_path);
	}
	r_path = r_path.simplify_path();
	r_path = r_path.replace("/", "\\");
	if (!r_path.is_network_share_path() && !r_path.begins_with(R"(\\?\)")) {
		r_path = R"(\\?\)" + r_path;
	}
	return r_path;
}

bool FileSystemProtocolOSWindows::file_exists_static(const String &p_path) {
	if (is_path_invalid(p_path)) {
		return false;
	}

	String filename = fix_path(p_path);
	DWORD file_attr = GetFileAttributesW((LPCWSTR)(filename.utf16().get_data()));
	return (file_attr != INVALID_FILE_ATTRIBUTES) && !(file_attr & FILE_ATTRIBUTE_DIRECTORY);
}
BitField<FileAccess::UnixPermissionFlags> FileSystemProtocolOSWindows::get_unix_permissions_static(const String &p_path) {
	return 0;
}
Error FileSystemProtocolOSWindows::set_unix_permissions_static(const String &p_path, BitField<FileAccess::UnixPermissionFlags> p_permissions) {
	return ERR_UNAVAILABLE;
}
uint64_t FileSystemProtocolOSWindows::get_modified_time_static(const String &p_path) {
	if (is_path_invalid(p_path)) {
		return 0;
	}

	String file = fix_path(p_path);
	if (file.ends_with("\\") && file != "\\") {
		file = file.substr(0, file.length() - 1);
	}

	HANDLE handle = CreateFileW((LPCWSTR)(file.utf16().get_data()), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);

	if (handle != INVALID_HANDLE_VALUE) {
		FILETIME ft_create, ft_write;

		bool status = GetFileTime(handle, &ft_create, nullptr, &ft_write);

		CloseHandle(handle);

		if (status) {
			uint64_t ret = 0;

			// If write time is invalid, fallback to creation time.
			if (ft_write.dwHighDateTime == 0 && ft_write.dwLowDateTime == 0) {
				ret = ft_create.dwHighDateTime;
				ret <<= 32;
				ret |= ft_create.dwLowDateTime;
			} else {
				ret = ft_write.dwHighDateTime;
				ret <<= 32;
				ret |= ft_write.dwLowDateTime;
			}

			const uint64_t WINDOWS_TICKS_PER_SECOND = 10000000;
			const uint64_t TICKS_TO_UNIX_EPOCH = 116444736000000000LL;

			if (ret >= TICKS_TO_UNIX_EPOCH) {
				return (ret - TICKS_TO_UNIX_EPOCH) / WINDOWS_TICKS_PER_SECOND;
			}
		}
	}

	return 0;
}

bool FileSystemProtocolOSWindows::get_hidden_attribute_static(const String &p_path) {
	String file = fix_path(p_path);

	DWORD attrib = GetFileAttributesW((LPCWSTR)file.utf16().get_data());
	ERR_FAIL_COND_V_MSG(attrib == INVALID_FILE_ATTRIBUTES, false, "Failed to get attributes for: " + p_path);
	return (attrib & FILE_ATTRIBUTE_HIDDEN);
}
Error FileSystemProtocolOSWindows::set_hidden_attribute_static(const String &p_path, bool p_hidden) {
	String file = fix_path(p_path);
	const Char16String &file_utf16 = file.utf16();

	DWORD attrib = GetFileAttributesW((LPCWSTR)file_utf16.get_data());
	ERR_FAIL_COND_V_MSG(attrib == INVALID_FILE_ATTRIBUTES, FAILED, "Failed to get attributes for: " + p_path);
	BOOL ok;
	if (p_hidden) {
		ok = SetFileAttributesW((LPCWSTR)file_utf16.get_data(), attrib | FILE_ATTRIBUTE_HIDDEN);
	} else {
		ok = SetFileAttributesW((LPCWSTR)file_utf16.get_data(), attrib & ~FILE_ATTRIBUTE_HIDDEN);
	}
	ERR_FAIL_COND_V_MSG(!ok, FAILED, "Failed to set attributes for: " + p_path);

	return OK;
}
bool FileSystemProtocolOSWindows::get_read_only_attribute_static(const String &p_path) {
	String file = fix_path(p_path);

	DWORD attrib = GetFileAttributesW((LPCWSTR)file.utf16().get_data());
	ERR_FAIL_COND_V_MSG(attrib == INVALID_FILE_ATTRIBUTES, false, "Failed to get attributes for: " + p_path);
	return (attrib & FILE_ATTRIBUTE_READONLY);
}
Error FileSystemProtocolOSWindows::set_read_only_attribute_static(const String &p_path, bool p_ro) {
	String file = fix_path(p_path);
	const Char16String &file_utf16 = file.utf16();

	DWORD attrib = GetFileAttributesW((LPCWSTR)file_utf16.get_data());
	ERR_FAIL_COND_V_MSG(attrib == INVALID_FILE_ATTRIBUTES, FAILED, "Failed to get attributes for: " + p_path);
	BOOL ok;
	if (p_ro) {
		ok = SetFileAttributesW((LPCWSTR)file_utf16.get_data(), attrib | FILE_ATTRIBUTE_READONLY);
	} else {
		ok = SetFileAttributesW((LPCWSTR)file_utf16.get_data(), attrib & ~FILE_ATTRIBUTE_READONLY);
	}
	ERR_FAIL_COND_V_MSG(!ok, FAILED, "Failed to set attributes for: " + p_path);

	return OK;
}

Ref<FileAccess> FileSystemProtocolOSWindows::open_file(const String &p_path, int p_mode_flags, Error &r_error) const {
	Ref<FileAccessWindows> file = Ref<FileAccessWindows>();
	file.instantiate();

	r_error = file->open_internal(p_path, p_mode_flags);

	if (r_error != OK) {
		file.unref();
	}

	return file;
}
bool FileSystemProtocolOSWindows::file_exists(const String &p_path) const {
	return file_exists_static(p_path);
}

uint64_t FileSystemProtocolOSWindows::get_modified_time(const String &p_path) const {
	return get_modified_time_static(p_path);
}
BitField<FileAccess::UnixPermissionFlags> FileSystemProtocolOSWindows::get_unix_permissions(const String &p_path) const {
	return get_unix_permissions_static(p_path);
}
Error FileSystemProtocolOSWindows::set_unix_permissions(const String &p_path, BitField<FileAccess::UnixPermissionFlags> p_permissions) const {
	return set_unix_permissions_static(p_path, p_permissions);
}
bool FileSystemProtocolOSWindows::get_hidden_attribute(const String &p_path) const {
	return get_hidden_attribute_static(p_path);
}
Error FileSystemProtocolOSWindows::set_hidden_attribute(const String &p_path, bool p_hidden) const {
	return set_hidden_attribute_static(p_path, p_hidden);
}
bool FileSystemProtocolOSWindows::get_read_only_attribute(const String &p_path) const {
	return get_read_only_attribute_static(p_path);
}
Error FileSystemProtocolOSWindows::set_read_only_attribute(const String &p_path, bool p_ro) const {
	return set_read_only_attribute_static(p_path, p_ro);
}
#endif // WINDOWS_ENABLED

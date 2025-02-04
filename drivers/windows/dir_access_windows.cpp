/**************************************************************************/
/*  dir_access_windows.cpp                                                */
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

#if defined(WINDOWS_ENABLED)

#include "dir_access_windows.h"
#include "file_access_windows.h"

#include "core/config/project_settings.h"
#include "core/os/memory.h"
#include "core/os/os.h"
#include "core/string/print_string.h"

#include <stdio.h>
#include <wchar.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct _NT_IO_STATUS_BLOCK {
	union {
		LONG Status;
		PVOID Pointer;
	} DUMMY;
	ULONG_PTR Information;
} NT_IO_STATUS_BLOCK;

typedef struct _NT_FILE_CASE_SENSITIVE_INFO {
	ULONG Flags;
} NT_FILE_CASE_SENSITIVE_INFO;

typedef enum _NT_FILE_INFORMATION_CLASS {
	FileCaseSensitiveInformation = 71,
} NT_FILE_INFORMATION_CLASS;

#define NT_FILE_CS_FLAG_CASE_SENSITIVE_DIR 0x00000001

extern "C" NTSYSAPI LONG NTAPI NtQueryInformationFile(HANDLE FileHandle, NT_IO_STATUS_BLOCK *IoStatusBlock, PVOID FileInformation, ULONG Length, NT_FILE_INFORMATION_CLASS FileInformationClass);

struct DirAccessWindowsPrivate {
	HANDLE h; // handle for FindFirstFile.
	WIN32_FIND_DATA f;
	WIN32_FIND_DATAW fu; // Unicode version.
};

String DirAccessWindows::fix_path(const String &p_path) const {
	String r_path = DirAccess::fix_path(p_path.trim_prefix(R"(\\?\)").replace("\\", "/"));
	if (r_path.ends_with(":")) {
		r_path += "/";
	}
	if (r_path.is_relative_path()) {
		r_path = current_dir.trim_prefix(R"(\\?\)").replace("\\", "/").path_join(r_path);
	} else if (r_path == ".") {
		r_path = current_dir.trim_prefix(R"(\\?\)").replace("\\", "/");
	}
	r_path = r_path.simplify_path();
	r_path = r_path.replace("/", "\\");
	if (!r_path.is_network_share_path() && !r_path.begins_with(R"(\\?\)")) {
		r_path = R"(\\?\)" + r_path;
	}
	return r_path;
}

// CreateFolderAsync

Error DirAccessWindows::list_dir_begin() {
	_cisdir = false;
	_cishidden = false;

	list_dir_end();
	p->h = FindFirstFileExW((LPCWSTR)(String(current_dir + "\\*").utf16().get_data()), FindExInfoStandard, &p->fu, FindExSearchNameMatch, nullptr, 0);

	if (p->h == INVALID_HANDLE_VALUE) {
		return ERR_CANT_OPEN;
	}

	return OK;
}

String DirAccessWindows::get_next() {
	if (p->h == INVALID_HANDLE_VALUE) {
		return "";
	}

	_cisdir = (p->fu.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
	_cishidden = (p->fu.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN);

	String name = String::utf16((const char16_t *)(p->fu.cFileName));

	if (FindNextFileW(p->h, &p->fu) == 0) {
		FindClose(p->h);
		p->h = INVALID_HANDLE_VALUE;
	}

	return name;
}

bool DirAccessWindows::current_is_dir() const {
	return _cisdir;
}

bool DirAccessWindows::current_is_hidden() const {
	return _cishidden;
}

void DirAccessWindows::list_dir_end() {
	if (p->h != INVALID_HANDLE_VALUE) {
		FindClose(p->h);
		p->h = INVALID_HANDLE_VALUE;
	}
}

int DirAccessWindows::get_drive_count() {
	return drive_count;
}

String DirAccessWindows::get_drive(int p_drive) {
	if (p_drive < 0 || p_drive >= drive_count) {
		return "";
	}

	return String::chr(drives[p_drive]) + ":";
}

Error DirAccessWindows::change_dir(String p_dir) {
	GLOBAL_LOCK_FUNCTION

	String dir = fix_path(p_dir);

	Char16String real_current_dir_name;
	size_t str_len = GetCurrentDirectoryW(0, nullptr);
	real_current_dir_name.resize(str_len + 1);
	GetCurrentDirectoryW(real_current_dir_name.size(), (LPWSTR)real_current_dir_name.ptrw());
	String prev_dir = String::utf16((const char16_t *)real_current_dir_name.get_data());

	SetCurrentDirectoryW((LPCWSTR)(current_dir.utf16().get_data()));
	bool worked = (SetCurrentDirectoryW((LPCWSTR)(dir.utf16().get_data())) != 0);

	String base = _get_root_path();
	if (!base.is_empty()) {
		str_len = GetCurrentDirectoryW(0, nullptr);
		real_current_dir_name.resize(str_len + 1);
		GetCurrentDirectoryW(real_current_dir_name.size(), (LPWSTR)real_current_dir_name.ptrw());
		String new_dir = String::utf16((const char16_t *)real_current_dir_name.get_data()).trim_prefix(R"(\\?\)").replace("\\", "/");
		if (!new_dir.begins_with(base)) {
			worked = false;
		}
	}

	if (worked) {
		str_len = GetCurrentDirectoryW(0, nullptr);
		real_current_dir_name.resize(str_len + 1);
		GetCurrentDirectoryW(real_current_dir_name.size(), (LPWSTR)real_current_dir_name.ptrw());
		current_dir = String::utf16((const char16_t *)real_current_dir_name.get_data());
	}

	SetCurrentDirectoryW((LPCWSTR)(prev_dir.utf16().get_data()));

	return worked ? OK : ERR_INVALID_PARAMETER;
}

Error DirAccessWindows::make_dir(String p_dir) {
	GLOBAL_LOCK_FUNCTION

	if (FileAccessWindows::is_path_invalid(p_dir)) {
#ifdef DEBUG_ENABLED
		WARN_PRINT("The path :" + p_dir + " is a reserved Windows system pipe, so it can't be used for creating directories.");
#endif
		return ERR_INVALID_PARAMETER;
	}

	String dir = fix_path(p_dir);

	bool success;
	int err;

	success = CreateDirectoryW((LPCWSTR)(dir.utf16().get_data()), nullptr);
	err = GetLastError();

	if (success) {
		return OK;
	}

	if (err == ERROR_ALREADY_EXISTS || err == ERROR_ACCESS_DENIED) {
		return ERR_ALREADY_EXISTS;
	}

	return ERR_CANT_CREATE;
}

String DirAccessWindows::get_current_dir(bool p_include_drive) const {
	String cdir = current_dir.trim_prefix(R"(\\?\)").replace("\\", "/");
	String base = _get_root_path();
	if (!base.is_empty()) {
		String bd = cdir.replace_first(base, "");
		if (bd.begins_with("/")) {
			return _get_root_string() + bd.substr(1);
		} else {
			return _get_root_string() + bd;
		}
	}

	if (p_include_drive) {
		return cdir;
	} else {
		if (_get_root_string().is_empty()) {
			int pos = cdir.find_char(':');
			if (pos != -1) {
				return cdir.substr(pos + 1);
			}
		}
		return cdir;
	}
}

bool DirAccessWindows::file_exists(String p_file) {
	GLOBAL_LOCK_FUNCTION

	String file = fix_path(p_file);

	DWORD fileAttr;
	fileAttr = GetFileAttributesW((LPCWSTR)(file.utf16().get_data()));
	if (INVALID_FILE_ATTRIBUTES == fileAttr) {
		return false;
	}

	return !(fileAttr & FILE_ATTRIBUTE_DIRECTORY);
}

bool DirAccessWindows::dir_exists(String p_dir) {
	GLOBAL_LOCK_FUNCTION

	String dir = fix_path(p_dir);

	DWORD fileAttr;
	fileAttr = GetFileAttributesW((LPCWSTR)(dir.utf16().get_data()));
	if (INVALID_FILE_ATTRIBUTES == fileAttr) {
		return false;
	}
	return (fileAttr & FILE_ATTRIBUTE_DIRECTORY);
}

Error DirAccessWindows::rename(String p_path, String p_new_path) {
	String path = fix_path(p_path);
	String new_path = fix_path(p_new_path);

	// If we're only changing file name case we need to do a little juggling
	if (path.to_lower() == new_path.to_lower()) {
		if (dir_exists(path)) {
			// The path is a dir; just rename
			return MoveFileW((LPCWSTR)(path.utf16().get_data()), (LPCWSTR)(new_path.utf16().get_data())) != 0 ? OK : FAILED;
		}
		// The path is a file; juggle
		// Note: do not use GetTempFileNameW, it's not long path aware!
		Char16String tmpfile_utf16;
		uint64_t id = OS::get_singleton()->get_ticks_usec();
		while (true) {
			tmpfile_utf16 = (path + itos(id++) + ".tmp").utf16();
			HANDLE handle = CreateFileW((LPCWSTR)tmpfile_utf16.get_data(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (handle != INVALID_HANDLE_VALUE) {
				CloseHandle(handle);
				break;
			}
			if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_SHARING_VIOLATION) {
				return FAILED;
			}
		}

		if (!::ReplaceFileW((LPCWSTR)tmpfile_utf16.get_data(), (LPCWSTR)(path.utf16().get_data()), nullptr, 0, nullptr, nullptr)) {
			DeleteFileW((LPCWSTR)tmpfile_utf16.get_data());
			return FAILED;
		}

		return MoveFileW((LPCWSTR)tmpfile_utf16.get_data(), (LPCWSTR)(new_path.utf16().get_data())) != 0 ? OK : FAILED;

	} else {
		if (file_exists(new_path)) {
			if (remove(new_path) != OK) {
				return FAILED;
			}
		}

		return MoveFileW((LPCWSTR)(path.utf16().get_data()), (LPCWSTR)(new_path.utf16().get_data())) != 0 ? OK : FAILED;
	}
}

Error DirAccessWindows::remove(String p_path) {
	String path = fix_path(p_path);
	const Char16String &path_utf16 = path.utf16();

	DWORD fileAttr;

	fileAttr = GetFileAttributesW((LPCWSTR)(path_utf16.get_data()));
	if (INVALID_FILE_ATTRIBUTES == fileAttr) {
		return FAILED;
	}
	if ((fileAttr & FILE_ATTRIBUTE_DIRECTORY)) {
		return RemoveDirectoryW((LPCWSTR)(path_utf16.get_data())) != 0 ? OK : FAILED;
	} else {
		return DeleteFileW((LPCWSTR)(path_utf16.get_data())) != 0 ? OK : FAILED;
	}
}

uint64_t DirAccessWindows::get_space_left() {
	uint64_t bytes = 0;
	if (!GetDiskFreeSpaceEx(nullptr, (PULARGE_INTEGER)&bytes, nullptr, nullptr)) {
		return 0;
	}

	// This is either 0 or a value in bytes.
	return bytes;
}

String DirAccessWindows::get_filesystem_type() const {
	String path = current_dir.trim_prefix(R"(\\?\)");

	if (path.is_network_share_path()) {
		return "Network Share";
	}

	int unit_end = path.find_char(':');
	ERR_FAIL_COND_V(unit_end == -1, String());
	String unit = path.substr(0, unit_end + 1) + "\\";

	WCHAR szVolumeName[100];
	WCHAR szFileSystemName[10];
	DWORD dwSerialNumber = 0;
	DWORD dwMaxFileNameLength = 0;
	DWORD dwFileSystemFlags = 0;

	if (::GetVolumeInformationW((LPCWSTR)(unit.utf16().get_data()),
				szVolumeName,
				sizeof(szVolumeName),
				&dwSerialNumber,
				&dwMaxFileNameLength,
				&dwFileSystemFlags,
				szFileSystemName,
				sizeof(szFileSystemName)) == TRUE) {
		return String::utf16((const char16_t *)szFileSystemName);
	}

	ERR_FAIL_V("");
}

bool DirAccessWindows::is_case_sensitive(const String &p_path) const {
	String f = fix_path(p_path);

	HANDLE h_file = ::CreateFileW((LPCWSTR)(f.utf16().get_data()), 0,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);

	if (h_file == INVALID_HANDLE_VALUE) {
		return false;
	}

	NT_IO_STATUS_BLOCK io_status_block;
	NT_FILE_CASE_SENSITIVE_INFO file_info;
	LONG out = NtQueryInformationFile(h_file, &io_status_block, &file_info, sizeof(NT_FILE_CASE_SENSITIVE_INFO), FileCaseSensitiveInformation);
	::CloseHandle(h_file);

	if (out >= 0) {
		return file_info.Flags & NT_FILE_CS_FLAG_CASE_SENSITIVE_DIR;
	} else {
		return false;
	}
}

bool DirAccessWindows::is_link(String p_file) {
	String f = fix_path(p_file);

	DWORD attr = GetFileAttributesW((LPCWSTR)(f.utf16().get_data()));
	if (attr == INVALID_FILE_ATTRIBUTES) {
		return false;
	}

	return (attr & FILE_ATTRIBUTE_REPARSE_POINT);
}

String DirAccessWindows::read_link(String p_file) {
	String f = fix_path(p_file);

	HANDLE hfile = CreateFileW((LPCWSTR)(f.utf16().get_data()), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
	if (hfile == INVALID_HANDLE_VALUE) {
		return f;
	}

	DWORD ret = GetFinalPathNameByHandleW(hfile, nullptr, 0, VOLUME_NAME_DOS | FILE_NAME_NORMALIZED);
	if (ret == 0) {
		return f;
	}
	Char16String cs;
	cs.resize(ret + 1);
	GetFinalPathNameByHandleW(hfile, (LPWSTR)cs.ptrw(), ret, VOLUME_NAME_DOS | FILE_NAME_NORMALIZED);
	CloseHandle(hfile);

	return String::utf16((const char16_t *)cs.ptr(), ret).trim_prefix(R"(\\?\)").replace("\\", "/");
}

Error DirAccessWindows::create_link(String p_source, String p_target) {
	String source = fix_path(p_source);
	String target = fix_path(p_target);

	DWORD file_attr = GetFileAttributesW((LPCWSTR)(source.utf16().get_data()));
	bool is_dir = (file_attr & FILE_ATTRIBUTE_DIRECTORY);

	DWORD flags = ((is_dir) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0) | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
	if (CreateSymbolicLinkW((LPCWSTR)target.utf16().get_data(), (LPCWSTR)source.utf16().get_data(), flags) != 0) {
		return OK;
	} else {
		return FAILED;
	}
}

DirAccessWindows::DirAccessWindows() {
	p = memnew(DirAccessWindowsPrivate);
	p->h = INVALID_HANDLE_VALUE;

	Char16String real_current_dir_name;
	size_t str_len = GetCurrentDirectoryW(0, nullptr);
	real_current_dir_name.resize(str_len + 1);
	GetCurrentDirectoryW(real_current_dir_name.size(), (LPWSTR)real_current_dir_name.ptrw());
	current_dir = String::utf16((const char16_t *)real_current_dir_name.get_data());

	DWORD mask = GetLogicalDrives();

	for (int i = 0; i < MAX_DRIVES; i++) {
		if (mask & (1 << i)) { //DRIVE EXISTS

			drives[drive_count] = 'A' + i;
			drive_count++;
		}
	}

	change_dir(".");
}

DirAccessWindows::~DirAccessWindows() {
	list_dir_end();

	memdelete(p);
}

#endif // WINDOWS_ENABLED

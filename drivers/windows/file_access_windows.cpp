/**************************************************************************/
/*  file_access_windows.cpp                                               */
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

#include "file_access_windows.h"
#include "filesystem_protocol_os_windows.h"

#include "core/config/project_settings.h"
#include "core/os/os.h"
#include "core/string/print_string.h"

#include <share.h> // _SH_DENYNO
#include <shlwapi.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <errno.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <tchar.h>
#include <wchar.h>

#ifdef _MSC_VER
#define S_ISREG(m) ((m) & _S_IFREG)
#endif

void FileAccessWindows::check_errors(bool p_write) const {
	ERR_FAIL_NULL(f);

	last_error = OK;
	if (ferror(f)) {
		if (p_write) {
			last_error = ERR_FILE_CANT_WRITE;
		} else {
			last_error = ERR_FILE_CANT_READ;
		}
	}
	if (!p_write && feof(f)) {
		last_error = ERR_FILE_EOF;
	}
}

Error FileAccessWindows::open_internal(const String &p_path, int p_mode_flags) {
	if (FileSystemProtocolOSWindows::is_path_invalid(p_path)) {
#ifdef DEBUG_ENABLED
		if (p_mode_flags != READ) {
			WARN_PRINT("The path :" + p_path + " is a reserved Windows system pipe, so it can't be used for creating files.");
		}
#endif
		return ERR_INVALID_PARAMETER;
	}

	_close();

	path_src = p_path;
	path = FileSystemProtocolOSWindows::fix_path(p_path);

	const WCHAR *mode_string;

	if (p_mode_flags == READ) {
		mode_string = L"rb";
	} else if (p_mode_flags == WRITE) {
		mode_string = L"wb";
	} else if (p_mode_flags == READ_WRITE) {
		mode_string = L"rb+";
	} else if (p_mode_flags == WRITE_READ) {
		mode_string = L"wb+";
	} else {
		return ERR_INVALID_PARAMETER;
	}

	if (path.ends_with(":\\") || path.ends_with(":")) {
		return ERR_FILE_CANT_OPEN;
	}
	DWORD file_attr = GetFileAttributesW((LPCWSTR)(path.utf16().get_data()));
	if (file_attr != INVALID_FILE_ATTRIBUTES && (file_attr & FILE_ATTRIBUTE_DIRECTORY)) {
		return ERR_FILE_CANT_OPEN;
	}

// TODO: reimplement this in protocol level
#if 0 //TOOLS_ENABLED
	// Windows is case insensitive in the default configuration, but other platforms can be sensitive to it
	// To ease cross-platform development, we issue a warning if users try to access
	// a file using the wrong case (which *works* on Windows, but won't on other
	// platforms), we only check for relative paths, or paths in res:// or user://,
	// other paths aren't likely to be portable anyway.
	if (p_mode_flags == READ && (p_path.is_relative_path() || get_access_type() != ACCESS_FILESYSTEM)) {
		String base_path = p_path;
		String working_path;
		String proper_path;

		if (get_access_type() == ACCESS_RESOURCES) {
			if (ProjectSettings::get_singleton()) {
				working_path = ProjectSettings::get_singleton()->get_resource_path();
				if (!working_path.is_empty()) {
					base_path = working_path.path_to_file(base_path);
				}
			}
			proper_path = "res://";
		} else if (get_access_type() == ACCESS_USERDATA) {
			working_path = OS::get_singleton()->get_user_data_dir();
			if (!working_path.is_empty()) {
				base_path = working_path.path_to_file(base_path);
			}
			proper_path = "user://";
		}
		working_path = FileSystemProtocolOSWindows::fix_path(working_path);

		WIN32_FIND_DATAW d;
		Vector<String> parts = base_path.simplify_path().split("/");

		bool mismatch = false;

		for (const String &part : parts) {
			working_path = working_path + "\\" + part;

			HANDLE fnd = FindFirstFileW((LPCWSTR)(working_path.utf16().get_data()), &d);
			if (fnd == INVALID_HANDLE_VALUE) {
				mismatch = false;
				break;
			}

			const String fname = String::utf16((const char16_t *)(d.cFileName));

			FindClose(fnd);

			if (!mismatch) {
				mismatch = (part != fname && part.findn(fname) == 0);
			}

			proper_path = proper_path.path_join(fname);
		}

		if (mismatch) {
			WARN_PRINT("Case mismatch opening requested file '" + p_path + "', stored as '" + proper_path + "' in the filesystem. This file will not open when exported to other case-sensitive platforms.");
		}
	}
#endif

	if (is_backup_save_enabled() && p_mode_flags == WRITE) {
		save_path = path;
		// Create a temporary file in the same directory as the target file.
		// Note: do not use GetTempFileNameW, it's not long path aware!
		String tmpfile;
		uint64_t id = OS::get_singleton()->get_ticks_usec();
		while (true) {
			tmpfile = path + itos(id++) + ".tmp";
			HANDLE handle = CreateFileW((LPCWSTR)tmpfile.utf16().get_data(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (handle != INVALID_HANDLE_VALUE) {
				CloseHandle(handle);
				break;
			}
			if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_SHARING_VIOLATION) {
				last_error = ERR_FILE_CANT_WRITE;
				return FAILED;
			}
		}
		path = tmpfile;
	}

	f = _wfsopen((LPCWSTR)(path.utf16().get_data()), mode_string, is_backup_save_enabled() ? _SH_SECURE : _SH_DENYNO);

	if (f == nullptr) {
		switch (errno) {
			case ENOENT: {
				last_error = ERR_FILE_NOT_FOUND;
			} break;
			default: {
				last_error = ERR_FILE_CANT_OPEN;
			} break;
		}
		return last_error;
	} else {
		last_error = OK;
		flags = p_mode_flags;
		return OK;
	}
}

void FileAccessWindows::_close() {
	if (!f) {
		return;
	}

	fclose(f);
	f = nullptr;

	if (!save_path.is_empty()) {
		// This workaround of trying multiple times is added to deal with paranoid Windows
		// antiviruses that love reading just written files even if they are not executable, thus
		// locking the file and preventing renaming from happening.

		bool rename_error = true;
		const Char16String &path_utf16 = path.utf16();
		const Char16String &save_path_utf16 = save_path.utf16();
		for (int i = 0; i < 1000; i++) {
			if (ReplaceFileW((LPCWSTR)(save_path_utf16.get_data()), (LPCWSTR)(path_utf16.get_data()), nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS | REPLACEFILE_IGNORE_ACL_ERRORS, nullptr, nullptr)) {
				rename_error = false;
			} else {
				// Either the target exists and is locked (temporarily, hopefully)
				// or it doesn't exist; let's assume the latter before re-trying.
				rename_error = MoveFileW((LPCWSTR)(path_utf16.get_data()), (LPCWSTR)(save_path_utf16.get_data())) == 0;
			}

			if (!rename_error) {
				break;
			}

			OS::get_singleton()->delay_usec(1000);
		}

		if (rename_error) {
			if (close_fail_notify) {
				close_fail_notify(save_path);
			}
		}

		save_path = "";

		ERR_FAIL_COND_MSG(rename_error, "Safe save failed. This may be a permissions problem, but also may happen because you are running a paranoid antivirus. If this is the case, please switch to Windows Defender or disable the 'safe save' option in editor settings. This makes it work, but increases the risk of file corruption in a crash.");
	}
}

String FileAccessWindows::_get_path() const {
	return path_src;
}

String FileAccessWindows::get_path_absolute() const {
	return path.trim_prefix(R"(\\?\)").replace("\\", "/");
}

bool FileAccessWindows::is_open() const {
	return (f != nullptr);
}

void FileAccessWindows::seek(uint64_t p_position) {
	ERR_FAIL_NULL(f);

	if (_fseeki64(f, p_position, SEEK_SET)) {
		check_errors();
	}
	prev_op = 0;
}

void FileAccessWindows::seek_end(int64_t p_position) {
	ERR_FAIL_NULL(f);

	if (_fseeki64(f, p_position, SEEK_END)) {
		check_errors();
	}
	prev_op = 0;
}

uint64_t FileAccessWindows::get_position() const {
	int64_t aux_position = _ftelli64(f);
	if (aux_position < 0) {
		check_errors();
	}
	return aux_position;
}

uint64_t FileAccessWindows::get_length() const {
	ERR_FAIL_NULL_V(f, 0);

	uint64_t pos = get_position();
	_fseeki64(f, 0, SEEK_END);
	uint64_t size = get_position();
	_fseeki64(f, pos, SEEK_SET);

	return size;
}

bool FileAccessWindows::eof_reached() const {
	return feof(f);
}

uint64_t FileAccessWindows::get_buffer(uint8_t *p_dst, uint64_t p_length) const {
	ERR_FAIL_NULL_V(f, -1);
	ERR_FAIL_COND_V(!p_dst && p_length > 0, -1);

	if (flags == READ_WRITE || flags == WRITE_READ) {
		if (prev_op == WRITE) {
			fflush(f);
		}
		prev_op = READ;
	}

	uint64_t read = fread(p_dst, 1, p_length, f);
	check_errors();

	return read;
}

Error FileAccessWindows::get_error() const {
	return last_error;
}

Error FileAccessWindows::resize(int64_t p_length) {
	ERR_FAIL_NULL_V_MSG(f, FAILED, "File must be opened before use.");
	errno_t res = _chsize_s(_fileno(f), p_length);
	switch (res) {
		case 0:
			return OK;
		case EACCES:
		case EBADF:
			return ERR_FILE_CANT_OPEN;
		case ENOSPC:
			return ERR_OUT_OF_MEMORY;
		case EINVAL:
			return ERR_INVALID_PARAMETER;
		default:
			return FAILED;
	}
}

void FileAccessWindows::flush() {
	ERR_FAIL_NULL(f);

	fflush(f);
	if (prev_op == WRITE) {
		prev_op = 0;
	}
}

bool FileAccessWindows::store_buffer(const uint8_t *p_src, uint64_t p_length) {
	ERR_FAIL_NULL_V(f, false);
	ERR_FAIL_COND_V(!p_src && p_length > 0, false);

	if (flags == READ_WRITE || flags == WRITE_READ) {
		if (prev_op == READ) {
			if (last_error != ERR_FILE_EOF) {
				fseek(f, 0, SEEK_CUR);
			}
		}
		prev_op = WRITE;
	}

	bool res = fwrite(p_src, 1, p_length, f) == (size_t)p_length;
	check_errors(true);
	return res;
}

void FileAccessWindows::close() {
	_close();
}

FileAccessWindows::~FileAccessWindows() {
	_close();
}

#endif // WINDOWS_ENABLED

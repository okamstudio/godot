/**************************************************************************/
/*  path_utils.cpp                                                        */
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

#include "path_utils.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"

#include <stdlib.h>

#ifdef WINDOWS_ENABLED
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define ENV_PATH_SEP ";"
#else
#include <limits.h>
#include <unistd.h>

#define ENV_PATH_SEP ":"
#endif

namespace path {

String find_executable(const String &p_name) {
#ifdef WINDOWS_ENABLED
	Vector<String> exts = OS::get_singleton()->get_environment("PATHEXT").split(ENV_PATH_SEP, false);
#endif
	Vector<String> env_path = OS::get_singleton()->get_environment("PATH").split(ENV_PATH_SEP, false);

	if (env_path.is_empty()) {
		return String();
	}

	for (int i = 0; i < env_path.size(); i++) {
		String p = path::join(env_path[i], p_name);

#ifdef WINDOWS_ENABLED
		for (int j = 0; j < exts.size(); j++) {
			String p2 = p + exts[j].to_lower(); // lowercase to reduce risk of case mismatch warning

			if (FileAccess::exists(p2)) {
				return p2;
			}
		}
#else
		if (FileAccess::exists(p)) {
			return p;
		}
#endif
	}

	return String();
}

String cwd() {
#ifdef WINDOWS_ENABLED
	const DWORD expected_size = ::GetCurrentDirectoryW(0, nullptr);

	Char16String buffer;
	buffer.resize((int)expected_size);
	if (::GetCurrentDirectoryW(expected_size, (wchar_t *)buffer.ptrw()) == 0) {
		return ".";
	}

	String result;
	result.parse_utf16(buffer.ptr());
	if (result.is_empty()) {
		return ".";
	}
	return result.simplify_path();
#else
	char buffer[PATH_MAX];
	if (::getcwd(buffer, sizeof(buffer)) == nullptr) {
		return ".";
	}

	String result;
	if (result.parse_utf8(buffer) != OK) {
		return ".";
	}

	return result.simplify_path();
#endif
}

String abspath(const String &p_path) {
	if (p_path.is_absolute_path()) {
		return p_path.simplify_path();
	} else {
		return path::join(path::cwd(), p_path).simplify_path();
	}
}

String realpath(const String &p_path) {
#ifdef WINDOWS_ENABLED
	// Open file without read/write access
	HANDLE hFile = ::CreateFileW((LPCWSTR)(p_path.utf16().get_data()), 0,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (hFile == INVALID_HANDLE_VALUE) {
		return p_path;
	}

	const DWORD expected_size = ::GetFinalPathNameByHandleW(hFile, nullptr, 0, FILE_NAME_NORMALIZED);

	if (expected_size == 0) {
		::CloseHandle(hFile);
		return p_path;
	}

	Char16String buffer;
	buffer.resize((int)expected_size);
	::GetFinalPathNameByHandleW(hFile, (wchar_t *)buffer.ptrw(), expected_size, FILE_NAME_NORMALIZED);

	::CloseHandle(hFile);

	String result;
	result.parse_utf16(buffer.ptr());
	if (result.is_empty()) {
		return p_path;
	}

	return result.simplify_path();
#elif defined(UNIX_ENABLED)
	char *resolved_path = ::realpath(p_path.utf8().get_data(), nullptr);

	if (!resolved_path) {
		return p_path;
	}

	String result;
	Error parse_ok = result.parse_utf8(resolved_path);
	::free(resolved_path);

	if (parse_ok != OK) {
		return p_path;
	}

	return result.simplify_path();
#endif
}

String join(const String &p_a, const String &p_b) {
	if (p_a.is_empty()) {
		return p_b;
	}

	const char32_t a_last = p_a[p_a.length() - 1];
	if ((a_last == '/' || a_last == '\\') ||
			(p_b.size() > 0 && (p_b[0] == '/' || p_b[0] == '\\'))) {
		return p_a + p_b;
	}

	return p_a + "/" + p_b;
}

String join(const String &p_a, const String &p_b, const String &p_c) {
	return path::join(path::join(p_a, p_b), p_c);
}

String join(const String &p_a, const String &p_b, const String &p_c, const String &p_d) {
	return path::join(path::join(path::join(p_a, p_b), p_c), p_d);
}

String relative_to_impl(const String &p_path, const String &p_relative_to) {
	// This function assumes arguments are normalized and absolute paths

	if (p_path.begins_with(p_relative_to)) {
		return p_path.substr(p_relative_to.length() + 1);
	} else {
		String base_dir = p_relative_to.get_base_dir();

		if (base_dir.length() <= 2 && (base_dir.is_empty() || base_dir.ends_with(":"))) {
			return p_path;
		}

		return String("..").path_join(relative_to_impl(p_path, base_dir));
	}
}

#ifdef WINDOWS_ENABLED
String get_drive_letter(const String &p_norm_path) {
	int idx = p_norm_path.find(":/");
	if (idx != -1 && idx < p_norm_path.find_char('/')) {
		return p_norm_path.substr(0, idx + 1);
	}
	return String();
}
#endif

String relative_to(const String &p_path, const String &p_relative_to) {
	String relative_to_abs_norm = abspath(p_relative_to);
	String path_abs_norm = abspath(p_path);

#ifdef WINDOWS_ENABLED
	if (get_drive_letter(relative_to_abs_norm) != get_drive_letter(path_abs_norm)) {
		return path_abs_norm;
	}
#endif

	return relative_to_impl(path_abs_norm, relative_to_abs_norm);
}

const Vector<String> reserved_assembly_names = { "GodotSharp", "GodotSharpEditor", "Godot.SourceGenerators" };

String get_csharp_project_name() {
	String name = GLOBAL_GET("dotnet/project/assembly_name");
	if (name.is_empty()) {
		name = GLOBAL_GET("application/config/name");
		Vector<String> invalid_chars = Vector<String>({ //
				// Windows reserved filename chars.
				":", "*", "?", "\"", "<", ">", "|",
				// Directory separators.
				"/", "\\",
				// Other chars that have been found to break assembly loading.
				";", "'", "=", "," });
		name = name.strip_edges();
		for (int i = 0; i < invalid_chars.size(); i++) {
			name = name.replace(invalid_chars[i], "-");
		}
	}

	if (name.is_empty()) {
		name = "UnnamedProject";
	}

	// Avoid reserved names that conflict with Godot assemblies.
	if (reserved_assembly_names.has(name)) {
		name += "_";
	}

	return name;
}

} // namespace path

/**************************************************************************/
/*  hostfxr_resolver.cpp                                                  */
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

/*
Adapted to Godot from the nethost library: https://github.com/dotnet/runtime/tree/main/src/native/corehost
*/

/*
The MIT License (MIT)

Copyright (c) .NET Foundation and Contributors

All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "hostfxr_resolver.h"

#include "../utils/path_utils.h"
#include "semver.h"

#include "core/config/engine.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"

#ifdef WINDOWS_ENABLED
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// We don't use libnethost as it gives us issues with some compilers.
// This file tries to mimic libnethost's hostfxr_resolver search logic. We try to use the
// same function names for easier comparing in case we need to update this in the future.

namespace {

String get_hostfxr_file_name() {
#if defined(WINDOWS_ENABLED)
	return "hostfxr.dll";
#elif defined(MACOS_ENABLED) || defined(IOS_ENABLED)
	return "libhostfxr.dylib";
#else
	return "libhostfxr.so";
#endif
}

bool get_latest_fxr(const String &fxr_root, String &r_fxr_path) {
	godotsharp::SemVerParser sem_ver_parser;

	bool found_ver = false;
	godotsharp::SemVer latest_ver;
	String latest_ver_str;

	Ref<DirAccess> da = DirAccess::open(fxr_root);
	da->list_dir_begin();
	for (String dir = da->get_next(); !dir.is_empty(); dir = da->get_next()) {
		if (!da->current_is_dir() || dir == "." || dir == "..") {
			continue;
		}

		String ver = dir.get_file();

		godotsharp::SemVer fx_ver;
		if (sem_ver_parser.parse(ver, fx_ver)) {
			if (!found_ver || fx_ver > latest_ver) {
				latest_ver = fx_ver;
				latest_ver_str = ver;
				found_ver = true;
			}
		}
	}

	if (!found_ver) {
		return false;
	}

	String fxr_with_ver = path::join(fxr_root, latest_ver_str);
	String hostfxr_file_path = path::join(fxr_with_ver, get_hostfxr_file_name());

	ERR_FAIL_COND_V_MSG(!FileAccess::exists(hostfxr_file_path), false, "Missing hostfxr library in directory: " + fxr_with_ver);

	r_fxr_path = hostfxr_file_path;

	return true;
}

#ifdef WINDOWS_ENABLED
typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS)(HANDLE, PBOOL);

BOOL is_wow64() {
	BOOL wow64 = FALSE;

	LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process");

	if (fnIsWow64Process) {
		if (!fnIsWow64Process(GetCurrentProcess(), &wow64)) {
			wow64 = FALSE;
		}
	}

	return wow64;
}
#endif

static const char *arch_name_map[][2] = {
	{ "arm32", "arm" },
	{ "arm64", "arm64" },
	{ "rv64", "riscv64" },
	{ "x86_64", "x64" },
	{ "x86_32", "x86" },
	{ nullptr, nullptr }
};

String get_dotnet_arch() {
	String arch = Engine::get_singleton()->get_architecture_name();

	int idx = 0;
	while (arch_name_map[idx][0] != nullptr) {
		if (arch_name_map[idx][0] == arch) {
			return arch_name_map[idx][1];
		}
		idx++;
	}

	return "";
}

bool get_default_installation_dir(String &r_dotnet_root) {
#if defined(WINDOWS_ENABLED)
	String program_files_env;
	if (is_wow64()) {
		// Running x86 on x64, looking for x86 install
		program_files_env = "ProgramFiles(x86)";
	} else {
		program_files_env = "ProgramFiles";
	}

	String program_files_dir = OS::get_singleton()->get_environment(program_files_env);

	if (program_files_dir.is_empty()) {
		return false;
	}

#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
	// When emulating x64 on arm
	String dotnet_root_emulated = path::join(program_files_dir, "dotnet", "x64");
	if (FileAccess::exists(path::join(dotnet_root_emulated, "dotnet.exe"))) {
		r_dotnet_root = dotnet_root_emulated;
		return true;
	}
#endif

	r_dotnet_root = path::join(program_files_dir, "dotnet");
	return true;
#elif defined(MACOS_ENABLED)
	r_dotnet_root = "/usr/local/share/dotnet";

#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64__) || defined(_M_X64)
	// When emulating x64 on arm
	String dotnet_root_emulated = path::join(r_dotnet_root, "x64");
	if (FileAccess::exists(path::join(dotnet_root_emulated, "dotnet"))) {
		r_dotnet_root = dotnet_root_emulated;
		return true;
	}
#endif

	return true;
#else
	r_dotnet_root = "/usr/share/dotnet";
	return true;
#endif
}

#ifndef WINDOWS_ENABLED
bool get_install_location_from_file(const String &p_file_path, String &r_dotnet_root) {
	Error err = OK;
	Ref<FileAccess> f = FileAccess::open(p_file_path, FileAccess::READ, &err);

	if (f.is_null() || err != OK) {
		return false;
	}

	String line = f->get_line();

	if (line.is_empty()) {
		return false;
	}

	r_dotnet_root = line;
	return true;
}
#endif

bool get_dotnet_self_registered_dir(String &r_dotnet_root) {
#if defined(WINDOWS_ENABLED)
	String sub_key = "SOFTWARE\\dotnet\\Setup\\InstalledVersions\\" + get_dotnet_arch();
	Char16String value = String("InstallLocation").utf16();

	HKEY hkey = nullptr;
	LSTATUS result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, (LPCWSTR)(sub_key.utf16().get_data()), 0, KEY_READ | KEY_WOW64_32KEY, &hkey);
	if (result != ERROR_SUCCESS) {
		return false;
	}

	DWORD size = 0;
	result = RegGetValueW(hkey, nullptr, (LPCWSTR)(value.get_data()), RRF_RT_REG_SZ, nullptr, nullptr, &size);
	if (result != ERROR_SUCCESS || size == 0) {
		RegCloseKey(hkey);
		return false;
	}

	Vector<WCHAR> buffer;
	buffer.resize(size / sizeof(WCHAR));
	result = RegGetValueW(hkey, nullptr, (LPCWSTR)(value.get_data()), RRF_RT_REG_SZ, nullptr, (LPBYTE)buffer.ptrw(), &size);
	if (result != ERROR_SUCCESS) {
		RegCloseKey(hkey);
		return false;
	}

	r_dotnet_root = String::utf16((const char16_t *)buffer.ptr()).replace_char('\\', '/');
	RegCloseKey(hkey);
	return true;
#else
	String install_location_file = path::join("/etc/dotnet", "install_location_" + get_dotnet_arch().to_lower());
	if (get_install_location_from_file(install_location_file, r_dotnet_root)) {
		return true;
	}

	if (FileAccess::exists(install_location_file)) {
		// Don't try with the legacy location, this will fall back to the hard-coded default install location
		return false;
	}

	String legacy_install_location_file = path::join("/etc/dotnet", "install_location");
	return get_install_location_from_file(legacy_install_location_file, r_dotnet_root);
#endif
}

bool get_file_path_from_env(const String &p_env_key, String &r_dotnet_root) {
	String env_value = OS::get_singleton()->get_environment(p_env_key);

	if (!env_value.is_empty()) {
		env_value = path::realpath(env_value);

		if (DirAccess::exists(env_value)) {
			r_dotnet_root = env_value;
			return true;
		}
	}

	return false;
}

bool get_dotnet_root_from_env(String &r_dotnet_root) {
	String dotnet_root_env = "DOTNET_ROOT";
	String arch_for_env = get_dotnet_arch();

	if (!arch_for_env.is_empty()) {
		// DOTNET_ROOT_<arch>
		if (get_file_path_from_env(dotnet_root_env + "_" + arch_for_env.to_upper(), r_dotnet_root)) {
			return true;
		}
	}

#ifdef WINDOWS_ENABLED
	// WoW64-only: DOTNET_ROOT(x86)
	if (is_wow64() && get_file_path_from_env("DOTNET_ROOT(x86)", r_dotnet_root)) {
		return true;
	}
#endif

	// DOTNET_ROOT
	return get_file_path_from_env(dotnet_root_env, r_dotnet_root);
}

} //namespace

bool godotsharp::hostfxr_resolver::try_get_path_from_dotnet_root(const String &p_dotnet_root, String &r_fxr_path) {
	String fxr_dir = path::join(p_dotnet_root, "host", "fxr");
	if (!DirAccess::exists(fxr_dir)) {
		if (OS::get_singleton()->is_stdout_verbose()) {
			ERR_PRINT("The host fxr folder does not exist: " + fxr_dir + ".");
		}
		return false;
	}
	return get_latest_fxr(fxr_dir, r_fxr_path);
}

bool godotsharp::hostfxr_resolver::try_get_path(String &r_dotnet_root, String &r_fxr_path) {
	if (!get_dotnet_root_from_env(r_dotnet_root) &&
			!get_dotnet_self_registered_dir(r_dotnet_root) &&
			!get_default_installation_dir(r_dotnet_root)) {
		return false;
	}

	return try_get_path_from_dotnet_root(r_dotnet_root, r_fxr_path);
}

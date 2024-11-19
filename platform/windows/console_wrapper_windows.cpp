/**************************************************************************/
/*  console_wrapper_windows.cpp                                           */
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

#include <windows.h>

#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x4
#endif

int main(int argc, char *argv[]) {
	// Get executable name.
	WCHAR exe_name[32767] = {};
	if (!GetModuleFileNameW(nullptr, exe_name, 32767)) {
		wprintf(L"GetModuleFileName failed, error %d\n", GetLastError());
		return -1;
	}

	// Get product name from the resources and set console title.
	DWORD ver_info_handle = 0;
	DWORD ver_info_size = GetFileVersionInfoSizeW(exe_name, &ver_info_handle);
	if (ver_info_size > 0) {
		LPBYTE ver_info = (LPBYTE)malloc(ver_info_size);
		if (ver_info) {
			if (GetFileVersionInfoW(exe_name, ver_info_handle, ver_info_size, ver_info)) {
				LPCWSTR text_ptr = nullptr;
				UINT text_size = 0;
				if (VerQueryValueW(ver_info, L"\\StringFileInfo\\040904b0\\ProductName", (void **)&text_ptr, &text_size) && (text_size > 0)) {
					SetConsoleTitleW(text_ptr);
				}
			}
			free(ver_info);
		}
	}

	// Enable virtual terminal sequences processing.
	HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD out_mode = 0;
	GetConsoleMode(stdout_handle, &out_mode);
	out_mode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(stdout_handle, out_mode);

	// Find main executable name and check if it exist.
	static PCWSTR exe_renames[] = {
		L".console.exe",
		L"_console.exe",
		L" console.exe",
		L"console.exe",
		nullptr,
	};

	bool rename_found = false;
	for (int i = 0; exe_renames[i]; i++) {
		PWSTR c = StrRStrIW(exe_name, nullptr, exe_renames[i]);
		if (c) {
			CopyMemory(c, L".exe", sizeof(WCHAR) * 5);
			rename_found = true;
			break;
		}
	}
	if (!rename_found) {
		wprintf(L"Invalid wrapper executable name.\n");
		return -1;
	}

	DWORD file_attrib = GetFileAttributesW(exe_name);
	if (file_attrib == INVALID_FILE_ATTRIBUTES || (file_attrib & FILE_ATTRIBUTE_DIRECTORY)) {
		wprintf(L"Main executable %ls not found.\n", exe_name);
		return -1;
	}

	// Create job to monitor process tree.
	HANDLE job_handle = CreateJobObjectW(nullptr, nullptr);
	if (!job_handle) {
		wprintf(L"CreateJobObject failed, error %d\n", GetLastError());
		return -1;
	}

	HANDLE io_port_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
	if (!io_port_handle) {
		wprintf(L"CreateIoCompletionPort failed, error %d\n", GetLastError());
		return -1;
	}

	JOBOBJECT_ASSOCIATE_COMPLETION_PORT compl_port;
	ZeroMemory(&compl_port, sizeof(compl_port));
	compl_port.CompletionKey = job_handle;
	compl_port.CompletionPort = io_port_handle;

	if (!SetInformationJobObject(job_handle, JobObjectAssociateCompletionPortInformation, &compl_port, sizeof(compl_port))) {
		wprintf(L"SetInformationJobObject(AssociateCompletionPortInformation) failed, error %d\n", GetLastError());
		return -1;
	}

	JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli;
	ZeroMemory(&jeli, sizeof(jeli));
	jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

	if (!SetInformationJobObject(job_handle, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli))) {
		wprintf(L"SetInformationJobObject(ExtendedLimitInformation) failed, error %d\n", GetLastError());
		return -1;
	}

	// Start the main process.
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	STARTUPINFOW si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

	WCHAR new_command_line[32767];
	_snwprintf_s(new_command_line, 32767, _TRUNCATE, L"\"%ls\" %ls", exe_name, PathGetArgsW(GetCommandLineW()));

	if (!CreateProcessW(nullptr, new_command_line, nullptr, nullptr, true, CREATE_SUSPENDED, nullptr, nullptr, &si, &pi)) {
		wprintf(L"CreateProcess failed, error %d\n", GetLastError());
		return -1;
	}

	if (!AssignProcessToJobObject(job_handle, pi.hProcess)) {
		wprintf(L"AssignProcessToJobObject failed, error %d\n", GetLastError());
		return -1;
	}

	ResumeThread(pi.hThread);
	CloseHandle(pi.hThread);

	// Wait until main process and all of its children are finished.
	DWORD completion_code = 0;
	ULONG_PTR completion_key = 0;
	LPOVERLAPPED overlapped = nullptr;

	while (GetQueuedCompletionStatus(io_port_handle, &completion_code, &completion_key, &overlapped, INFINITE)) {
		if ((HANDLE)completion_key == job_handle && completion_code == JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO) {
			break;
		}
	}

	CloseHandle(job_handle);
	CloseHandle(io_port_handle);

	// Get exit code of the main process.
	DWORD exit_code = 0;
	GetExitCodeProcess(pi.hProcess, &exit_code);

	CloseHandle(pi.hProcess);

	return exit_code;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	return main(0, nullptr);
}

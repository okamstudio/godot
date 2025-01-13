/**************************************************************************/
/*  windows_utils.cpp                                                     */
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

#include "windows_utils.h"

#ifdef WINDOWS_ENABLED

#include "core/error/error_macros.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef FAILED // Overrides Error::FAILED

// dbghelp is linked only in DEBUG_ENABLED builds.
#ifdef DEBUG_ENABLED
#include <dbghelp.h>
#endif
#include <winnt.h>

HashMap<String, Vector<String>> WindowsUtils::temp_pdbs;

Error WindowsUtils::copy_and_rename_pdb(const String &p_dll_path) {
#ifdef DEBUG_ENABLED
	// 1000 ought to be enough for anybody, in case the debugger does not unblock previous PDBs.
	// Usually no more than 2 will be used.
	const int max_pdb_names = 1000;

	struct PDBResourceInfo {
		uint32_t address = 0;
		String path;
	} pdb_info;

	// Open and read the PDB information if available.
	{
		ULONG dbg_info_size = 0;
		DWORD dbg_info_position = 0;

		{
			// The custom LoadLibraryExW is used instead of open_dynamic_library
			// to avoid loading the original PDB into the debugger.
			HMODULE library_ptr = LoadLibraryExW((LPCWSTR)(p_dll_path.utf16().get_data()), nullptr, LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE);

			ERR_FAIL_NULL_V_MSG(library_ptr, ERR_FILE_CANT_OPEN, vformat("Failed to load library '%s'.", p_dll_path));

			IMAGE_DEBUG_DIRECTORY *dbg_dir = (IMAGE_DEBUG_DIRECTORY *)ImageDirectoryEntryToDataEx(library_ptr, FALSE, IMAGE_DIRECTORY_ENTRY_DEBUG, &dbg_info_size, nullptr);

			bool has_debug = dbg_dir && dbg_dir->Type == IMAGE_DEBUG_TYPE_CODEVIEW;
			if (has_debug) {
				dbg_info_position = dbg_dir->PointerToRawData;
				dbg_info_size = dbg_dir->SizeOfData;
			}

			ERR_FAIL_COND_V_MSG(!FreeLibrary((HMODULE)library_ptr), FAILED, vformat("Failed to free library '%s'.", p_dll_path));

			if (!has_debug) {
				// Skip with no debugging symbols.
				return ERR_SKIP;
			}
		}

		struct CV_HEADER {
			DWORD Signature;
			DWORD Offset;
		};

		const DWORD nb10_magic = 0x3031424e; // "01BN" (little-endian)
		struct CV_INFO_PDB20 {
			CV_HEADER CvHeader; // CvHeader.Signature = "NB10"
			DWORD Signature;
			DWORD Age;
			BYTE PdbFileName[1];
		};

		const DWORD rsds_magic = 0x53445352; // "SDSR" (little-endian)
		struct CV_INFO_PDB70 {
			DWORD Signature; // "RSDS"
			BYTE Guid[16];
			DWORD Age;
			BYTE PdbFileName[1];
		};

		Vector<uint8_t> dll_data;

		{
			Error err = OK;
			Ref<FileAccess> file = FileAccess::open(p_dll_path, FileAccess::READ, &err);
			ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Failed to read library '%s'.", p_dll_path));

			file->seek(dbg_info_position);
			dll_data = file->get_buffer(dbg_info_size);
			ERR_FAIL_COND_V_MSG(file->get_error() != OK, file->get_error(), vformat("Failed to read data from library '%s'.", p_dll_path));
		}

		const char *raw_pdb_path = nullptr;
		int raw_pdb_offset = 0;
		DWORD *pdb_info_signature = (DWORD *)dll_data.ptr();

		if (*pdb_info_signature == rsds_magic) {
			raw_pdb_path = (const char *)(((CV_INFO_PDB70 *)pdb_info_signature)->PdbFileName);
			raw_pdb_offset = offsetof(CV_INFO_PDB70, PdbFileName);
		} else if (*pdb_info_signature == nb10_magic) {
			// Not even sure if this format still exists anywhere...
			raw_pdb_path = (const char *)(((CV_INFO_PDB20 *)pdb_info_signature)->PdbFileName);
			raw_pdb_offset = offsetof(CV_INFO_PDB20, PdbFileName);
		} else {
			ERR_FAIL_V_MSG(FAILED, vformat("Unknown PDB format in '%s'.", p_dll_path));
		}

		String utf_path;
		Error err = utf_path.parse_utf8(raw_pdb_path);
		ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Failed to read PDB path from '%s'.", p_dll_path));

		pdb_info.path = utf_path;
		pdb_info.address = dbg_info_position + raw_pdb_offset;
	}

	String dll_base_dir = p_dll_path.get_base_dir();
	String copy_pdb_path = pdb_info.path;

	// Attempting to find the PDB by absolute and relative paths.
	if (copy_pdb_path.is_relative_path()) {
		copy_pdb_path = dll_base_dir.path_join(copy_pdb_path);
		if (!FileAccess::exists(copy_pdb_path)) {
			copy_pdb_path = dll_base_dir.path_join(copy_pdb_path.get_file());
		}
	} else if (!FileAccess::exists(copy_pdb_path)) {
		copy_pdb_path = dll_base_dir.path_join(copy_pdb_path.get_file());
	}
	if (!FileAccess::exists(copy_pdb_path)) {
		// The PDB file may be distributed separately on purpose, so we don't consider this an error.
		WARN_VERBOSE(vformat("PDB file '%s' for library '%s' was not found, skipping copy/rename.", copy_pdb_path, p_dll_path));
		return ERR_SKIP;
	}

	String new_pdb_base_name = p_dll_path.get_file().get_basename() + "_";

	// Checking the available space for the updated string
	// and trying to shorten it if there is not much space.
	{
		// e.g. 999.pdb
		const uint8_t suffix_size = String::num_characters((int64_t)max_pdb_names - 1) + 4;
		// e.g. ~lib_ + 1 for the \0
		const uint8_t min_base_size = 5 + 1;
		int original_path_size = pdb_info.path.utf8().length();
		CharString utf8_name = new_pdb_base_name.utf8();
		int new_expected_buffer_size = utf8_name.length() + suffix_size;

		// Since we have limited space inside the DLL to patch the path to the PDB,
		// it is necessary to limit the size based on the number of bytes occupied by the string.
		if (new_expected_buffer_size > original_path_size) {
			ERR_FAIL_COND_V_MSG(original_path_size < min_base_size + suffix_size, FAILED, vformat("The original PDB path size in bytes is too small: '%s'. Expected size: %d or more bytes, but available %d.", pdb_info.path, min_base_size + suffix_size, original_path_size));

			new_pdb_base_name.parse_utf8(utf8_name, original_path_size - suffix_size);
			new_pdb_base_name[new_pdb_base_name.length() - 1] = '_'; // Restore the last '_'
			WARN_PRINT(vformat("The original path size of '%s' in bytes was too small to fit the new name, so it was shortened to '%s%d.pdb'.", pdb_info.path, new_pdb_base_name, max_pdb_names - 1));
		}
	}

	// Delete old PDB files.
	for (const String &file : DirAccess::get_files_at(dll_base_dir)) {
		if (file.begins_with(new_pdb_base_name) && file.ends_with(".pdb")) {
			String path = dll_base_dir.path_join(file);

			// Just try to delete without showing any errors.
			Error err = DirAccess::remove_absolute(path);
			if (err == OK && temp_pdbs[p_dll_path].has(path)) {
				temp_pdbs[p_dll_path].erase(path);
			}
		}
	}

	// Try to copy PDB with new name and patch DLL.
	Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	for (int i = 0; i < max_pdb_names; i++) {
		String new_pdb_name = vformat("%s%d.pdb", new_pdb_base_name, i);
		String new_pdb_path = dll_base_dir.path_join(new_pdb_name);
		Error err = OK;

		Ref<FileAccess> test_pdb_is_locked = FileAccess::open(new_pdb_path, FileAccess::READ_WRITE, &err);
		if (err == ERR_FILE_CANT_OPEN) {
			// If the file is blocked, continue searching.
			continue;
		} else if (err != OK && err != ERR_FILE_NOT_FOUND) {
			ERR_FAIL_V_MSG(err, vformat("Failed to open '%s' to check if it is locked.", new_pdb_path));
		}

		err = d->copy(copy_pdb_path, new_pdb_path);
		ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Failed to copy PDB from '%s' to '%s'.", copy_pdb_path, new_pdb_path));
		temp_pdbs[p_dll_path].append(new_pdb_path);

		Ref<FileAccess> file = FileAccess::open(p_dll_path, FileAccess::READ_WRITE, &err);
		ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Failed to open '%s' to patch the PDB path.", p_dll_path));

		int original_path_size = pdb_info.path.utf8().length();
		// Double-check file bounds.
		ERR_FAIL_UNSIGNED_INDEX_V_MSG(pdb_info.address + original_path_size, file->get_length(), FAILED, vformat("Failed to write a new PDB path. Probably '%s' has been changed.", p_dll_path));

		Vector<uint8_t> u8 = new_pdb_name.to_utf8_buffer();
		file->seek(pdb_info.address);
		file->store_buffer(u8);

		// Terminate string and fill the remaining part of the original string with the '\0'.
		// Can be replaced by file->store_8('\0');
		Vector<uint8_t> padding_buffer;
		padding_buffer.resize((int64_t)original_path_size - u8.size());
		padding_buffer.fill('\0');
		file->store_buffer(padding_buffer);
		ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Failed to write a new PDB path to '%s'.", p_dll_path));

		return OK;
	}

	ERR_FAIL_V_MSG(FAILED, vformat("Failed to find an unblocked PDB name for '%s' among %d files.", p_dll_path, max_pdb_names));
#else
	WARN_PRINT_ONCE("Renaming PDB files is only available in debug builds. If your libraries use PDB files, then the original ones will be used.");
	return ERR_SKIP;
#endif
}

void WindowsUtils::remove_temp_pdbs(const String &p_dll_path) {
#ifdef DEBUG_ENABLED
	if (temp_pdbs.has(p_dll_path)) {
		Vector<String> removed;
		int failed = 0;
		const int failed_limit = 10;
		for (const String &pdb : temp_pdbs[p_dll_path]) {
			if (FileAccess::exists(pdb)) {
				Error err = DirAccess::remove_absolute(pdb);
				if (err == OK) {
					removed.append(pdb);
				} else {
					failed++;
					if (failed <= failed_limit) {
						print_verbose("Failed to remove temp PDB: " + pdb);
					}
				}
			} else {
				removed.append(pdb);
			}
		}

		if (failed > failed_limit) {
			print_verbose(vformat("And %d more PDB files could not be removed....", failed - failed_limit));
		}

		for (const String &pdb : removed) {
			temp_pdbs[p_dll_path].erase(pdb);
		}
	}
#endif
}

#endif // WINDOWS_ENABLED

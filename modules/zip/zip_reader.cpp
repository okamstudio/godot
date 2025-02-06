/**************************************************************************/
/*  zip_reader.cpp                                                        */
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

#include "zip_reader.h"

#include "core/error/error_macros.h"
#include "core/io/zip_io.h"

Error ZIPReader::open(const String &p_path) {
	if (fa.is_valid()) {
		close();
	}

	zlib_filefunc_def io = zipio_create_io(&fa);
	uzf = unzOpen2(p_path.utf8().get_data(), &io);
	return uzf != nullptr ? OK : FAILED;
}

Error ZIPReader::close() {
	ERR_FAIL_COND_V_MSG(fa.is_null(), FAILED, "ZIPReader cannot be closed because it is not open.");

	Error err = unzClose(uzf) == UNZ_OK ? OK : FAILED;
	if (err == OK) {
		DEV_ASSERT(fa.is_null());
		uzf = nullptr;
	}

	return err;
}

PackedStringArray ZIPReader::get_files() {
	ERR_FAIL_COND_V_MSG(fa.is_null(), PackedStringArray(), "ZIPReader must be opened before use.");

	unz_global_info gi;
	int err = unzGetGlobalInfo(uzf, &gi);
	ERR_FAIL_COND_V(err != UNZ_OK, PackedStringArray());
	if (gi.number_entry == 0) {
		return PackedStringArray();
	}

	err = unzGoToFirstFile(uzf);
	ERR_FAIL_COND_V(err != UNZ_OK, PackedStringArray());

	List<String> s;
	do {
		unz_file_info64 file_info;
		String filepath;

		err = godot_unzip_get_current_file_info(uzf, file_info, filepath);
		if (err == UNZ_OK) {
			s.push_back(filepath);
		}
	} while (unzGoToNextFile(uzf) == UNZ_OK);

	PackedStringArray arr;
	arr.resize(s.size());
	int idx = 0;
	for (const List<String>::Element *E = s.front(); E; E = E->next()) {
		arr.set(idx++, E->get());
	}
	return arr;
}

Array ZIPReader::get_files_raw() {
	ERR_FAIL_COND_V_MSG(fa.is_null(), Array(), "ZIPReader must be opened before use.");

	unz_global_info gi;
	int err = unzGetGlobalInfo(uzf, &gi);
	ERR_FAIL_COND_V(err != UNZ_OK, Array());
	if (gi.number_entry == 0) {
		return Array();
	}

	err = unzGoToFirstFile(uzf);
	ERR_FAIL_COND_V(err != UNZ_OK, Array());

	List<PackedByteArray> s;
	do {
		unz_file_info64 file_info;
		PackedByteArray filepath;

		err = godot_unzip_get_current_file_info_raw(uzf, file_info, filepath);
		if (err == UNZ_OK) {
			s.push_back(filepath);
		}
	} while (unzGoToNextFile(uzf) == UNZ_OK);

	Array arr;
	arr.resize(s.size());
	int idx = 0;
	for (const List<PackedByteArray>::Element *E = s.front(); E; E = E->next()) {
		arr.set(idx++, E->get());
	}
	return arr;
}

PackedByteArray ZIPReader::read_file(const String &p_path, bool p_case_sensitive) {
	ERR_FAIL_COND_V_MSG(fa.is_null(), PackedByteArray(), "ZIPReader must be opened before use.");

	// Locate and open the file.
	int err = godot_unzip_locate_file(uzf, p_path, p_case_sensitive);
	ERR_FAIL_COND_V_MSG(err != UNZ_OK, PackedByteArray(), vformat("File does not exist in zip archive: %s", p_path));

	return _read_file_base();
}

PackedByteArray ZIPReader::read_file_at(int64_t p_index) {
	ERR_FAIL_COND_V_MSG(fa.is_null(), PackedByteArray(), "ZIPReader must be opened before use.");

	// Locate and open the file.
	int err = godot_unzip_open_file(uzf, p_index);
	ERR_FAIL_COND_V_MSG(err != UNZ_OK, PackedByteArray(), vformat("File does not exist in zip archive: %s", p_index));

	return _read_file_base();
}

PackedByteArray ZIPReader::_read_file_base() {
	int err = unzOpenCurrentFile(uzf);
	ERR_FAIL_COND_V_MSG(err != UNZ_OK, PackedByteArray(), "Could not open file within zip archive.");

	// Read the file info.
	unz_file_info info;
	err = unzGetCurrentFileInfo(uzf, &info, nullptr, 0, nullptr, 0, nullptr, 0);
	ERR_FAIL_COND_V_MSG(err != UNZ_OK, PackedByteArray(), "Unable to read file information from zip archive.");
	ERR_FAIL_COND_V_MSG(info.uncompressed_size > INT_MAX, PackedByteArray(), "File contents too large to read from zip archive (>2 GB).");

	// Read the file data.
	PackedByteArray data;
	data.resize(info.uncompressed_size);
	uint8_t *buffer = data.ptrw();
	int to_read = data.size();
	while (to_read > 0) {
		int bytes_read = unzReadCurrentFile(uzf, buffer, to_read);
		ERR_FAIL_COND_V_MSG(bytes_read < 0, PackedByteArray(), "IO/zlib error reading file from zip archive.");
		ERR_FAIL_COND_V_MSG(bytes_read == UNZ_EOF && to_read != 0, PackedByteArray(), "Incomplete file read from zip archive.");
		DEV_ASSERT(bytes_read <= to_read);
		buffer += bytes_read;
		to_read -= bytes_read;
	}

	// Verify the data and return.
	err = unzCloseCurrentFile(uzf);
	ERR_FAIL_COND_V_MSG(err != UNZ_OK, PackedByteArray(), "CRC error reading file from zip archive.");
	return data;
}

bool ZIPReader::file_exists(const String &p_path, bool p_case_sensitive) {
	ERR_FAIL_COND_V_MSG(fa.is_null(), false, "ZIPReader must be opened before use.");

	int cs = p_case_sensitive ? 1 : 2;
	if (unzLocateFile(uzf, p_path.utf8().get_data(), cs) != UNZ_OK) {
		return false;
	}
	if (unzOpenCurrentFile(uzf) != UNZ_OK) {
		return false;
	}

	unzCloseCurrentFile(uzf);
	return true;
}

ZIPReader::ZIPReader() {}

ZIPReader::~ZIPReader() {
	if (fa.is_valid()) {
		close();
	}
}

void ZIPReader::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open", "path"), &ZIPReader::open);
	ClassDB::bind_method(D_METHOD("close"), &ZIPReader::close);
	ClassDB::bind_method(D_METHOD("get_files"), &ZIPReader::get_files);
	ClassDB::bind_method(D_METHOD("get_files_raw"), &ZIPReader::get_files_raw);
	ClassDB::bind_method(D_METHOD("read_file", "path", "case_sensitive"), &ZIPReader::read_file, DEFVAL(Variant(true)));
	ClassDB::bind_method(D_METHOD("read_file_at", "index"), &ZIPReader::read_file_at);
	ClassDB::bind_method(D_METHOD("file_exists", "path", "case_sensitive"), &ZIPReader::file_exists, DEFVAL(Variant(true)));
}

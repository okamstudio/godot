/*************************************************************************/
/*  zip_reader.cpp                                                       */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "zip_reader.h"

#include "core/error_macros.h"
#include "core/io/zip_io.h"

Error ZIPReader::open(String p_path) {
	if (fa) {
		close();
		fa = NULL;
	}

	zlib_filefunc_def io = zipio_create_io_from_file(&fa);
	uzf = unzOpen2(p_path.utf8().get_data(), &io);
	return uzf != NULL ? OK : FAILED;
}

Error ZIPReader::close() {
	ERR_FAIL_COND_V_MSG(!fa, FAILED, "ZIPReader cannot be closed because it is not open.");

	return unzClose(uzf) == UNZ_OK ? OK : FAILED;
}

PoolStringArray ZIPReader::get_files() {
	ERR_FAIL_COND_V_MSG(!fa, PoolStringArray(), "ZIPReader must be opened before use.");

	List<String> s;

	if (unzGoToFirstFile(uzf) != UNZ_OK) {
		return PoolStringArray();
	}

	do {
		unz_file_info64 file_info;
		char filename[256]; // Note filename is a path !
		int err = unzGetCurrentFileInfo64(uzf, &file_info, filename, sizeof(filename), NULL, 0, NULL, 0);
		if (err == UNZ_OK) {
			s.push_back(filename);
		} else {
			// Assume filename buffer was too small
			char *long_filename_buff = (char *)memalloc(file_info.size_filename);
			int err2 = unzGetCurrentFileInfo64(uzf, NULL, long_filename_buff, sizeof(long_filename_buff), NULL, 0, NULL, 0);
			if (err2 == UNZ_OK) {
				s.push_back(long_filename_buff);
				memfree(long_filename_buff);
			}
		}
	} while (unzGoToNextFile(uzf) == UNZ_OK);

	PoolStringArray arr;
	arr.resize(s.size());
	int idx = 0;
	for (const List<String>::Element *E = s.front(); E; E = E->next()) {
		arr.set(idx++, E->get());
	}
	return arr;
}

PoolByteArray ZIPReader::read_file(String p_path, bool p_case_sensitive) {
	ERR_FAIL_COND_V_MSG(!fa, PoolByteArray(), "ZIPReader must be opened before use.");

	int cs = p_case_sensitive ? 1 : 2;
	if (unzLocateFile(uzf, p_path.utf8().get_data(), cs) != UNZ_OK) {
		ERR_FAIL_V_MSG(PoolByteArray(), "File does not exist in zip archive: " + p_path);
	}
	if (unzOpenCurrentFile(uzf) != UNZ_OK) {
		ERR_FAIL_V_MSG(PoolByteArray(), "Could not open file within zip archive.");
	}

	unz_file_info info;
	unzGetCurrentFileInfo(uzf, &info, NULL, 0, NULL, 0, NULL, 0);
	PoolByteArray data;
	data.resize(info.uncompressed_size);

	PoolByteArray::Write w = data.write();
	unzReadCurrentFile(uzf, &w[0], info.uncompressed_size);

	unzCloseCurrentFile(uzf);
	return data;
}

ZIPReader::ZIPReader() {
	fa = NULL;
}

ZIPReader::~ZIPReader() {
	if (fa) {
		close();
		fa = NULL;
	}
}

void ZIPReader::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open", "path"), &ZIPReader::open);
	ClassDB::bind_method(D_METHOD("close"), &ZIPReader::close);
	ClassDB::bind_method(D_METHOD("get_files"), &ZIPReader::get_files);
	ClassDB::bind_method(D_METHOD("read_file", "path", "case_sensitive"), &ZIPReader::read_file, DEFVAL(Variant(true)));
}

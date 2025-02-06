/**************************************************************************/
/*  zip_packer.cpp                                                        */
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

#include "zip_packer.h"

#include "core/io/zip_io.h"
#include "core/os/os.h"

Error ZIPPacker::open(const String &p_path, ZipAppend p_append) {
	if (fa.is_valid()) {
		close();
	}

	zlib_filefunc_def io = zipio_create_io(&fa);
	zf = zipOpen2(p_path.utf8().get_data(), p_append, nullptr, &io);
	return zf != nullptr ? OK : FAILED;
}

Error ZIPPacker::close() {
	ERR_FAIL_COND_V_MSG(fa.is_null(), FAILED, "ZIPPacker cannot be closed because it is not open.");

	Error err = zipClose(zf, nullptr) == ZIP_OK ? OK : FAILED;
	if (err == OK) {
		DEV_ASSERT(fa.is_null());
		zf = nullptr;
	}

	return err;
}

Error ZIPPacker::start_file(const String &p_path) {
	ERR_FAIL_COND_V_MSG(fa.is_null(), FAILED, "ZIPPacker must be opened before use.");

	zip_fileinfo zipfi;

	OS::DateTime time = OS::get_singleton()->get_datetime();

	zipfi.tmz_date.tm_sec = time.second;
	zipfi.tmz_date.tm_min = time.minute;
	zipfi.tmz_date.tm_hour = time.hour;
	zipfi.tmz_date.tm_mday = time.day;
	zipfi.tmz_date.tm_mon = time.month - 1;
	zipfi.tmz_date.tm_year = time.year;
	zipfi.dosDate = 0;
	zipfi.internal_fa = 0;
	zipfi.external_fa = 0;

	int err = zipOpenNewFileInZip4(zf,
			p_path.utf8().get_data(),
			&zipfi,
			nullptr,
			0,
			nullptr,
			0,
			nullptr,
			Z_DEFLATED,
			Z_DEFAULT_COMPRESSION,
			0,
			-MAX_WBITS,
			DEF_MEM_LEVEL,
			Z_DEFAULT_STRATEGY,
			nullptr,
			0,
			0, // "version made by", indicates the compatibility of the file attribute information (the `external_fa` field above).
			1 << 11); // Bit 11 is the language encoding flag. When set, filename and comment fields must be encoded using UTF-8.
	return err == ZIP_OK ? OK : FAILED;
}

Error ZIPPacker::write_file(const Vector<uint8_t> &p_data) {
	ERR_FAIL_COND_V_MSG(fa.is_null(), FAILED, "ZIPPacker must be opened before use.");

	return zipWriteInFileInZip(zf, p_data.ptr(), p_data.size()) == ZIP_OK ? OK : FAILED;
}

Error ZIPPacker::close_file() {
	ERR_FAIL_COND_V_MSG(fa.is_null(), FAILED, "ZIPPacker must be opened before use.");

	return zipCloseFileInZip(zf) == ZIP_OK ? OK : FAILED;
}

Error ZIPPacker::copy_file(String p_source, String p_target_path) {
	Error err = start_file(p_target_path);
	if (err != OK) {
		return err;
	}

	const PackedByteArray bytes = FileAccess::get_file_as_bytes(p_source, &err);
	if (err != OK) {
		close_file();
		return err;
	}

	err = write_file(bytes);
	if (err != OK) {
		close_file();
		return err;
	}

	err = close_file();
	return err;
}

void ZIPPacker::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open", "path", "append"), &ZIPPacker::open, DEFVAL(Variant(APPEND_CREATE)));
	ClassDB::bind_method(D_METHOD("start_file", "path"), &ZIPPacker::start_file);
	ClassDB::bind_method(D_METHOD("write_file", "data"), &ZIPPacker::write_file);
	ClassDB::bind_method(D_METHOD("copy_file", "source", "path"), &ZIPPacker::copy_file);
	ClassDB::bind_method(D_METHOD("close_file"), &ZIPPacker::close_file);
	ClassDB::bind_method(D_METHOD("close"), &ZIPPacker::close);

	BIND_ENUM_CONSTANT(APPEND_CREATE);
	BIND_ENUM_CONSTANT(APPEND_CREATEAFTER);
	BIND_ENUM_CONSTANT(APPEND_ADDINZIP);
}

ZIPPacker::ZIPPacker() {}

ZIPPacker::~ZIPPacker() {
	if (fa.is_valid()) {
		close();
	}
}

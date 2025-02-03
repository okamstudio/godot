/*************************************************************************/
/*  zip_packer.cpp                                                       */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2018 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2018 Godot Engine contributors (cf. AUTHORS.md)    */
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

#include "core/io/zip_io.h"

#include "zip_packer.h"

Error ZIPPacker::open(String path, ZipAppend append) {
	if (f) {
		close();
	}

	zlib_filefunc_def io = zipio_create_io_from_file(&f);
	zf = zipOpen2(path.utf8().get_data(), append, NULL, &io);
	return zf != NULL ? OK : FAILED;
}

Error ZIPPacker::close() {
	ERR_FAIL_COND_V_MSG(!f, FAILED, "ZIPPacker cannot be closed because it is not open.");

	return zipClose(zf, NULL) == ZIP_OK ? OK : FAILED;
}

Error ZIPPacker::start_file(String path) {
	ERR_FAIL_COND_V_MSG(!f, FAILED, "ZIPPacker must be opened before use.");

	zip_fileinfo zipfi;

	OS::Time time = OS::get_singleton()->get_time();
	OS::Date date = OS::get_singleton()->get_date();

	zipfi.tmz_date.tm_hour = time.hour;
	zipfi.tmz_date.tm_mday = date.day;
	zipfi.tmz_date.tm_min = time.min;
	zipfi.tmz_date.tm_mon = date.month - 1;
	zipfi.tmz_date.tm_sec = time.sec;
	zipfi.tmz_date.tm_year = date.year;
	zipfi.dosDate = 0;
	zipfi.external_fa = 0;
	zipfi.internal_fa = 0;

	int ret = zipOpenNewFileInZip(zf, path.utf8().get_data(), &zipfi, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_DEFAULT_COMPRESSION);
	return ret == ZIP_OK ? OK : FAILED;
}

Error ZIPPacker::write_file(Vector<uint8_t> data) {
	ERR_FAIL_COND_V_MSG(!f, FAILED, "ZIPPacker must be opened before use.");

	return zipWriteInFileInZip(zf, data.ptr(), data.size()) == ZIP_OK ? OK : FAILED;
}

Error ZIPPacker::close_file() {
	ERR_FAIL_COND_V_MSG(!f, FAILED, "ZIPPacker must be opened before use.");

	return zipCloseFileInZip(zf) == ZIP_OK ? OK : FAILED;
}

void ZIPPacker::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open", "path", "append"), &ZIPPacker::open, DEFVAL(Variant(APPEND_CREATE)));
	ClassDB::bind_method(D_METHOD("start_file", "path"), &ZIPPacker::start_file);
	ClassDB::bind_method(D_METHOD("write_file", "data"), &ZIPPacker::write_file);
	ClassDB::bind_method(D_METHOD("close_file"), &ZIPPacker::close_file);
	ClassDB::bind_method(D_METHOD("close"), &ZIPPacker::close);

	BIND_ENUM_CONSTANT(APPEND_CREATE);
	BIND_ENUM_CONSTANT(APPEND_CREATEAFTER);
	BIND_ENUM_CONSTANT(APPEND_ADDINZIP);
}

ZIPPacker::ZIPPacker() {
	f = NULL;
}

ZIPPacker::~ZIPPacker() {
	if (f) {
		close();
	}
}

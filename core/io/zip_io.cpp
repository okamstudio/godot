/**************************************************************************/
/*  zip_io.cpp                                                            */
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

#include "zip_io.h"

#include "core/templates/local_vector.h"

int godot_unzip_get_current_file_info(unzFile p_zip_file, unz_file_info64 &r_file_info, String &r_filepath) {
	const uLong short_file_path_buffer_size = 16384ul;
	char short_file_path_buffer[short_file_path_buffer_size];

	int err = unzGetCurrentFileInfo64(p_zip_file, &r_file_info, short_file_path_buffer, short_file_path_buffer_size, nullptr, 0, nullptr, 0);
	if (unlikely((err != UNZ_OK) || (r_file_info.size_filename > short_file_path_buffer_size))) {
		LocalVector<char> long_file_path_buffer;
		long_file_path_buffer.resize(r_file_info.size_filename);

		err = unzGetCurrentFileInfo64(p_zip_file, &r_file_info, long_file_path_buffer.ptr(), long_file_path_buffer.size(), nullptr, 0, nullptr, 0);
		if (err != UNZ_OK) {
			return err;
		}
		r_filepath = String::utf8(long_file_path_buffer.ptr(), r_file_info.size_filename);
	} else {
		r_filepath = String::utf8(short_file_path_buffer, r_file_info.size_filename);
	}

	return err;
}

int godot_unzip_get_current_file_info_raw(unzFile p_zip_file, unz_file_info64 &r_file_info, PackedByteArray &r_filepath) {
	const uLong short_file_path_buffer_size = 16384ul;
	char short_file_path_buffer[short_file_path_buffer_size];

	int err = unzGetCurrentFileInfo64(p_zip_file, &r_file_info, short_file_path_buffer, short_file_path_buffer_size, nullptr, 0, nullptr, 0);
	if (unlikely((err != UNZ_OK) || (r_file_info.size_filename > short_file_path_buffer_size))) {
		PackedByteArray long_file_path_buffer;
		long_file_path_buffer.resize(r_file_info.size_filename);

		err = unzGetCurrentFileInfo64(p_zip_file, &r_file_info, (char *)long_file_path_buffer.ptr(), long_file_path_buffer.size(), nullptr, 0, nullptr, 0);
		if (err != UNZ_OK) {
			return err;
		}
		r_filepath = long_file_path_buffer;
	} else {
		r_filepath.resize(r_file_info.size_filename);
		memcpy(r_filepath.ptrw(), short_file_path_buffer, r_file_info.size_filename);
	}

	return err;
}

int godot_unzip_locate_file(unzFile p_zip_file, const String &p_filepath, bool p_case_sensitive) {
	int err = unzGoToFirstFile(p_zip_file);
	while (err == UNZ_OK) {
		unz_file_info64 current_file_info;
		String current_filepath;
		err = godot_unzip_get_current_file_info(p_zip_file, current_file_info, current_filepath);
		if (err == UNZ_OK) {
			bool filepaths_are_equal = p_case_sensitive ? (p_filepath == current_filepath) : (p_filepath.nocasecmp_to(current_filepath) == 0);
			if (filepaths_are_equal) {
				return UNZ_OK;
			}
			err = unzGoToNextFile(p_zip_file);
		}
	}
	return err;
}

int godot_unzip_open_file(unzFile p_zip_file, int64_t p_index) {
	int64_t idx = 0;
	int err = unzGoToFirstFile(p_zip_file);
	while (err == UNZ_OK && idx < p_index) {
		err = unzGoToNextFile(p_zip_file);
		idx++;
	}
	return err;
}

//

void *zipio_open(voidpf opaque, const char *p_fname, int mode) {
	Ref<FileAccess> *fa = reinterpret_cast<Ref<FileAccess> *>(opaque);
	ERR_FAIL_NULL_V(fa, nullptr);

	String fname;
	fname.parse_utf8(p_fname);

	int file_access_mode = 0;
	if (mode & ZLIB_FILEFUNC_MODE_READ) {
		file_access_mode |= FileAccess::READ;
	}
	if (mode & ZLIB_FILEFUNC_MODE_WRITE) {
		file_access_mode |= FileAccess::WRITE;
	}
	if (mode & ZLIB_FILEFUNC_MODE_CREATE) {
		file_access_mode |= FileAccess::WRITE_READ;
	}

	(*fa) = FileAccess::open(fname, file_access_mode);
	if (fa->is_null()) {
		return nullptr;
	}

	return opaque;
}

uLong zipio_read(voidpf opaque, voidpf stream, void *buf, uLong size) {
	Ref<FileAccess> *fa = reinterpret_cast<Ref<FileAccess> *>(opaque);
	ERR_FAIL_NULL_V(fa, 0);
	ERR_FAIL_COND_V(fa->is_null(), 0);

	return (*fa)->get_buffer((uint8_t *)buf, size);
}

uLong zipio_write(voidpf opaque, voidpf stream, const void *buf, uLong size) {
	Ref<FileAccess> *fa = reinterpret_cast<Ref<FileAccess> *>(opaque);
	ERR_FAIL_NULL_V(fa, 0);
	ERR_FAIL_COND_V(fa->is_null(), 0);

	(*fa)->store_buffer((uint8_t *)buf, size);
	return size;
}

long zipio_tell(voidpf opaque, voidpf stream) {
	Ref<FileAccess> *fa = reinterpret_cast<Ref<FileAccess> *>(opaque);
	ERR_FAIL_NULL_V(fa, 0);
	ERR_FAIL_COND_V(fa->is_null(), 0);

	return (*fa)->get_position();
}

long zipio_seek(voidpf opaque, voidpf stream, uLong offset, int origin) {
	Ref<FileAccess> *fa = reinterpret_cast<Ref<FileAccess> *>(opaque);
	ERR_FAIL_NULL_V(fa, 0);
	ERR_FAIL_COND_V(fa->is_null(), 0);

	uint64_t pos = offset;
	switch (origin) {
		case ZLIB_FILEFUNC_SEEK_CUR:
			pos = (*fa)->get_position() + offset;
			break;
		case ZLIB_FILEFUNC_SEEK_END:
			pos = (*fa)->get_length() + offset;
			break;
		default:
			break;
	}

	(*fa)->seek(pos);
	return 0;
}

int zipio_close(voidpf opaque, voidpf stream) {
	Ref<FileAccess> *fa = reinterpret_cast<Ref<FileAccess> *>(opaque);
	ERR_FAIL_NULL_V(fa, 0);
	ERR_FAIL_COND_V(fa->is_null(), 0);

	fa->unref();
	return 0;
}

int zipio_testerror(voidpf opaque, voidpf stream) {
	Ref<FileAccess> *fa = reinterpret_cast<Ref<FileAccess> *>(opaque);
	ERR_FAIL_NULL_V(fa, 1);
	ERR_FAIL_COND_V(fa->is_null(), 0);

	return (fa->is_valid() && (*fa)->get_error() != OK) ? 1 : 0;
}

voidpf zipio_alloc(voidpf opaque, uInt items, uInt size) {
	voidpf ptr = memalloc((size_t)items * size);
	memset(ptr, 0, items * size);
	return ptr;
}

void zipio_free(voidpf opaque, voidpf address) {
	memfree(address);
}

zlib_filefunc_def zipio_create_io(Ref<FileAccess> *p_data) {
	zlib_filefunc_def io;
	io.opaque = (void *)p_data;
	io.zopen_file = zipio_open;
	io.zread_file = zipio_read;
	io.zwrite_file = zipio_write;
	io.ztell_file = zipio_tell;
	io.zseek_file = zipio_seek;
	io.zclose_file = zipio_close;
	io.zerror_file = zipio_testerror;
	io.alloc_mem = zipio_alloc;
	io.free_mem = zipio_free;
	return io;
}

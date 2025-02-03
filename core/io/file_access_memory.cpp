/**************************************************************************/
/*  file_access_memory.cpp                                                */
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

#include "file_access_memory.h"

#include "core/config/project_settings.h"

HashMap<String, FileAccessMemory::FileInfo *> FileAccessMemory::files;

Ref<FileAccess> FileAccessMemory::create() {
	return memnew(FileAccessMemory);
}

bool FileAccessMemory::file_exists(const String &p_name) {
	String name = fix_path(p_name).simplify_path();
	return files.has(name);
}

Error FileAccessMemory::open_custom(const uint8_t *p_data, uint64_t p_len) {
	close();

	data = (uint8_t *)p_data;
	length = p_len;
	return OK;
}

Error FileAccessMemory::open_internal(const String &p_path, int p_mode_flags) {
	close();

	String name = fix_path(p_path).simplify_path();
	HashMap<String, FileAccessMemory::FileInfo *>::Iterator it = files.find(name);
	if (it != nullptr) {
		info = it->value;
	} else {
		info = memnew(FileAccessMemory::FileInfo);
		files.insert(name, info);
	}
	ERR_FAIL_COND_V(!info, ERR_CANT_CREATE);

	filename = name;
	info->refc.increment();
	if (p_mode_flags == WRITE || p_mode_flags == WRITE_READ) {
		info->data.clear();
	}
	read_only = !(p_mode_flags & WRITE);

	return OK;
}

bool FileAccessMemory::is_open() const {
	return (data != nullptr) || (info != nullptr);
}

void FileAccessMemory::seek(uint64_t p_position) {
	ERR_FAIL_COND(!data && !info);
	pos = p_position;
}

void FileAccessMemory::seek_end(int64_t p_position) {
	ERR_FAIL_COND(!data && !info);
	if (info) {
		pos = (uint64_t)info->data.size() + p_position;
	} else {
		pos = length + p_position;
	}
}

uint64_t FileAccessMemory::get_position() const {
	ERR_FAIL_COND_V(!data && !info, 0);
	return pos;
}

uint64_t FileAccessMemory::get_length() const {
	ERR_FAIL_COND_V(!data && !info, 0);
	if (info) {
		return (uint64_t)info->data.size();
	} else {
		return length;
	}
}

bool FileAccessMemory::eof_reached() const {
	ERR_FAIL_COND_V(!data && !info, true);
	if (info) {
		return pos >= (uint64_t)info->data.size();
	} else {
		return pos >= length;
	}
}

uint64_t FileAccessMemory::get_buffer(uint8_t *p_dst, uint64_t p_length) const {
	if (!p_length) {
		return 0;
	}

	ERR_FAIL_COND_V(!p_dst && p_length > 0, -1);
	ERR_FAIL_COND_V(!data && !info, -1);

	uint64_t read = 0;
	if (info) {
		ERR_FAIL_COND_V(pos >= (uint64_t)info->data.size(), -1);
		read = MIN(p_length, (uint64_t)info->data.size() - pos);
		memcpy(p_dst, &info->data.ptr()[pos], read);
	} else {
		ERR_FAIL_COND_V(pos >= length, -1);
		read = MIN(p_length, length - pos);
		memcpy(p_dst, &data[pos], read);
	}
	pos += read;

	return read;
}

Error FileAccessMemory::get_error() const {
	if (info) {
		return pos >= (uint64_t)info->data.size() ? ERR_FILE_EOF : OK;
	} else {
		return pos >= length ? ERR_FILE_EOF : OK;
	}
}

void FileAccessMemory::flush() {
	ERR_FAIL_COND(!data && !info);
}

bool FileAccessMemory::store_buffer(const uint8_t *p_src, uint64_t p_length) {
	if (!p_length) {
		return true;
	}

	ERR_FAIL_COND_V(!p_src && p_length > 0, false);
	ERR_FAIL_COND_V(!data && !info, false);
	ERR_FAIL_COND_V(read_only, false);

	uint64_t write = 0;
	if (info) {
		write = MIN(p_length, (uint64_t)info->data.size() - pos);
		if (write < p_length) {
			info->data.resize((uint64_t)info->data.size() + p_length);
			write = MIN(p_length, (uint64_t)info->data.size() - pos);
		}
		ERR_FAIL_COND_V(pos >= (uint64_t)info->data.size(), false);
		memcpy(&info->data.ptrw()[pos], p_src, write);
	} else {
		ERR_FAIL_COND_V(pos >= length, false);
		write = MIN(p_length, length - pos);
		memcpy(&data[pos], p_src, write);
	}
	pos += write;

	ERR_FAIL_COND_V_MSG(write < p_length, false, "Writing less data than requested.");

	return true;
}

Error FileAccessMemory::resize(int64_t p_length) {
	ERR_FAIL_COND_V(!data && !info, FAILED);
	if (info) {
		info->data.resize(p_length);
		return OK;
	} else {
		return FAILED;
	}
}

void FileAccessMemory::close() {
	if (info) {
		info->refc.decrement();
		if (info->refc.get() == 0) {
			memdelete(info);
			files.erase(filename);
		}
	}
	filename = String();
	info = nullptr;
	read_only = false;
	data = nullptr;
	length = 0;
	pos = 0;
}

FileAccessMemory::~FileAccessMemory() {
	close();
}

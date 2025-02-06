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

static HashMap<String, Vector<uint8_t>> *files = nullptr;
const static PackedByteArray DIRECTORY = String("<DIRECTORY>").to_utf8_buffer();

void FileAccessMemory::initialize() {
	if (files) {
		cleanup();
	}
	files = memnew((HashMap<String, Vector<uint8_t>>));
}

void FileAccessMemory::cleanup() {
	if (!files) {
		return;
	}
	memdelete(files);
	files = nullptr;
}

Ref<FileAccess> FileAccessMemory::create() {
	return memnew(FileAccessMemory);
}

bool FileAccessMemory::file_exists(const String &p_name) {
	return files->has(p_name);
}

Error FileAccessMemory::open_custom(const uint8_t *p_data, uint64_t p_len) {
	if (!files) {
		initialize();
	}
	current_file = "__temp__";
	files->erase(current_file);
	open_internal(current_file, FileAccess::WRITE);
	store_buffer(p_data, p_len);
	pos = 0;
	return OK;
}

Error FileAccessMemory::open_internal(const String &p_path, int p_mode_flags) {
	ERR_FAIL_NULL_V(files, ERR_FILE_NOT_FOUND);

	String name = p_path.simplify_path();

	if (p_mode_flags & WRITE) {
		files->insert(name, PackedByteArray());
	}

	HashMap<String, Vector<uint8_t>>::Iterator E = files->find(name);
	ERR_FAIL_COND_V_MSG(!E, ERR_FILE_NOT_FOUND, vformat("Can't find file '%s'.", name));
	current_file = name;
	pos = 0;

	return OK;
}

bool FileAccessMemory::is_open() const {
	return !current_file.is_empty();
}

void FileAccessMemory::seek(uint64_t p_position) {
	pos = p_position;
}

void FileAccessMemory::seek_end(int64_t p_position) {
	pos = get_length() + p_position;
}

uint64_t FileAccessMemory::get_position() const {
	ERR_FAIL_COND_V(current_file.is_empty(), 0);
	return pos;
}

uint64_t FileAccessMemory::get_length() const {
	ERR_FAIL_COND_V(current_file.is_empty(), 0);
	return files->get(current_file).size();
}

bool FileAccessMemory::eof_reached() const {
	return pos >= get_length();
}

uint64_t FileAccessMemory::get_buffer(uint8_t *p_dst, uint64_t p_length) const {
	if (!p_length) {
		return 0;
	}

	ERR_FAIL_NULL_V(p_dst, -1);
	ERR_FAIL_COND_V(current_file.is_empty(), -1);

	uint64_t left = get_length() - pos;
	uint64_t read = MIN(p_length, left);

	if (read < p_length) {
		WARN_PRINT("Reading less data than requested");
	}

	memcpy(p_dst, files->get(current_file).ptrw() + pos, read);
	pos += read;

	return read;
}

Error FileAccessMemory::get_error() const {
	return pos >= get_length() ? ERR_FILE_EOF : OK;
}

void FileAccessMemory::flush() {
	ERR_FAIL_COND_MSG(current_file.is_empty(), "No opened file");
}

bool FileAccessMemory::store_buffer(const uint8_t *p_src, uint64_t p_length) {
	ERR_FAIL_COND_V(!p_src && p_length > 0, false);

	PackedByteArray &p = files->get(current_file);
	uint64_t length = get_length();
	if (pos + p_length > length) {
		p.resize(pos + p_length);
	}
	uint8_t *dst = p.ptrw() + pos;
	memcpy(dst, p_src, p_length * sizeof(uint8_t));

	pos += p_length;
	return true;
}

Error DirAccessMemory::list_dir_begin() {
	list_items.clear();
	for (auto f = files->begin(); f != files->end(); ++f) {
		if (f->key.begins_with(current_dir)) {
			String rest = f->key.substr(current_dir.length());
			if (rest.begins_with("/")) {
				rest = rest.substr(1);
			}
			if (!rest.is_empty()) {
				list_items.push_back(rest);
			}
		}
	}
	return Error();
}

String DirAccessMemory::get_next() {
	if (list_items.size()) {
		current_item = list_items.front()->get();
		list_items.pop_front();
		return current_item;
	}
	return String();
}

bool DirAccessMemory::current_is_dir() const {
	String name = current_dir.path_join(current_item);
	return files->has(name) && files->get(name) == DIRECTORY;
}

bool DirAccessMemory::current_is_hidden() const {
	return false;
}

void DirAccessMemory::list_dir_end() {
	current_item = "";
	list_items.clear();
}

int DirAccessMemory::get_drive_count() {
	return 0;
}

String DirAccessMemory::get_drive(int p_drive) {
	return "";
}

Error DirAccessMemory::change_dir(String p_dir) {
	String name = p_dir;
	if (name.is_relative_path()) {
		name = current_dir.path_join(name);
	}
	name = name.simplify_path();
	if (name == "res://") {
		files->insert(name, DIRECTORY);
	}

	if (dir_exists(name)) {
		current_dir = name;
		return OK;
	}

	return ERR_INVALID_PARAMETER;
}

String DirAccessMemory::get_current_dir(bool p_include_drive) const {
	return current_dir;
}

String DirAccessMemory::_localize(const String &p_name) const {
	String result = p_name;
	if (result.is_relative_path()) {
		result = current_dir.path_join(result);
	}
	result = result.simplify_path();
	return result;
}

bool DirAccessMemory::file_exists(String p_file) {
	String name = _localize(p_file);
	return files->has(name) && files->get(name) != DIRECTORY;
}

bool DirAccessMemory::dir_exists(String p_dir) {
	String name = _localize(p_dir);
	return files->has(name) && files->get(name) == DIRECTORY;
}

Error DirAccessMemory::make_dir(String p_dir) {
	String name = _localize(p_dir);
	if (!dir_exists(name.get_base_dir())) {
		return ERR_CANT_CREATE;
	}
	files->insert(name, DIRECTORY);
	return OK;
}

Error DirAccessMemory::rename(String p_from, String p_to) {
	return ERR_UNAVAILABLE;
}

Error DirAccessMemory::remove(String p_name) {
	String name = _localize(p_name);
	files->erase(name);
	return OK;
}

uint64_t DirAccessMemory::get_space_left() {
	return 0;
}

String DirAccessMemory::get_filesystem_type() const {
	return "MEMORY";
}

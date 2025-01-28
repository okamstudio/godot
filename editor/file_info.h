/**************************************************************************/
/*  file_info.h                                                           */
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

#ifndef FILE_INFO_H
#define FILE_INFO_H

#include "core/string/string_name.h"
#include "core/templates/list.h"

enum class FileSortOption {
	FILE_SORT_NAME = 0,
	FILE_SORT_NAME_REVERSE = 1,
	FILE_SORT_TYPE = 2,
	FILE_SORT_TYPE_REVERSE = 3,
	FILE_SORT_MODIFIED_TIME = 4,
	FILE_SORT_MODIFIED_TIME_REVERSE = 5,
	FILE_SORT_MAX = 6,
};

struct FileInfo {
	String name;
	String path;
	String icon_path;
	StringName type;
	Vector<String> sources;
	bool import_broken = false;
	uint64_t modified_time = 0;

	bool operator<(const FileInfo &p_fi) const {
		return FileNoCaseComparator()(name, p_fi.name);
	}
};

struct FileInfoTypeComparator {
	bool operator()(const FileInfo &p_a, const FileInfo &p_b) const {
		return FileNoCaseComparator()(p_a.name.get_extension() + p_a.type + p_a.name.get_basename(), p_b.name.get_extension() + p_b.type + p_b.name.get_basename());
	}
};

struct FileInfoModifiedTimeComparator {
	bool operator()(const FileInfo &p_a, const FileInfo &p_b) const {
		return p_a.modified_time > p_b.modified_time;
	}
};

void sort_file_info_list(List<FileInfo> &r_file_list, FileSortOption p_file_sort_option);

#endif // FILE_INFO_H

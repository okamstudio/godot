/**************************************************************************/
/*  node_path.cpp                                                         */
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

#include "node_path.h"

#include "core/string/print_string.h"

void NodePath::_update_hash_cache() const {
	uint32_t h = data->absolute ? 1 : 0;
	int pc = data->path.size();
	const StringName *sn = data->path.ptr();
	for (int i = 0; i < pc; i++) {
		h = h ^ sn[i].hash();
	}
	int spc = data->subpath.size();
	const StringName *ssn = data->subpath.ptr();
	for (int i = 0; i < spc; i++) {
		h = h ^ ssn[i].hash();
	}

	data->hash_cache_valid = true;
	data->hash_cache = h;
}

void NodePath::prepend_period() {
	if (data->path.size() && data->path[0].operator String() != ".") {
		data->path.insert(0, ".");
		data->hash_cache_valid = false;
	}
}

bool NodePath::is_absolute() const {
	if (!data) {
		return false;
	}

	return data->absolute;
}

int NodePath::get_name_count() const {
	if (!data) {
		return 0;
	}

	return data->path.size();
}

StringName NodePath::get_name(int p_idx) const {
	ERR_FAIL_NULL_V(data, StringName());
	ERR_FAIL_INDEX_V(p_idx, data->path.size(), StringName());
	return data->path[p_idx];
}

int NodePath::get_subname_count() const {
	if (!data) {
		return 0;
	}

	return data->subpath.size();
}

StringName NodePath::get_subname(int p_idx) const {
	ERR_FAIL_NULL_V(data, StringName());
	ERR_FAIL_INDEX_V(p_idx, data->subpath.size(), StringName());
	return data->subpath[p_idx];
}

int NodePath::get_total_name_count() const {
	if (!data) {
		return 0;
	}

	return data->path.size() + data->subpath.size();
}

void NodePath::unref() {
	if (data && data->refcount.unref()) {
		memdelete(data);
	}
	data = nullptr;
}

bool NodePath::operator==(const NodePath &p_path) const {
	if (data == p_path.data) {
		return true;
	}

	if (!data || !p_path.data) {
		return false;
	}

	if (data->absolute != p_path.data->absolute) {
		return false;
	}

	int path_size = data->path.size();

	if (path_size != p_path.data->path.size()) {
		return false;
	}

	int subpath_size = data->subpath.size();

	if (subpath_size != p_path.data->subpath.size()) {
		return false;
	}

	const StringName *l_path_ptr = data->path.ptr();
	const StringName *r_path_ptr = p_path.data->path.ptr();

	for (int i = 0; i < path_size; i++) {
		if (l_path_ptr[i] != r_path_ptr[i]) {
			return false;
		}
	}

	const StringName *l_subpath_ptr = data->subpath.ptr();
	const StringName *r_subpath_ptr = p_path.data->subpath.ptr();

	for (int i = 0; i < subpath_size; i++) {
		if (l_subpath_ptr[i] != r_subpath_ptr[i]) {
			return false;
		}
	}

	return true;
}

bool NodePath::operator!=(const NodePath &p_path) const {
	return (!(*this == p_path));
}

void NodePath::operator=(const NodePath &p_path) {
	if (this == &p_path) {
		return;
	}

	unref();

	if (p_path.data && p_path.data->refcount.ref()) {
		data = p_path.data;
	}
}

NodePath::operator String() const {
	if (!data) {
		return String();
	}

	String ret;
	if (data->absolute) {
		ret = "/";
	}

	for (int i = 0; i < data->path.size(); i++) {
		if (i > 0) {
			ret += "/";
		}
		ret += data->path[i].operator String();
	}

	for (int i = 0; i < data->subpath.size(); i++) {
		ret += ":" + data->subpath[i].operator String();
	}

	return ret;
}

Vector<StringName> NodePath::get_names() const {
	if (data) {
		return data->path;
	}
	return Vector<StringName>();
}

Vector<StringName> NodePath::get_subnames() const {
	if (data) {
		return data->subpath;
	}
	return Vector<StringName>();
}

StringName NodePath::get_concatenated_names() const {
	ERR_FAIL_NULL_V(data, StringName());

	if (!data->concatenated_path) {
		int pc = data->path.size();
		String concatenated;
		const StringName *sn = data->path.ptr();
		for (int i = 0; i < pc; i++) {
			if (i > 0) {
				concatenated += "/";
			}
			concatenated += sn[i].operator String();
		}
		data->concatenated_path = concatenated;
	}
	return data->concatenated_path;
}

StringName NodePath::get_concatenated_subnames() const {
	ERR_FAIL_NULL_V(data, StringName());

	if (!data->concatenated_subpath) {
		int spc = data->subpath.size();
		String concatenated;
		const StringName *ssn = data->subpath.ptr();
		for (int i = 0; i < spc; i++) {
			if (i > 0) {
				concatenated += ":";
			}
			concatenated += ssn[i].operator String();
		}
		data->concatenated_subpath = concatenated;
	}
	return data->concatenated_subpath;
}

NodePath NodePath::slice(int p_begin, int p_end) const {
	const int name_count = get_name_count();
	const int total_count = get_total_name_count();

	int begin = CLAMP(p_begin, -total_count, total_count);
	if (begin < 0) {
		begin += total_count;
	}
	int end = CLAMP(p_end, -total_count, total_count);
	if (end < 0) {
		end += total_count;
	}
	const int sub_begin = MAX(begin - name_count - 1, 0);
	const int sub_end = MAX(end - name_count, 0);

	const Vector<StringName> names = get_names().slice(begin, end);
	const Vector<StringName> sub_names = get_subnames().slice(sub_begin, sub_end);
	const bool absolute = is_absolute() && (begin == 0);
	return NodePath(names, sub_names, absolute);
}

NodePath NodePath::rel_path_to(const NodePath &p_np) const {
	ERR_FAIL_COND_V(!is_absolute(), NodePath());
	ERR_FAIL_COND_V(!p_np.is_absolute(), NodePath());

	Vector<StringName> src_dirs = get_names();
	Vector<StringName> dst_dirs = p_np.get_names();

	//find common parent
	int common_parent = 0;

	while (true) {
		if (src_dirs.size() == common_parent) {
			break;
		}
		if (dst_dirs.size() == common_parent) {
			break;
		}
		if (src_dirs[common_parent] != dst_dirs[common_parent]) {
			break;
		}
		common_parent++;
	}

	common_parent--;

	Vector<StringName> relpath;
	relpath.resize(src_dirs.size() + dst_dirs.size() + 1);

	StringName *relpath_ptr = relpath.ptrw();

	int path_size = 0;
	StringName back_str("..");
	for (int i = common_parent + 1; i < src_dirs.size(); i++) {
		relpath_ptr[path_size++] = back_str;
	}

	for (int i = common_parent + 1; i < dst_dirs.size(); i++) {
		relpath_ptr[path_size++] = dst_dirs[i];
	}

	if (path_size == 0) {
		relpath_ptr[path_size++] = ".";
	}

	relpath.resize(path_size);

	return NodePath(relpath, p_np.get_subnames(), false);
}

NodePath NodePath::get_as_property_path() const {
	if (!data || !data->path.size()) {
		return *this;
	} else {
		Vector<StringName> new_path = data->subpath;

		String initial_subname = data->path[0];

		for (int i = 1; i < data->path.size(); i++) {
			initial_subname += "/" + data->path[i];
		}
		new_path.insert(0, initial_subname);

		return NodePath(Vector<StringName>(), new_path, false);
	}
}

bool NodePath::is_empty() const {
	return !data;
}

void NodePath::simplify() {
	if (!data) {
		return;
	}
	for (int i = 0; i < data->path.size(); i++) {
		if (data->path.size() == 1) {
			break;
		}
		if (data->path[i].operator String() == ".") {
			data->path.remove_at(i);
			i--;
		} else if (i > 0 && data->path[i].operator String() == ".." && data->path[i - 1].operator String() != "." && data->path[i - 1].operator String() != "..") {
			//remove both
			data->path.remove_at(i - 1);
			data->path.remove_at(i - 1);
			i -= 2;
			if (data->path.size() == 0) {
				data->path.push_back(".");
				break;
			}
		}
	}
	data->hash_cache_valid = false;
}

NodePath NodePath::simplified() const {
	NodePath np = *this;
	np.simplify();
	return np;
}

NodePath::NodePath(const Vector<StringName> &p_path, bool p_absolute) {
	if (p_path.size() == 0 && !p_absolute) {
		return;
	}

	data = memnew(Data);
	data->refcount.init();
	data->absolute = p_absolute;
	data->path = p_path;
	data->hash_cache_valid = false;
}

NodePath::NodePath(const Vector<StringName> &p_path, const Vector<StringName> &p_subpath, bool p_absolute) {
	if (p_path.size() == 0 && p_subpath.size() == 0 && !p_absolute) {
		return;
	}

	data = memnew(Data);
	data->refcount.init();
	data->absolute = p_absolute;
	data->path = p_path;
	data->subpath = p_subpath;
	data->hash_cache_valid = false;
}

NodePath::NodePath(const NodePath &p_path) {
	if (p_path.data && p_path.data->refcount.ref()) {
		data = p_path.data;
	}
}

NodePath::NodePath(const String &p_path) {
	if (p_path.length() == 0) {
		return;
	}

	String path = p_path;
	Vector<StringName> subpath;

	bool absolute = (path[0] == '/');
	bool last_is_slash = true;
	int slices = 0;
	int subpath_pos = path.find_char(':');

	if (subpath_pos != -1) {
		int from = subpath_pos + 1;

		for (int i = from; i <= path.length(); i++) {
			if (path[i] == ':' || path[i] == 0) {
				String str = path.substr(from, i - from);
				if (str.is_empty()) {
					if (path[i] == 0) {
						continue; // Allow end-of-path :
					}

					ERR_FAIL_MSG(vformat("Invalid NodePath '%s'.", p_path));
				}
				subpath.push_back(str);

				from = i + 1;
			}
		}

		path = path.substr(0, subpath_pos);
	}

	for (int i = (int)absolute; i < path.length(); i++) {
		if (path[i] == '/') {
			last_is_slash = true;
		} else {
			if (last_is_slash) {
				slices++;
			}

			last_is_slash = false;
		}
	}

	if (slices == 0 && !absolute && !subpath.size()) {
		return;
	}

	data = memnew(Data);
	data->refcount.init();
	data->absolute = absolute;
	data->subpath = subpath;
	data->hash_cache_valid = false;

	if (slices == 0) {
		return;
	}
	data->path.resize(slices);
	last_is_slash = true;
	int from = (int)absolute;
	int slice = 0;

	for (int i = (int)absolute; i < path.length() + 1; i++) {
		if (path[i] == '/' || path[i] == 0) {
			if (!last_is_slash) {
				String name = path.substr(from, i - from);
				ERR_FAIL_INDEX(slice, data->path.size());
				data->path.write[slice++] = name;
			}
			from = i + 1;
			last_is_slash = true;
		} else {
			last_is_slash = false;
		}
	}
}

NodePath::~NodePath() {
	unref();
}

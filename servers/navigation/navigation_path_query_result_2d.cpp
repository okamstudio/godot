/**************************************************************************/
/*  navigation_path_query_result_2d.cpp                                   */
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

#include "navigation_path_query_result_2d.h"

void NavigationPathQueryResult2D::set_path(const Vector<Vector2> &p_path) {
	path = p_path;
}

const Vector<Vector2> &NavigationPathQueryResult2D::get_path() const {
	return path;
}

void NavigationPathQueryResult2D::set_path_types(const Vector<int32_t> &p_path_types) {
	path_types = p_path_types;
}

const Vector<int32_t> &NavigationPathQueryResult2D::get_path_types() const {
	return path_types;
}

void NavigationPathQueryResult2D::set_path_rids(const TypedArray<RID> &p_path_rids) {
	path_rids = p_path_rids;
}

TypedArray<RID> NavigationPathQueryResult2D::get_path_rids() const {
	return path_rids;
}

void NavigationPathQueryResult2D::set_path_owner_ids(const Vector<int64_t> &p_path_owner_ids) {
	path_owner_ids = p_path_owner_ids;
}

const Vector<int64_t> &NavigationPathQueryResult2D::get_path_owner_ids() const {
	return path_owner_ids;
}

void NavigationPathQueryResult2D::reset() {
	path.clear();
	path_types.clear();
	path_rids.clear();
	path_owner_ids.clear();
}

void NavigationPathQueryResult2D::set_data(const LocalVector<Vector2> &p_path, const LocalVector<int32_t> &p_path_types, const LocalVector<RID> &p_path_rids, const LocalVector<int64_t> &p_path_owner_ids) {
	path.clear();
	path_types.clear();
	path_rids.clear();
	path_owner_ids.clear();

	{
		path.resize(p_path.size());
		Vector2 *w = path.ptrw();
		const Vector2 *r = p_path.ptr();
		for (uint32_t i = 0; i < p_path.size(); i++) {
			w[i] = r[i];
		}
	}

	{
		path_types.resize(p_path_types.size());
		int32_t *w = path_types.ptrw();
		const int32_t *r = p_path_types.ptr();
		for (uint32_t i = 0; i < p_path_types.size(); i++) {
			w[i] = r[i];
		}
	}

	{
		path_rids.resize(p_path_rids.size());
		for (uint32_t i = 0; i < p_path_rids.size(); i++) {
			path_rids[i] = p_path_rids[i];
		}
	}

	{
		path_owner_ids.resize(p_path_owner_ids.size());
		int64_t *w = path_owner_ids.ptrw();
		const int64_t *r = p_path_owner_ids.ptr();
		for (uint32_t i = 0; i < p_path_owner_ids.size(); i++) {
			w[i] = r[i];
		}
	}
}

void NavigationPathQueryResult2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_path", "path"), &NavigationPathQueryResult2D::set_path);
	ClassDB::bind_method(D_METHOD("get_path"), &NavigationPathQueryResult2D::get_path);

	ClassDB::bind_method(D_METHOD("set_path_types", "path_types"), &NavigationPathQueryResult2D::set_path_types);
	ClassDB::bind_method(D_METHOD("get_path_types"), &NavigationPathQueryResult2D::get_path_types);

	ClassDB::bind_method(D_METHOD("set_path_rids", "path_rids"), &NavigationPathQueryResult2D::set_path_rids);
	ClassDB::bind_method(D_METHOD("get_path_rids"), &NavigationPathQueryResult2D::get_path_rids);

	ClassDB::bind_method(D_METHOD("set_path_owner_ids", "path_owner_ids"), &NavigationPathQueryResult2D::set_path_owner_ids);
	ClassDB::bind_method(D_METHOD("get_path_owner_ids"), &NavigationPathQueryResult2D::get_path_owner_ids);

	ClassDB::bind_method(D_METHOD("reset"), &NavigationPathQueryResult2D::reset);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "path"), "set_path", "get_path");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "path_types"), "set_path_types", "get_path_types");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "path_rids", PROPERTY_HINT_ARRAY_TYPE, "RID"), "set_path_rids", "get_path_rids");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT64_ARRAY, "path_owner_ids"), "set_path_owner_ids", "get_path_owner_ids");

	BIND_ENUM_CONSTANT(PATH_SEGMENT_TYPE_REGION);
	BIND_ENUM_CONSTANT(PATH_SEGMENT_TYPE_LINK);
}

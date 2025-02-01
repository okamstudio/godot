/**************************************************************************/
/*  navigation_path_query_result_3d.h                                     */
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

#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/typed_array.h"
#include "servers/navigation/navigation_utilities.h"

class NavigationPathQueryResult3D : public RefCounted {
	GDCLASS(NavigationPathQueryResult3D, RefCounted);

	Vector<Vector3> path;
	Vector<int32_t> path_types;
	TypedArray<RID> path_rids;
	Vector<int64_t> path_owner_ids;

protected:
	static void _bind_methods();

public:
	enum PathSegmentType {
		PATH_SEGMENT_TYPE_REGION = NavigationUtilities::PathSegmentType::PATH_SEGMENT_TYPE_REGION,
		PATH_SEGMENT_TYPE_LINK = NavigationUtilities::PathSegmentType::PATH_SEGMENT_TYPE_LINK,
	};

	void set_path(const Vector<Vector3> &p_path);
	const Vector<Vector3> &get_path() const;

	void set_path_types(const Vector<int32_t> &p_path_types);
	const Vector<int32_t> &get_path_types() const;

	void set_path_rids(const TypedArray<RID> &p_path_rids);
	TypedArray<RID> get_path_rids() const;

	void set_path_owner_ids(const Vector<int64_t> &p_path_owner_ids);
	const Vector<int64_t> &get_path_owner_ids() const;

	void reset();

	void set_data(const LocalVector<Vector3> &p_path, const LocalVector<int32_t> &p_path_types, const LocalVector<RID> &p_path_rids, const LocalVector<int64_t> &p_path_owner_ids);
};

VARIANT_ENUM_CAST(NavigationPathQueryResult3D::PathSegmentType);

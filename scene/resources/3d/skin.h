/**************************************************************************/
/*  skin.h                                                                */
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

#include "core/io/resource.h"

class Skin : public Resource {
	GDCLASS(Skin, Resource)

	struct Bind {
		int bone = -1;
		StringName name;
		Transform3D pose;
	};

	Vector<Bind> binds;

	Bind *binds_ptr = nullptr;
	int bind_count = 0;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	virtual void reset_state() override;
	static void _bind_methods();

public:
	void set_bind_count(int p_size);
	inline int get_bind_count() const { return bind_count; }

	void add_bind(int p_bone, const Transform3D &p_pose);
	void add_named_bind(const String &p_name, const Transform3D &p_pose);

	void set_bind_bone(int p_index, int p_bone);
	void set_bind_pose(int p_index, const Transform3D &p_pose);
	void set_bind_name(int p_index, const StringName &p_name);

	inline int get_bind_bone(int p_index) const {
		ERR_FAIL_INDEX_V(p_index, bind_count, -1);
		return binds_ptr[p_index].bone;
	}

	inline StringName get_bind_name(int p_index) const {
		ERR_FAIL_INDEX_V(p_index, bind_count, StringName());
		return binds_ptr[p_index].name;
	}

	inline Transform3D get_bind_pose(int p_index) const {
		ERR_FAIL_INDEX_V(p_index, bind_count, Transform3D());
		return binds_ptr[p_index].pose;
	}

	void clear_binds();

	Skin();
};

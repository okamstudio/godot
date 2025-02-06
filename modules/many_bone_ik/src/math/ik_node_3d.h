/**************************************************************************/
/*  ik_node_3d.h                                                          */
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

#ifndef IK_NODE_3D_H
#define IK_NODE_3D_H

#include "core/object/ref_counted.h"
#include "core/templates/list.h"

#include "core/io/resource.h"
#include "core/math/transform_3d.h"

class IKNode3D : public RefCounted {
	GDCLASS(IKNode3D, RefCounted);

	enum TransformDirty {
		DIRTY_NONE = 0,
		DIRTY_VECTORS = 1,
		DIRTY_LOCAL = 2,
		DIRTY_GLOBAL = 4
	};

	mutable Transform3D global_transform;
	mutable Transform3D local_transform;
	mutable Basis rotation;
	mutable Vector3 scale = Vector3(1, 1, 1);

	mutable int dirty = DIRTY_NONE;

	WeakRef parent;
	List<Ref<IKNode3D>> children;

	bool disable_scale = false;

	void _update_local_transform() const;

protected:
	void _notification(int p_what);
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("_propagate_transform_changed"), &IKNode3D::_propagate_transform_changed);
		ClassDB::bind_method(D_METHOD("_update_local_transform"), &IKNode3D::_update_local_transform);
		ClassDB::bind_method(D_METHOD("rotate_local_with_global", "p_basis", "p_propagate"), &IKNode3D::rotate_local_with_global, DEFVAL(false));
		ClassDB::bind_method(D_METHOD("set_transform", "p_transform"), &IKNode3D::set_transform);
		ClassDB::bind_method(D_METHOD("set_global_transform", "p_transform"), &IKNode3D::set_global_transform);
		ClassDB::bind_method(D_METHOD("get_transform"), &IKNode3D::get_transform);
		ClassDB::bind_method(D_METHOD("get_global_transform"), &IKNode3D::get_global_transform);
		ClassDB::bind_method(D_METHOD("set_disable_scale", "p_enabled"), &IKNode3D::set_disable_scale);
		ClassDB::bind_method(D_METHOD("is_scale_disabled"), &IKNode3D::is_scale_disabled);
		ClassDB::bind_method(D_METHOD("set_parent", "p_parent"), &IKNode3D::set_parent);
		ClassDB::bind_method(D_METHOD("get_parent"), &IKNode3D::get_parent);
		ClassDB::bind_method(D_METHOD("to_local", "p_global"), &IKNode3D::to_local);
		ClassDB::bind_method(D_METHOD("to_global", "p_local"), &IKNode3D::to_global);
	}

public:
	void _propagate_transform_changed();
	void set_transform(const Transform3D &p_transform);
	void set_global_transform(const Transform3D &p_transform);
	Transform3D get_transform() const;
	Transform3D get_global_transform() const;

	void set_disable_scale(bool p_enabled);
	bool is_scale_disabled() const;

	void set_parent(Ref<IKNode3D> p_parent);
	Ref<IKNode3D> get_parent() const;

	Vector3 to_local(const Vector3 &p_global) const;
	Vector3 to_global(const Vector3 &p_local) const;
	void rotate_local_with_global(const Basis &p_basis, bool p_propagate = false);
	void cleanup();
	~IKNode3D();
};

#endif // IK_NODE_3D_H

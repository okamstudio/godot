/**************************************************************************/
/*  test_ik_node_3d.h                                                     */
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

#ifndef TEST_IK_NODE_3D_H
#define TEST_IK_NODE_3D_H

#include "modules/many_bone_ik/src/math/ik_node_3d.h"
#include "tests/test_macros.h"

namespace TestIKNode3D {

TEST_CASE("[Modules][IKNode3D] Transform operations") {
	Ref<IKNode3D> node;
	node.instantiate();

	// Test set_transform and get_transform
	Transform3D t;
	t.origin = Vector3(1, 2, 3);
	node->set_transform(t);
	CHECK(node->get_transform() == t);

	// Test set_global_transform and get_global_transform
	Transform3D gt;
	gt.origin = Vector3(4, 5, 6);
	node->set_global_transform(gt);
	CHECK(node->get_global_transform() == gt);
}

TEST_CASE("[Modules][IKNode3D] Scale operations") {
	Ref<IKNode3D> node;
	node.instantiate();

	// Test set_disable_scale and is_scale_disabled
	node->set_disable_scale(true);
	CHECK(node->is_scale_disabled());
}

TEST_CASE("[Modules][IKNode3D] Parent operations") {
	Ref<IKNode3D> node;
	node.instantiate();
	Ref<IKNode3D> parent;
	parent.instantiate();

	// Test set_parent and get_parent
	node->set_parent(parent);
	CHECK(node->get_parent() == parent);
}

TEST_CASE("[Modules][IKNode3D] Coordinate transformations") {
	Ref<IKNode3D> node;
	node.instantiate();

	// Test to_local and to_global
	Vector3 global(1, 2, 3);
	Vector3 local = node->to_local(global);
	CHECK(node->to_global(local) == global);
}

TEST_CASE("[Modules][IKNode3D] Test local transform calculation") {
	Ref<IKNode3D> node;
	node.instantiate();

	Transform3D node_transform;
	node_transform.origin = Vector3(1.0, 2.0, 3.0); // Translation by (1, 2, 3)
	node->set_global_transform(node_transform);

	Ref<IKNode3D> parent_node;
	parent_node.instantiate();

	Transform3D parent_transform;
	parent_transform.origin = Vector3(4.0, 5.0, 6.0); // Translation by (4, 5, 6)
	parent_node->set_global_transform(parent_transform);

	node->set_parent(parent_node);

	Transform3D expected_local_transform = parent_node->get_global_transform().affine_inverse() * node->get_global_transform();

	CHECK(node->get_transform() == expected_local_transform);
}
} // namespace TestIKNode3D

#endif // TEST_IK_NODE_3D_H

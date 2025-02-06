/**************************************************************************/
/*  test_node_3d.h                                                        */
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

#ifndef TEST_NODE_3D_H
#define TEST_NODE_3D_H

#include "scene/3d/node_3d.h"
#include "scene/main/window.h"
#include "tests/test_macros.h"

namespace TestNode3D {

Vector3 wrap_rotation_angles(Vector3 rotation) {
	if (Math::is_zero_approx(rotation.x)) {
		rotation.x = 0;
	} else if (rotation.x < 0) {
		rotation.x += 360;
	}
	if (Math::is_zero_approx(rotation.y)) {
		rotation.y = 0;
	} else if (rotation.y < 0) {
		rotation.y += 360;
	}
	if (Math::is_zero_approx(rotation.z)) {
		rotation.z = 0;
	} else if (rotation.z < 0) {
		rotation.z += 360;
	}
	return rotation;
}

TEST_CASE("[SceneTree][Node3D] Basic Test") {
	Node3D *test_node = memnew(Node3D);
	test_node->set_name("node");
	Node3D *test_child = memnew(Node3D);
	test_child->set_name("child");
	test_node->add_child(test_child);

	SUBCASE("[Node3D] Not in SceneTree") {
		ERR_PRINT_OFF;
		test_child->set_position(Vector3(1, 1, 1));

		test_node->set_global_position(Vector3(1, 0, 1));
		CHECK_EQ(test_node->get_global_position(), Vector3(0, 0, 0));
		CHECK_EQ(test_child->get_global_position(), Vector3(0, 0, 0));

		test_node->set_global_position(Vector3(2, 0, 2));
		CHECK_EQ(test_node->get_global_position(), Vector3(0, 0, 0));
		CHECK_EQ(test_child->get_global_position(), Vector3(0, 0, 0));

		Basis basis = Basis(Vector3(1, 0, 0), Vector3(0, 0, 1), Vector3(0, 1, 0));
		test_node->set_global_transform(Transform3D(basis, Vector3(3, 0, 3)));
		CHECK_EQ(test_node->get_global_position(), Vector3(0, 0, 0));
		CHECK_EQ(test_child->get_global_position(), Vector3(0, 0, 0));
		ERR_PRINT_ON;
	}

	SUBCASE("[Node3D] In SceneTree") {
		SceneTree::get_singleton()->get_root()->add_child(test_node);
		test_child->set_position(Vector3(1, 1, 1));

		test_node->set_global_position(Vector3(1, 0, 1));
		CHECK_EQ(test_node->get_global_position(), Vector3(1, 0, 1));
		CHECK_EQ(test_child->get_global_position(), Vector3(2, 1, 2));

		test_node->set_global_position(Vector3(2, 0, 2));
		CHECK_EQ(test_node->get_global_position(), Vector3(2, 0, 2));
		CHECK_EQ(test_child->get_global_position(), Vector3(3, 1, 3));

		Basis basis = Basis(Vector3(1, 0, 0), Vector3(0, 0, 1), Vector3(0, 1, 0));
		test_node->set_global_transform(Transform3D(basis, Vector3(3, 0, 3)));
		CHECK_EQ(test_node->get_global_position(), Vector3(3, 0, 3));
		CHECK_EQ(test_child->get_global_position(), Vector3(4, 1, 4));
	}

	memdelete(test_child);
	memdelete(test_node);
}

TEST_CASE("[SceneTree][Node3D] Utility methods") {
	Node3D *test_node1 = memnew(Node3D);
	Node3D *test_node2 = memnew(Node3D);
	Node3D *test_node3 = memnew(Node3D);
	Node3D *test_sibling = memnew(Node3D);
	SceneTree::get_singleton()->get_root()->add_child(test_node1);

	test_node1->set_position(Vector3(100, 0, 100));
	test_node1->set_rotation(Vector3(0, 0, 0));
	test_node1->set_scale(Vector3(1, 1, 1));
	test_node1->add_child(test_node2);

	test_node2->set_position(Vector3(100, 0, 0));
	test_node2->set_rotation(Vector3(0, 0, 0));
	test_node2->set_scale(Vector3(1, 1, 1));
	test_node2->add_child(test_node3);
	test_node2->add_child(test_sibling);

	test_node3->set_position(Vector3(0, 0, 10));
	test_node3->set_rotation(Vector3(0, 0, 0));
	test_node3->set_scale(Vector3(2, 2, 2));

	test_sibling->set_position(Vector3(5, 0, 10));
	test_sibling->set_rotation(Vector3(0, 0, 0));
	test_sibling->set_scale(Vector3(2, 2, 1));

	SUBCASE("[Node3D] look_at") {
		test_node3->look_at(Vector3(1, 0, 1));

		CHECK(test_node3->get_global_position().is_equal_approx(Vector3(200, 0, 110)));
		CHECK(test_node3->get_global_rotation().is_equal_approx(Vector3(0, 1.069691, 0)));
		CHECK(test_node3->get_position().is_equal_approx(Vector3(0, 0, 10)));
		CHECK(test_node3->get_rotation().is_equal_approx(Vector3(0, 1.069691, 0)));
		CHECK(test_node3->get_scale().is_equal_approx(Vector3(2, 2, 2)));

		test_node3->look_at(Vector3(0, 0, 10));

		CHECK(test_node3->get_global_position().is_equal_approx(Vector3(200, 0, 110)));
		CHECK(test_node3->get_global_rotation().is_equal_approx(Vector3(0, 1.107149, 0)));
		CHECK(test_node3->get_position().is_equal_approx(Vector3(0, 0, 10)));
		CHECK(test_node3->get_rotation().is_equal_approx(Vector3(0, 1.107149, 0)));
		CHECK(test_node3->get_scale().is_equal_approx(Vector3(2, 2, 2)));

		// Revert any rotation caused by look_at
		test_node3->set_rotation(Vector3(0, 0, 0));

		// look_at non-local target
		Node3D *target_node = memnew(Node3D);
		SceneTree::get_singleton()->get_root()->add_child(target_node);

		target_node->set_position(Vector3(10, 10, 10));
		test_node3->look_at(target_node->get_global_position());
		CHECK(test_node3->get_global_rotation().is_equal_approx(Vector3(0.093183, 1.086318, 0)));

		test_node3->set_rotation(Vector3(0, 0, 0));
		memdelete(target_node);
	}

	SUBCASE("[Node3D] rotate_object_local") {
		// Basic rotate_object_local tests
		test_node3->rotate_object_local(Vector3(0, 1, 0), Math_PI);
		CHECK(test_node3->get_global_position().is_equal_approx(Vector3(200, 0, 110)));
		CHECK(wrap_rotation_angles(test_node3->get_global_rotation_degrees()).is_equal_approx(Vector3(0, 180, 0)));
		CHECK(test_node3->get_position().is_equal_approx(Vector3(0, 0, 10)));
		CHECK(wrap_rotation_angles(test_node3->get_rotation_degrees()).is_equal_approx(Vector3(0, 180, 0)));
		CHECK(test_node3->get_scale().is_equal_approx(Vector3(2, 2, 2)));
		CHECK_EQ(test_node3->get_rotation_edit_mode(), Node3D::ROTATION_EDIT_MODE_EULER);
		CHECK_EQ(test_node3->get_rotation_order(), EulerOrder::YXZ);

		// Change rotation order and rotate again
		test_node3->set_rotation_order(EulerOrder::XYZ);
		test_node3->rotate_object_local(Vector3(0, 0, -1), Math_PI / 2.0);
		CHECK(test_node3->get_global_position().is_equal_approx(Vector3(200, 0, 110)));
		CHECK(wrap_rotation_angles(test_node3->get_global_rotation_degrees()).is_equal_approx(Vector3(0, 180, 270)));
		CHECK(test_node3->get_position().is_equal_approx(Vector3(0, 0, 10)));
		CHECK(wrap_rotation_angles(test_node3->get_rotation_degrees()).is_equal_approx(Vector3(180, 0, 90)));
		CHECK(test_node3->get_scale().is_equal_approx(Vector3(2, 2, 2)));
		CHECK_EQ(test_node3->get_rotation_order(), EulerOrder::XYZ);

		// Do a rotation on several axes
		test_node3->rotate_object_local(Vector3(-1, -1, -1).normalized(), Math_PI * 1.5);
		CHECK(test_node3->get_global_position().is_equal_approx(Vector3(200, 0, 110)));
		CHECK(test_node3->get_global_rotation().is_equal_approx(Vector3(1.570796, -2.790713, 0)));
		CHECK(test_node3->get_position().is_equal_approx(Vector3(0, 0, 10)));
		CHECK(test_node3->get_rotation().is_equal_approx(Vector3(1.921676, 0.246506, 2.790713)));
		CHECK(test_node3->get_scale().is_equal_approx(Vector3(2, 2, 2)));
	}

	SUBCASE("[Node3D] to_local") {
		Vector3 node3_local = test_node3->to_local(Vector3(1, 1, 1));
		CHECK(node3_local.is_equal_approx(Vector3(-99.5, 0.5, -54.5)));

		node3_local = test_node3->to_local(Vector3(-1, -1, -1));
		CHECK(node3_local.is_equal_approx(Vector3(-100.5, -0.5, -55.5)));

		node3_local = test_node3->to_local(Vector3(0, 0, 0));
		CHECK(node3_local.is_equal_approx(Vector3(-100, 0, -55)));

		node3_local = test_node3->to_local(test_node3->get_global_position());
		CHECK(node3_local.is_equal_approx(Vector3(0, 0, 0)));
	}

	SUBCASE("[Node3D] to_global") {
		Vector3 node3_global = test_node3->to_global(Vector3(1, 1, 1));
		CHECK(node3_global.is_equal_approx(Vector3(202, 2, 112)));

		node3_global = test_node3->to_global(Vector3(-1, -1, -1));
		CHECK(node3_global.is_equal_approx(Vector3(198, -2, 108)));

		node3_global = test_node3->to_global(Vector3(0, 0, 0));
		CHECK(node3_global.is_equal_approx(test_node3->get_global_position()));
	}

	SUBCASE("[Node3D] set_as_top_level") {
		test_node1->set_position(Vector3(1, 1, 1));
		test_node2->set_position(Vector3(5, 5, 5));
		test_node3->set_position(Vector3(-10, -10, -10));

		test_node3->set_as_top_level(true);
		CHECK_EQ(test_node3->get_position(), Vector3(-4, -4, -4));
		CHECK_EQ(test_node3->get_global_position(), Vector3(-4, -4, -4));
		CHECK_EQ(test_node2->get_position(), Vector3(5, 5, 5));
		CHECK_EQ(test_node2->get_global_position(), Vector3(6, 6, 6));
		CHECK_EQ(test_node1->get_position(), Vector3(1, 1, 1));
		CHECK_EQ(test_node1->get_global_position(), Vector3(1, 1, 1));

		test_node3->set_as_top_level(false);
		CHECK_EQ(test_node3->get_position(), Vector3(-10, -10, -10));
		CHECK_EQ(test_node3->get_global_position(), Vector3(-4, -4, -4));
		CHECK_EQ(test_node2->get_position(), Vector3(5, 5, 5));
		CHECK_EQ(test_node2->get_global_position(), Vector3(6, 6, 6));
		CHECK_EQ(test_node1->get_position(), Vector3(1, 1, 1));
		CHECK_EQ(test_node1->get_global_position(), Vector3(1, 1, 1));

		test_node3->set_as_top_level(true);
		test_node3->set_position(Vector3(100, 0, 100));
		CHECK_EQ(test_node3->get_position(), Vector3(100, 0, 100));
		CHECK_EQ(test_node3->get_global_position(), Vector3(100, 0, 100));
		CHECK_EQ(test_node2->get_position(), Vector3(5, 5, 5));
		CHECK_EQ(test_node2->get_global_position(), Vector3(6, 6, 6));
		CHECK_EQ(test_node1->get_position(), Vector3(1, 1, 1));
		CHECK_EQ(test_node1->get_global_position(), Vector3(1, 1, 1));
	}

	SUBCASE("[Node3D] get_relative_transform") {
		test_node2->set_rotation(Vector3(270, -180, 90));
		test_node3->set_rotation(Vector3(90, -90, 270));
		Transform3D relative_xform = test_node3->get_relative_transform(test_node3);
		CHECK(relative_xform.is_equal_approx(Transform3D()));

		relative_xform = test_node3->get_relative_transform(test_node2);
		CHECK(relative_xform.get_origin().is_equal_approx(Vector3(0, 0, 10)));
		CHECK(relative_xform.get_basis().get_euler().is_equal_approx(Vector3(1.570796, -1.904797, 0)));
		CHECK(relative_xform.get_basis().get_scale().is_equal_approx(Vector3(2, 2, 2)));

		relative_xform = test_node3->get_relative_transform(test_node1);
		CHECK(relative_xform.get_origin().is_equal_approx(Vector3(107.8864, 1.760459, -5.891133)));
		CHECK(relative_xform.get_basis().get_euler().is_equal_approx(Vector3(-1.570796, 0.650316, 0)));
		CHECK(relative_xform.get_basis().get_scale().is_equal_approx(Vector3(2, 2, 2)));

		ERR_PRINT_OFF;
		// In case of a sibling all transforms until the root are accumulated.
		Transform3D xform = test_node3->get_relative_transform(test_sibling);
		Transform3D return_xform = test_node1->get_global_transform().inverse() * test_node3->get_global_transform();
		CHECK(xform.is_equal_approx(return_xform));
		ERR_PRINT_ON;
	}

	SUBCASE("[Node3D] scale_object_local") {
		test_node1->set_scale(Vector3(1, 1, 1));
		test_node1->scale_object_local(Vector3(2, 2, 2));
		CHECK_EQ(test_node1->get_scale(), Vector3(2, 2, 2));

		test_node1->scale_object_local(Vector3(0.5, 0.5, 0.5));
		CHECK_EQ(test_node1->get_scale(), Vector3(1, 1, 1));
	}

	SUBCASE("[Node3D] translate_object_local") {
		test_node1->set_position(Vector3(1, 1, 1));
		test_node1->translate_object_local(Vector3(1, 1, 1));
		CHECK_EQ(test_node1->get_position(), Vector3(2, 2, 2));

		test_node1->translate_object_local(Vector3(-1, -1, -1));
		CHECK_EQ(test_node1->get_position(), Vector3(1, 1, 1));
	}

	SUBCASE("[Node3D] orthonormalize") {
		// Set a non-orthogonal transformation
		test_node1->set_transform(Transform3D(Basis(Vector3(1, 1, 0), Vector3(0, 1, 1), Vector3(1, 0, 1)), Vector3(0, 0, 0)));
		test_node1->orthonormalize();
		CHECK(test_node1->get_transform().basis.is_orthogonal());
	}

	memdelete(test_sibling);
	memdelete(test_node3);
	memdelete(test_node2);
	memdelete(test_node1);
}
} // namespace TestNode3D

#endif // TEST_NODE_3D_H

/**************************************************************************/
/*  test_path_follow_3d.h                                                 */
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

#include "scene/3d/path_3d.h"
#include "scene/main/window.h"

#include "tests/test_macros.h"

namespace TestPathFollow3D {

bool is_equal_approx(const Vector3 &p_a, const Vector3 &p_b) {
	const real_t tolerance = 0.001;
	return Math::is_equal_approx(p_a.x, p_b.x, tolerance) &&
			Math::is_equal_approx(p_a.y, p_b.y, tolerance) &&
			Math::is_equal_approx(p_a.z, p_b.z, tolerance);
}

TEST_CASE("[SceneTree][PathFollow3D] Sampling with progress ratio") {
	Ref<Curve3D> curve = memnew(Curve3D);
	curve->add_point(Vector3(0, 0, 0));
	curve->add_point(Vector3(100, 0, 0));
	curve->add_point(Vector3(100, 100, 0));
	curve->add_point(Vector3(100, 100, 100));
	curve->add_point(Vector3(100, 0, 100));
	Path3D *path = memnew(Path3D);
	path->set_curve(curve);
	PathFollow3D *path_follow_3d = memnew(PathFollow3D);
	path_follow_3d->set_loop(false);
	path->add_child(path_follow_3d);
	SceneTree::get_singleton()->get_root()->add_child(path);

	path_follow_3d->set_progress_ratio(0);
	CHECK(is_equal_approx(Vector3(0, 0, 0), path_follow_3d->get_transform().get_origin()));

	path_follow_3d->set_progress_ratio(0.125);
	CHECK(is_equal_approx(Vector3(50, 0, 0), path_follow_3d->get_transform().get_origin()));

	path_follow_3d->set_progress_ratio(0.25);
	CHECK(is_equal_approx(Vector3(100, 0, 0), path_follow_3d->get_transform().get_origin()));

	path_follow_3d->set_progress_ratio(0.375);
	CHECK(is_equal_approx(Vector3(100, 50, 0), path_follow_3d->get_transform().get_origin()));

	path_follow_3d->set_progress_ratio(0.5);
	CHECK(is_equal_approx(Vector3(100, 100, 0), path_follow_3d->get_transform().get_origin()));

	path_follow_3d->set_progress_ratio(0.625);
	CHECK(is_equal_approx(Vector3(100, 100, 50), path_follow_3d->get_transform().get_origin()));

	path_follow_3d->set_progress_ratio(0.75);
	CHECK(is_equal_approx(Vector3(100, 100, 100), path_follow_3d->get_transform().get_origin()));

	path_follow_3d->set_progress_ratio(0.875);
	CHECK(is_equal_approx(Vector3(100, 50, 100), path_follow_3d->get_transform().get_origin()));

	path_follow_3d->set_progress_ratio(1);
	CHECK(is_equal_approx(Vector3(100, 0, 100), path_follow_3d->get_transform().get_origin()));

	memdelete(path);
}

TEST_CASE("[SceneTree][PathFollow3D] Sampling with progress") {
	Ref<Curve3D> curve = memnew(Curve3D);
	curve->add_point(Vector3(0, 0, 0));
	curve->add_point(Vector3(100, 0, 0));
	curve->add_point(Vector3(100, 100, 0));
	curve->add_point(Vector3(100, 100, 100));
	curve->add_point(Vector3(100, 0, 100));
	Path3D *path = memnew(Path3D);
	path->set_curve(curve);
	PathFollow3D *path_follow_3d = memnew(PathFollow3D);
	path_follow_3d->set_loop(false);
	path->add_child(path_follow_3d);
	SceneTree::get_singleton()->get_root()->add_child(path);

	path_follow_3d->set_progress(0);
	CHECK(is_equal_approx(Vector3(0, 0, 0), path_follow_3d->get_transform().get_origin()));

	path_follow_3d->set_progress(50);
	CHECK(is_equal_approx(Vector3(50, 0, 0), path_follow_3d->get_transform().get_origin()));

	path_follow_3d->set_progress(100);
	CHECK(is_equal_approx(Vector3(100, 0, 0), path_follow_3d->get_transform().get_origin()));

	path_follow_3d->set_progress(150);
	CHECK(is_equal_approx(Vector3(100, 50, 0), path_follow_3d->get_transform().get_origin()));

	path_follow_3d->set_progress(200);
	CHECK(is_equal_approx(Vector3(100, 100, 0), path_follow_3d->get_transform().get_origin()));

	path_follow_3d->set_progress(250);
	CHECK(is_equal_approx(Vector3(100, 100, 50), path_follow_3d->get_transform().get_origin()));

	path_follow_3d->set_progress(300);
	CHECK(is_equal_approx(Vector3(100, 100, 100), path_follow_3d->get_transform().get_origin()));

	path_follow_3d->set_progress(350);
	CHECK(is_equal_approx(Vector3(100, 50, 100), path_follow_3d->get_transform().get_origin()));

	path_follow_3d->set_progress(400);
	CHECK(is_equal_approx(Vector3(100, 0, 100), path_follow_3d->get_transform().get_origin()));

	memdelete(path);
}

TEST_CASE("[SceneTree][PathFollow3D] Removal of a point in curve") {
	Ref<Curve3D> curve = memnew(Curve3D);
	curve->add_point(Vector3(0, 0, 0));
	curve->add_point(Vector3(100, 0, 0));
	curve->add_point(Vector3(100, 100, 0));
	Path3D *path = memnew(Path3D);
	path->set_curve(curve);
	PathFollow3D *path_follow_3d = memnew(PathFollow3D);
	path->add_child(path_follow_3d);
	SceneTree::get_singleton()->get_root()->add_child(path);

	path_follow_3d->set_progress_ratio(0.5);
	CHECK(is_equal_approx(Vector3(100, 0, 0), path_follow_3d->get_transform().get_origin()));

	curve->remove_point(1);

	path_follow_3d->set_progress_ratio(0.5);
	CHECK_MESSAGE(
			is_equal_approx(Vector3(50, 50, 0), path_follow_3d->get_transform().get_origin()),
			"Path follow's position should be updated after removing a point from the curve");

	memdelete(path);
}

TEST_CASE("[SceneTree][PathFollow3D] Progress ratio out of range") {
	Ref<Curve3D> curve = memnew(Curve3D);
	curve->add_point(Vector3(0, 0, 0));
	curve->add_point(Vector3(100, 0, 0));
	Path3D *path = memnew(Path3D);
	path->set_curve(curve);
	PathFollow3D *path_follow_3d = memnew(PathFollow3D);
	path->add_child(path_follow_3d);
	SceneTree::get_singleton()->get_root()->add_child(path);

	path_follow_3d->set_loop(true);

	path_follow_3d->set_progress_ratio(-0.3);
	CHECK_MESSAGE(
			Math::is_equal_approx(path_follow_3d->get_progress_ratio(), (real_t)0.7),
			"Progress Ratio should loop back from the end in the opposite direction");

	path_follow_3d->set_progress_ratio(1.3);
	CHECK_MESSAGE(
			Math::is_equal_approx(path_follow_3d->get_progress_ratio(), (real_t)0.3),
			"Progress Ratio should loop back from the end in the opposite direction");

	path_follow_3d->set_loop(false);

	path_follow_3d->set_progress_ratio(-0.3);
	CHECK_MESSAGE(
			Math::is_equal_approx(path_follow_3d->get_progress_ratio(), 0),
			"Progress Ratio should be clamped at 0");

	path_follow_3d->set_progress_ratio(1.3);
	CHECK_MESSAGE(
			Math::is_equal_approx(path_follow_3d->get_progress_ratio(), 1),
			"Progress Ratio should be clamped at 1");

	memdelete(path);
}

TEST_CASE("[SceneTree][PathFollow3D] Progress out of range") {
	Ref<Curve3D> curve = memnew(Curve3D);
	curve->add_point(Vector3(0, 0, 0));
	curve->add_point(Vector3(100, 0, 0));
	Path3D *path = memnew(Path3D);
	path->set_curve(curve);
	PathFollow3D *path_follow_3d = memnew(PathFollow3D);
	path->add_child(path_follow_3d);
	SceneTree::get_singleton()->get_root()->add_child(path);

	path_follow_3d->set_loop(true);

	path_follow_3d->set_progress(-50);
	CHECK_MESSAGE(
			Math::is_equal_approx(path_follow_3d->get_progress(), 50),
			"Progress should loop back from the end in the opposite direction");

	path_follow_3d->set_progress(150);
	CHECK_MESSAGE(
			Math::is_equal_approx(path_follow_3d->get_progress(), 50),
			"Progress should loop back from the end in the opposite direction");

	path_follow_3d->set_loop(false);

	path_follow_3d->set_progress(-50);
	CHECK_MESSAGE(
			Math::is_equal_approx(path_follow_3d->get_progress(), 0),
			"Progress should be clamped at 0");

	path_follow_3d->set_progress(150);
	CHECK_MESSAGE(
			Math::is_equal_approx(path_follow_3d->get_progress(), 100),
			"Progress should be clamped at max value of curve");

	memdelete(path);
}

TEST_CASE("[SceneTree][PathFollow3D] Calculate forward vector") {
	const real_t dist_cube_100 = 100 * Math::sqrt(3.0);
	Ref<Curve3D> curve = memnew(Curve3D);
	curve->add_point(Vector3(0, 0, 0));
	curve->add_point(Vector3(100, 0, 0));
	curve->add_point(Vector3(200, 100, -100));
	curve->add_point(Vector3(200, 100, 200));
	curve->add_point(Vector3(100, 0, 100));
	curve->add_point(Vector3(0, 0, 100));
	Path3D *path = memnew(Path3D);
	path->set_curve(curve);
	PathFollow3D *path_follow_3d = memnew(PathFollow3D);
	path->add_child(path_follow_3d);
	SceneTree::get_singleton()->get_root()->add_child(path);

	path_follow_3d->set_loop(false);
	path_follow_3d->set_rotation_mode(PathFollow3D::RotationMode::ROTATION_ORIENTED);

	path_follow_3d->set_progress(-50);
	CHECK(is_equal_approx(Vector3(-1, 0, 0), path_follow_3d->get_transform().get_basis().get_column(2)));

	path_follow_3d->set_progress(0);
	CHECK(is_equal_approx(Vector3(-1, 0, 0), path_follow_3d->get_transform().get_basis().get_column(2)));

	path_follow_3d->set_progress(50);
	CHECK(is_equal_approx(Vector3(-1, 0, 0), path_follow_3d->get_transform().get_basis().get_column(2)));

	path_follow_3d->set_progress(100);
	CHECK(is_equal_approx(Vector3(-1, 0, 0), path_follow_3d->get_transform().get_basis().get_column(2)));

	path_follow_3d->set_progress(100 + dist_cube_100 / 2);
	CHECK(is_equal_approx(Vector3(-0.577348, -0.577348, 0.577348), path_follow_3d->get_transform().get_basis().get_column(2)));

	path_follow_3d->set_progress(100 + dist_cube_100 - 0.01);
	CHECK(is_equal_approx(Vector3(-0.577348, -0.577348, 0.577348), path_follow_3d->get_transform().get_basis().get_column(2)));

	path_follow_3d->set_progress(250 + dist_cube_100);
	CHECK(is_equal_approx(Vector3(0, 0, -1), path_follow_3d->get_transform().get_basis().get_column(2)));

	path_follow_3d->set_progress(400 + dist_cube_100 - 0.01);
	CHECK(is_equal_approx(Vector3(0, 0, -1), path_follow_3d->get_transform().get_basis().get_column(2)));

	path_follow_3d->set_progress(400 + 1.5 * dist_cube_100);
	CHECK(is_equal_approx(Vector3(0.577348, 0.577348, 0.577348), path_follow_3d->get_transform().get_basis().get_column(2)));

	path_follow_3d->set_progress(400 + 2 * dist_cube_100 - 0.01);
	CHECK(is_equal_approx(Vector3(0.577348, 0.577348, 0.577348), path_follow_3d->get_transform().get_basis().get_column(2)));

	path_follow_3d->set_progress(500 + 2 * dist_cube_100);
	CHECK(is_equal_approx(Vector3(1, 0, 0), path_follow_3d->get_transform().get_basis().get_column(2)));

	memdelete(path);
}
} // namespace TestPathFollow3D

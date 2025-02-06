/**************************************************************************/
/*  test_path_3d.h                                                        */
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

#include "tests/test_macros.h"

namespace TestPath3D {

TEST_CASE("[Path3D] Initialization") {
	SUBCASE("Path should be empty right after initialization") {
		Path3D *test_path = memnew(Path3D);
		CHECK(test_path->get_curve().is_null());
		memdelete(test_path);
	}
}

TEST_CASE("[Path3D] Curve setter and getter") {
	SUBCASE("Curve passed to the class should remain the same") {
		Path3D *test_path = memnew(Path3D);
		const Ref<Curve3D> &curve = memnew(Curve3D);

		test_path->set_curve(curve);
		CHECK(test_path->get_curve() == curve);
		memdelete(test_path);
	}
	SUBCASE("Curve passed many times to the class should remain the same") {
		Path3D *test_path = memnew(Path3D);
		const Ref<Curve3D> &curve = memnew(Curve3D);

		test_path->set_curve(curve);
		test_path->set_curve(curve);
		test_path->set_curve(curve);
		CHECK(test_path->get_curve() == curve);
		memdelete(test_path);
	}
	SUBCASE("Curve rewrite testing") {
		Path3D *test_path = memnew(Path3D);
		const Ref<Curve3D> &curve1 = memnew(Curve3D);
		const Ref<Curve3D> &curve2 = memnew(Curve3D);

		test_path->set_curve(curve1);
		test_path->set_curve(curve2);
		CHECK_MESSAGE(test_path->get_curve() != curve1,
				"After rewrite, second curve should be in class");
		CHECK_MESSAGE(test_path->get_curve() == curve2,
				"After rewrite, second curve should be in class");
		memdelete(test_path);
	}
}

} // namespace TestPath3D

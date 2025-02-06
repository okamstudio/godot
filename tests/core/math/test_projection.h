/**************************************************************************/
/*  test_projection.h                                                     */
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

#ifndef TEST_PROJECTION_H
#define TEST_PROJECTION_H

#include "core/math/aabb.h"
#include "core/math/plane.h"
#include "core/math/projection.h"
#include "core/math/rect2.h"
#include "core/math/transform_3d.h"

#include "thirdparty/doctest/doctest.h"

namespace TestProjection {

TEST_CASE("[Projection] Construction") {
	Projection default_proj;

	CHECK(default_proj[0].is_equal_approx(Vector4(1, 0, 0, 0)));
	CHECK(default_proj[1].is_equal_approx(Vector4(0, 1, 0, 0)));
	CHECK(default_proj[2].is_equal_approx(Vector4(0, 0, 1, 0)));
	CHECK(default_proj[3].is_equal_approx(Vector4(0, 0, 0, 1)));

	Projection from_vec4(
			Vector4(1, 2, 3, 4),
			Vector4(5, 6, 7, 8),
			Vector4(9, 10, 11, 12),
			Vector4(13, 14, 15, 16));

	CHECK(from_vec4[0].is_equal_approx(Vector4(1, 2, 3, 4)));
	CHECK(from_vec4[1].is_equal_approx(Vector4(5, 6, 7, 8)));
	CHECK(from_vec4[2].is_equal_approx(Vector4(9, 10, 11, 12)));
	CHECK(from_vec4[3].is_equal_approx(Vector4(13, 14, 15, 16)));

	Transform3D transform(
			Basis(
					Vector3(1, 0, 0),
					Vector3(0, 2, 0),
					Vector3(0, 0, 3)),
			Vector3(4, 5, 6));

	Projection from_transform(transform);

	CHECK(from_transform[0].is_equal_approx(Vector4(1, 0, 0, 0)));
	CHECK(from_transform[1].is_equal_approx(Vector4(0, 2, 0, 0)));
	CHECK(from_transform[2].is_equal_approx(Vector4(0, 0, 3, 0)));
	CHECK(from_transform[3].is_equal_approx(Vector4(4, 5, 6, 1)));
}

TEST_CASE("[Projection] set_zero()") {
	Projection proj;
	proj.set_zero();

	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			CHECK(proj.columns[i][j] == 0);
		}
	}
}

TEST_CASE("[Projection] set_identity()") {
	Projection proj;
	proj.set_identity();

	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			CHECK(proj.columns[i][j] == (i == j ? 1 : 0));
		}
	}
}

TEST_CASE("[Projection] determinant()") {
	Projection proj(
			Vector4(1, 5, 9, 13),
			Vector4(2, 6, 11, 15),
			Vector4(4, 7, 11, 15),
			Vector4(4, 8, 12, 16));

	CHECK(proj.determinant() == -12);
}

TEST_CASE("[Projection] Inverse and invert") {
	SUBCASE("[Projection] Arbitrary projection matrix inversion") {
		Projection proj(
				Vector4(1, 5, 9, 13),
				Vector4(2, 6, 11, 15),
				Vector4(4, 7, 11, 15),
				Vector4(4, 8, 12, 16));

		Projection inverse_truth(
				Vector4(-4.0 / 12, 0, 1, -8.0 / 12),
				Vector4(8.0 / 12, -1, -1, 16.0 / 12),
				Vector4(-20.0 / 12, 2, -1, 5.0 / 12),
				Vector4(1, -1, 1, -0.75));

		Projection inverse = proj.inverse();
		CHECK(inverse[0].is_equal_approx(inverse_truth[0]));
		CHECK(inverse[1].is_equal_approx(inverse_truth[1]));
		CHECK(inverse[2].is_equal_approx(inverse_truth[2]));
		CHECK(inverse[3].is_equal_approx(inverse_truth[3]));

		proj.invert();
		CHECK(proj[0].is_equal_approx(inverse_truth[0]));
		CHECK(proj[1].is_equal_approx(inverse_truth[1]));
		CHECK(proj[2].is_equal_approx(inverse_truth[2]));
		CHECK(proj[3].is_equal_approx(inverse_truth[3]));
	}

	SUBCASE("[Projection] Orthogonal projection matrix inversion") {
		Projection p = Projection::create_orthogonal(-125.0f, 125.0f, -125.0f, 125.0f, 0.01f, 25.0f);
		p = p.inverse() * p;

		CHECK(p[0].is_equal_approx(Vector4(1, 0, 0, 0)));
		CHECK(p[1].is_equal_approx(Vector4(0, 1, 0, 0)));
		CHECK(p[2].is_equal_approx(Vector4(0, 0, 1, 0)));
		CHECK(p[3].is_equal_approx(Vector4(0, 0, 0, 1)));
	}

	SUBCASE("[Projection] Perspective projection matrix inversion") {
		Projection p = Projection::create_perspective(90.0f, 1.77777f, 0.05f, 4000.0f);
		p = p.inverse() * p;

		CHECK(p[0].is_equal_approx(Vector4(1, 0, 0, 0)));
		CHECK(p[1].is_equal_approx(Vector4(0, 1, 0, 0)));
		CHECK(p[2].is_equal_approx(Vector4(0, 0, 1, 0)));
		CHECK(p[3].is_equal_approx(Vector4(0, 0, 0, 1)));
	}
}

TEST_CASE("[Projection] Matrix product") {
	Projection proj1(
			Vector4(1, 5, 9, 13),
			Vector4(2, 6, 11, 15),
			Vector4(4, 7, 11, 15),
			Vector4(4, 8, 12, 16));

	Projection proj2(
			Vector4(0, 1, 2, 3),
			Vector4(10, 11, 12, 13),
			Vector4(20, 21, 22, 23),
			Vector4(30, 31, 32, 33));

	Projection prod = proj1 * proj2;

	CHECK(prod[0].is_equal_approx(Vector4(22, 44, 69, 93)));
	CHECK(prod[1].is_equal_approx(Vector4(132, 304, 499, 683)));
	CHECK(prod[2].is_equal_approx(Vector4(242, 564, 929, 1273)));
	CHECK(prod[3].is_equal_approx(Vector4(352, 824, 1359, 1863)));
}

TEST_CASE("[Projection] Vector transformation") {
	Projection proj(
			Vector4(1, 5, 9, 13),
			Vector4(2, 6, 11, 15),
			Vector4(4, 7, 11, 15),
			Vector4(4, 8, 12, 16));

	Projection inverse(
			Vector4(-4.0 / 12, 0, 1, -8.0 / 12),
			Vector4(8.0 / 12, -1, -1, 16.0 / 12),
			Vector4(-20.0 / 12, 2, -1, 5.0 / 12),
			Vector4(1, -1, 1, -0.75));

	Vector4 vec4(1, 2, 3, 4);
	CHECK(proj.xform(vec4).is_equal_approx(Vector4(33, 70, 112, 152)));
	CHECK(proj.xform_inv(vec4).is_equal_approx(Vector4(90, 107, 111, 120)));

	Vector3 vec3(1, 2, 3);
	CHECK(proj.xform(vec3).is_equal_approx(Vector3(21, 46, 76) / 104));
}

TEST_CASE("[Projection] Plane transformation") {
	Projection proj(
			Vector4(1, 5, 9, 13),
			Vector4(2, 6, 11, 15),
			Vector4(4, 7, 11, 15),
			Vector4(4, 8, 12, 16));

	Plane plane(1, 2, 3, 4);
	CHECK(proj.xform4(plane).is_equal_approx(Plane(33, 70, 112, 152)));
}

TEST_CASE("[Projection] Values access") {
	Projection proj(
			Vector4(00, 01, 02, 03),
			Vector4(10, 11, 12, 13),
			Vector4(20, 21, 22, 23),
			Vector4(30, 31, 32, 33));

	CHECK(proj[0] == Vector4(00, 01, 02, 03));
	CHECK(proj[1] == Vector4(10, 11, 12, 13));
	CHECK(proj[2] == Vector4(20, 21, 22, 23));
	CHECK(proj[3] == Vector4(30, 31, 32, 33));
}

TEST_CASE("[Projection] flip_y() and flipped_y()") {
	Projection proj(
			Vector4(00, 01, 02, 03),
			Vector4(10, 11, 12, 13),
			Vector4(20, 21, 22, 23),
			Vector4(30, 31, 32, 33));

	Projection flipped = proj.flipped_y();

	CHECK(flipped[0] == proj[0]);
	CHECK(flipped[1] == -proj[1]);
	CHECK(flipped[2] == proj[2]);
	CHECK(flipped[3] == proj[3]);

	proj.flip_y();

	CHECK(proj[0] == flipped[0]);
	CHECK(proj[1] == flipped[1]);
	CHECK(proj[2] == flipped[2]);
	CHECK(proj[3] == flipped[3]);
}

TEST_CASE("[Projection] Jitter offset") {
	Projection proj(
			Vector4(00, 01, 02, 03),
			Vector4(10, 11, 12, 13),
			Vector4(20, 21, 22, 23),
			Vector4(30, 31, 32, 33));

	Projection offsetted = proj.jitter_offseted(Vector2(1, 2));

	CHECK(offsetted[0] == proj[0]);
	CHECK(offsetted[1] == proj[1]);
	CHECK(offsetted[2] == proj[2]);
	CHECK(offsetted[3] == proj[3] + Vector4(1, 2, 0, 0));

	proj.add_jitter_offset(Vector2(1, 2));

	CHECK(proj[0] == offsetted[0]);
	CHECK(proj[1] == offsetted[1]);
	CHECK(proj[2] == offsetted[2]);
	CHECK(proj[3] == offsetted[3]);
}

TEST_CASE("[Projection] Adjust znear") {
	Projection persp = Projection::create_perspective(90, 0.5, 1, 50, false);
	Projection adjusted = persp.perspective_znear_adjusted(2);

	CHECK(adjusted[0] == persp[0]);
	CHECK(adjusted[1] == persp[1]);
	CHECK(adjusted[2].is_equal_approx(Vector4(persp[2][0], persp[2][1], -1.083333, persp[2][3])));
	CHECK(adjusted[3].is_equal_approx(Vector4(persp[3][0], persp[3][1], -4.166666, persp[3][3])));

	persp.adjust_perspective_znear(2);

	CHECK(persp[0] == adjusted[0]);
	CHECK(persp[1] == adjusted[1]);
	CHECK(persp[2] == adjusted[2]);
	CHECK(persp[3] == adjusted[3]);
}

TEST_CASE("[Projection] Set light bias") {
	Projection proj;
	proj.set_light_bias();

	CHECK(proj[0] == Vector4(0.5, 0, 0, 0));
	CHECK(proj[1] == Vector4(0, 0.5, 0, 0));
	CHECK(proj[2] == Vector4(0, 0, 0.5, 0));
	CHECK(proj[3] == Vector4(0.5, 0.5, 0.5, 1));
}

TEST_CASE("[Projection] Depth correction") {
	Projection corrected = Projection::create_depth_correction(true);

	CHECK(corrected[0] == Vector4(1, 0, 0, 0));
	CHECK(corrected[1] == Vector4(0, -1, 0, 0));
	CHECK(corrected[2] == Vector4(0, 0, -0.5, 0));
	CHECK(corrected[3] == Vector4(0, 0, 0.5, 1));

	Projection proj;
	proj.set_depth_correction(true, true, true);

	CHECK(proj[0] == corrected[0]);
	CHECK(proj[1] == corrected[1]);
	CHECK(proj[2] == corrected[2]);
	CHECK(proj[3] == corrected[3]);

	proj.set_depth_correction(false, true, true);

	CHECK(proj[0] == Vector4(1, 0, 0, 0));
	CHECK(proj[1] == Vector4(0, 1, 0, 0));
	CHECK(proj[2] == Vector4(0, 0, -0.5, 0));
	CHECK(proj[3] == Vector4(0, 0, 0.5, 1));

	proj.set_depth_correction(false, false, true);

	CHECK(proj[0] == Vector4(1, 0, 0, 0));
	CHECK(proj[1] == Vector4(0, 1, 0, 0));
	CHECK(proj[2] == Vector4(0, 0, 0.5, 0));
	CHECK(proj[3] == Vector4(0, 0, 0.5, 1));

	proj.set_depth_correction(false, false, false);

	CHECK(proj[0] == Vector4(1, 0, 0, 0));
	CHECK(proj[1] == Vector4(0, 1, 0, 0));
	CHECK(proj[2] == Vector4(0, 0, 1, 0));
	CHECK(proj[3] == Vector4(0, 0, 0, 1));

	proj.set_depth_correction(true, true, false);

	CHECK(proj[0] == Vector4(1, 0, 0, 0));
	CHECK(proj[1] == Vector4(0, -1, 0, 0));
	CHECK(proj[2] == Vector4(0, 0, -1, 0));
	CHECK(proj[3] == Vector4(0, 0, 0, 1));
}

TEST_CASE("[Projection] Light atlas rect") {
	Projection rect = Projection::create_light_atlas_rect(Rect2(1, 2, 30, 40));

	CHECK(rect[0] == Vector4(30, 0, 0, 0));
	CHECK(rect[1] == Vector4(0, 40, 0, 0));
	CHECK(rect[2] == Vector4(0, 0, 1, 0));
	CHECK(rect[3] == Vector4(1, 2, 0, 1));

	Projection proj;
	proj.set_light_atlas_rect(Rect2(1, 2, 30, 40));

	CHECK(proj[0] == rect[0]);
	CHECK(proj[1] == rect[1]);
	CHECK(proj[2] == rect[2]);
	CHECK(proj[3] == rect[3]);
}

TEST_CASE("[Projection] Make scale") {
	Projection proj;
	proj.make_scale(Vector3(2, 3, 4));

	CHECK(proj[0] == Vector4(2, 0, 0, 0));
	CHECK(proj[1] == Vector4(0, 3, 0, 0));
	CHECK(proj[2] == Vector4(0, 0, 4, 0));
	CHECK(proj[3] == Vector4(0, 0, 0, 1));
}

TEST_CASE("[Projection] Scale translate to fit aabb") {
	Projection fit = Projection::create_fit_aabb(AABB(Vector3(), Vector3(0.1, 0.2, 0.4)));

	CHECK(fit[0] == Vector4(20, 0, 0, 0));
	CHECK(fit[1] == Vector4(0, 10, 0, 0));
	CHECK(fit[2] == Vector4(0, 0, 5, 0));
	CHECK(fit[3] == Vector4(-1, -1, -1, 1));

	Projection proj;
	proj.scale_translate_to_fit(AABB(Vector3(), Vector3(0.1, 0.2, 0.4)));

	CHECK(proj[0] == fit[0]);
	CHECK(proj[1] == fit[1]);
	CHECK(proj[2] == fit[2]);
	CHECK(proj[3] == fit[3]);
}

TEST_CASE("[Projection] Perspective") {
	Projection persp = Projection::create_perspective(90, 0.5, 5, 15, false);

	CHECK(persp[0].is_equal_approx(Vector4(2, 0, 0, 0)));
	CHECK(persp[1].is_equal_approx(Vector4(0, 1, 0, 0)));
	CHECK(persp[2].is_equal_approx(Vector4(0, 0, -2, -1)));
	CHECK(persp[3].is_equal_approx(Vector4(0, 0, -15, 0)));

	Projection proj;
	proj.set_perspective(90, 0.5, 5, 15, false);

	CHECK(proj[0] == persp[0]);
	CHECK(proj[1] == persp[1]);
	CHECK(proj[2] == persp[2]);
	CHECK(proj[3] == persp[3]);
}

TEST_CASE("[Projection] Frustum") {
	Projection frustum = Projection::create_frustum(15, 20, 10, 12, 5, 15);

	CHECK(frustum[0].is_equal_approx(Vector4(2, 0, 0, 0)));
	CHECK(frustum[1].is_equal_approx(Vector4(0, 5, 0, 0)));
	CHECK(frustum[2].is_equal_approx(Vector4(7, 11, -2, -1)));
	CHECK(frustum[3].is_equal_approx(Vector4(0, 0, -15, 0)));

	Projection proj;
	proj.set_frustum(15, 20, 10, 12, 5, 15);

	CHECK(proj[0] == frustum[0]);
	CHECK(proj[1] == frustum[1]);
	CHECK(proj[2] == frustum[2]);
	CHECK(proj[3] == frustum[3]);
}

TEST_CASE("[Projection] Ortho") {
	Projection ortho = Projection::create_orthogonal(15, 20, 10, 12, 5, 15);

	CHECK(ortho[0].is_equal_approx(Vector4(0.4, 0, 0, 0)));
	CHECK(ortho[1].is_equal_approx(Vector4(0, 1, 0, 0)));
	CHECK(ortho[2].is_equal_approx(Vector4(0, 0, -0.2, 0)));
	CHECK(ortho[3].is_equal_approx(Vector4(-7, -11, -2, 1)));

	Projection proj;
	proj.set_orthogonal(15, 20, 10, 12, 5, 15);

	CHECK(proj[0] == ortho[0]);
	CHECK(proj[1] == ortho[1]);
	CHECK(proj[2] == ortho[2]);
	CHECK(proj[3] == ortho[3]);
}

TEST_CASE("[Projection] get_fovy()") {
	double fov = Projection::get_fovy(90, 0.5);
	CHECK(fov == doctest::Approx(53.1301));
}

TEST_CASE("[Projection] Perspective values extraction") {
	Projection persp = Projection::create_perspective(90, 0.5, 1, 50, true);

	double znear = persp.get_z_near();
	double zfar = persp.get_z_far();
	double aspect = persp.get_aspect();
	double fov = persp.get_fov();

	CHECK(znear == doctest::Approx(1));
	CHECK(zfar == doctest::Approx(50));
	CHECK(aspect == doctest::Approx(0.5));
	CHECK(fov == doctest::Approx(90));

	persp.set_perspective(38, 1.3, 0.2, 8, false);

	znear = persp.get_z_near();
	zfar = persp.get_z_far();
	aspect = persp.get_aspect();
	fov = persp.get_fov();

	CHECK(znear == doctest::Approx(0.2));
	CHECK(zfar == doctest::Approx(8));
	CHECK(aspect == doctest::Approx(1.3));
	CHECK(fov == doctest::Approx(Projection::get_fovy(38, 1.3)));

	persp.set_perspective(47, 2.5, 0.9, 14, true);

	znear = persp.get_z_near();
	zfar = persp.get_z_far();
	aspect = persp.get_aspect();
	fov = persp.get_fov();

	CHECK(znear == doctest::Approx(0.9));
	CHECK(zfar == doctest::Approx(14));
	CHECK(aspect == doctest::Approx(2.5));
	CHECK(fov == doctest::Approx(47));
}

TEST_CASE("[Projection] Frustum values extraction") {
	Projection frustum = Projection::create_frustum_aspect(1.0, 4.0 / 3.0, Vector2(0.5, -0.25), 0.5, 50, true);

	double znear = frustum.get_z_near();
	double zfar = frustum.get_z_far();
	double aspect = frustum.get_aspect();
	double fov = frustum.get_fov();

	CHECK(znear == doctest::Approx(0.5));
	CHECK(zfar == doctest::Approx(50));
	CHECK(aspect == doctest::Approx(4.0 / 3.0));
	CHECK(fov == doctest::Approx(Math::rad_to_deg(Math::atan(2.0))));

	frustum.set_frustum(2.0, 1.5, Vector2(-0.5, 2), 2, 12, false);

	znear = frustum.get_z_near();
	zfar = frustum.get_z_far();
	aspect = frustum.get_aspect();
	fov = frustum.get_fov();

	CHECK(znear == doctest::Approx(2));
	CHECK(zfar == doctest::Approx(12));
	CHECK(aspect == doctest::Approx(1.5));
	CHECK(fov == doctest::Approx(Math::rad_to_deg(Math::atan(1.0) + Math::atan(0.5))));
}

TEST_CASE("[Projection] Orthographic values extraction") {
	Projection ortho = Projection::create_orthogonal(-2, 3, -0.5, 1.5, 1.2, 15);

	double znear = ortho.get_z_near();
	double zfar = ortho.get_z_far();
	double aspect = ortho.get_aspect();

	CHECK(znear == doctest::Approx(1.2));
	CHECK(zfar == doctest::Approx(15));
	CHECK(aspect == doctest::Approx(2.5));

	ortho.set_orthogonal(-7, 2, 2.5, 5.5, 0.5, 6);

	znear = ortho.get_z_near();
	zfar = ortho.get_z_far();
	aspect = ortho.get_aspect();

	CHECK(znear == doctest::Approx(0.5));
	CHECK(zfar == doctest::Approx(6));
	CHECK(aspect == doctest::Approx(3));
}

TEST_CASE("[Projection] Orthographic check") {
	Projection persp = Projection::create_perspective(90, 0.5, 1, 50, false);
	Projection ortho = Projection::create_orthogonal(15, 20, 10, 12, 5, 15);

	CHECK(!persp.is_orthogonal());
	CHECK(ortho.is_orthogonal());
}

TEST_CASE("[Projection] Planes extraction") {
	Projection persp = Projection::create_perspective(90, 1, 1, 40, false);
	Vector<Plane> planes = persp.get_projection_planes(Transform3D());

	CHECK(planes[Projection::PLANE_NEAR].normalized().is_equal_approx(Plane(0, 0, 1, -1)));
	CHECK(planes[Projection::PLANE_FAR].normalized().is_equal_approx(Plane(0, 0, -1, 40)));
	CHECK(planes[Projection::PLANE_LEFT].normalized().is_equal_approx(Plane(-0.707107, 0, 0.707107, 0)));
	CHECK(planes[Projection::PLANE_TOP].normalized().is_equal_approx(Plane(0, 0.707107, 0.707107, 0)));
	CHECK(planes[Projection::PLANE_RIGHT].normalized().is_equal_approx(Plane(0.707107, 0, 0.707107, 0)));
	CHECK(planes[Projection::PLANE_BOTTOM].normalized().is_equal_approx(Plane(0, -0.707107, 0.707107, 0)));

	Plane plane_array[6]{
		persp.get_projection_plane(Projection::PLANE_NEAR),
		persp.get_projection_plane(Projection::PLANE_FAR),
		persp.get_projection_plane(Projection::PLANE_LEFT),
		persp.get_projection_plane(Projection::PLANE_TOP),
		persp.get_projection_plane(Projection::PLANE_RIGHT),
		persp.get_projection_plane(Projection::PLANE_BOTTOM)
	};

	CHECK(plane_array[Projection::PLANE_NEAR].normalized().is_equal_approx(planes[Projection::PLANE_NEAR].normalized()));
	CHECK(plane_array[Projection::PLANE_FAR].normalized().is_equal_approx(planes[Projection::PLANE_FAR].normalized()));
	CHECK(plane_array[Projection::PLANE_LEFT].normalized().is_equal_approx(planes[Projection::PLANE_LEFT].normalized()));
	CHECK(plane_array[Projection::PLANE_TOP].normalized().is_equal_approx(planes[Projection::PLANE_TOP].normalized()));
	CHECK(plane_array[Projection::PLANE_RIGHT].normalized().is_equal_approx(planes[Projection::PLANE_RIGHT].normalized()));
	CHECK(plane_array[Projection::PLANE_BOTTOM].normalized().is_equal_approx(planes[Projection::PLANE_BOTTOM].normalized()));
}

TEST_CASE("[Projection] Perspective Half extents") {
	constexpr real_t sqrt3 = 1.7320508;
	Projection persp = Projection::create_perspective(90, 1, 1, 40, false);
	Vector2 ne = persp.get_viewport_half_extents();
	Vector2 fe = persp.get_far_plane_half_extents();

	CHECK(ne.is_equal_approx(Vector2(1, 1) * 1));
	CHECK(fe.is_equal_approx(Vector2(1, 1) * 40));

	persp.set_perspective(120, sqrt3, 0.8, 10, true);
	ne = persp.get_viewport_half_extents();
	fe = persp.get_far_plane_half_extents();

	CHECK(ne.is_equal_approx(Vector2(sqrt3, 1.0) * 0.8));
	CHECK(fe.is_equal_approx(Vector2(sqrt3, 1.0) * 10));

	persp.set_perspective(60, 1.2, 0.5, 15, false);
	ne = persp.get_viewport_half_extents();
	fe = persp.get_far_plane_half_extents();

	CHECK(ne.is_equal_approx(Vector2(sqrt3 / 3 * 1.2, sqrt3 / 3) * 0.5));
	CHECK(fe.is_equal_approx(Vector2(sqrt3 / 3 * 1.2, sqrt3 / 3) * 15));
}

TEST_CASE("[Projection] Orthographic Half extents") {
	Projection ortho = Projection::create_orthogonal(-3, 3, -1.5, 1.5, 1.2, 15);
	Vector2 ne = ortho.get_viewport_half_extents();
	Vector2 fe = ortho.get_far_plane_half_extents();

	CHECK(ne.is_equal_approx(Vector2(3, 1.5)));
	CHECK(fe.is_equal_approx(Vector2(3, 1.5)));

	ortho.set_orthogonal(-7, 7, -2.5, 2.5, 0.5, 6);
	ne = ortho.get_viewport_half_extents();
	fe = ortho.get_far_plane_half_extents();

	CHECK(ne.is_equal_approx(Vector2(7, 2.5)));
	CHECK(fe.is_equal_approx(Vector2(7, 2.5)));
}

TEST_CASE("[Projection] Endpoints") {
	constexpr real_t sqrt3 = 1.7320508;
	Projection persp = Projection::create_perspective(90, 1, 1, 40, false);
	Vector3 ep[8];
	persp.get_endpoints(Transform3D(), ep);

	CHECK(ep[0].is_equal_approx(Vector3(-1, 1, -1) * 40));
	CHECK(ep[1].is_equal_approx(Vector3(-1, -1, -1) * 40));
	CHECK(ep[2].is_equal_approx(Vector3(1, 1, -1) * 40));
	CHECK(ep[3].is_equal_approx(Vector3(1, -1, -1) * 40));
	CHECK(ep[4].is_equal_approx(Vector3(-1, 1, -1) * 1));
	CHECK(ep[5].is_equal_approx(Vector3(-1, -1, -1) * 1));
	CHECK(ep[6].is_equal_approx(Vector3(1, 1, -1) * 1));
	CHECK(ep[7].is_equal_approx(Vector3(1, -1, -1) * 1));

	persp.set_perspective(120, sqrt3, 0.8, 10, true);
	persp.get_endpoints(Transform3D(), ep);

	CHECK(ep[0].is_equal_approx(Vector3(-sqrt3, 1, -1) * 10));
	CHECK(ep[1].is_equal_approx(Vector3(-sqrt3, -1, -1) * 10));
	CHECK(ep[2].is_equal_approx(Vector3(sqrt3, 1, -1) * 10));
	CHECK(ep[3].is_equal_approx(Vector3(sqrt3, -1, -1) * 10));
	CHECK(ep[4].is_equal_approx(Vector3(-sqrt3, 1, -1) * 0.8));
	CHECK(ep[5].is_equal_approx(Vector3(-sqrt3, -1, -1) * 0.8));
	CHECK(ep[6].is_equal_approx(Vector3(sqrt3, 1, -1) * 0.8));
	CHECK(ep[7].is_equal_approx(Vector3(sqrt3, -1, -1) * 0.8));

	persp.set_perspective(60, 1.2, 0.5, 15, false);
	persp.get_endpoints(Transform3D(), ep);

	CHECK(ep[0].is_equal_approx(Vector3(-sqrt3 / 3 * 1.2, sqrt3 / 3, -1) * 15));
	CHECK(ep[1].is_equal_approx(Vector3(-sqrt3 / 3 * 1.2, -sqrt3 / 3, -1) * 15));
	CHECK(ep[2].is_equal_approx(Vector3(sqrt3 / 3 * 1.2, sqrt3 / 3, -1) * 15));
	CHECK(ep[3].is_equal_approx(Vector3(sqrt3 / 3 * 1.2, -sqrt3 / 3, -1) * 15));
	CHECK(ep[4].is_equal_approx(Vector3(-sqrt3 / 3 * 1.2, sqrt3 / 3, -1) * 0.5));
	CHECK(ep[5].is_equal_approx(Vector3(-sqrt3 / 3 * 1.2, -sqrt3 / 3, -1) * 0.5));
	CHECK(ep[6].is_equal_approx(Vector3(sqrt3 / 3 * 1.2, sqrt3 / 3, -1) * 0.5));
	CHECK(ep[7].is_equal_approx(Vector3(sqrt3 / 3 * 1.2, -sqrt3 / 3, -1) * 0.5));
}

TEST_CASE("[Projection] LOD multiplier") {
	constexpr real_t sqrt3 = 1.7320508;
	Projection proj;
	real_t multiplier;

	proj.set_perspective(60, 1, 1, 40, false);
	multiplier = proj.get_lod_multiplier();
	CHECK(multiplier == doctest::Approx(2 * sqrt3 / 3));

	proj.set_perspective(120, 1.5, 0.5, 20, false);
	multiplier = proj.get_lod_multiplier();
	CHECK(multiplier == doctest::Approx(3 * sqrt3));

	proj.set_orthogonal(15, 20, 10, 12, 5, 15);
	multiplier = proj.get_lod_multiplier();
	CHECK(multiplier == doctest::Approx(5));

	proj.set_orthogonal(-5, 15, -8, 10, 1.5, 10);
	multiplier = proj.get_lod_multiplier();
	CHECK(multiplier == doctest::Approx(20));

	proj.set_frustum(1.0, 4.0 / 3.0, Vector2(0.5, -0.25), 0.5, 50, false);
	multiplier = proj.get_lod_multiplier();
	CHECK(multiplier == doctest::Approx(8.0 / 3.0));

	proj.set_frustum(2.0, 1.2, Vector2(-0.1, 0.8), 1, 10, true);
	multiplier = proj.get_lod_multiplier();
	CHECK(multiplier == doctest::Approx(2));
}

TEST_CASE("[Projection] Pixels per meter") {
	constexpr real_t sqrt3 = 1.7320508;
	Projection proj;
	int ppm;

	proj.set_perspective(60, 1, 1, 40, false);
	ppm = proj.get_pixels_per_meter(1024);
	CHECK(ppm == int(1536.0f / sqrt3));

	proj.set_perspective(120, 1.5, 0.5, 20, false);
	ppm = proj.get_pixels_per_meter(1200);
	CHECK(ppm == int(800.0f / sqrt3));

	proj.set_orthogonal(15, 20, 10, 12, 5, 15);
	ppm = proj.get_pixels_per_meter(500);
	CHECK(ppm == 100);

	proj.set_orthogonal(-5, 15, -8, 10, 1.5, 10);
	ppm = proj.get_pixels_per_meter(640);
	CHECK(ppm == 32);

	proj.set_frustum(1.0, 4.0 / 3.0, Vector2(0.5, -0.25), 0.5, 50, false);
	ppm = proj.get_pixels_per_meter(2048);
	CHECK(ppm == 1536);

	proj.set_frustum(2.0, 1.2, Vector2(-0.1, 0.8), 1, 10, true);
	ppm = proj.get_pixels_per_meter(800);
	CHECK(ppm == 400);
}
} //namespace TestProjection

#endif // TEST_PROJECTION_H

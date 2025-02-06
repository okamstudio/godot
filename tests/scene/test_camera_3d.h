/**************************************************************************/
/*  test_camera_3d.h                                                      */
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

#include "scene/3d/camera_3d.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"

#include "tests/test_macros.h"

// Constants.
#define SQRT3 (1.7320508f)

TEST_CASE("[SceneTree][Camera3D] Getters and setters") {
	Camera3D *test_camera = memnew(Camera3D);

	SUBCASE("Cull mask") {
		constexpr int cull_mask = (1 << 5) | (1 << 7) | (1 << 9);
		constexpr int set_enable_layer = 3;
		constexpr int set_disable_layer = 5;
		test_camera->set_cull_mask(cull_mask);
		CHECK(test_camera->get_cull_mask() == cull_mask);
		test_camera->set_cull_mask_value(set_enable_layer, true);
		CHECK(test_camera->get_cull_mask_value(set_enable_layer));
		test_camera->set_cull_mask_value(set_disable_layer, false);
		CHECK_FALSE(test_camera->get_cull_mask_value(set_disable_layer));
	}

	SUBCASE("Attributes") {
		Ref<CameraAttributes> attributes = memnew(CameraAttributes);
		test_camera->set_attributes(attributes);
		CHECK(test_camera->get_attributes() == attributes);
		Ref<CameraAttributesPhysical> physical_attributes = memnew(CameraAttributesPhysical);
		test_camera->set_attributes(physical_attributes);
		CHECK(test_camera->get_attributes() == physical_attributes);
	}

	SUBCASE("Camera frustum properties") {
		constexpr float depth_near = 0.2f;
		constexpr float depth_far = 995.0f;
		constexpr float fov = 120.0f;
		constexpr float size = 7.0f;
		constexpr float h_offset = 1.1f;
		constexpr float v_offset = -1.6f;
		const Vector2 frustum_offset(5, 7);
		test_camera->set_near(depth_near);
		CHECK(test_camera->get_near() == depth_near);
		test_camera->set_far(depth_far);
		CHECK(test_camera->get_far() == depth_far);
		test_camera->set_fov(fov);
		CHECK(test_camera->get_fov() == fov);
		test_camera->set_size(size);
		CHECK(test_camera->get_size() == size);
		test_camera->set_h_offset(h_offset);
		CHECK(test_camera->get_h_offset() == h_offset);
		test_camera->set_v_offset(v_offset);
		CHECK(test_camera->get_v_offset() == v_offset);
		test_camera->set_frustum_offset(frustum_offset);
		CHECK(test_camera->get_frustum_offset() == frustum_offset);
		test_camera->set_keep_aspect_mode(Camera3D::KeepAspect::KEEP_HEIGHT);
		CHECK(test_camera->get_keep_aspect_mode() == Camera3D::KeepAspect::KEEP_HEIGHT);
		test_camera->set_keep_aspect_mode(Camera3D::KeepAspect::KEEP_WIDTH);
		CHECK(test_camera->get_keep_aspect_mode() == Camera3D::KeepAspect::KEEP_WIDTH);
	}

	SUBCASE("Projection mode") {
		test_camera->set_projection(Camera3D::ProjectionType::PROJECTION_ORTHOGONAL);
		CHECK(test_camera->get_projection() == Camera3D::ProjectionType::PROJECTION_ORTHOGONAL);
		test_camera->set_projection(Camera3D::ProjectionType::PROJECTION_PERSPECTIVE);
		CHECK(test_camera->get_projection() == Camera3D::ProjectionType::PROJECTION_PERSPECTIVE);
	}

	SUBCASE("Helper setters") {
		constexpr float fov = 90.0f, size = 6.0f;
		constexpr float near1 = 0.1f, near2 = 0.5f;
		constexpr float far1 = 1001.0f, far2 = 1005.0f;
		test_camera->set_perspective(fov, near1, far1);
		CHECK(test_camera->get_projection() == Camera3D::ProjectionType::PROJECTION_PERSPECTIVE);
		CHECK(test_camera->get_near() == near1);
		CHECK(test_camera->get_far() == far1);
		CHECK(test_camera->get_fov() == fov);
		test_camera->set_orthogonal(size, near2, far2);
		CHECK(test_camera->get_projection() == Camera3D::ProjectionType::PROJECTION_ORTHOGONAL);
		CHECK(test_camera->get_near() == near2);
		CHECK(test_camera->get_far() == far2);
		CHECK(test_camera->get_size() == size);
	}

	SUBCASE("Doppler tracking") {
		test_camera->set_doppler_tracking(Camera3D::DopplerTracking::DOPPLER_TRACKING_IDLE_STEP);
		CHECK(test_camera->get_doppler_tracking() == Camera3D::DopplerTracking::DOPPLER_TRACKING_IDLE_STEP);
		test_camera->set_doppler_tracking(Camera3D::DopplerTracking::DOPPLER_TRACKING_PHYSICS_STEP);
		CHECK(test_camera->get_doppler_tracking() == Camera3D::DopplerTracking::DOPPLER_TRACKING_PHYSICS_STEP);
		test_camera->set_doppler_tracking(Camera3D::DopplerTracking::DOPPLER_TRACKING_DISABLED);
		CHECK(test_camera->get_doppler_tracking() == Camera3D::DopplerTracking::DOPPLER_TRACKING_DISABLED);
	}

	memdelete(test_camera);
}

TEST_CASE("[SceneTree][Camera3D] Position queries") {
	// Cameras need a viewport to know how to compute their frustums, so we make a fake one here.
	Camera3D *test_camera = memnew(Camera3D);
	SubViewport *mock_viewport = memnew(SubViewport);
	// 4:2.
	mock_viewport->set_size(Vector2(400, 200));
	SceneTree::get_singleton()->get_root()->add_child(mock_viewport);
	mock_viewport->add_child(test_camera);
	test_camera->set_keep_aspect_mode(Camera3D::KeepAspect::KEEP_WIDTH);
	REQUIRE_MESSAGE(test_camera->is_current(), "Camera3D should be made current upon entering tree.");

	SUBCASE("Orthogonal projection") {
		test_camera->set_projection(Camera3D::ProjectionType::PROJECTION_ORTHOGONAL);
		// The orthogonal case is simpler, so we test a more random position + rotation combination here.
		// For the other cases we'll use zero translation and rotation instead.
		test_camera->set_global_position(Vector3(1, 2, 3));
		test_camera->look_at(Vector3(-4, 5, 1));
		// Width = 5, Aspect Ratio = 400 / 200 = 2, so Height is 2.5.
		test_camera->set_orthogonal(5.0f, 0.5f, 1000.0f);
		const Basis basis = test_camera->get_global_basis();
		// Subtract near so offset starts from the near plane.
		const Vector3 offset1 = basis.xform(Vector3(-1.5f, 3.5f, 0.2f - test_camera->get_near()));
		const Vector3 offset2 = basis.xform(Vector3(2.0f, -0.5f, -0.6f - test_camera->get_near()));
		const Vector3 offset3 = basis.xform(Vector3(-3.0f, 1.0f, -0.6f - test_camera->get_near()));
		const Vector3 offset4 = basis.xform(Vector3(-2.0f, 1.5f, -0.6f - test_camera->get_near()));
		const Vector3 offset5 = basis.xform(Vector3(0, 0, 10000.0f - test_camera->get_near()));

		SUBCASE("is_position_behind") {
			CHECK(test_camera->is_position_behind(test_camera->get_global_position() + offset1));
			CHECK_FALSE(test_camera->is_position_behind(test_camera->get_global_position() + offset2));

			SUBCASE("h/v offset should have no effect on the result of is_position_behind") {
				test_camera->set_h_offset(-11.0f);
				test_camera->set_v_offset(22.1f);
				CHECK(test_camera->is_position_behind(test_camera->get_global_position() + offset1));
				test_camera->set_h_offset(4.7f);
				test_camera->set_v_offset(-3.0f);
				CHECK_FALSE(test_camera->is_position_behind(test_camera->get_global_position() + offset2));
			}
			// Reset h/v offsets.
			test_camera->set_h_offset(0);
			test_camera->set_v_offset(0);
		}

		SUBCASE("is_position_in_frustum") {
			// If the point is behind the near plane, it is outside the camera frustum.
			// So offset1 is not in frustum.
			CHECK_FALSE(test_camera->is_position_in_frustum(test_camera->get_global_position() + offset1));
			// If |right| > 5 / 2 or |up| > 2.5 / 2, the point is outside the camera frustum.
			// So offset2 is in frustum and offset3 and offset4 are not.
			CHECK(test_camera->is_position_in_frustum(test_camera->get_global_position() + offset2));
			CHECK_FALSE(test_camera->is_position_in_frustum(test_camera->get_global_position() + offset3));
			CHECK_FALSE(test_camera->is_position_in_frustum(test_camera->get_global_position() + offset4));
			// offset5 is beyond the far plane, so it is not in frustum.
			CHECK_FALSE(test_camera->is_position_in_frustum(test_camera->get_global_position() + offset5));
		}
	}

	SUBCASE("Perspective projection") {
		test_camera->set_projection(Camera3D::ProjectionType::PROJECTION_PERSPECTIVE);
		// Camera at origin, looking at +Z.
		test_camera->set_global_position(Vector3(0, 0, 0));
		test_camera->set_global_rotation(Vector3(0, 0, 0));
		// Keep width, so horizontal fov = 120.
		// Since the near plane distance is 1,
		// with trig we know the near plane's width is 2 * sqrt(3), so its height is sqrt(3).
		test_camera->set_perspective(120.0f, 1.0f, 1000.0f);

		SUBCASE("is_position_behind") {
			CHECK_FALSE(test_camera->is_position_behind(Vector3(0, 0, -1.5f)));
			CHECK(test_camera->is_position_behind(Vector3(2, 0, -0.2f)));
		}

		SUBCASE("is_position_in_frustum") {
			CHECK(test_camera->is_position_in_frustum(Vector3(-1.3f, 0, -1.1f)));
			CHECK_FALSE(test_camera->is_position_in_frustum(Vector3(2, 0, -1.1f)));
			CHECK(test_camera->is_position_in_frustum(Vector3(1, 0.5f, -1.1f)));
			CHECK_FALSE(test_camera->is_position_in_frustum(Vector3(1, 1, -1.1f)));
			CHECK(test_camera->is_position_in_frustum(Vector3(0, 0, -1.5f)));
			CHECK_FALSE(test_camera->is_position_in_frustum(Vector3(0, 0, -0.5f)));
		}
	}

	memdelete(test_camera);
	memdelete(mock_viewport);
}

TEST_CASE("[SceneTree][Camera3D] Project/Unproject position") {
	// Cameras need a viewport to know how to compute their frustums, so we make a fake one here.
	Camera3D *test_camera = memnew(Camera3D);
	SubViewport *mock_viewport = memnew(SubViewport);
	// 4:2.
	mock_viewport->set_size(Vector2(400, 200));
	SceneTree::get_singleton()->get_root()->add_child(mock_viewport);
	mock_viewport->add_child(test_camera);
	test_camera->set_global_position(Vector3(0, 0, 0));
	test_camera->set_global_rotation(Vector3(0, 0, 0));
	test_camera->set_keep_aspect_mode(Camera3D::KeepAspect::KEEP_HEIGHT);

	SUBCASE("project_position") {
		SUBCASE("Orthogonal projection") {
			test_camera->set_orthogonal(5.0f, 0.5f, 1000.0f);
			// Center.
			CHECK(test_camera->project_position(Vector2(200, 100), 0.5f).is_equal_approx(Vector3(0, 0, -0.5f)));
			CHECK(test_camera->project_position(Vector2(200, 100), test_camera->get_far()).is_equal_approx(Vector3(0, 0, -test_camera->get_far())));
			// Top left.
			CHECK(test_camera->project_position(Vector2(0, 0), 1.5f).is_equal_approx(Vector3(-5.0f, 2.5f, -1.5f)));
			CHECK(test_camera->project_position(Vector2(0, 0), test_camera->get_near()).is_equal_approx(Vector3(-5.0f, 2.5f, -test_camera->get_near())));
			// Bottom right.
			CHECK(test_camera->project_position(Vector2(400, 200), 5.0f).is_equal_approx(Vector3(5.0f, -2.5f, -5.0f)));
			CHECK(test_camera->project_position(Vector2(400, 200), test_camera->get_far()).is_equal_approx(Vector3(5.0f, -2.5f, -test_camera->get_far())));
		}

		SUBCASE("Perspective projection") {
			test_camera->set_perspective(120.0f, 0.5f, 1000.0f);
			// Center.
			CHECK(test_camera->project_position(Vector2(200, 100), 0.5f).is_equal_approx(Vector3(0, 0, -0.5f)));
			CHECK(test_camera->project_position(Vector2(200, 100), 100.0f).is_equal_approx(Vector3(0, 0, -100.0f)));
			CHECK(test_camera->project_position(Vector2(200, 100), test_camera->get_far()).is_equal_approx(Vector3(0, 0, -1.0f) * test_camera->get_far()));
			// 3/4th way to Top left.
			CHECK(test_camera->project_position(Vector2(100, 50), 0.5f).is_equal_approx(Vector3(-SQRT3 * 0.5f, SQRT3 * 0.25f, -0.5f)));
			CHECK(test_camera->project_position(Vector2(100, 50), 1.0f).is_equal_approx(Vector3(-SQRT3, SQRT3 * 0.5f, -1.0f)));
			CHECK(test_camera->project_position(Vector2(100, 50), test_camera->get_near()).is_equal_approx(Vector3(-SQRT3, SQRT3 * 0.5f, -1.0f) * test_camera->get_near()));
			// 3/4th way to Bottom right.
			CHECK(test_camera->project_position(Vector2(300, 150), 0.5f).is_equal_approx(Vector3(SQRT3 * 0.5f, -SQRT3 * 0.25f, -0.5f)));
			CHECK(test_camera->project_position(Vector2(300, 150), 1.0f).is_equal_approx(Vector3(SQRT3, -SQRT3 * 0.5f, -1.0f)));
			CHECK(test_camera->project_position(Vector2(300, 150), test_camera->get_far()).is_equal_approx(Vector3(SQRT3, -SQRT3 * 0.5f, -1.0f) * test_camera->get_far()));
		}
	}

	// Uses cases that are the inverse of the above sub-case.
	SUBCASE("unproject_position") {
		SUBCASE("Orthogonal projection") {
			test_camera->set_orthogonal(5.0f, 0.5f, 1000.0f);
			// Center
			CHECK(test_camera->unproject_position(Vector3(0, 0, -0.5f)).is_equal_approx(Vector2(200, 100)));
			// Top left
			CHECK(test_camera->unproject_position(Vector3(-5.0f, 2.5f, -1.5f)).is_equal_approx(Vector2(0, 0)));
			// Bottom right
			CHECK(test_camera->unproject_position(Vector3(5.0f, -2.5f, -5.0f)).is_equal_approx(Vector2(400, 200)));
		}

		SUBCASE("Perspective projection") {
			test_camera->set_perspective(120.0f, 0.5f, 1000.0f);
			// Center.
			CHECK(test_camera->unproject_position(Vector3(0, 0, -0.5f)).is_equal_approx(Vector2(200, 100)));
			CHECK(test_camera->unproject_position(Vector3(0, 0, -100.0f)).is_equal_approx(Vector2(200, 100)));
			// 3/4th way to Top left.
			WARN(test_camera->unproject_position(Vector3(-SQRT3 * 0.5f, SQRT3 * 0.25f, -0.5f)).is_equal_approx(Vector2(100, 50)));
			WARN(test_camera->unproject_position(Vector3(-SQRT3, SQRT3 * 0.5f, -1.0f)).is_equal_approx(Vector2(100, 50)));
			// 3/4th way to Bottom right.
			CHECK(test_camera->unproject_position(Vector3(SQRT3 * 0.5f, -SQRT3 * 0.25f, -0.5f)).is_equal_approx(Vector2(300, 150)));
			CHECK(test_camera->unproject_position(Vector3(SQRT3, -SQRT3 * 0.5f, -1.0f)).is_equal_approx(Vector2(300, 150)));
		}
	}

	memdelete(test_camera);
	memdelete(mock_viewport);
}

TEST_CASE("[SceneTree][Camera3D] Project ray") {
	// Cameras need a viewport to know how to compute their frustums, so we make a fake one here.
	Camera3D *test_camera = memnew(Camera3D);
	SubViewport *mock_viewport = memnew(SubViewport);
	// 4:2.
	mock_viewport->set_size(Vector2(400, 200));
	SceneTree::get_singleton()->get_root()->add_child(mock_viewport);
	mock_viewport->add_child(test_camera);
	test_camera->set_global_position(Vector3(0, 0, 0));
	test_camera->set_global_rotation(Vector3(0, 0, 0));
	test_camera->set_keep_aspect_mode(Camera3D::KeepAspect::KEEP_HEIGHT);

	SUBCASE("project_ray_origin") {
		SUBCASE("Orthogonal projection") {
			test_camera->set_orthogonal(5.0f, 0.5f, 1000.0f);
			// Center.
			CHECK(test_camera->project_ray_origin(Vector2(200, 100)).is_equal_approx(Vector3(0, 0, -0.5f)));
			// Top left.
			CHECK(test_camera->project_ray_origin(Vector2(0, 0)).is_equal_approx(Vector3(-5.0f, 2.5f, -0.5f)));
			// Bottom right.
			CHECK(test_camera->project_ray_origin(Vector2(400, 200)).is_equal_approx(Vector3(5.0f, -2.5f, -0.5f)));
		}

		SUBCASE("Perspective projection") {
			test_camera->set_perspective(120.0f, 0.5f, 1000.0f);
			// Center.
			CHECK(test_camera->project_ray_origin(Vector2(200, 100)).is_equal_approx(Vector3(0, 0, 0)));
			// Top left.
			CHECK(test_camera->project_ray_origin(Vector2(0, 0)).is_equal_approx(Vector3(0, 0, 0)));
			// Bottom right.
			CHECK(test_camera->project_ray_origin(Vector2(400, 200)).is_equal_approx(Vector3(0, 0, 0)));
		}
	}

	SUBCASE("project_ray_normal") {
		SUBCASE("Orthogonal projection") {
			test_camera->set_orthogonal(5.0f, 0.5f, 1000.0f);
			// Center.
			CHECK(test_camera->project_ray_normal(Vector2(200, 100)).is_equal_approx(Vector3(0, 0, -1)));
			// Top left.
			CHECK(test_camera->project_ray_normal(Vector2(0, 0)).is_equal_approx(Vector3(0, 0, -1)));
			// Bottom right.
			CHECK(test_camera->project_ray_normal(Vector2(400, 200)).is_equal_approx(Vector3(0, 0, -1)));
		}

		SUBCASE("Perspective projection") {
			test_camera->set_perspective(120.0f, 0.5f, 1000.0f);
			// Center.
			CHECK(test_camera->project_ray_normal(Vector2(200, 100)).is_equal_approx(Vector3(0, 0, -1)));
			// Top left.
			CHECK(test_camera->project_ray_normal(Vector2(0, 0)).is_equal_approx(Vector3(-SQRT3, SQRT3 / 2, -0.5f).normalized()));
			// Bottom right.
			CHECK(test_camera->project_ray_normal(Vector2(400, 200)).is_equal_approx(Vector3(SQRT3, -SQRT3 / 2, -0.5f).normalized()));
		}
	}

	SUBCASE("project_local_ray_normal") {
		test_camera->set_rotation_degrees(Vector3(60, 60, 60));

		SUBCASE("Orthogonal projection") {
			test_camera->set_orthogonal(5.0f, 0.5f, 1000.0f);
			// Center.
			CHECK(test_camera->project_local_ray_normal(Vector2(200, 100)).is_equal_approx(Vector3(0, 0, -1)));
			// Top left.
			CHECK(test_camera->project_local_ray_normal(Vector2(0, 0)).is_equal_approx(Vector3(0, 0, -1)));
			// Bottom right.
			CHECK(test_camera->project_local_ray_normal(Vector2(400, 200)).is_equal_approx(Vector3(0, 0, -1)));
		}

		SUBCASE("Perspective projection") {
			test_camera->set_perspective(120.0f, 0.5f, 1000.0f);
			// Center.
			CHECK(test_camera->project_local_ray_normal(Vector2(200, 100)).is_equal_approx(Vector3(0, 0, -1)));
			// Top left.
			CHECK(test_camera->project_local_ray_normal(Vector2(0, 0)).is_equal_approx(Vector3(-SQRT3, SQRT3 / 2, -0.5f).normalized()));
			// Bottom right.
			CHECK(test_camera->project_local_ray_normal(Vector2(400, 200)).is_equal_approx(Vector3(SQRT3, -SQRT3 / 2, -0.5f).normalized()));
		}
	}

	memdelete(test_camera);
	memdelete(mock_viewport);
}

#undef SQRT3

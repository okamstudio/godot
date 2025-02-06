/**************************************************************************/
/*  test_camera_2d.h                                                      */
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

#include "scene/2d/camera_2d.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "tests/test_macros.h"

namespace TestCamera2D {

TEST_CASE("[SceneTree][Camera2D] Getters and setters") {
	Camera2D *test_camera = memnew(Camera2D);

	SUBCASE("AnchorMode") {
		test_camera->set_anchor_mode(Camera2D::AnchorMode::ANCHOR_MODE_FIXED_TOP_LEFT);
		CHECK(test_camera->get_anchor_mode() == Camera2D::AnchorMode::ANCHOR_MODE_FIXED_TOP_LEFT);
		test_camera->set_anchor_mode(Camera2D::AnchorMode::ANCHOR_MODE_DRAG_CENTER);
		CHECK(test_camera->get_anchor_mode() == Camera2D::AnchorMode::ANCHOR_MODE_DRAG_CENTER);
	}

	SUBCASE("ProcessCallback") {
		test_camera->set_process_callback(Camera2D::Camera2DProcessCallback::CAMERA2D_PROCESS_PHYSICS);
		CHECK(test_camera->get_process_callback() == Camera2D::Camera2DProcessCallback::CAMERA2D_PROCESS_PHYSICS);
		test_camera->set_process_callback(Camera2D::Camera2DProcessCallback::CAMERA2D_PROCESS_IDLE);
		CHECK(test_camera->get_process_callback() == Camera2D::Camera2DProcessCallback::CAMERA2D_PROCESS_IDLE);
	}

	SUBCASE("Drag") {
		constexpr float drag_left_margin = 0.8f;
		constexpr float drag_top_margin = 0.8f;
		constexpr float drag_right_margin = 0.8f;
		constexpr float drag_bottom_margin = 0.8f;
		constexpr float drag_horizontal_offset1 = 0.5f;
		constexpr float drag_horizontal_offset2 = -0.5f;
		constexpr float drag_vertical_offset1 = 0.5f;
		constexpr float drag_vertical_offset2 = -0.5f;
		test_camera->set_drag_margin(SIDE_LEFT, drag_left_margin);
		CHECK(test_camera->get_drag_margin(SIDE_LEFT) == drag_left_margin);
		test_camera->set_drag_margin(SIDE_TOP, drag_top_margin);
		CHECK(test_camera->get_drag_margin(SIDE_TOP) == drag_top_margin);
		test_camera->set_drag_margin(SIDE_RIGHT, drag_right_margin);
		CHECK(test_camera->get_drag_margin(SIDE_RIGHT) == drag_right_margin);
		test_camera->set_drag_margin(SIDE_BOTTOM, drag_bottom_margin);
		CHECK(test_camera->get_drag_margin(SIDE_BOTTOM) == drag_bottom_margin);
		test_camera->set_drag_horizontal_enabled(true);
		CHECK(test_camera->is_drag_horizontal_enabled());
		test_camera->set_drag_horizontal_enabled(false);
		CHECK_FALSE(test_camera->is_drag_horizontal_enabled());
		test_camera->set_drag_horizontal_offset(drag_horizontal_offset1);
		CHECK(test_camera->get_drag_horizontal_offset() == drag_horizontal_offset1);
		test_camera->set_drag_horizontal_offset(drag_horizontal_offset2);
		CHECK(test_camera->get_drag_horizontal_offset() == drag_horizontal_offset2);
		test_camera->set_drag_vertical_enabled(true);
		CHECK(test_camera->is_drag_vertical_enabled());
		test_camera->set_drag_vertical_enabled(false);
		CHECK_FALSE(test_camera->is_drag_vertical_enabled());
		test_camera->set_drag_vertical_offset(drag_vertical_offset1);
		CHECK(test_camera->get_drag_vertical_offset() == drag_vertical_offset1);
		test_camera->set_drag_vertical_offset(drag_vertical_offset2);
		CHECK(test_camera->get_drag_vertical_offset() == drag_vertical_offset2);
	}

	SUBCASE("Drawing") {
		test_camera->set_margin_drawing_enabled(true);
		CHECK(test_camera->is_margin_drawing_enabled());
		test_camera->set_margin_drawing_enabled(false);
		CHECK_FALSE(test_camera->is_margin_drawing_enabled());
		test_camera->set_limit_drawing_enabled(true);
		CHECK(test_camera->is_limit_drawing_enabled());
		test_camera->set_limit_drawing_enabled(false);
		CHECK_FALSE(test_camera->is_limit_drawing_enabled());
		test_camera->set_screen_drawing_enabled(true);
		CHECK(test_camera->is_screen_drawing_enabled());
		test_camera->set_screen_drawing_enabled(false);
		CHECK_FALSE(test_camera->is_screen_drawing_enabled());
	}

	SUBCASE("Enabled") {
		test_camera->set_enabled(true);
		CHECK(test_camera->is_enabled());
		test_camera->set_enabled(false);
		CHECK_FALSE(test_camera->is_enabled());
	}

	SUBCASE("Rotation") {
		constexpr float rotation_smoothing_speed = 20.0f;
		test_camera->set_ignore_rotation(true);
		CHECK(test_camera->is_ignoring_rotation());
		test_camera->set_ignore_rotation(false);
		CHECK_FALSE(test_camera->is_ignoring_rotation());
		test_camera->set_rotation_smoothing_enabled(true);
		CHECK(test_camera->is_rotation_smoothing_enabled());
		test_camera->set_rotation_smoothing_speed(rotation_smoothing_speed);
		CHECK(test_camera->get_rotation_smoothing_speed() == rotation_smoothing_speed);
	}

	SUBCASE("Zoom") {
		const Vector2 zoom = Vector2(4, 4);
		test_camera->set_zoom(zoom);
		CHECK(test_camera->get_zoom() == zoom);
	}

	SUBCASE("Offset") {
		const Vector2 offset = Vector2(100, 100);
		test_camera->set_offset(offset);
		CHECK(test_camera->get_offset() == offset);
	}

	SUBCASE("Limit") {
		constexpr int limit_left = 100;
		constexpr int limit_top = 100;
		constexpr int limit_right = 100;
		constexpr int limit_bottom = 100;
		test_camera->set_limit_smoothing_enabled(true);
		CHECK(test_camera->is_limit_smoothing_enabled());
		test_camera->set_limit_smoothing_enabled(false);
		CHECK_FALSE(test_camera->is_limit_smoothing_enabled());
		test_camera->set_limit(SIDE_LEFT, limit_left);
		CHECK(test_camera->get_limit(SIDE_LEFT) == limit_left);
		test_camera->set_limit(SIDE_TOP, limit_top);
		CHECK(test_camera->get_limit(SIDE_TOP) == limit_top);
		test_camera->set_limit(SIDE_RIGHT, limit_right);
		CHECK(test_camera->get_limit(SIDE_RIGHT) == limit_right);
		test_camera->set_limit(SIDE_BOTTOM, limit_bottom);
		CHECK(test_camera->get_limit(SIDE_BOTTOM) == limit_bottom);
	}

	SUBCASE("Position") {
		constexpr float smoothing_speed = 20.0f;
		test_camera->set_position_smoothing_enabled(true);
		CHECK(test_camera->is_position_smoothing_enabled());
		test_camera->set_position_smoothing_speed(smoothing_speed);
		CHECK(test_camera->get_position_smoothing_speed() == smoothing_speed);
	}

	memdelete(test_camera);
}

TEST_CASE("[SceneTree][Camera2D] Camera positioning") {
	SubViewport *mock_viewport = memnew(SubViewport);
	Camera2D *test_camera = memnew(Camera2D);

	mock_viewport->set_size(Vector2(400, 200));
	SceneTree::get_singleton()->get_root()->add_child(mock_viewport);
	mock_viewport->add_child(test_camera);

	SUBCASE("Anchor mode") {
		test_camera->set_anchor_mode(Camera2D::ANCHOR_MODE_DRAG_CENTER);
		CHECK(test_camera->get_camera_screen_center().is_equal_approx(Vector2(0, 0)));
		CHECK(test_camera->get_camera_position().is_equal_approx(Vector2(0, 0)));

		test_camera->set_anchor_mode(Camera2D::ANCHOR_MODE_FIXED_TOP_LEFT);
		CHECK(test_camera->get_camera_screen_center().is_equal_approx(Vector2(200, 100)));
		CHECK(test_camera->get_camera_position().is_equal_approx(Vector2(0, 0)));
	}

	SUBCASE("Offset") {
		test_camera->set_offset(Vector2(100, 100));
		CHECK(test_camera->get_camera_screen_center().is_equal_approx(Vector2(100, 100)));
		CHECK(test_camera->get_camera_position().is_equal_approx(Vector2(0, 0)));

		test_camera->set_offset(Vector2(-100, 300));
		CHECK(test_camera->get_camera_screen_center().is_equal_approx(Vector2(-100, 300)));
		CHECK(test_camera->get_camera_position().is_equal_approx(Vector2(0, 0)));

		test_camera->set_offset(Vector2(0, 0));
		CHECK(test_camera->get_camera_screen_center().is_equal_approx(Vector2(0, 0)));
		CHECK(test_camera->get_camera_position().is_equal_approx(Vector2(0, 0)));
	}

	SUBCASE("Limits") {
		test_camera->set_limit(SIDE_LEFT, 100);
		test_camera->set_limit(SIDE_TOP, 50);

		CHECK(test_camera->get_camera_screen_center().is_equal_approx(Vector2(300, 150)));
		CHECK(test_camera->get_camera_position().is_equal_approx(Vector2(0, 0)));

		test_camera->set_limit(SIDE_LEFT, 0);
		test_camera->set_limit(SIDE_TOP, 0);

		CHECK(test_camera->get_camera_screen_center().is_equal_approx(Vector2(200, 100)));
		CHECK(test_camera->get_camera_position().is_equal_approx(Vector2(0, 0)));
	}

	SUBCASE("Drag") {
		CHECK(test_camera->get_camera_screen_center().is_equal_approx(Vector2(0, 0)));

		// horizontal
		test_camera->set_drag_horizontal_enabled(true);
		test_camera->set_drag_margin(SIDE_RIGHT, 0.5);

		test_camera->set_position(Vector2(100, 100));
		test_camera->force_update_scroll();
		CHECK(test_camera->get_camera_position().is_equal_approx(Vector2(0, 100)));
		CHECK(test_camera->get_camera_screen_center().is_equal_approx(Vector2(0, 100)));
		test_camera->set_position(Vector2(101, 101));
		test_camera->force_update_scroll();
		CHECK(test_camera->get_camera_position().is_equal_approx(Vector2(1, 101)));
		CHECK(test_camera->get_camera_screen_center().is_equal_approx(Vector2(1, 101)));

		// test align
		test_camera->set_position(Vector2(0, 0));
		test_camera->align();
		CHECK(test_camera->get_camera_position().is_equal_approx(Vector2(0, 0)));
		CHECK(test_camera->get_camera_screen_center().is_equal_approx(Vector2(0, 0)));

		// vertical
		test_camera->set_drag_vertical_enabled(true);
		test_camera->set_drag_horizontal_enabled(false);
		test_camera->set_drag_margin(SIDE_TOP, 0.3);

		test_camera->set_position(Vector2(200, -20));
		test_camera->force_update_scroll();
		CHECK(test_camera->get_camera_position().is_equal_approx(Vector2(200, 0)));
		CHECK(test_camera->get_camera_screen_center().is_equal_approx(Vector2(200, 0)));
		test_camera->set_position(Vector2(250, -55));
		test_camera->force_update_scroll();
		CHECK(test_camera->get_camera_position().is_equal_approx(Vector2(250, -25)));
		CHECK(test_camera->get_camera_screen_center().is_equal_approx(Vector2(250, -25)));
	}

	memdelete(test_camera);
	memdelete(mock_viewport);
}

TEST_CASE("[SceneTree][Camera2D] Transforms") {
	SubViewport *mock_viewport = memnew(SubViewport);
	Camera2D *test_camera = memnew(Camera2D);

	mock_viewport->set_size(Vector2(400, 200));
	SceneTree::get_singleton()->get_root()->add_child(mock_viewport);
	mock_viewport->add_child(test_camera);

	SUBCASE("Default camera") {
		Transform2D xform = mock_viewport->get_canvas_transform();
		// x,y are basis vectors, origin = screen center
		Transform2D test_xform = Transform2D(Vector2(1, 0), Vector2(0, 1), Vector2(200, 100));
		CHECK(xform.is_equal_approx(test_xform));
	}

	SUBCASE("Zoom") {
		test_camera->set_zoom(Vector2(0.5, 2));
		Transform2D xform = mock_viewport->get_canvas_transform();
		Transform2D test_xform = Transform2D(Vector2(0.5, 0), Vector2(0, 2), Vector2(200, 100));
		CHECK(xform.is_equal_approx(test_xform));

		test_camera->set_zoom(Vector2(10, 10));
		xform = mock_viewport->get_canvas_transform();
		test_xform = Transform2D(Vector2(10, 0), Vector2(0, 10), Vector2(200, 100));
		CHECK(xform.is_equal_approx(test_xform));

		test_camera->set_zoom(Vector2(1, 1));
		xform = mock_viewport->get_canvas_transform();
		test_xform = Transform2D(Vector2(1, 0), Vector2(0, 1), Vector2(200, 100));
		CHECK(xform.is_equal_approx(test_xform));
	}

	SUBCASE("Rotation") {
		test_camera->set_rotation(Math_PI / 2);
		Transform2D xform = mock_viewport->get_canvas_transform();
		Transform2D test_xform = Transform2D(Vector2(1, 0), Vector2(0, 1), Vector2(200, 100));
		CHECK(xform.is_equal_approx(test_xform));

		test_camera->set_ignore_rotation(false);
		xform = mock_viewport->get_canvas_transform();
		test_xform = Transform2D(Vector2(0, -1), Vector2(1, 0), Vector2(200, 100));
		CHECK(xform.is_equal_approx(test_xform));

		test_camera->set_rotation(-1 * Math_PI);
		test_camera->force_update_scroll();
		xform = mock_viewport->get_canvas_transform();
		test_xform = Transform2D(Vector2(-1, 0), Vector2(0, -1), Vector2(200, 100));
		CHECK(xform.is_equal_approx(test_xform));

		test_camera->set_rotation(0);
		test_camera->force_update_scroll();
		xform = mock_viewport->get_canvas_transform();
		test_xform = Transform2D(Vector2(1, 0), Vector2(0, 1), Vector2(200, 100));
		CHECK(xform.is_equal_approx(test_xform));
	}

	memdelete(test_camera);
	memdelete(mock_viewport);
}

} // namespace TestCamera2D

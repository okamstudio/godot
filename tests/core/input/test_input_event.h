/**************************************************************************/
/*  test_input_event.h                                                    */
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

#include "core/input/input_event.h"
#include "core/math/rect2.h"
#include "core/os/memory.h"
#include "core/variant/array.h"

#include "tests/test_macros.h"

namespace TestInputEvent {
TEST_CASE("[InputEvent] Signal is emitted when device is changed") {
	Ref<InputEventKey> input_event;
	input_event.instantiate();

	SIGNAL_WATCH(*input_event, CoreStringName(changed));
	Array args1;
	Array empty_args;
	empty_args.push_back(args1);

	input_event->set_device(1);

	SIGNAL_CHECK("changed", empty_args);
	CHECK(input_event->get_device() == 1);

	SIGNAL_UNWATCH(*input_event, CoreStringName(changed));
}

TEST_CASE("[InputEvent] Test accumulate") {
	Ref<InputEventMouseMotion> iemm1, iemm2;
	Ref<InputEventKey> iek;

	iemm1.instantiate(), iemm2.instantiate();
	iek.instantiate();

	iemm1->set_button_mask(MouseButtonMask::LEFT);

	CHECK_FALSE(iemm1->accumulate(iemm2));

	iemm2->set_button_mask(MouseButtonMask::LEFT);

	CHECK(iemm1->accumulate(iemm2));

	CHECK_FALSE(iemm1->accumulate(iek));
	CHECK_FALSE(iemm2->accumulate(iek));
}

TEST_CASE("[InputEvent][SceneTree] Test methods that interact with the InputMap") {
	const String mock_action = "mock_action";
	Ref<InputEventJoypadMotion> iejm;
	iejm.instantiate();

	InputMap::get_singleton()->add_action(mock_action, 0.5);
	InputMap::get_singleton()->action_add_event(mock_action, iejm);

	CHECK(iejm->is_action_type());
	CHECK(iejm->is_action(mock_action));

	CHECK(iejm->is_action_released(mock_action));
	CHECK(Math::is_equal_approx(iejm->get_action_strength(mock_action), 0.0f));

	iejm->set_axis_value(0.8f);
	// Since deadzone is 0.5, action_strength grows linearly from 0.5 to 1.0.
	CHECK(Math::is_equal_approx(iejm->get_action_strength(mock_action), 0.6f));
	CHECK(Math::is_equal_approx(iejm->get_action_raw_strength(mock_action), 0.8f));
	CHECK(iejm->is_action_pressed(mock_action));

	InputMap::get_singleton()->erase_action(mock_action);
}

TEST_CASE("[InputEvent] Test xformed_by") {
	Ref<InputEventMouseMotion> iemm1;
	iemm1.instantiate();

	iemm1->set_position(Vector2(0.0f, 0.0f));
	Transform2D transform;
	transform = transform.translated(Vector2(2.0f, 3.0f));

	Ref<InputEventMouseMotion> iemm2 = iemm1->xformed_by(transform);

	CHECK(iemm2->get_position().is_equal_approx(Vector2(2.0f, 3.0f)));
}
} // namespace TestInputEvent

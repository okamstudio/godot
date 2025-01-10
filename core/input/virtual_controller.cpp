/**************************************************************************/
/*  virtual_controller.cpp                                                */
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

#include "virtual_controller.h"

void VirtualController::_bind_methods() {
	ClassDB::bind_method(D_METHOD("enable"), &VirtualController::enable);
	ClassDB::bind_method(D_METHOD("disable"), &VirtualController::disable);
	ClassDB::bind_method(D_METHOD("is_enabled"), &VirtualController::is_enabled);
	ClassDB::bind_method(D_METHOD("set_enabled_left_thumbstick", "enable"), &VirtualController::set_enabled_left_thumbstick);
	ClassDB::bind_method(D_METHOD("is_enabled_left_thumbstick"), &VirtualController::is_enabled_left_thumbstick);
	ClassDB::bind_method(D_METHOD("set_enabled_right_thumbstick", "enable"), &VirtualController::set_enabled_right_thumbstick);
	ClassDB::bind_method(D_METHOD("is_enabled_right_thumbstick"), &VirtualController::is_enabled_right_thumbstick);
	ClassDB::bind_method(D_METHOD("set_enabled_button_a", "enable"), &VirtualController::set_enabled_button_a);
	ClassDB::bind_method(D_METHOD("is_enabled_button_a"), &VirtualController::is_enabled_button_a);
	ClassDB::bind_method(D_METHOD("set_enabled_button_b", "enable"), &VirtualController::set_enabled_button_b);
	ClassDB::bind_method(D_METHOD("is_enabled_button_b"), &VirtualController::is_enabled_button_b);
	ClassDB::bind_method(D_METHOD("set_enabled_button_x", "enable"), &VirtualController::set_enabled_button_x);
	ClassDB::bind_method(D_METHOD("is_enabled_button_x"), &VirtualController::is_enabled_button_x);
	ClassDB::bind_method(D_METHOD("set_enabled_button_y", "enable"), &VirtualController::set_enabled_button_y);
	ClassDB::bind_method(D_METHOD("is_enabled_button_y"), &VirtualController::is_enabled_button_y);
}

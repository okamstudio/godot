/**************************************************************************/
/*  openxr_haptic_feedback.cpp                                            */
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

#include "openxr_haptic_feedback.h"

////////////////////////////////////////////////////////////////////////////
// OpenXRHapticBase

void OpenXRHapticBase::_bind_methods() {
}

////////////////////////////////////////////////////////////////////////////
// OpenXRHapticVibration

void OpenXRHapticVibration::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_duration", "duration"), &OpenXRHapticVibration::set_duration);
	ClassDB::bind_method(D_METHOD("get_duration"), &OpenXRHapticVibration::get_duration);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "duration"), "set_duration", "get_duration");

	ClassDB::bind_method(D_METHOD("set_frequency", "frequency"), &OpenXRHapticVibration::set_frequency);
	ClassDB::bind_method(D_METHOD("get_frequency"), &OpenXRHapticVibration::get_frequency);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "frequency"), "set_frequency", "get_frequency");

	ClassDB::bind_method(D_METHOD("set_amplitude", "amplitude"), &OpenXRHapticVibration::set_amplitude);
	ClassDB::bind_method(D_METHOD("get_amplitude"), &OpenXRHapticVibration::get_amplitude);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "amplitude", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_amplitude", "get_amplitude");
}

void OpenXRHapticVibration::set_duration(int64_t p_duration) {
	haptic_vibration.duration = p_duration;
	emit_changed();
}

int64_t OpenXRHapticVibration::get_duration() const {
	return haptic_vibration.duration;
}

void OpenXRHapticVibration::set_frequency(float p_frequency) {
	haptic_vibration.frequency = p_frequency;
	emit_changed();
}

float OpenXRHapticVibration::get_frequency() const {
	return haptic_vibration.frequency;
}

void OpenXRHapticVibration::set_amplitude(float p_amplitude) {
	haptic_vibration.amplitude = p_amplitude;
	emit_changed();
}

float OpenXRHapticVibration::get_amplitude() const {
	return haptic_vibration.amplitude;
}

const XrHapticBaseHeader *OpenXRHapticVibration::get_xr_structure() {
	return (XrHapticBaseHeader *)&haptic_vibration;
}

OpenXRHapticVibration::OpenXRHapticVibration() {
	haptic_vibration.type = XR_TYPE_HAPTIC_VIBRATION;
	haptic_vibration.next = nullptr;
	haptic_vibration.duration = -1;
	haptic_vibration.frequency = 0.0;
	haptic_vibration.amplitude = 1.0;
}

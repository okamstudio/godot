/**************************************************************************/
/*  openxr_haptic_feedback.h                                              */
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

#include "core/io/resource.h"
#include <openxr/openxr.h>

class OpenXRHapticBase : public Resource {
	GDCLASS(OpenXRHapticBase, Resource);

private:
protected:
	static void _bind_methods();

public:
	virtual const XrHapticBaseHeader *get_xr_structure() = 0;
};

class OpenXRHapticVibration : public OpenXRHapticBase {
	GDCLASS(OpenXRHapticVibration, OpenXRHapticBase);

private:
	XrHapticVibration haptic_vibration;

protected:
	static void _bind_methods();

public:
	void set_duration(int64_t p_duration);
	int64_t get_duration() const;

	void set_frequency(float p_frequency);
	float get_frequency() const;

	void set_amplitude(float p_amplitude);
	float get_amplitude() const;

	virtual const XrHapticBaseHeader *get_xr_structure() override;

	OpenXRHapticVibration();
};

/**************************************************************************/
/*  openxr_eye_gaze_interaction.h                                         */
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

#include "openxr_extension_wrapper.h"

class OpenXREyeGazeInteractionExtension : public OpenXRExtensionWrapper {
public:
	static OpenXREyeGazeInteractionExtension *get_singleton();

	OpenXREyeGazeInteractionExtension();
	~OpenXREyeGazeInteractionExtension();

	virtual HashMap<String, bool *> get_requested_extensions() override;
	virtual void *set_system_properties_and_get_next_pointer(void *p_next_pointer) override;

	PackedStringArray get_suggested_tracker_names() override;

	bool is_available();
	bool supports_eye_gaze_interaction();

	virtual void on_register_metadata() override;

	bool get_eye_gaze_pose(double p_dist, Vector3 &r_eye_pose);

private:
	static OpenXREyeGazeInteractionExtension *singleton;

	bool available = false;
	XrSystemEyeGazeInteractionPropertiesEXT properties;

	bool init_eye_gaze_pose = false;
	RID eye_tracker;
	RID eye_action;
};

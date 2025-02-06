/**************************************************************************/
/*  webxr_interface.h                                                     */
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

#include "servers/xr/xr_controller_tracker.h"
#include "servers/xr/xr_interface.h"

/**
	The WebXR interface is a VR/AR interface that can be used on the web.
*/

class WebXRInterface : public XRInterface {
	GDCLASS(WebXRInterface, XRInterface);

protected:
	static void _bind_methods();

#ifndef DISABLE_DEPRECATED
	static void _bind_compatibility_methods();
	Ref<XRPositionalTracker> _get_input_source_tracker_bind_compat_90645(int p_input_source_id) const;
#endif

public:
	enum TargetRayMode {
		TARGET_RAY_MODE_UNKNOWN,
		TARGET_RAY_MODE_GAZE,
		TARGET_RAY_MODE_TRACKED_POINTER,
		TARGET_RAY_MODE_SCREEN,
	};

	virtual void is_session_supported(const String &p_session_mode) = 0;
	virtual void set_session_mode(String p_session_mode) = 0;
	virtual String get_session_mode() const = 0;
	virtual void set_required_features(String p_required_features) = 0;
	virtual String get_required_features() const = 0;
	virtual void set_optional_features(String p_optional_features) = 0;
	virtual String get_optional_features() const = 0;
	virtual void set_requested_reference_space_types(String p_requested_reference_space_types) = 0;
	virtual String get_requested_reference_space_types() const = 0;
	virtual String get_reference_space_type() const = 0;
	virtual String get_enabled_features() const = 0;
	virtual bool is_input_source_active(int p_input_source_id) const = 0;
	virtual Ref<XRControllerTracker> get_input_source_tracker(int p_input_source_id) const = 0;
	virtual TargetRayMode get_input_source_target_ray_mode(int p_input_source_id) const = 0;
	virtual String get_visibility_state() const = 0;
	virtual float get_display_refresh_rate() const = 0;
	virtual void set_display_refresh_rate(float p_refresh_rate) = 0;
	virtual Array get_available_display_refresh_rates() const = 0;
};

VARIANT_ENUM_CAST(WebXRInterface::TargetRayMode);

/**************************************************************************/
/*  openxr_dpad_binding_extension.h                                       */
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

#include "../action_map/openxr_action_set.h"
#include "../action_map/openxr_binding_modifier.h"
#include "../action_map/openxr_haptic_feedback.h"
#include "../util.h"
#include "openxr_extension_wrapper.h"

class OpenXRDPadBindingExtension : public OpenXRExtensionWrapper {
public:
	static OpenXRDPadBindingExtension *get_singleton();

	OpenXRDPadBindingExtension();
	virtual ~OpenXRDPadBindingExtension() override;

	virtual HashMap<String, bool *> get_requested_extensions() override;

	bool is_available();

private:
	static OpenXRDPadBindingExtension *singleton;

	bool binding_modifier_ext = false;
	bool dpad_binding_ext = false;
};

class OpenXRDpadBindingModifier : public OpenXRIPBindingModifier {
	GDCLASS(OpenXRDpadBindingModifier, OpenXRIPBindingModifier);

private:
	PackedByteArray dpad_bindings_data;
	XrInteractionProfileDpadBindingEXT *dpad_bindings = nullptr;
	String input_path;
	Ref<OpenXRActionSet> action_set;
	Ref<OpenXRHapticBase> on_haptic;
	Ref<OpenXRHapticBase> off_haptic;

protected:
	static void _bind_methods();

public:
	OpenXRDpadBindingModifier();

	void set_action_set(const Ref<OpenXRActionSet> p_action_set);
	Ref<OpenXRActionSet> get_action_set() const;

	void set_input_path(const String &p_input_path);
	String get_input_path() const;

	void set_threshold(float p_threshold);
	float get_threshold() const;

	void set_threshold_released(float p_threshold);
	float get_threshold_released() const;

	void set_center_region(float p_center_region);
	float get_center_region() const;

	void set_wedge_angle(float p_wedge_angle);
	float get_wedge_angle() const;

	void set_wedge_angle_deg(float p_wedge_angle);
	float get_wedge_angle_deg() const;

	void set_is_sticky(bool p_sticky);
	bool get_is_sticky() const;

	void set_on_haptic(const Ref<OpenXRHapticBase> &p_haptic);
	Ref<OpenXRHapticBase> get_on_haptic() const;

	void set_off_haptic(const Ref<OpenXRHapticBase> &p_haptic);
	Ref<OpenXRHapticBase> get_off_haptic() const;

	virtual String get_description() const override { return "DPad modifier"; }
	virtual PackedByteArray get_ip_modification() override;
};

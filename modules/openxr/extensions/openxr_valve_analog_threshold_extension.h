/**************************************************************************/
/*  openxr_valve_analog_threshold_extension.h                             */
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

#ifndef OPENXR_VALVE_ANALOG_THRESHOLD_EXTENSION_H
#define OPENXR_VALVE_ANALOG_THRESHOLD_EXTENSION_H

#include "../action_map/openxr_binding_modifier.h"
#include "../action_map/openxr_haptic_feedback.h"
#include "../util.h"
#include "core/io/resource.h"
#include "openxr_extension_wrapper.h"

class OpenXRValveAnalogThresholdExtension : public OpenXRExtensionWrapper {
public:
	static OpenXRValveAnalogThresholdExtension *get_singleton();

	OpenXRValveAnalogThresholdExtension();
	virtual ~OpenXRValveAnalogThresholdExtension() override;

	virtual HashMap<String, bool *> get_requested_extensions() override;

	bool is_available();

private:
	static OpenXRValveAnalogThresholdExtension *singleton;

	bool binding_modifier_ext = false;
	bool threshold_ext = false;
};

class OpenXRAnalogThresholdModifier : public OpenXRActionBindingModifier {
	GDCLASS(OpenXRAnalogThresholdModifier, OpenXRActionBindingModifier);

private:
	XrInteractionProfileAnalogThresholdVALVE analog_threshold;
	Ref<OpenXRHapticBase> on_haptic;
	Ref<OpenXRHapticBase> off_haptic;

protected:
	static void _bind_methods();

public:
	OpenXRAnalogThresholdModifier();

	void set_on_threshold(float p_threshold);
	float get_on_threshold() const;

	void set_off_threshold(float p_threshold);
	float get_off_threshold() const;

	void set_on_haptic(const Ref<OpenXRHapticBase> &p_haptic);
	Ref<OpenXRHapticBase> get_on_haptic() const;

	void set_off_haptic(const Ref<OpenXRHapticBase> &p_haptic);
	Ref<OpenXRHapticBase> get_off_haptic() const;

	virtual String get_description() const override { return "Analog threshold modifier"; }
	virtual PackedByteArray get_ip_modification() override;
};

#endif // OPENXR_VALVE_ANALOG_THRESHOLD_EXTENSION_H

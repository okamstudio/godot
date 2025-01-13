/**************************************************************************/
/*  openxr_valve_analog_threshold_extension.cpp                           */
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

#include "openxr_valve_analog_threshold_extension.h"
#include "../action_map/openxr_action_set.h"
#include "../action_map/openxr_interaction_profile.h"
#include "../openxr_api.h"

// Implementation for:
// https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html#XR_VALVE_analog_threshold

///////////////////////////////////////////////////////////////////////////////////////////////////
// OpenXRValveAnalogThresholdExtension

OpenXRValveAnalogThresholdExtension *OpenXRValveAnalogThresholdExtension::singleton = nullptr;

OpenXRValveAnalogThresholdExtension *OpenXRValveAnalogThresholdExtension::get_singleton() {
	return singleton;
}

OpenXRValveAnalogThresholdExtension::OpenXRValveAnalogThresholdExtension() {
	singleton = this;
}

OpenXRValveAnalogThresholdExtension::~OpenXRValveAnalogThresholdExtension() {
	singleton = nullptr;
}

HashMap<String, bool *> OpenXRValveAnalogThresholdExtension::get_requested_extensions() {
	HashMap<String, bool *> request_extensions;

	// Note, we're dependent on the binding modifier extension, this may be requested by multiple extension wrappers.
	request_extensions[XR_KHR_BINDING_MODIFICATION_EXTENSION_NAME] = &binding_modifier_ext;
	request_extensions[XR_VALVE_ANALOG_THRESHOLD_EXTENSION_NAME] = &threshold_ext;

	return request_extensions;
}

bool OpenXRValveAnalogThresholdExtension::is_available() {
	return binding_modifier_ext && threshold_ext;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// OpenXRAnalogThresholdModifier

void OpenXRAnalogThresholdModifier::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_on_threshold", "on_threshold"), &OpenXRAnalogThresholdModifier::set_on_threshold);
	ClassDB::bind_method(D_METHOD("get_on_threshold"), &OpenXRAnalogThresholdModifier::get_on_threshold);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "on_threshold", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_on_threshold", "get_on_threshold");

	ClassDB::bind_method(D_METHOD("set_off_threshold", "off_threshold"), &OpenXRAnalogThresholdModifier::set_off_threshold);
	ClassDB::bind_method(D_METHOD("get_off_threshold"), &OpenXRAnalogThresholdModifier::get_off_threshold);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "off_threshold", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_off_threshold", "get_off_threshold");

	ClassDB::bind_method(D_METHOD("set_on_haptic", "haptic"), &OpenXRAnalogThresholdModifier::set_on_haptic);
	ClassDB::bind_method(D_METHOD("get_on_haptic"), &OpenXRAnalogThresholdModifier::get_on_haptic);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "on_haptic", PROPERTY_HINT_RESOURCE_TYPE, "OpenXRHapticBase"), "set_on_haptic", "get_on_haptic");

	ClassDB::bind_method(D_METHOD("set_off_haptic", "haptic"), &OpenXRAnalogThresholdModifier::set_off_haptic);
	ClassDB::bind_method(D_METHOD("get_off_haptic"), &OpenXRAnalogThresholdModifier::get_off_haptic);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "off_haptic", PROPERTY_HINT_RESOURCE_TYPE, "OpenXRHapticBase"), "set_off_haptic", "get_off_haptic");
}

OpenXRAnalogThresholdModifier::OpenXRAnalogThresholdModifier() {
	analog_threshold.type = XR_TYPE_INTERACTION_PROFILE_ANALOG_THRESHOLD_VALVE;
	analog_threshold.next = nullptr;

	analog_threshold.onThreshold = 0.6;
	analog_threshold.offThreshold = 0.4;
}

void OpenXRAnalogThresholdModifier::set_on_threshold(float p_threshold) {
	ERR_FAIL_COND(p_threshold < 0.0 || p_threshold > 1.0);

	analog_threshold.onThreshold = p_threshold;
	emit_changed();
}

float OpenXRAnalogThresholdModifier::get_on_threshold() const {
	return analog_threshold.onThreshold;
}

void OpenXRAnalogThresholdModifier::set_off_threshold(float p_threshold) {
	ERR_FAIL_COND(p_threshold < 0.0 || p_threshold > 1.0);

	analog_threshold.offThreshold = p_threshold;
	emit_changed();
}

float OpenXRAnalogThresholdModifier::get_off_threshold() const {
	return analog_threshold.offThreshold;
}

void OpenXRAnalogThresholdModifier::set_on_haptic(const Ref<OpenXRHapticBase> &p_haptic) {
	on_haptic = p_haptic;
	emit_changed();
}

Ref<OpenXRHapticBase> OpenXRAnalogThresholdModifier::get_on_haptic() const {
	return on_haptic;
}

void OpenXRAnalogThresholdModifier::set_off_haptic(const Ref<OpenXRHapticBase> &p_haptic) {
	off_haptic = p_haptic;
	emit_changed();
}

Ref<OpenXRHapticBase> OpenXRAnalogThresholdModifier::get_off_haptic() const {
	return off_haptic;
}

PackedByteArray OpenXRAnalogThresholdModifier::get_ip_modification() {
	PackedByteArray ret;

	OpenXRAPI *openxr_api = OpenXRAPI::get_singleton();
	ERR_FAIL_NULL_V(openxr_api, ret);

	OpenXRValveAnalogThresholdExtension *analog_threshold_ext = OpenXRValveAnalogThresholdExtension::get_singleton();
	if (!analog_threshold_ext || !analog_threshold_ext->is_available()) {
		// Extension not enabled!
		WARN_PRINT("Analog threshold extension is not enabled or available.");
		return ret;
	}

	ERR_FAIL_NULL_V(ip_binding, ret);

	Ref<OpenXRAction> action = ip_binding->get_action();
	ERR_FAIL_COND_V(!action.is_valid(), ret);

	// Get our action set
	Ref<OpenXRActionSet> action_set = action->get_action_set();
	ERR_FAIL_COND_V(!action_set.is_valid(), ret);
	RID action_set_rid = openxr_api->find_action_set(action_set->get_name());
	ERR_FAIL_COND_V(!action_set_rid.is_valid(), ret);

	// Get our action
	RID action_rid = openxr_api->find_action(action->get_name(), action_set_rid);
	ERR_FAIL_COND_V(!action_rid.is_valid(), ret);

	analog_threshold.action = openxr_api->action_get_handle(action_rid);

	analog_threshold.binding = openxr_api->get_xr_path(ip_binding->get_binding_path());
	ERR_FAIL_COND_V(analog_threshold.binding == XR_NULL_PATH, ret);

	// These are set already:
	// - analog_threshold.onThreshold
	// - analog_threshold.offThreshold

	if (on_haptic.is_valid()) {
		analog_threshold.onHaptic = on_haptic->get_xr_structure();
	} else {
		analog_threshold.onHaptic = nullptr;
	}

	if (off_haptic.is_valid()) {
		analog_threshold.offHaptic = off_haptic->get_xr_structure();
	} else {
		analog_threshold.offHaptic = nullptr;
	}

	// Copy into byte array so we can return it.
	ERR_FAIL_COND_V(ret.resize(sizeof(XrInteractionProfileAnalogThresholdVALVE)) != OK, ret);
	memcpy(ret.ptrw(), &analog_threshold, sizeof(XrInteractionProfileAnalogThresholdVALVE));

	return ret;
}
